// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "chrome/test/fuzzing/webidl_fuzzing/webidl_fuzzer_grammar.h"
#include "chrome/test/fuzzing/webidl_fuzzing/webidl_fuzzer_grammar.pb.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "testing/libfuzzer/research/domatolpm/domatolpm.h"

struct Environment {
  Environment() {}
  const bool kDumpStats = getenv("DUMP_FUZZER_STATS");
  const bool kDumpNativeInput = getenv("LPM_DUMP_NATIVE_INPUT");
};

constexpr std::optional<base::TimeDelta> kJsExecutionTimeout =
    base::Seconds(10);
constexpr RunLoopTimeoutBehavior kJsRunLoopTimeoutBehavior =
    RunLoopTimeoutBehavior::kContinue;

// This fuzzer uses DomatoLPM to generate JS based on an existing Domato
// rule.
class WebIDLInProcessFuzzer
    : public InProcessBinaryProtoFuzzer<
          domatolpm::generated::webidl_fuzzer_grammar::fuzzcase> {
 public:
  using FuzzCase = domatolpm::generated::webidl_fuzzer_grammar::fuzzcase;
  WebIDLInProcessFuzzer();

  int Fuzz(const FuzzCase& fuzz_case) override;
};

REGISTER_BINARY_PROTO_IN_PROCESS_FUZZER(WebIDLInProcessFuzzer)

WebIDLInProcessFuzzer::WebIDLInProcessFuzzer()
    : InProcessBinaryProtoFuzzer(InProcessFuzzerOptions{
          .run_loop_timeout_behavior = kJsRunLoopTimeoutBehavior,
          .run_loop_timeout = kJsExecutionTimeout,
      }) {}

int WebIDLInProcessFuzzer::Fuzz(const FuzzCase& fuzz_case) {
  static Environment env;
  domatolpm::Context ctx;
  CHECK(domatolpm::webidl_fuzzer_grammar::handle_fuzzer(&ctx, fuzz_case));
  std::string_view js_str(ctx.GetBuilder()->view());
  if (env.kDumpNativeInput) {
    LOG(INFO) << "native_input: " << js_str;
  }
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  testing::AssertionResult res = content::ExecJs(rfh, js_str);
  return 0;
}
