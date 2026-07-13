// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/web_ui_all_urls_browser_test.h"

#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/webui_urls_for_test.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/collaboration/public/features.h"
#include "components/history_clusters/core/features.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/regional_capabilities/regional_capabilities_switches.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/search_engines_switches.h"
#include "components/variations/variations_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "printing/buildflags/buildflags.h"
#include "third_party/abseil-cpp/absl/strings/ascii.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "ash/constants/ash_extension_constants.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_login_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "chrome/browser/ash/file_system_provider/fake_extension_provider.h"
#include "chrome/browser/ash/file_system_provider/service.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/prefs/pref_service.h"
#else
#include "chrome/browser/ui/webui/whats_new/whats_new_util.h"
#include "components/signin/public/base/signin_switches.h"
#endif

WebUIAllUrlsBrowserTest::WebUIAllUrlsBrowserTest() {
  std::vector<base::test::FeatureRefAndParams> enabled_features;
  auto enable_feature = [&](const base::Feature& feature,
                            std::map<std::string, std::string> params = {}) {
    enabled_features.push_back({feature, std::move(params)});
  };

  enable_feature(ntp_features::kCustomizeChromeWallpaperSearch);
  enable_feature(
      optimization_guide::features::kOptimizationGuideModelExecution);
  enable_feature(collaboration::features::kCollaborationComments);
  enable_feature(omnibox::kComposeboxDriveContextMenuOption);

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS)
  enable_feature(features::kAiOverlayDialog);
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  enable_feature(whats_new::kForceEnabled);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  enable_feature(ash::features::kDriveFsMirroring);
  enable_feature(ash::features::kShimlessRMAOsUpdate);
  enable_feature(chromeos::features::kUploadOfficeToCloud);
#endif

  enable_feature(features::kTabsFromOtherDevicesSidePanel);
  enable_feature(contextual_cueing::kContextualCueingV2);

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  enable_feature(switches::kFirstRunDesktopRefresh);
  enable_feature(switches::kFirstRunDesktopChoiceScreenRefresh);
  enable_feature(switches::kFirstRunDesktopRevamp);
#endif

  const std::vector<base::test::FeatureRef> disabled_features = {
      privacy_sandbox::kPrivacySandboxAdPrivacyUxDeprecation};

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  enable_feature(switches::kMagiChromePasskeySignIn, {{"flow_type", "banner"}});
#endif

  feature_list_.InitWithFeaturesAndParameters(enabled_features,
                                              disabled_features);
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
  auto* service =
      ash::file_system_provider::Service::Get(browser()->GetProfile());
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
    const testing::TestParamInfo<std::string_view>& info) {
  std::string name(info.param);
  std::replace_if(
      name.begin(), name.end(),
      [](unsigned char c) { return !absl::ascii_isalnum(c); }, '_');
  return name;
}
