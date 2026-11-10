// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "chrome/test/fuzzing/in_process_fuzzer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

// This is an example use of the InProcessFuzzer framework.
// It serves arbitrary HTML from a test server, then navigates to it.
// It's not really intended to be an effective fuzzer, but just to
// show an example of how this framework can be used.

class HtmlInProcessFuzzer : public InProcessFuzzer {
 public:
  HtmlInProcessFuzzer() = default;

  void SetUpOnMainThread() override {
    InProcessFuzzer::SetUpOnMainThread();
    CHECK(embedded_https_test_server().Start());
  }

  int Fuzz(const uint8_t* data, size_t size) override;
};

REGISTER_IN_PROCESS_FUZZER(HtmlInProcessFuzzer)

int HtmlInProcessFuzzer::Fuzz(const uint8_t* data, size_t size) {
  std::string_view html_string(reinterpret_cast<const char*>(data), size);
  std::ignore = NavigateToHtml(html_string);
  return 0;
}
