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
    ON_CALL(*aim_eligibility_service, IsAimUrl(testing::_, testing::_))
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

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       App_TracksFinishedTopLevelNavigation) {
  RunTest("contextual_tasks/app_test.js",
          "runMochaTest('ContextualTasksAppTest', "
          "'tracks finished top level navigation')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       App_TracksFinishedTopLevelNavigationRace) {
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

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Smoke_ForkTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkSmokeTest "
          "\\\\(useContextualTasksComposeboxFork = true\\\\)')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Smoke_ForkFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkSmokeTest "
          "\\\\(useContextualTasksComposeboxFork = false\\\\)')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_BasicInput_ForkTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkBasicInputTest "
          "\\\\(useContextualTasksComposeboxFork = true\\\\)')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_BasicInput_ForkFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkBasicInputTest "
          "\\\\(useContextualTasksComposeboxFork = false\\\\)')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Dropdown_ForkTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkDropdownTest "
          "\\\\(useContextualTasksComposeboxFork = true\\\\)')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Dropdown_ForkFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkDropdownTest "
          "\\\\(useContextualTasksComposeboxFork = false\\\\)')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_ContextMenu_ForkTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkContextMenuTest "
          "\\\\(useContextualTasksComposeboxFork = true\\\\)')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_ContextMenu_ForkFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkContextMenuTest "
          "\\\\(useContextualTasksComposeboxFork = false\\\\)')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Files) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxFilesTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Files_ForkTrue_LensBehavior) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkFilesTest "
          "\\\\(useContextualTasksComposeboxFork = true\\\\) "
          "LensBehavior')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Files_ForkTrue_FileInputsAndUploads) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkFilesTest "
          "\\\\(useContextualTasksComposeboxFork = true\\\\) "
          "FileInputsAndUploads')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Files_ForkTrue_PlaceholderHints) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkFilesTest "
          "\\\\(useContextualTasksComposeboxFork = true\\\\) "
          "PlaceholderHints')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Files_ForkFalse_LensBehavior) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkFilesTest "
          "\\\\(useContextualTasksComposeboxFork = false\\\\) "
          "LensBehavior')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Files_ForkFalse_FileInputsAndUploads) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkFilesTest "
          "\\\\(useContextualTasksComposeboxFork = false\\\\) "
          "FileInputsAndUploads')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_Files_ForkFalse_PlaceholderHints) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkFilesTest "
          "\\\\(useContextualTasksComposeboxFork = false\\\\) "
          "PlaceholderHints')");
}

// Run each AutoTab arm's nested suites as separate browser tests so each group
// has its own per-test time budget.
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_ForkTrue_ChipCreationAndMismatch) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "\\\\(useContextualTasksComposeboxFork = true\\\\) "
          "ChipCreationAndMismatch')");
}

// TODO(crbug.com/556296442): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_Composebox_AutoTab_ForkTrue_DeletionSemantics \
  DISABLED_Composebox_AutoTab_ForkTrue_DeletionSemantics
#else
#define MAYBE_Composebox_AutoTab_ForkTrue_DeletionSemantics \
  Composebox_AutoTab_ForkTrue_DeletionSemantics
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       MAYBE_Composebox_AutoTab_ForkTrue_DeletionSemantics) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "\\\\(useContextualTasksComposeboxFork = true\\\\) "
          "DeletionSemantics')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_ForkTrue_SourcesAndUploadTiming) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "\\\\(useContextualTasksComposeboxFork = true\\\\) "
          "SourcesAndUploadTiming')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_ForkTrue_UserActionsAndLifecycle) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "\\\\(useContextualTasksComposeboxFork = true\\\\) "
          "UserActionsAndLifecycle')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_ForkTrue_SmartTabSharing) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "\\\\(useContextualTasksComposeboxFork = true\\\\) "
          "SmartTabSharing')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_ForkFalse_ChipCreationAndMismatch) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "\\\\(useContextualTasksComposeboxFork = false\\\\) "
          "ChipCreationAndMismatch')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_ForkFalse_DeletionSemantics) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "\\\\(useContextualTasksComposeboxFork = false\\\\) "
          "DeletionSemantics')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_ForkFalse_SourcesAndUploadTiming) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "\\\\(useContextualTasksComposeboxFork = false\\\\) "
          "SourcesAndUploadTiming')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_ForkFalse_UserActionsAndLifecycle) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "\\\\(useContextualTasksComposeboxFork = false\\\\) "
          "UserActionsAndLifecycle')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_AutoTab_ForkFalse_SmartTabSharing) {
  RunTest("contextual_tasks/composebox_files_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkAutoTabTest "
          "\\\\(useContextualTasksComposeboxFork = false\\\\) "
          "SmartTabSharing')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Resize) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxResizeTest')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Glow_ForkTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkGlowTest "
          "\\\\(useContextualTasksComposeboxFork = true\\\\)')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Glow_ForkFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkGlowTest "
          "\\\\(useContextualTasksComposeboxFork = false\\\\)')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_ErrorScrim_ForkTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkErrorScrimTest "
          "\\\\(useContextualTasksComposeboxFork = true\\\\)')");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       Composebox_ErrorScrim_ForkFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkErrorScrimTest "
          "\\\\(useContextualTasksComposeboxFork = false\\\\)')");
}

#if BUILDFLAG(ENABLE_EXTENSIONS_CORE)
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, InputPlate) {
  RunTest("contextual_tasks/input_plate_test.js", "mocha.run();");
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

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_SurfaceAndStartup_ForkTrue_CoherenceTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = true, "
          "coherence = true\\\\) SurfaceAndStartup')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_SurfaceAndStartup_ForkTrue_CoherenceFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = true, "
          "coherence = false\\\\) SurfaceAndStartup')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_SurfaceAndStartup_ForkFalse_CoherenceTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = false, "
          "coherence = true\\\\) SurfaceAndStartup')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_SurfaceAndStartup_ForkFalse_CoherenceFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = false, "
          "coherence = false\\\\) SurfaceAndStartup')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_RecognitionAndSubmission_ForkTrue_CoherenceTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = true, "
          "coherence = true\\\\) RecognitionAndSubmission')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_RecognitionAndSubmission_ForkTrue_CoherenceFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = true, "
          "coherence = false\\\\) RecognitionAndSubmission')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_RecognitionAndSubmission_ForkFalse_CoherenceTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = false, "
          "coherence = true\\\\) RecognitionAndSubmission')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_RecognitionAndSubmission_ForkFalse_CoherenceFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = false, "
          "coherence = false\\\\) RecognitionAndSubmission')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_ErrorPermissionAndLayout_ForkTrue_CoherenceTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = true, "
          "coherence = true\\\\) ErrorPermissionAndLayout')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_ErrorPermissionAndLayout_ForkTrue_CoherenceFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = true, "
          "coherence = false\\\\) ErrorPermissionAndLayout')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_ErrorPermissionAndLayout_ForkFalse_CoherenceTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = false, "
          "coherence = true\\\\) ErrorPermissionAndLayout')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_ErrorPermissionAndLayout_ForkFalse_CoherenceFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = false, "
          "coherence = false\\\\) ErrorPermissionAndLayout')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_NonCoherenceTranscriptAndCancel_ForkTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = true, "
          "coherence = false\\\\) NonCoherenceTranscriptAndCancel')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_NonCoherenceTranscriptAndCancel_ForkFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = false, "
          "coherence = false\\\\) NonCoherenceTranscriptAndCancel')");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    Composebox_Voice_CoherenceControlsFilesAndLifecycle_ForkTrue) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = true, "
          "coherence = true\\\\) CoherenceControlsFilesAndLifecycle')");
}

// TODO(crbug.com/556296442): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_Composebox_Voice_CoherenceControlsFilesAndLifecycle_ForkFalse \
  DISABLED_Composebox_Voice_CoherenceControlsFilesAndLifecycle_ForkFalse
#else
#define MAYBE_Composebox_Voice_CoherenceControlsFilesAndLifecycle_ForkFalse \
  Composebox_Voice_CoherenceControlsFilesAndLifecycle_ForkFalse
#endif
IN_PROC_BROWSER_TEST_F(
    ContextualTasksBrowserTest,
    MAYBE_Composebox_Voice_CoherenceControlsFilesAndLifecycle_ForkFalse) {
  RunTest("contextual_tasks/composebox_test.js",
          "runMochaSuite('ContextualTasksComposeboxForkVoiceTest "
          "\\\\(useContextualTasksComposeboxFork = false, "
          "coherence = true\\\\) CoherenceControlsFilesAndLifecycle')");
}
#endif  // !BUILDFLAG(IS_ANDROID)

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, PostMessageHandler) {
  RunTest("contextual_tasks/post_message_handler_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, TopToolbarTest) {
  RunTest("contextual_tasks/top_toolbar_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, OverflowMenu) {
  RunTest("contextual_tasks/overflow_menu_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Toolbar) {
  RunTest("contextual_tasks/toolbar_test.js", "mocha.run();");
}
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, OnboardingTooltip) {
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

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, ClipPath) {
  RunTest("contextual_tasks/utils/clip_path_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, WindowManager) {
  RunTest("contextual_tasks/window_manager_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Utils) {
  RunTest("contextual_tasks/utils_test.js", "mocha.run();");
}
