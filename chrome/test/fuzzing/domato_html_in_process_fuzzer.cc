// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/strings/escape.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/domato_html_fuzzer_grammar.h"
#include "chrome/test/fuzzing/domato_html_fuzzer_grammar.pb.h"
#include "chrome/test/fuzzing/in_process_fuzzer.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "testing/libfuzzer/research/domatolpm/domatolpm.h"

// This fuzzer uses DomatoLPM to generate HTML based on an existing Domato
// rule.
class DomatoHtmlInProcessFuzzer : public InProcessFuzzer {
 public:
  using FuzzCase = domatolpm::generated::domato_html_fuzzer_grammar::fuzzcase;
  DomatoHtmlInProcessFuzzer() = default;

  int Fuzz(const uint8_t* data, size_t size) override;
};

DEFINE_CUSTOM_PROTO_MUTATOR_IMPL(true, DomatoHtmlInProcessFuzzer::FuzzCase)
DEFINE_CUSTOM_PROTO_CROSSOVER_IMPL(true, DomatoHtmlInProcessFuzzer::FuzzCase)
DEFINE_POST_PROCESS_PROTO_MUTATION_IMPL(DomatoHtmlInProcessFuzzer::FuzzCase)
REGISTER_IN_PROCESS_FUZZER(DomatoHtmlInProcessFuzzer)

int DomatoHtmlInProcessFuzzer::Fuzz(const uint8_t* data, size_t size) {
  FuzzCase fuzz_case;
  if (!protobuf_mutator::libfuzzer::LoadProtoInput(false, data, size,
                                                   &fuzz_case)) {
    return -1;
  }
  domatolpm::Context ctx;
  CHECK(domatolpm::domato_html_fuzzer_grammar::handle_fuzzer(&ctx, fuzz_case));
  std::string_view html_string(ctx.GetBuilder()->view());
  // See
  // docs/security/url_display_guidelines/url_display_guidelines.md#url-length
  const size_t kMaxUrlLength = 2 * 1024 * 1024;
  std::string url_string = "data:text/html;charset=utf-8,";
  const bool kUsePlus = false;
  url_string.append(base::EscapeQueryParamValue(html_string, kUsePlus));
  if (url_string.length() > kMaxUrlLength) {
    return -1;
  }

  // We nee to call `DisableUnloadTimerForTesting` because on a low resource
  // device environment, the generated HTML pages can take more than 500ms to
  // load, and we want to make sure we catch bugs that could potentially happen
  // after this time frame. We need to call into `DisableUnloadTimerForTesting`
  // every time because the primary main frame might change depending on the
  // content of the loaded content. That's no big deal given the exec/s that's
  // mostly due to the navigation.
  auto* rfh = browser()
                  ->tab_strip_model()
                  ->GetActiveWebContents()
                  ->GetPrimaryMainFrame();
  DisableUnloadTimerForTesting(rfh);
  base::IgnoreResult(ui_test_utils::NavigateToURL(browser(), GURL(url_string)));
  return 0;
}
