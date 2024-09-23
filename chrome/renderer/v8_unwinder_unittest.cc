// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/v8_unwinder.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/profiler/module_cache.h"
#include "base/profiler/stack_sampling_profiler_test_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "gin/public/isolate_holder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace {

using ::testing::AllOf;
using ::testing::AnyOf;
using ::testing::Contains;
using ::testing::Eq;
using ::testing::Field;
using ::testing::NotNull;
using ::testing::Pointee;
using ::testing::Property;

v8::Local<v8::String> ToV8String(const char* str) {
  return v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), str)
      .ToLocalChecked();
}

v8::Local<v8::Object> CreatePointerHolder(const void* ptr) {
  v8::Local<v8::ObjectTemplate> object_template =
      v8::ObjectTemplate::New(v8::Isolate::GetCurrent());
  object_template->SetInternalFieldCount(1);
  v8::Local<v8::Object> holder =
      object_template
          ->NewInstance(v8::Isolate::GetCurrent()->GetCurrentContext())
          .ToLocalChecked();
  holder->SetAlignedPointerInInternalField(0, const_cast<void*>(ptr));
  return holder;
}

template <typename T>
T* GetPointerFromHolder(v8::Local<v8::Object> holder) {
  return reinterpret_cast<T*>(holder->GetAlignedPointerFromInternalField(0));
}

// Sets up the environment necessary to execute V8 code.
class ScopedV8Environment {
 public:
  ScopedV8Environment()
      : isolate_holder_(task_environment_.GetMainThreadTaskRunner(),
                        gin::IsolateHolder::IsolateType::kBlinkMainThread) {
    isolate()->Enter();
    v8::HandleScope handle_scope(isolate());
    context_.Reset(isolate(), v8::Context::New(isolate()));
    v8::Local<v8::Context>::New(isolate(), context_)->Enter();
  }

  ~ScopedV8Environment() {
    {
      v8::HandleScope handle_scope(isolate());
      v8::Local<v8::Context>::New(isolate(), context_)->Exit();
      context_.Reset();
    }
    isolate()->Exit();
  }

  v8::Isolate* isolate() { return isolate_holder_.isolate(); }

 private:
  base::test::TaskEnvironment task_environment_;
  gin::IsolateHolder isolate_holder_;
  v8::Persistent<v8::Context> context_;
};

// C++ function to be invoked from V8 which calls back into the provided closure
// pointer (passed via a holder object) to wait for a stack sample to be taken.
void WaitForSampleNative(const v8::FunctionCallbackInfo<v8::Value>& info) {
  base::OnceClosure* wait_for_sample =
      GetPointerFromHolder<base::OnceClosure>(info[0].As<v8::Object>());
  if (wait_for_sample)
    std::move(*wait_for_sample).Run();
}

// Causes a stack sample to be taken after setting up a call stack from C++ to
// JavaScript and back into C++.
base::FunctionAddressRange CallThroughV8(
    const base::RepeatingCallback<void(v8::Isolate*)>& report_isolate,
    base::OnceClosure wait_for_sample) {
  const void* start_program_counter = base::GetProgramCounter();

  if (wait_for_sample) {
    // Set up V8 runtime environment.
    // Allows use of natives (functions starting with '%') within JavaScript
    // code, which allows us to control compilation of the JavaScript function
    // we define.
    // TODO(wittman): The flag should be set only for the duration of this test
    // but the V8 API currently doesn't support this. http://crbug.com/v8/9210
    // covers adding the necessary functionality to V8.
    v8::V8::SetFlagsFromString("--allow-natives-syntax");
    ScopedV8Environment v8_environment;
    v8::Isolate* isolate = v8_environment.isolate();
    report_isolate.Run(isolate);
    v8::HandleScope handle_scope(isolate);
    v8::Local<v8::Context> context = isolate->GetCurrentContext();

    // Define a V8 function WaitForSampleNative() backed by the C++ function
    // WaitForSampleNative().
    v8::Local<v8::FunctionTemplate> js_wait_for_sample_native_template =
        v8::FunctionTemplate::New(isolate, WaitForSampleNative);
    v8::Local<v8::Function> js_wait_for_sample_native =
        js_wait_for_sample_native_template->GetFunction(context)
            .ToLocalChecked();
    js_wait_for_sample_native->SetName(ToV8String("WaitForSampleNative"));
    context->Global()
        ->Set(context, ToV8String("WaitForSampleNative"),
              js_wait_for_sample_native)
        .FromJust();

    // Run a script to create the V8 function waitForSample() that invokes
    // WaitForSampleNative(), and a function that ensures that waitForSample()
    // gets compiled. waitForSample() just passes the holder object for the
    // pointer to the wait_for_sample Closure back into the C++ code. We ensure
    // that the function is compiled to test walking through both builtin and
    // runtime-generated code.
    const char kWaitForSampleJs[] = R"(
        function waitForSample(closure_pointer_holder) {
          if (closure_pointer_holder)
            WaitForSampleNative(closure_pointer_holder);
        }

        // Set up the function to be compiled rather than interpreted.
        function compileWaitForSample(closure_pointer_holder) {
          %PrepareFunctionForOptimization(waitForSample);
          waitForSample(closure_pointer_holder);
          waitForSample(closure_pointer_holder);
          %OptimizeFunctionOnNextCall(waitForSample);
        }
        )";
    v8::Local<v8::Script> script =
        v8::Script::Compile(context, ToV8String(kWaitForSampleJs))
            .ToLocalChecked();
    script->Run(context).ToLocalChecked();

    // Run compileWaitForSample(), using a null closure pointer to avoid
    // actually waiting.
    v8::Local<v8::Function> js_compile_wait_for_sample =
        v8::Local<v8::Function>::Cast(
            context->Global()
                ->Get(context, ToV8String("compileWaitForSample"))
                .ToLocalChecked());
    v8::Local<v8::Value> argv[] = {CreatePointerHolder(nullptr)};
    js_compile_wait_for_sample
        ->Call(context, v8::Undefined(isolate), std::size(argv), argv)
        .ToLocalChecked();

    // Run waitForSample() with the real closure pointer.
    argv[0] = CreatePointerHolder(&wait_for_sample);
    v8::Local<v8::Function> js_wait_for_sample = v8::Local<v8::Function>::Cast(
        context->Global()
            ->Get(context, ToV8String("waitForSample"))
            .ToLocalChecked());
    js_wait_for_sample
        ->Call(context, v8::Undefined(isolate), std::size(argv), argv)
        .ToLocalChecked();
  }

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile end_program_counter = base::GetProgramCounter();
  return {start_program_counter, end_program_counter};
}

class UpdateModulesTestUnwinder : public V8Unwinder {
 public:
  explicit UpdateModulesTestUnwinder(v8::Isolate* isolate)
      : V8Unwinder(isolate) {}

  void SetCodePages(std::vector<v8::MemoryRange> code_pages) {
    code_pages_to_provide_ = code_pages;
  }

 protected:
  size_t CopyCodePages(size_t capacity, v8::MemoryRange* code_pages) override {
    std::copy_n(code_pages_to_provide_.begin(),
                std::min(capacity, code_pages_to_provide_.size()), code_pages);
    return code_pages_to_provide_.size();
  }

 private:
  std::vector<v8::MemoryRange> code_pages_to_provide_;
};

v8::MemoryRange GetEmbeddedCodeRange(v8::Isolate* isolate) {
  v8::MemoryRange range;
  isolate->GetEmbeddedCodeRange(&range.start, &range.length_in_bytes);
  return range;
}

}  // namespace

TEST(V8UnwinderTest, EmbeddedCodeRangeModule) {
  ScopedV8Environment v8_environment;
  V8Unwinder unwinder(v8_environment.isolate());
  base::ModuleCache module_cache;

  unwinder.Initialize(&module_cache);

  v8::MemoryRange embedded_code_range;
  v8_environment.isolate()->GetEmbeddedCodeRange(
      &embedded_code_range.start, &embedded_code_range.length_in_bytes);

  const base::ModuleCache::Module* module = module_cache.GetModuleForAddress(
      reinterpret_cast<uintptr_t>(embedded_code_range.start));
  ASSERT_NE(nullptr, module);
  EXPECT_EQ(V8Unwinder::kV8EmbeddedCodeRangeBuildId, module->GetId());
}

TEST(V8UnwinderTest, EmbeddedCodeRangeModulePreservedOnUpdate) {
  ScopedV8Environment v8_environment;
  UpdateModulesTestUnwinder unwinder(v8_environment.isolate());
  base::ModuleCache module_cache;

  unwinder.Initialize(&module_cache);

  unwinder.SetCodePages({{reinterpret_cast<void*>(1), 10},
                         GetEmbeddedCodeRange(v8_environment.isolate())});

  auto capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  v8::MemoryRange embedded_code_range;
  v8_environment.isolate()->GetEmbeddedCodeRange(
      &embedded_code_range.start, &embedded_code_range.length_in_bytes);

  const base::ModuleCache::Module* module = module_cache.GetModuleForAddress(
      reinterpret_cast<uintptr_t>(embedded_code_range.start));
  ASSERT_NE(nullptr, module);
  EXPECT_EQ(V8Unwinder::kV8EmbeddedCodeRangeBuildId, module->GetId());
}

// Checks that the embedded code range is preserved even if it wasn't included
// in the code pages due to insufficient capacity.
TEST(V8UnwinderTest, EmbeddedCodeRangeModulePreservedOnOverCapacityUpdate) {
  ScopedV8Environment v8_environment;
  UpdateModulesTestUnwinder unwinder(v8_environment.isolate());
  base::ModuleCache module_cache;

  unwinder.Initialize(&module_cache);

  const int kDefaultCapacity = v8::Isolate::kMinCodePagesBufferSize;
  std::vector<v8::MemoryRange> code_pages;
  code_pages.reserve(kDefaultCapacity + 1);
  for (int i = 0; i < kDefaultCapacity + 1; ++i)
    code_pages.push_back({reinterpret_cast<void*>(i + 1), 1});
  unwinder.SetCodePages(code_pages);

  auto capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  v8::MemoryRange embedded_code_range;
  v8_environment.isolate()->GetEmbeddedCodeRange(
      &embedded_code_range.start, &embedded_code_range.length_in_bytes);

  const base::ModuleCache::Module* module = module_cache.GetModuleForAddress(
      reinterpret_cast<uintptr_t>(embedded_code_range.start));
  ASSERT_NE(nullptr, module);
  EXPECT_EQ(V8Unwinder::kV8EmbeddedCodeRangeBuildId, module->GetId());
}

TEST(V8UnwinderTest, UpdateModules_ModuleAdded) {
  ScopedV8Environment v8_environment;
  UpdateModulesTestUnwinder unwinder(v8_environment.isolate());
  base::ModuleCache module_cache;

  unwinder.Initialize(&module_cache);
  unwinder.SetCodePages({{reinterpret_cast<void*>(1), 10},
                         GetEmbeddedCodeRange(v8_environment.isolate())});
  auto capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  const base::ModuleCache::Module* module = module_cache.GetModuleForAddress(1);
  ASSERT_NE(nullptr, module);
  EXPECT_EQ(1u, module->GetBaseAddress());
  EXPECT_EQ(10u, module->GetSize());
  EXPECT_EQ(V8Unwinder::kV8CodeRangeBuildId, module->GetId());
  EXPECT_EQ("V8 Code Range", module->GetDebugBasename().MaybeAsASCII());
}

// Check that modules added before the last module are propagated to the
// ModuleCache. This case takes a different code path in the implementation.
TEST(V8UnwinderTest, UpdateModules_ModuleAddedBeforeLast) {
  ScopedV8Environment v8_environment;
  UpdateModulesTestUnwinder unwinder(v8_environment.isolate());
  base::ModuleCache module_cache;

  unwinder.Initialize(&module_cache);

  unwinder.SetCodePages({{reinterpret_cast<void*>(100), 10},
                         GetEmbeddedCodeRange(v8_environment.isolate())});
  auto capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  unwinder.SetCodePages({{reinterpret_cast<void*>(1), 10},
                         {reinterpret_cast<void*>(100), 10},
                         GetEmbeddedCodeRange(v8_environment.isolate())});
  capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  const base::ModuleCache::Module* module = module_cache.GetModuleForAddress(1);
  ASSERT_NE(nullptr, module);
  EXPECT_EQ(1u, module->GetBaseAddress());
  EXPECT_EQ(10u, module->GetSize());
  EXPECT_EQ(V8Unwinder::kV8CodeRangeBuildId, module->GetId());
  EXPECT_EQ("V8 Code Range", module->GetDebugBasename().MaybeAsASCII());
}

TEST(V8UnwinderTest, UpdateModules_ModuleRetained) {
  ScopedV8Environment v8_environment;
  UpdateModulesTestUnwinder unwinder(v8_environment.isolate());
  base::ModuleCache module_cache;

  unwinder.Initialize(&module_cache);

  unwinder.SetCodePages({{reinterpret_cast<void*>(1), 10},
                         GetEmbeddedCodeRange(v8_environment.isolate())});
  auto capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  // Code pages remain the same for this stack capture.
  capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  const base::ModuleCache::Module* module = module_cache.GetModuleForAddress(1);
  ASSERT_NE(nullptr, module);
  EXPECT_EQ(1u, module->GetBaseAddress());
  EXPECT_EQ(10u, module->GetSize());
  EXPECT_EQ(V8Unwinder::kV8CodeRangeBuildId, module->GetId());
  EXPECT_EQ("V8 Code Range", module->GetDebugBasename().MaybeAsASCII());
}

TEST(V8UnwinderTest, UpdateModules_ModuleRetainedWithDifferentSize) {
  ScopedV8Environment v8_environment;
  UpdateModulesTestUnwinder unwinder(v8_environment.isolate());
  base::ModuleCache module_cache;

  unwinder.Initialize(&module_cache);

  unwinder.SetCodePages({{reinterpret_cast<void*>(1), 10},
                         GetEmbeddedCodeRange(v8_environment.isolate())});
  auto capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  // Code pages remain the same for this stack capture.
  unwinder.SetCodePages({{reinterpret_cast<void*>(1), 20},
                         GetEmbeddedCodeRange(v8_environment.isolate())});
  capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  const base::ModuleCache::Module* module =
      module_cache.GetModuleForAddress(11);
  ASSERT_NE(nullptr, module);
  EXPECT_EQ(1u, module->GetBaseAddress());
  EXPECT_EQ(20u, module->GetSize());
}

TEST(V8UnwinderTest, UpdateModules_ModuleRemoved) {
  ScopedV8Environment v8_environment;
  UpdateModulesTestUnwinder unwinder(v8_environment.isolate());
  base::ModuleCache module_cache;

  unwinder.Initialize(&module_cache);

  unwinder.SetCodePages({{{reinterpret_cast<void*>(1), 10},
                          GetEmbeddedCodeRange(v8_environment.isolate())}});
  auto capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  unwinder.SetCodePages({GetEmbeddedCodeRange(v8_environment.isolate())});
  capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  EXPECT_EQ(nullptr, module_cache.GetModuleForAddress(1));
}

// Check that modules removed before the last module are propagated to the
// ModuleCache. This case takes a different code path in the implementation.
TEST(V8UnwinderTest, UpdateModules_ModuleRemovedBeforeLast) {
  ScopedV8Environment v8_environment;
  UpdateModulesTestUnwinder unwinder(v8_environment.isolate());
  base::ModuleCache module_cache;

  unwinder.Initialize(&module_cache);

  unwinder.SetCodePages({{{reinterpret_cast<void*>(1), 10},
                          {reinterpret_cast<void*>(100), 10},
                          GetEmbeddedCodeRange(v8_environment.isolate())}});
  auto capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  unwinder.SetCodePages({{reinterpret_cast<void*>(100), 10},
                         GetEmbeddedCodeRange(v8_environment.isolate())});
  capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  EXPECT_EQ(nullptr, module_cache.GetModuleForAddress(1));
}

TEST(V8UnwinderTest, UpdateModules_CapacityExceeded) {
  ScopedV8Environment v8_environment;
  UpdateModulesTestUnwinder unwinder(v8_environment.isolate());
  base::ModuleCache module_cache;

  unwinder.Initialize(&module_cache);

  const int kDefaultCapacity = v8::Isolate::kMinCodePagesBufferSize;

  std::vector<v8::MemoryRange> code_pages;
  // Create kDefaultCapacity + 2 code pages, with the last being the embedded
  // code page.
  code_pages.reserve(kDefaultCapacity + 2);
  for (int i = 0; i < kDefaultCapacity + 1; ++i)
    code_pages.push_back({reinterpret_cast<void*>(i + 1), 1});
  code_pages.push_back(GetEmbeddedCodeRange(v8_environment.isolate()));

  // The first sample should successfully create modules up to the default
  // capacity.
  unwinder.SetCodePages(code_pages);
  auto capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  EXPECT_NE(nullptr, module_cache.GetModuleForAddress(kDefaultCapacity));
  EXPECT_EQ(nullptr, module_cache.GetModuleForAddress(kDefaultCapacity + 1));

  // The capacity should be expanded by the second sample.
  unwinder.SetCodePages(code_pages);
  capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  EXPECT_NE(nullptr, module_cache.GetModuleForAddress(kDefaultCapacity));
  EXPECT_NE(nullptr, module_cache.GetModuleForAddress(kDefaultCapacity + 1));
}

// Checks that the implementation can handle the capacity being exceeded by a
// large amount.
TEST(V8UnwinderTest, UpdateModules_CapacitySubstantiallyExceeded) {
  ScopedV8Environment v8_environment;
  UpdateModulesTestUnwinder unwinder(v8_environment.isolate());
  base::ModuleCache module_cache;

  unwinder.Initialize(&module_cache);

  const int kDefaultCapacity = v8::Isolate::kMinCodePagesBufferSize;
  const int kCodePages = kDefaultCapacity * 3;
  std::vector<v8::MemoryRange> code_pages;
  code_pages.reserve(kCodePages);
  // Create kCodePages with the last being the embedded code page.
  for (int i = 0; i < kCodePages - 1; ++i)
    code_pages.push_back({reinterpret_cast<void*>(i + 1), 1});
  code_pages.push_back(GetEmbeddedCodeRange(v8_environment.isolate()));

  // The first sample should successfully create modules up to the default
  // capacity.
  unwinder.SetCodePages(code_pages);
  auto capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  EXPECT_NE(nullptr, module_cache.GetModuleForAddress(kDefaultCapacity));
  EXPECT_EQ(nullptr, module_cache.GetModuleForAddress(kDefaultCapacity + 1));

  // The capacity should be expanded by the second sample to handle all the
  // available modules.
  unwinder.SetCodePages(code_pages);
  capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  EXPECT_NE(nullptr, module_cache.GetModuleForAddress(kCodePages - 1));
}

TEST(V8UnwinderTest, CanUnwindFrom_V8Module) {
  ScopedV8Environment v8_environment;
  UpdateModulesTestUnwinder unwinder(v8_environment.isolate());
  base::ModuleCache module_cache;

  unwinder.Initialize(&module_cache);

  unwinder.SetCodePages({{reinterpret_cast<void*>(1), 10},
                         GetEmbeddedCodeRange(v8_environment.isolate())});
  auto capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  const base::ModuleCache::Module* module = module_cache.GetModuleForAddress(1);
  ASSERT_NE(nullptr, module);

  EXPECT_TRUE(unwinder.CanUnwindFrom({1, module}));
}

TEST(V8UnwinderTest, CanUnwindFrom_OtherModule) {
  ScopedV8Environment v8_environment;
  UpdateModulesTestUnwinder unwinder(v8_environment.isolate());
  base::ModuleCache module_cache;

  unwinder.Initialize(&module_cache);

  unwinder.SetCodePages({GetEmbeddedCodeRange(v8_environment.isolate())});
  auto capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  auto other_module = std::make_unique<base::TestModule>(1, 10);
  const base::ModuleCache::Module* other_module_ptr = other_module.get();
  module_cache.AddCustomNativeModule(std::move(other_module));

  EXPECT_FALSE(unwinder.CanUnwindFrom({1, other_module_ptr}));
}

TEST(V8UnwinderTest, CanUnwindFrom_NullModule) {
  ScopedV8Environment v8_environment;
  UpdateModulesTestUnwinder unwinder(v8_environment.isolate());
  base::ModuleCache module_cache;

  unwinder.Initialize(&module_cache);

  // Insert a non-native module to potentially exercise the Module comparator.
  unwinder.SetCodePages({{reinterpret_cast<void*>(1), 10},
                         GetEmbeddedCodeRange(v8_environment.isolate())});
  auto capture_state = unwinder.CreateUnwinderStateCapture();
  unwinder.OnStackCapture(capture_state.get());
  unwinder.UpdateModules(capture_state.get());

  EXPECT_FALSE(unwinder.CanUnwindFrom({20, nullptr}));
}

// Checks that unwinding from C++ through JavaScript and back into C++ succeeds.
#if (BUILDFLAG(IS_WIN) && defined(ARCH_CPU_64_BITS)) || BUILDFLAG(IS_MAC) || \
    (BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_ARMEL))
#define MAYBE_UnwindThroughV8Frames UnwindThroughV8Frames
#else
#define MAYBE_UnwindThroughV8Frames DISABLED_UnwindThroughV8Frames
#endif
TEST(V8UnwinderTest, MAYBE_UnwindThroughV8Frames) {
  v8::Isolate* isolate = nullptr;
  base::WaitableEvent isolate_available;

  const auto set_isolate = [&](v8::Isolate* isolate_state) {
    isolate = isolate_state;
    isolate_available.Signal();
  };

  const auto create_v8_unwinder = [&]() -> std::unique_ptr<base::Unwinder> {
    isolate_available.Wait();
    return std::make_unique<V8Unwinder>(isolate);
  };

  base::UnwindScenario scenario(base::BindRepeating(
      &CallThroughV8, base::BindLambdaForTesting(set_isolate)));
  base::ModuleCache module_cache;

  std::vector<base::Frame> sample = SampleScenario(
      &scenario, &module_cache, base::BindLambdaForTesting(create_v8_unwinder));

  // The stack should contain a full unwind.
  ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});

  // The stack should contain a frame from a JavaScript module.
  EXPECT_THAT(sample,
              Contains(Field(
                  "module", &base::Frame::module,
                  AllOf(NotNull(),
                        Pointee(Property(
                            "module.id", &base::ModuleCache::Module::GetId,
                            AnyOf(Eq(V8Unwinder::kV8EmbeddedCodeRangeBuildId),
                                  Eq(V8Unwinder::kV8CodeRangeBuildId))))))));
}
