// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/escape.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_fuzzer.h"

// This is an example use of the InProcessFuzzer framework.
// It fetches arbitrary HTML from an HTTPS server. It's not really
// intended to be an effective fuzzer, but just to show an example
// of how this framework can be used.

class HtmlInProcessFuzzer : public InProcessFuzzer {
 public:
  HtmlInProcessFuzzer() = default;

  int Fuzz(const uint8_t* data, size_t size) override;

 private:
  std::string current_fuzz_case_;
};

REGISTER_IN_PROCESS_FUZZER(HtmlInProcessFuzzer)

int HtmlInProcessFuzzer::Fuzz(const uint8_t* data, size_t size) {
  std::string html_string(reinterpret_cast<const char*>(data), size);
  // See
  // docs/security/url_display_guidelines/url_display_guidelines.md#url-length
  const size_t kMaxUrlLength = 2 * 1024 * 1024;
  std::string url_string = "data:text/html;charset=utf-8,";
  const bool kUsePlus = false;
  url_string.append(base::EscapeQueryParamValue(html_string, kUsePlus));
  if (url_string.length() > kMaxUrlLength) {
    return -1;
  }
  base::IgnoreResult(ui_test_utils::NavigateToURL(browser(), GURL(url_string)));
  return 0;
}
