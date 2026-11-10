// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/fuzzing/domato_html_fuzzer_grammar.h"
#include "chrome/test/fuzzing/domato_html_fuzzer_grammar.pb.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/libfuzzer/research/domatolpm/domatolpm.h"

// This fuzzer uses DomatoLPM to generate HTML based on an existing Domato
// rule.
class DomatoHtmlInProcessFuzzer
    : public InProcessBinaryProtoFuzzer<
          domatolpm::generated::domato_html_fuzzer_grammar::fuzzcase> {
 public:
  using FuzzCase = domatolpm::generated::domato_html_fuzzer_grammar::fuzzcase;
  DomatoHtmlInProcessFuzzer() = default;

  void SetUpOnMainThread() override {
    InProcessBinaryProtoFuzzer::SetUpOnMainThread();
    CHECK(embedded_https_test_server().Start());
  }

  int Fuzz(const FuzzCase& fuzz_case) override;
};

REGISTER_BINARY_PROTO_IN_PROCESS_FUZZER(DomatoHtmlInProcessFuzzer)

int DomatoHtmlInProcessFuzzer::Fuzz(const FuzzCase& fuzz_case) {
  domatolpm::Context ctx;
  CHECK(domatolpm::domato_html_fuzzer_grammar::handle_fuzzer(&ctx, fuzz_case));
  std::string_view html_string(ctx.GetBuilder()->view());

  // We need to call `DisableUnloadTimerForTesting` because on a low resource
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
  std::ignore = NavigateToHtml(html_string);
  return 0;
}
