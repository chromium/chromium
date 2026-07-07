// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/benchmarking_bindings.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/profiler/module_cache.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/net_benchmarking.mojom.h"
#include "components/variations/variations_switches.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/chrome_object_extensions_utils.h"
#include "content/public/renderer/render_thread.h"
#include "gin/converter.h"
#include "gin/object_template_builder.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/blink/public/platform/web_cache.h"
#include "v8/include/cppgc/allocation.h"
#include "v8/include/v8-context.h"
#include "v8/include/v8-function.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-microtask-queue.h"
#include "v8/include/v8-object.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-template.h"

namespace {

void StartMark(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TRACE_EVENT_INSTANT("benchmark", "Benchmarking::StartMark");
}

void StopMark(const v8::FunctionCallbackInfo<v8::Value>& info) {
  TRACE_EVENT_INSTANT("benchmark", "Benchmarking::StopMark");
}

v8::Local<v8::Object> GetMark(
    v8::Isolate* isolate,
    v8::Local<v8::Context> context,
    base::ModuleCache& cache,
    void (*func)(const v8::FunctionCallbackInfo<v8::Value>&)) {
  uintptr_t vaddr = 0;
  std::string module_id;
  std::string module_name;
  uintptr_t module_base_addr = 0;

  uintptr_t addr = reinterpret_cast<uintptr_t>(func);
  if (auto* module = cache.GetModuleForAddress(addr); module) {
    module_id = module->GetId();
    module_name = module->GetDebugBasename().AsUTF8Unsafe();
    vaddr = reinterpret_cast<uintptr_t>(func) - module->GetBaseAddress();
    module_base_addr = module->GetBaseAddress();
  }

  v8::Local<v8::Object> result = v8::Object::New(isolate);
  result
      ->Set(context, gin::StringToSymbol(isolate, "module_id"),
            gin::StringToV8(isolate, module_id))
      .Check();
  result
      ->Set(context, gin::StringToSymbol(isolate, "module_basename"),
            gin::StringToV8(isolate, module_name))
      .Check();
  result
      ->Set(context, gin::StringToSymbol(isolate, "module_base_address"),
            v8::BigInt::New(isolate, module_base_addr))
      .Check();
  result
      ->Set(context, gin::StringToSymbol(isolate, "vaddr"),
            v8::BigInt::New(isolate, vaddr))
      .Check();
  result
      ->Set(context, gin::StringToSymbol(isolate, "function"),
            v8::FunctionTemplate::New(isolate, func)
                ->GetFunction(context)
                .ToLocalChecked())
      .Check();
  return result;
}

}  // namespace

// static
void BenchmarkingBindings::InstallConditionally(
    v8::Local<v8::Context> context) {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  bool enable_benchmarking =
      command_line->HasSwitch(variations::switches::kEnableBenchmarkingApi);
  bool enable_net_benchmarking =
      command_line->HasSwitch(switches::kEnableNetBenchmarking);

  if (!enable_benchmarking && !enable_net_benchmarking) {
    return;
  }

  v8::Isolate* isolate = v8::Isolate::GetCurrent();

  // The WebLocalFrame and its AgentGroupScheduler must be active.
  CHECK(isolate);
  v8::HandleScope handle_scope(isolate);
  if (context.IsEmpty()) {
    return;
  }

  v8::MicrotasksScope microtasks_scope(
      isolate, context->GetMicrotaskQueue(),
      v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::Context::Scope context_scope(context);

  auto* bindings = cppgc::MakeGarbageCollected<BenchmarkingBindings>(
      isolate->GetCppHeap()->GetAllocationHandle());
  v8::Local<v8::Object> wrapper;
  if (!bindings->GetWrapper(isolate).ToLocal(&wrapper)) {
    return;
  }

  v8::Local<v8::Object> chrome =
      content::GetOrCreateChromeObject(isolate, context);
  chrome->Set(context, gin::StringToSymbol(isolate, "benchmarking"), wrapper)
      .ToChecked();

  if (enable_benchmarking) {
    v8::Local<v8::String> source =
        gin::StringToV8(isolate,
                        "(function() {"
                        "  if (typeof(chrome) == 'undefined') {"
                        "    chrome = {};"
                        "  }"
                        "  chrome.Interval = function() {"
                        "    var start_ = 0;"
                        "    var stop_ = 0;"
                        "    this.start = function() {"
                        "      stop_ = 0;"
                        "      start_ = chrome.benchmarking.hiResTime();"
                        "    };"
                        "    this.stop = function() {"
                        "      stop_ = chrome.benchmarking.hiResTime();"
                        "      if (start_ == 0)"
                        "        stop_ = 0;"
                        "    };"
                        "    this.microseconds = function() {"
                        "      var stop = stop_;"
                        "      if (stop == 0 && start_ != 0)"
                        "        stop = chrome.benchmarking.hiResTime();"
                        "      return Math.ceil(stop - start_);"
                        "    };"
                        "  };"
                        "})();");
    v8::TryCatch try_catch(isolate);
    v8::Local<v8::Script> script;
    if (v8::Script::Compile(context, source).ToLocal(&script)) {
      std::ignore = script->Run(context);
    }
  }
}

BenchmarkingBindings::BenchmarkingBindings() = default;

BenchmarkingBindings::~BenchmarkingBindings() = default;

gin::ObjectTemplateBuilder BenchmarkingBindings::GetObjectTemplateBuilder(
    v8::Isolate* isolate) {
  auto builder =
      gin::Wrappable<BenchmarkingBindings>::GetObjectTemplateBuilder(isolate);

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          variations::switches::kEnableBenchmarkingApi)) {
    builder.SetMethod("isSingleProcess", &BenchmarkingBindings::IsSingleProcess)
        .SetMethod("getRendererPid", &BenchmarkingBindings::GetRendererPid)
        .SetMethod("getRendererMainTid",
                   &BenchmarkingBindings::GetRendererMainTid)
        .SetMethod("hiResTime", &BenchmarkingBindings::HiResTime)
        .SetMethod("getMarkFunctions", &BenchmarkingBindings::GetMarkFunctions);
  }

  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEnableNetBenchmarking)) {
    builder.SetMethod("clearCache", &BenchmarkingBindings::ClearCache)
        .SetMethod("clearHostResolverCache",
                   &BenchmarkingBindings::ClearHostResolverCache)
        .SetMethod("clearPredictorCache",
                   &BenchmarkingBindings::ClearPredictorCache)
        .SetMethod("closeConnections", &BenchmarkingBindings::CloseConnections);
  }

  return builder;
}

bool BenchmarkingBindings::IsSingleProcess() {
  return base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kSingleProcess);
}

int BenchmarkingBindings::GetRendererPid() {
  return static_cast<int>(base::GetCurrentProcId());
}

int32_t BenchmarkingBindings::GetRendererMainTid() {
  // The current thread ID might be an int64, however int64 values are not
  // representable in JS and JSON (cf. crbug.com/40228085) since JS numbers
  // are float64. Since thread IDs are likely to be allocated sequentially,
  // truncation of the high bits is preferable to loss of precision in the low
  // bits, as threads are more likely to differ in their low bit values, so we
  // truncate the value to int32. Since this is only used for dumping
  // benchmark state, the loss of information is not catastrophic and won't
  // happen in normal browser execution.
  return base::PlatformThread::CurrentId().truncate_to_int32_for_display_only();
}

double BenchmarkingBindings::HiResTime() {
  return base::TimeTicks::Now().since_origin().InMicrosecondsF();
}

v8::Local<v8::Object> BenchmarkingBindings::GetMarkFunctions(
    v8::Isolate* isolate) {
  base::ModuleCache cache;
  v8::Local<v8::Context> context = isolate->GetCurrentContext();
  v8::Local<v8::Object> result = v8::Object::New(isolate);
  result
      ->Set(context, gin::StringToSymbol(isolate, "start"),
            GetMark(isolate, context, cache, StartMark))
      .Check();
  result
      ->Set(context, gin::StringToSymbol(isolate, "stop"),
            GetMark(isolate, context, cache, StopMark))
      .Check();
  return result;
}

chrome::mojom::NetBenchmarking* BenchmarkingBindings::GetNetBenchmarking() {
  if (!net_benchmarking_.is_bound()) {
    if (content::RenderThread* thread = content::RenderThread::Get()) {
      thread->BindHostReceiver(net_benchmarking_.BindNewPipeAndPassReceiver());
    }
  }
  return net_benchmarking_.is_bound() ? net_benchmarking_.get() : nullptr;
}

void BenchmarkingBindings::ClearCache() {
  if (auto* net_benchmarking = GetNetBenchmarking()) {
    net_benchmarking->ClearCache();
  }
  if (content::RenderThread::Get()) {
    blink::WebCache::Clear();
  }
}

void BenchmarkingBindings::ClearHostResolverCache() {
  if (auto* net_benchmarking = GetNetBenchmarking()) {
    net_benchmarking->ClearHostResolverCache();
  }
}

void BenchmarkingBindings::ClearPredictorCache() {
  if (auto* net_benchmarking = GetNetBenchmarking()) {
    net_benchmarking->ClearPredictorCache();
  }
}

void BenchmarkingBindings::CloseConnections() {
  if (auto* net_benchmarking = GetNetBenchmarking()) {
    net_benchmarking->CloseCurrentConnections();
  }
}

const gin::WrapperInfo* BenchmarkingBindings::wrapper_info() const {
  return &kWrapperInfo;
}
