// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/test/browser_test.h"
#include "extensions/buildflags/buildflags.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

class ContextualTasksBrowserTest : public WebUIMochaBrowserTest {
 protected:
  ContextualTasksBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {contextual_tasks::kContextualTasks,
         contextual_tasks::kContextualTasksForceEntryPointEligibility},
        {});
    set_test_loader_host(chrome::kChromeUIContextualTasksHost);
  }

  void SetUpInProcessBrowserTestFixture() override {
    WebUIMochaBrowserTest::SetUpInProcessBrowserTestFixture();
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ContextualTasksBrowserTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context,
        base::BindRepeating(
            &ContextualTasksBrowserTest::BuildMockAimEligibilityService));
  }

  static std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto aim_eligibility_service =
        std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
            *profile->GetPrefs(), /*template_url_service=*/nullptr,
            /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr);

    ON_CALL(*aim_eligibility_service, IsAimEligible())
        .WillByDefault(testing::Return(true));
    ON_CALL(*aim_eligibility_service, IsCobrowseEligible())
        .WillByDefault(testing::Return(true));
    return aim_eligibility_service;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

// TODO(crbug.com/487147580): Re-enable the test
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, DISABLED_App) {
  RunTest("contextual_tasks/app_test.js", "mocha.run();");
}

// TODO(crbug.com/565086795): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_App_TracksFinishedTopLevelNavigation \
  DISABLED_App_TracksFinishedTopLevelNavigation
#define MAYBE_App_TracksFinishedTopLevelNavigationRace \
  DISABLED_App_TracksFinishedTopLevelNavigationRace
#else
#define MAYBE_App_TracksFinishedTopLevelNavigation \
  App_TracksFinishedTopLevelNavigation
#define MAYBE_App_TracksFinishedTopLevelNavigationRace \
  App_TracksFinishedTopLevelNavigationRace
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       MAYBE_App_TracksFinishedTopLevelNavigation) {
  RunTest("contextual_tasks/app_test.js",
          "runMochaTest('ContextualTasksAppTest', "
          "'tracks finished top level navigation')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       MAYBE_App_TracksFinishedTopLevelNavigationRace) {
  RunTest(
      "contextual_tasks/app_test.js",
      "runMochaTest('ContextualTasksAppTest', "
      "'tracks finished top level navigation when content load wins race')");
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, App_Composebox_BasicMode) {
  RunTest("contextual_tasks/app_composebox_basic_mode_test.js", "mocha.run();");
}

// TODO(crbug.com/480689282): Re-enable the test.
// TODO(crbug.com/527559266): Flaky on ChromeOS.
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, DISABLED_Composebox) {
  RunTest("contextual_tasks/composebox_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Smoke) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkSmokeTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_BasicInput) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkBasicInputTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Dropdown) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkDropdownTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_ContextMenu) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkContextMenuTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Files) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxFilesTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Files_LensBehavior) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkFilesTest "
          "LensBehavior')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Files_FileInputsAndUploads) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkFilesTest "
          "FileInputsAndUploads')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Files_PlaceholderHints) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkFilesTest "
          "PlaceholderHints')");
}

// Run each AutoTab nested suite as a separate test so each group has its own
// per-test time budget.
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_ChipCreationAndMismatch) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "ChipCreationAndMismatch')");
}

// TODO(crbug.com/556296442): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_Composebox_AutoTab_DeletionSemantics \
  DISABLED_Composebox_AutoTab_DeletionSemantics
#else
#define MAYBE_Composebox_AutoTab_DeletionSemantics \
  Composebox_AutoTab_DeletionSemantics
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       MAYBE_Composebox_AutoTab_DeletionSemantics) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "DeletionSemantics')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_SourcesAndUploadTiming) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "SourcesAndUploadTiming')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_UserActionsAndLifecycle) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "UserActionsAndLifecycle')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_SmartTabSharing) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "SmartTabSharing')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Resize) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxResizeTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Glow) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkGlowTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_ErrorScrim) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkErrorScrimTest')");
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       ExtensionPostMessageHandler) {
  RunTest("contextual_tasks/extension_post_message_handler_test.js",
          "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, LensButton) {
  RunTest("contextual_tasks/lens_button_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, LensChip) {
  RunTest("contextual_tasks/lens_chip_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, TabPicker) {
  RunTest("contextual_tasks/tab_picker_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Favicons) {
  RunTest("contextual_tasks/favicons_test.js", "mocha.run();");
}
#endif

// TODO(crbug.com/480689282): Flaky on ChromeOS debug.
// TODO(crbug.com/487147580): Re-enable on Linux.
// TODO(crbug.com/490250939): Flaky elsewhere as well.
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       DISABLED_Composebox_MiscInputs) {
  RunTest("contextual_tasks/composebox_misc_inputs_test.js", "mocha.run();");
}

// TODO(crbug.com/487147580): Re-enable the test
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Composebox_Submit DISABLED_Composebox_Submit
#else
#define MAYBE_Composebox_Submit Composebox_Submit
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_Composebox_Submit) {
  RunTest("contextual_tasks/composebox_submit_test.js", "mocha.run();");
}

// TODO(crbug.com/480689282): Flaky on Linux and ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Composebox_ZeroState DISABLED_Composebox_ZeroState
#else
#define MAYBE_Composebox_ZeroState Composebox_ZeroState
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_Composebox_ZeroState) {
  RunTest("contextual_tasks/composebox_zero_state_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, UnboundedMenu) {
  RunTest("contextual_tasks/unbounded_menu_test.js", "mocha.run();");
}

// TODO(crbug.com/564474841): Flaky on Linux-js coverage.
// Disabled on Linux after timing out during JS coverage collection, after all
// Mocha assertions passed.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_Composebox_Voice_SurfaceAndStartup_CoherenceTrue \
  DISABLED_Composebox_Voice_SurfaceAndStartup_CoherenceTrue
#define MAYBE_Composebox_Voice_SurfaceAndStartup_CoherenceFalse \
  DISABLED_Composebox_Voice_SurfaceAndStartup_CoherenceFalse
#else
#define MAYBE_Composebox_Voice_SurfaceAndStartup_CoherenceTrue \
  Composebox_Voice_SurfaceAndStartup_CoherenceTrue
#define MAYBE_Composebox_Voice_SurfaceAndStartup_CoherenceFalse \
  Composebox_Voice_SurfaceAndStartup_CoherenceFalse
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       MAYBE_Composebox_Voice_SurfaceAndStartup_CoherenceTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = true\\\\) SurfaceAndStartup')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       MAYBE_Composebox_Voice_SurfaceAndStartup_CoherenceFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = false\\\\) SurfaceAndStartup')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_RecognitionAndSubmission_CoherenceTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = true\\\\) RecognitionAndSubmission')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_RecognitionAndSubmission_CoherenceFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = false\\\\) RecognitionAndSubmission')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_ErrorPermissionAndLayout_CoherenceTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = true\\\\) ErrorPermissionAndLayout')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_ErrorPermissionAndLayout_CoherenceFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = false\\\\) ErrorPermissionAndLayout')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Voice_NonCoherenceTranscriptAndCancel) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = false\\\\) NonCoherenceTranscriptAndCancel')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Voice_CoherenceControlsFilesAndLifecycle) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(coherence = true\\\\) CoherenceControlsFilesAndLifecycle')");
}
#endif  // !BUILDFLAG(IS_ANDROID)

// TODO(crbug.com/565086795): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_PostMessageHandler DISABLED_PostMessageHandler
#define MAYBE_TopToolbarTest DISABLED_TopToolbarTest
#define MAYBE_OverflowMenu DISABLED_OverflowMenu
#define MAYBE_Toolbar DISABLED_Toolbar
#define MAYBE_OnboardingTooltip DISABLED_OnboardingTooltip
#else
#define MAYBE_PostMessageHandler PostMessageHandler
#define MAYBE_TopToolbarTest TopToolbarTest
#define MAYBE_OverflowMenu OverflowMenu
#define MAYBE_Toolbar Toolbar
#define MAYBE_OnboardingTooltip OnboardingTooltip
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_PostMessageHandler) {
  RunTest("contextual_tasks/post_message_handler_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_TopToolbarTest) {
  RunTest("contextual_tasks/top_toolbar_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_OverflowMenu) {
  RunTest("contextual_tasks/overflow_menu_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_Toolbar) {
  RunTest("contextual_tasks/toolbar_test.js", "mocha.run();");
}
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_OnboardingTooltip) {
  RunTest("contextual_tasks/onboarding_tooltip_test.js", "mocha.run();");
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, InfoTooltip) {
  RunTest("contextual_tasks/info_tooltip_test.js", "mocha.run();");
}
// TODO(crbug.com/529817776): Re-enable when the timeouts get fixed.
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_WebView DISABLED_WebView
#else
#define MAYBE_WebView WebView
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_WebView) {
  RunTest("contextual_tasks/contextual_tasks_webview_browsertest.js",
          "mocha.run();");
}
#endif

// TODO(crbug.com/565086795): Flaky on Android.
#if BUILDFLAG(IS_ANDROID)
#define MAYBE_ClipPath DISABLED_ClipPath
#define MAYBE_WindowManager DISABLED_WindowManager
#define MAYBE_Utils DISABLED_Utils
#else
#define MAYBE_ClipPath ClipPath
#define MAYBE_WindowManager WindowManager
#define MAYBE_Utils Utils
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_ClipPath) {
  RunTest("contextual_tasks/utils/clip_path_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_WindowManager) {
  RunTest("contextual_tasks/window_manager_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_Utils) {
  RunTest("contextual_tasks/utils_test.js", "mocha.run();");
}
