// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string_view>

#include "base/files/file_util.h"
#include "base/path_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/fuzzing/in_process_proto_fuzzer.h"
#include "chrome/test/fuzzing/notification_service_fuzzer_grammar.h"
#include "chrome/test/fuzzing/notification_service_fuzzer_grammar.pb.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/permissions/permissions_client.h"
#include "content/public/test/browser_test_utils.h"
#include "net/base/filename_util.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/libfuzzer/proto/lpm_interface.h"
#include "testing/libfuzzer/research/domatolpm/domatolpm.h"

struct Environment {
  Environment() {}
  const bool kDumpNativeInput = getenv("LPM_DUMP_NATIVE_INPUT");
};

// This fuzzer uses DomatoLPM to generate JS based on an existing Domato
// rule.
class NotificationServiceInProcessFuzzer
    : public InProcessBinaryProtoFuzzer<
          domatolpm::generated::notification_service_fuzzer_grammar::fuzzcase> {
 public:
  using FuzzCase =
      domatolpm::generated::notification_service_fuzzer_grammar::fuzzcase;
  NotificationServiceInProcessFuzzer() = default;

  base::CommandLine::StringVector GetChromiumCommandLineArguments() override;
  void SetUpOnMainThread() override;
  int Fuzz(const FuzzCase& fuzz_case) override;
};

REGISTER_BINARY_PROTO_IN_PROCESS_FUZZER(NotificationServiceInProcessFuzzer)

base::CommandLine::StringVector
NotificationServiceInProcessFuzzer::GetChromiumCommandLineArguments() {
  return {FILE_PATH_LITERAL("--enable-blink-features=MojoJS")};
}

void NotificationServiceInProcessFuzzer::SetUpOnMainThread() {
  InProcessFuzzer::SetUpOnMainThread();
  base::FilePath exe_path;
  CHECK(base::PathService::Get(base::DIR_EXE, &exe_path));
  embedded_https_test_server().ServeFilesFromDirectory(exe_path);
  CHECK(embedded_https_test_server().Start());

  // This html page includes the necessary scripts for the mojo js bindings.
  // Navigate to this page and execute the fuzzer generated JS in this context.
  CHECK(ui_test_utils::NavigateToURL(
      browser(), embedded_https_test_server().GetURL(
                     "/notification_service_in_process_fuzzer.html")));

  auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
  auto* host_content_setting_map =
      permissions::PermissionsClient::Get()->GetSettingsMap(
          web_contents->GetBrowserContext());
  host_content_setting_map->SetDefaultContentSetting(
      ContentSettingsType::NOTIFICATIONS,
      ContentSetting::CONTENT_SETTING_ALLOW);
}

int NotificationServiceInProcessFuzzer::Fuzz(const FuzzCase& fuzz_case) {
  static Environment env;
  domatolpm::Context ctx;
  CHECK(domatolpm::notification_service_fuzzer_grammar::handle_fuzzer(
      &ctx, fuzz_case));
  std::string_view js_str(ctx.GetBuilder()->view());
  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderFrameHost* rfh = contents->GetPrimaryMainFrame();
  if (env.kDumpNativeInput) {
    LOG(INFO) << "native_input: " << js_str;
  }
  CHECK(content::ExecJs(rfh, js_str));
  return 0;
}
