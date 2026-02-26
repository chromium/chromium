// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_ui_all_urls_browser_test.h"

#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "chrome/browser/ui/webui/webui_urls_for_test.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/collaboration/public/features.h"
#include "components/history_clusters/core/features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"
#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#else
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "components/signin/public/base/signin_switches.h"
#endif

WebUIAllUrlsBrowserTest::WebUIAllUrlsBrowserTest() {
  std::vector<base::test::FeatureRef> enabled_features;
  enabled_features.push_back(ntp_features::kCustomizeChromeWallpaperSearch);
  enabled_features.push_back(
      optimization_guide::features::kOptimizationGuideModelExecution);
  enabled_features.push_back(collaboration::features::kCollaborationComments);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  enabled_features.push_back(whats_new::kForceEnabled);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  enabled_features.push_back(ash::features::kDriveFsMirroring);
  enabled_features.push_back(ash::features::kShimlessRMAOsUpdate);
  enabled_features.push_back(chromeos::features::kUploadOfficeToCloud);
#endif
  feature_list_.InitWithFeatures(enabled_features, {});
}

WebUIAllUrlsBrowserTest::~WebUIAllUrlsBrowserTest() = default;

void WebUIAllUrlsBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  InProcessBrowserTest::SetUpCommandLine(command_line);
  if (GetParam() == std::string_view(chrome::kChromeUISearchEngineChoiceURL)) {
    // Command line arguments needed to render chrome://search-engine-choice.
    command_line->AppendSwitchASCII(switches::kSearchEngineChoiceCountry, "BE");
    command_line->AppendSwitchASCII(
        variations::switches::kVariationsOverrideCountry, "BE");
    command_line->AppendSwitch(switches::kForceSearchEngineChoiceScreen);
    command_line->AppendSwitch(
        switches::kIgnoreNoFirstRunForSearchEngineChoiceScreen);
  }
#if BUILDFLAG(IS_CHROMEOS)
  command_line->AppendSwitchASCII(ash::switches::kSamlPasswordChangeUrl,
                                  "http://password-change.example");
  if (GetParam() == std::string_view("chrome://shimless-rma")) {
    command_line->AppendSwitchASCII(ash::switches::kLaunchRma, "");
  }
#endif
}

#if BUILDFLAG(IS_CHROMEOS)
void WebUIAllUrlsBrowserTest::SetUpOnMainThread() {
  browser()->profile()->GetPrefs()->SetBoolean(
      ash::prefs::kSamlInSessionPasswordChangeEnabled, true);

  // This is needed to simulate the presence of the ODFS extension, which is
  // checked in `IsMicrosoftOfficeOneDriveIntegrationAllowedAndOdfsInstalled`.
  auto fake_provider = ash::file_system_provider::FakeExtensionProvider::Create(
      extension_misc::kODFSExtensionId);
  auto* service = ash::file_system_provider::Service::Get(browser()->profile());
  service->RegisterProvider(std::move(fake_provider));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

void WebUIAllUrlsBrowserTest::WaitBeforeNavigation() {
  // A number of these tests are flaky due to navigating to and from the
  // page faster than expected. This adds a small delay to ensure any
  // crashes represent a real bug and are not just a reflection of the
  // test navigating to and then away from a URL quicker than expected.
  base::RunLoop ui_thread_delayed_task_loop;
  content::GetUIThreadTaskRunner()->PostDelayedTask(
      FROM_HERE,
      base::BindLambdaForTesting([&]() { ui_thread_delayed_task_loop.Quit(); }),
      base::Milliseconds(10));
  ui_thread_delayed_task_loop.Run();
}

std::string WebUIAllUrlsBrowserTest::ParamInfoToString(
    const ::testing::TestParamInfo<const char*>& info) {
  std::string name(info.param);
  std::replace_if(
      name.begin(), name.end(),
      [](unsigned char c) { return !absl::ascii_isalnum(c); }, '_');
  return name;
}
