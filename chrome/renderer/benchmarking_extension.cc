// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/benchmarking_extension.h"

#include <cstdint>
#include <string>

#include "base/command_line.h"
#include "base/process/process_handle.h"
#include "base/profiler/module_cache.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/base_tracing.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_thread.h"
#include "v8-local-handle.h"
#include "v8/include//v8-function.h"
#include "v8/include/v8-extension.h"
#include "v8/include/v8-isolate.h"
#include "v8/include/v8-primitive.h"
#include "v8/include/v8-template.h"

const char kBenchmarkingExtensionName[] = "v8/Benchmarking";

namespace extensions_v8 {

class BenchmarkingWrapper : public v8::Extension {
 public:
  BenchmarkingWrapper()
      : v8::Extension(kBenchmarkingExtensionName,
                      "if (typeof(chrome) == 'undefined') {"
                      "  chrome = {};"
                      "};"
                      "if (typeof(chrome.benchmarking) == 'undefined') {"
                      "  chrome.benchmarking = {};"
                      "};"
                      "chrome.benchmarking.isSingleProcess = function() {"
                      "  native function IsSingleProcess();"
                      "  return IsSingleProcess();"
                      "};"
                      "chrome.benchmarking.getRendererPid = function() {"
                      "  native function GetRendererPid();"
                      "  return GetRendererPid();"
                      "};"
                      "chrome.benchmarking.getRendererMainTid = function() {"
                      "  native function GetRendererMainTid();"
                      "  return GetRendererMainTid();"
                      "};"
                      "chrome.benchmarking.getMarkFunctions = function() {"
                      "  native function GetMarkFunctions();"
                      "  return GetMarkFunctions();"
                      "};"
                      "chrome.Interval = function() {"
                      "  var start_ = 0;"
                      "  var stop_ = 0;"
                      "  native function HiResTime();"
                      "  this.start = function() {"
                      "    stop_ = 0;"
                      "    start_ = HiResTime();"
                      "  };"
                      "  this.stop = function() {"
                      "    stop_ = HiResTime();"
                      "    if (start_ == 0)"
                      "      stop_ = 0;"
                      "  };"
                      "  this.microseconds = function() {"
                      "    var stop = stop_;"
                      "    if (stop == 0 && start_ != 0)"
                      "      stop = HiResTime();"
                      "    return Math.ceil(stop - start_);"
                      "  };"
                      "}") {}

  v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Local<v8::String> name) override {
    if (name->StringEquals(GetString(isolate, "IsSingleProcess"))) {
      return v8::FunctionTemplate::New(isolate, IsSingleProcess);
    } else if (name->StringEquals(GetString(isolate, "HiResTime"))) {
      return v8::FunctionTemplate::New(isolate, HiResTime);
    } else if (name->StringEquals(GetString(isolate, "GetRendererPid"))) {
      return v8::FunctionTemplate::New(isolate, GetRendererPid);
    } else if (name->StringEquals(GetString(isolate, "GetRendererMainTid"))) {
      return v8::FunctionTemplate::New(isolate, GetRendererMainTid);
    } else if (name->StringEquals(GetString(isolate, "GetMarkFunctions"))) {
      return v8::FunctionTemplate::New(isolate, GetMarkFunctions);
    }

    return v8::Local<v8::FunctionTemplate>();
  }

  static void IsSingleProcess(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(base::CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kSingleProcess));
  }

  static void HiResTime(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(
        static_cast<double>(base::TimeTicks::Now().ToInternalValue()));
  }

  static void GetRendererPid(const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(static_cast<int>(base::GetCurrentProcId()));
  }

  static void GetRendererMainTid(
      const v8::FunctionCallbackInfo<v8::Value>& args) {
    args.GetReturnValue().Set(
        static_cast<int>(base::PlatformThread::CurrentId()));
  }

 private:
  static void StartMark(const v8::FunctionCallbackInfo<v8::Value>& info) {
    TRACE_EVENT_INSTANT("benchmark", "Benchmarking::StartMark");
  }

  static void StopMark(const v8::FunctionCallbackInfo<v8::Value>& info) {
    TRACE_EVENT_INSTANT("benchmark", "Benchmarking::StopMark");
  }

  static v8::Local<v8::String> GetString(v8::Isolate* isolate,
                                         const std::string& string) {
    return v8::String::NewFromUtf8(isolate, string.data(),
                                   v8::NewStringType::kInternalized,
                                   string.length())
        .ToLocalChecked();
  }

  static v8::Local<v8::Object> GetMark(
      const v8::FunctionCallbackInfo<v8::Value>& info,
      base::ModuleCache& cache,
      void (*func)(const v8::FunctionCallbackInfo<v8::Value>&)) {
    auto* isolate = info.GetIsolate();
    auto context = isolate->GetCurrentContext();

    uintptr_t vaddr = 0;
    std::string module_id;
    std::string module_name;
    uintptr_t module_base_addr = 0;

    uintptr_t addr = reinterpret_cast<intptr_t>(func);
    if (auto* module = cache.GetModuleForAddress(addr); module) {
      module_id = module->GetId();
      module_name = module->GetDebugBasename().AsUTF8Unsafe();
      vaddr = reinterpret_cast<intptr_t>(func) - module->GetBaseAddress();
      module_base_addr = module->GetBaseAddress();
    }

    v8::Local<v8::Object> result = v8::Object::New(isolate);
    result
        ->Set(context, GetString(isolate, "module_id"),
              GetString(isolate, module_id))
        .Check();
    result
        ->Set(context, GetString(isolate, "module_basename"),
              GetString(isolate, module_name))
        .Check();
    result
        ->Set(context, GetString(isolate, "module_base_address"),
              v8::BigInt::New(isolate, module_base_addr))
        .Check();
    result
        ->Set(context, GetString(isolate, "vaddr"),
              v8::BigInt::New(isolate, vaddr))
        .Check();
    result
        ->Set(context, GetString(isolate, "function"),
              v8::FunctionTemplate::New(isolate, func)
                  ->GetFunction(context)
                  .ToLocalChecked())
        .Check();
    return result;
  }

  static void GetMarkFunctions(
      const v8::FunctionCallbackInfo<v8::Value>& info) {
    base::ModuleCache cache;
    auto* isolate = info.GetIsolate();
    auto context = isolate->GetCurrentContext();
    v8::Local<v8::Object> result = v8::Object::New(isolate);
    result
        ->Set(context, GetString(isolate, "start"),
              GetMark(info, cache, StartMark))
        .Check();
    result
        ->Set(context, GetString(isolate, "stop"),
              GetMark(info, cache, StopMark))
        .Check();
    info.GetReturnValue().Set(result);
  }
};

std::unique_ptr<v8::Extension> BenchmarkingExtension::Get() {
  return std::make_unique<BenchmarkingWrapper>();
}

}  // namespace extensions_v8
