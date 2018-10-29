// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/benchmarking_extension.h"

#include "base/command_line.h"
#include "base/time/time.h"
#include "content/public/common/content_switches.h"
#include "content/public/renderer/render_thread.h"
#include "v8/include/v8.h"

const char kBenchmarkingExtensionName[] = "v8/Benchmarking";

namespace extensions_v8 {

class BenchmarkingWrapper : public v8::Extension {
 public:
  BenchmarkingWrapper() :
      v8::Extension(kBenchmarkingExtensionName,
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
        "}"
        ) {}

  v8::Local<v8::FunctionTemplate> GetNativeFunctionTemplate(
      v8::Isolate* isolate,
      v8::Local<v8::String> name) override {
    if (name->StringEquals(
            v8::String::NewFromUtf8(isolate, "IsSingleProcess",
                                    v8::NewStringType::kInternalized)
                .ToLocalChecked())) {
      return v8::FunctionTemplate::New(isolate, IsSingleProcess);
    } else if (name->StringEquals(
                   v8::String::NewFromUtf8(isolate, "HiResTime",
                                           v8::NewStringType::kInternalized)
                       .ToLocalChecked())) {
      return v8::FunctionTemplate::New(isolate, HiResTime);
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
};

v8::Extension* BenchmarkingExtension::Get() {
  return new BenchmarkingWrapper();
}

}  // namespace extensions_v8
