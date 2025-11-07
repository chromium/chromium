// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "chrome/test/fuzzing/mojo_js_fuzzing/mojo_js_fuzzer_grammar.h"
#include "chrome/test/fuzzing/mojo_js_fuzzing/mojo_js_fuzzer_grammar.pb.h"
#include "components/content_settings/core/browser/content_settings_registry.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/permissions_client.h"
#include "content/public/test/browser_test_utils.h"

struct Environment {
  Environment() {}
  const bool kDumpNativeInput = getenv("LPM_DUMP_NATIVE_INPUT");
};

constexpr std::optional<base::TimeDelta> kJsExecutionTimeout = base::Seconds(5);
constexpr RunLoopTimeoutBehavior kJsRunLoopTimeoutBehavior =
    RunLoopTimeoutBehavior::kContinue;

// This fuzzer uses DomatoLPM to generate JS based on an existing Domato
// rule.
class MojoJSInProcessFuzzer
    : public InProcessBinaryProtoFuzzer<
          domatolpm::generated::mojo_js_fuzzer_grammar::fuzzcase> {
 public:
  using FuzzCase = domatolpm::generated::mojo_js_fuzzer_grammar::fuzzcase;
  MojoJSInProcessFuzzer();
  void SetUpOnMainThread() override;

  int Fuzz(const FuzzCase& fuzz_case) override;
  base::CommandLine::StringVector GetChromiumCommandLineArguments() override {
    return {FILE_PATH_LITERAL("--enable-blink-features=MojoJS"),
            FILE_PATH_LITERAL("--enable-blink-test-features"),
            FILE_PATH_LITERAL("--enable-experimental-web-platform-features"),
            FILE_PATH_LITERAL("--disable-kill-after-bad-ipc")};
  }
};

REGISTER_BINARY_PROTO_IN_PROCESS_FUZZER(MojoJSInProcessFuzzer)

void MojoJSInProcessFuzzer::SetUpOnMainThread() {
  InProcessFuzzer::SetUpOnMainThread();
  base::FilePath exe_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &exe_path));
  embedded_https_test_server().ServeFilesFromDirectory(exe_path);
  embedded_test_server()->SetSSLConfig(net::EmbeddedTestServer::CERT_OK);
  CHECK(embedded_https_test_server().Start());

  // This html page includes the necessary scripts for the mojo js bindings.
  // Navigate to this page and execute the fuzzer generated JS in this context.
  CHECK(ui_test_utils::NavigateToURL(
      browser(),
      embedded_https_test_server().GetURL("/mojo_js_in_process_fuzzer.html")));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  permissions::PermissionRequestManager::FromWebContents(web_contents)
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::ACCEPT_ALL);

  auto* host_content_setting_map =
      permissions::PermissionsClient::Get()->GetSettingsMap(
          web_contents->GetBrowserContext());

  auto* content_setting_registry =
      content_settings::ContentSettingsRegistry::GetInstance();
  for (int i = static_cast<int>(ContentSettingsType::kMinValue);
       i < static_cast<int>(ContentSettingsType::kMaxValue); i++) {
    auto content_setting_type = static_cast<ContentSettingsType>(i);
    auto* content_setting_info =
        content_setting_registry->Get(content_setting_type);
    if (content_setting_info && content_setting_info->IsDefaultSettingValid(
                                    ContentSetting::CONTENT_SETTING_ALLOW)) {
      host_content_setting_map->SetDefaultContentSetting(
          content_setting_type, ContentSetting::CONTENT_SETTING_ALLOW);
    }
  }
}

MojoJSInProcessFuzzer::MojoJSInProcessFuzzer()
    : InProcessBinaryProtoFuzzer(InProcessFuzzerOptions{
          .run_loop_timeout_behavior = kJsRunLoopTimeoutBehavior,
          .run_loop_timeout = kJsExecutionTimeout,
      }) {}

int MojoJSInProcessFuzzer::Fuzz(const FuzzCase& fuzz_case) {
  static Environment env;
  domatolpm::Context ctx;
  CHECK(domatolpm::mojo_js_fuzzer_grammar::handle_fuzzer(&ctx, fuzz_case));
  std::string_view js_str(ctx.GetBuilder()->view());
  if (env.kDumpNativeInput) {
    LOG(INFO) << "native_input: " << js_str;
  }
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  auto res = content::ExecJs(rfh, js_str);
  if (!res) {
    return -1;
  }
  return 0;
}
