// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/test/browser_test.h"
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
// Only running on Mac and Desktop Android currently.
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_LINUX) || \
    (BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_DESKTOP_ANDROID))
#define MAYBE_App DISABLED_App
#else
#define MAYBE_App App
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_App) {
  RunTest("contextual_tasks/app_test.js", "mocha.run();");
}

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, App_Composebox_BasicMode) {
  RunTest("contextual_tasks/app_composebox_basic_mode_test.js", "mocha.run();");
}

// TODO(crbug.com/487147580): Re-enable the test
// TODO(crbug.com/527559266): Flaky on ChromeOS.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#define MAYBE_Composebox DISABLED_Composebox
#else
#define MAYBE_Composebox Composebox
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_Composebox) {
  RunTest("contextual_tasks/composebox_test.js", "mocha.run();");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, Composebox_Files) {
  RunTest("contextual_tasks/composebox_files_test.js", "mocha.run();");
}

// TODO(crbug.com/480689282): Flaky on ChromeOS debug.
// TODO(crbug.com/487147580): Re-enable on Linux.
// TODO(crbug.com/490250939): Flaky elsewhere as well.
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest,
                       DISABLED_Composebox_MiscInputs) {
  RunTest("contextual_tasks/composebox_misc_inputs_test.js", "mocha.run();");
}

// TODO(crbug.com/487147580): Re-enable the test
#if BUILDFLAG(IS_LINUX)
#define MAYBE_Composebox_Submit DISABLED_Composebox_Submit
#else
#define MAYBE_Composebox_Submit Composebox_Submit
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_Composebox_Submit) {
  RunTest("contextual_tasks/composebox_submit_test.js", "mocha.run();");
}

// TODO(crbug.com/480689282): Flaky on Linux and ChromeOS debug.
#if BUILDFLAG(IS_LINUX) || (BUILDFLAG(IS_CHROMEOS) && !defined(NDEBUG))
#define MAYBE_Composebox_ZeroState DISABLED_Composebox_ZeroState
#else
#define MAYBE_Composebox_ZeroState Composebox_ZeroState
#endif
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, MAYBE_Composebox_ZeroState) {
  RunTest("contextual_tasks/composebox_zero_state_test.js", "mocha.run();");
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

#if !BUILDFLAG(IS_ANDROID)
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, OnboardingTooltip) {
  RunTest("contextual_tasks/onboarding_tooltip_test.js", "mocha.run();");
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

#if !BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_DESKTOP_ANDROID)
IN_PROC_BROWSER_TEST_F(ContextualTasksBrowserTest, WindowManager) {
  RunTest("contextual_tasks/window_manager_test.js", "mocha.run();");
}
#endif
