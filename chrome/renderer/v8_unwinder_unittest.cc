// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/v8_unwinder.h"

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/profiler/stack_sampling_profiler_test_util.h"
#include "base/stl_util.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/bind_test_util.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "gin/public/isolate_holder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "v8/include/v8.h"

namespace {

v8::Local<v8::String> ToV8String(const char* str) {
  return v8::String::NewFromUtf8(v8::Isolate::GetCurrent(), str,
                                 v8::NewStringType::kNormal)
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
    const base::RepeatingCallback<void(const v8::UnwindState&)>&
        report_unwind_state,
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
    report_unwind_state.Run(isolate->GetUnwindState());
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
        ->Call(context, v8::Undefined(isolate), base::size(argv), argv)
        .ToLocalChecked();

    // Run waitForSample() with the real closure pointer.
    argv[0] = CreatePointerHolder(&wait_for_sample);
    v8::Local<v8::Function> js_wait_for_sample = v8::Local<v8::Function>::Cast(
        context->Global()
            ->Get(context, ToV8String("waitForSample"))
            .ToLocalChecked());
    js_wait_for_sample
        ->Call(context, v8::Undefined(isolate), base::size(argv), argv)
        .ToLocalChecked();
  }

  // Volatile to prevent a tail call to GetProgramCounter().
  const void* volatile end_program_counter = base::GetProgramCounter();
  return {start_program_counter, end_program_counter};
}

}  // namespace

// Checks that unwinding from C++ through JavaScript and back into C++ succeeds.
// NB: unwinding is only supported for 64 bit Windows and OS X.
#if (defined(OS_WIN) && defined(ARCH_CPU_64_BITS)) || defined(OS_MACOSX)
#define MAYBE_UnwindThroughV8Frames UnwindThroughV8Frames
#else
#define MAYBE_UnwindThroughV8Frames DISABLED_UnwindThroughV8Frames
#endif
TEST(V8UnwinderTest, MAYBE_UnwindThroughV8Frames) {
  v8::UnwindState unwind_state;
  base::WaitableEvent unwind_state_available;

  const auto set_unwind_state = [&](const v8::UnwindState& state) {
    unwind_state = state;
    unwind_state_available.Signal();
  };

  const auto create_v8_unwinder = [&]() -> std::unique_ptr<base::Unwinder> {
    unwind_state_available.Wait();
    return std::make_unique<V8Unwinder>(unwind_state);
  };

  base::UnwindScenario scenario(base::BindRepeating(
      &CallThroughV8, base::BindLambdaForTesting(set_unwind_state)));
  base::ModuleCache module_cache;

  std::vector<base::Frame> sample = SampleScenario(
      &scenario, &module_cache, base::BindLambdaForTesting(create_v8_unwinder));

  // The stack should contain a full unwind.
  ExpectStackContains(sample, {scenario.GetWaitForSampleAddressRange(),
                               scenario.GetSetupFunctionAddressRange(),
                               scenario.GetOuterFunctionAddressRange()});

  // The stack should contain a frame from a JavaScript module.
  auto loc =
      std::find_if(sample.begin(), sample.end(), [&](const base::Frame& frame) {
        return frame.module &&
               (frame.module->GetId() ==
                    V8Unwinder::kV8EmbeddedCodeRangeBuildId ||
                frame.module->GetId() == V8Unwinder::kV8CodeRangeBuildId);
      });
  EXPECT_NE(sample.end(), loc);
}
