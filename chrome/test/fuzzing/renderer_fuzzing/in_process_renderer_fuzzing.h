// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_FUZZING_RENDERER_FUZZING_IN_PROCESS_RENDERER_FUZZING_H_
#define CHROME_TEST_FUZZING_RENDERER_FUZZING_IN_PROCESS_RENDERER_FUZZING_H_

#include "base/base64.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/fuzzing/in_process_fuzzer.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/libfuzzer/renderer_fuzzing/renderer_fuzzing.h"

// This file introduces a new kind of InProcessFuzzer, that are meant to fuzz
// the renderer process. Developers can declare fuzzers that will be called via
// an internal mechanism and executed in the renderer process.
//
// Example:
//
// class MyRendererFuzzer : RendererFuzzerBase {
//  public:
//   const char* Id() override { return "MyRendererFuzzer"; }
//   // This method will be executed in the renderer process, called via the
//   // `internals.runFuzzer()` JS API.
//   void Run(
//      const blink::BrowserInterfaceBrokerProxy*
//      context_interface_broker_proxy,
//      blink::ThreadSafeBrowserInterfaceBrokerProxy*
//         process_interface_broker_proxy,
//      std::vector<uint8_t>&& input,
//      base::OnceClosure done_closure) override {
//     FuzzTheRenderer(input.data(), input.size());
//   }
// };
//
// REGISTER_IN_PROCESS_RENDERER_FUZZER(MyRendererFuzzer);
//

// `InProcessFuzzer` that forwards data passed to `Fuzz()` to an instance of
// `RendererFuzzer` in the renderer process.
// Uses the `internals.runFuzzer()` JS API to execute the testcase.
// In order to know which fuzzer to invoke, it will use the
// RendererFuzzer::Id() method.
// To build renderer in_process fuzzer, this class shouldn't be used. Please
// see the REGISTER_IN_PROCESS_RENDERER_FUZZER macro below.
template <typename RendererFuzzer>
class RendererFuzzerProxy : public InProcessFuzzer {
 public:
  // NOLINTNEXTLINE(runtime/explicit)
  RendererFuzzerProxy(InProcessFuzzerOptions options = {});

  int Fuzz(const uint8_t* data, size_t size) override;
  base::CommandLine::StringVector GetChromiumCommandLineArguments() override;

 private:
  std::string fuzzer_name_;
};

// Same as `RendererFuzzerProxy`, but accepts serialized proto inputs
// instead of arbitrary bytes.
// Similarly to RendererFuzzerProxy, this class should not be used as is,
// see REGISTER_IN_PROCESS_RENDERER_PROTO_FUZZER instead.
template <typename RendererFuzzer>
class ProtoRendererFuzzerProxy : public RendererFuzzerProxy<RendererFuzzer> {
 public:
  // This is necessary for the internal InProcessFuzzer mechanism to know about
  // the proto message.
  using FuzzCase = RendererFuzzer::FuzzCase;
};

template <typename RendererFuzzer>
RendererFuzzerProxy<RendererFuzzer>::RendererFuzzerProxy(
    InProcessFuzzerOptions options)
    : InProcessFuzzer(options), fuzzer_name_(RendererFuzzer().Id()) {}

template <typename RendererFuzzer>
int RendererFuzzerProxy<RendererFuzzer>::Fuzz(const uint8_t* data,
                                              size_t size) {
  base::span<const uint8_t> data_span(data, size);

  auto b64 = base::Base64Encode(data_span);
  auto* contents = browser()->tab_strip_model()->GetActiveWebContents();
  CHECK(content::ExecJs(contents, content::JsReplace(
                                      R"(
      function base64ToArrayBuffer(base64) {
        var binaryString = atob(base64);
        var bytes = new Uint8Array(binaryString.length);
        for (var i = 0; i < binaryString.length; i++) {
          bytes[i] = binaryString.charCodeAt(i);
        }
        return bytes.buffer;
      }
      internals.runFuzzer($1, base64ToArrayBuffer($2));
      )",
                                      fuzzer_name_, b64)));
  return 0;
}

template <typename RendererFuzzer>
base::CommandLine::StringVector
RendererFuzzerProxy<RendererFuzzer>::GetChromiumCommandLineArguments() {
  return {FILE_PATH_LITERAL("--disable-kill-after-bad-ipc")};
}

// Registers the given class as a renderer fuzzer which will be driven by an
// `InProcessFuzzer` in the browser process.
#define REGISTER_IN_PROCESS_RENDERER_FUZZER(RendererFuzzer)             \
  static_assert(std::is_base_of_v<RendererFuzzerBase, RendererFuzzer>); \
  REGISTER_IN_PROCESS_FUZZER(RendererFuzzerProxy<RendererFuzzer>)       \
  REGISTER_RENDERER_FUZZER(RendererFuzzer)

// Similar to the other macro, except that it creates a proto in_process
// fuzzer.
#define REGISTER_IN_PROCESS_RENDERER_PROTO_FUZZER(RendererFuzzer)       \
  static_assert(std::is_base_of_v<RendererFuzzerBase, RendererFuzzer>); \
  REGISTER_BINARY_PROTO_IN_PROCESS_FUZZER(                              \
      ProtoRendererFuzzerProxy<RendererFuzzer>)                         \
  REGISTER_RENDERER_FUZZER(RendererFuzzer)

#endif  // CHROME_TEST_FUZZING_RENDERER_FUZZING_IN_PROCESS_RENDERER_FUZZING_H_
