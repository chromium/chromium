// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "chrome/test/fuzzing/webidl_fuzzing/webidl_fuzzer_grammar.h"
#include "chrome/test/fuzzing/webidl_fuzzing/webidl_fuzzer_grammar.pb.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/default_handlers.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "testing/libfuzzer/research/domatolpm/domatolpm.h"

struct Environment {
  Environment() {}
  const bool kDumpNativeInput = getenv("LPM_DUMP_NATIVE_INPUT");
};

constexpr std::optional<base::TimeDelta> kJsExecutionTimeout = base::Seconds(3);
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
  void SetUpOnMainThread() override;

  int Fuzz(const FuzzCase& fuzz_case) override;
  base::CommandLine::StringVector GetChromiumCommandLineArguments() override {
    return {
        FILE_PATH_LITERAL("--enable-blink-test-features"),
        FILE_PATH_LITERAL("--enable-experimental-web-platform-features"),
    };
  }
};

REGISTER_BINARY_PROTO_IN_PROCESS_FUZZER(WebIDLInProcessFuzzer)

static std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_content_type("text/html");
  http_response->set_content("Echo");
  return http_response;
}

void WebIDLInProcessFuzzer::SetUpOnMainThread() {
  InProcessFuzzer::SetUpOnMainThread();
  embedded_https_test_server().RegisterRequestHandler(
      base::BindRepeating(&HandleRequest));
  ASSERT_TRUE(embedded_https_test_server().Start());
  CHECK(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL("/echo")));
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  permissions::PermissionRequestManager::FromWebContents(contents)
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::ACCEPT_ALL);
}

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
