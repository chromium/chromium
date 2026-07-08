// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_delegate_desktop.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace {

std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<MockAimEligibilityService> mock_service =
      std::make_unique<MockAimEligibilityService>(
          CHECK_DEREF(profile->GetPrefs()), /*template_url_service=*/nullptr,
          /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr,
          AimEligibilityService::Configuration{});

  omnibox::RuleSet* rule_set = mock_service->config().mutable_rule_set();
  auto* input_rule = rule_set->add_input_type_rules();
  input_rule->set_input_type(omnibox::INPUT_TYPE_LENS_IMAGE);
  input_rule->set_max_instance(1);
  input_rule->add_allowed_input_types(omnibox::INPUT_TYPE_LENS_IMAGE);

  mock_service->config().add_input_type_configs()->set_input_type(
      omnibox::INPUT_TYPE_LENS_IMAGE);

  EXPECT_CALL(*mock_service, GetSearchboxConfig())
      .WillRepeatedly(testing::Return(&mock_service->config()));

  return std::move(mock_service);
}

class MockContextualTasksUiServiceForAuth
    : public contextual_tasks::ContextualTasksUiService {
 public:
  MockContextualTasksUiServiceForAuth(
      Profile* profile,
      contextual_tasks::ContextualTasksService* ct_service,
      AimEligibilityService* aim_service,
      std::unique_ptr<contextual_tasks::ContextualTasksUiServiceDelegate>
          delegate)
      : ContextualTasksUiService(profile,
                                 std::move(delegate),
                                 ct_service,
                                 /*identity_manager=*/nullptr,
                                 aim_service,
                                 /*eligibility_manager=*/nullptr,
                                 /*cookie_synchronizer=*/nullptr) {}
  ~MockContextualTasksUiServiceForAuth() override = default;

  MOCK_METHOD(bool, IsSignedInToBrowserWithValidCredentials, (), (override));
  MOCK_METHOD(bool, CookieJarContainsPrimaryAccount, (), (override));
};

std::unique_ptr<KeyedService> BuildMockUiServiceForAuth(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<MockContextualTasksUiServiceForAuth>(
      profile,
      contextual_tasks::ContextualTasksServiceFactory::GetForProfile(profile),
      AimEligibilityServiceFactory::GetForProfile(profile),
      std::make_unique<
          contextual_tasks::ContextualTasksUiServiceDelegateDesktop>(profile));
}

}  // namespace

class LensSearchControllerUnificationBrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{contextual_tasks::kContextualTasks, {}},
         {lens::features::kLensSidePanelUnification,
          {{"allow-signed-out", "true"}}}},
        {});
    InProcessBrowserTest::SetUp();
  }

  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    InProcessBrowserTest::SetUpBrowserContextKeyedServices(context);
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindOnce(&BuildMockAimEligibilityService));
    contextual_tasks::ContextualTasksUiServiceFactory::GetInstance()
        ->SetTestingFactory(context,
                            base::BindOnce(&BuildMockUiServiceForAuth));
  }

  MockAimEligibilityService* GetMockAimService() {
    return static_cast<MockAimEligibilityService*>(
        AimEligibilityServiceFactory::GetInstance()->GetForProfile(
            browser()->GetProfile()));
  }

  MockContextualTasksUiServiceForAuth* GetMockUiService() {
    return static_cast<MockContextualTasksUiServiceForAuth*>(
        contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
            browser()->GetProfile()));
  }

 protected:
  void VerifyLinkClickInPanelOpensNewTab(bool aim_eligible,
                                         bool cobrowse_eligible) {
    // Set up ineligibility.
    auto* mock_aim = GetMockAimService();
    ASSERT_TRUE(mock_aim);
    EXPECT_CALL(*mock_aim, IsAimEligible())
        .WillRepeatedly(testing::Return(aim_eligible));
    EXPECT_CALL(*mock_aim, IsCobrowseEligible())
        .WillRepeatedly(testing::Return(cobrowse_eligible));

    auto* ui_service = GetMockUiService();
    ASSERT_TRUE(ui_service);

    auto* side_panel_ui = SidePanelUI::From(browser());
    ASSERT_TRUE(side_panel_ui);

    ASSERT_TRUE(embedded_test_server()->Start());
    GURL guest_url = embedded_test_server()->GetURL("/empty.html");

    ui_service->StartTaskUiInSidePanel(
        browser(), browser()->GetActiveTabInterface(), guest_url,
        /*session_handle=*/nullptr);

    // Wait for side panel to show.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return side_panel_ui->IsSidePanelShowing() &&
             side_panel_ui->GetCurrentEntryId() ==
                 SidePanelEntry::Id::kContextualTasks;
    }));

    // Get the Panel WebContents.
    auto* controller =
        contextual_tasks::ContextualTasksPanelController::From(browser());
    ASSERT_TRUE(controller);
    content::WebContents* panel_wc = controller->GetActiveWebContents();
    ASSERT_TRUE(panel_wc);

    content::WaitForLoadStop(panel_wc);

    // Wait for the WebUI to load and the webview to be present.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return content::EvalJs(
                 panel_wc,
                 "document.querySelector('contextual-tasks-app')."
                 "shadowRoot.getElementById('threadFrame') !== null")
          .ExtractBool();
    }));

    // Wait until webview has src.
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return content::EvalJs(
                 panel_wc,
                 "document.querySelector('contextual-tasks-app').shadowRoot."
                 "getElementById('threadFrame').src !== ''")
          .ExtractBool();
    }));

    // Helper to execute script in webview.
    auto execute_script_in_webview = [](content::WebContents* wc,
                                        const std::string& script) {
      return content::EvalJs(wc, base::StringPrintf(R"(
        new Promise((resolve) => {
          const app = document.querySelector('contextual-tasks-app');
          const webview = app ? app.shadowRoot.getElementById('threadFrame')
                              : null;
          if (!webview) {
            resolve("no_webview");
            return;
          }
          try {
            webview.executeScript({code: "%s"}, (results) => {
              if (chrome.runtime.lastError) {
                resolve("error: " + chrome.runtime.lastError.message);
              } else {
                resolve(results && results.length > 0
                            ? String(results[0])
                            : "null_results");
              }
            });
          } catch (e) {
            resolve("exception: " + e.message);
          }
        });
      )",
                                                    script.c_str()));
    };

    // Wait until we can execute script in webview (which means it is loaded).
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return execute_script_in_webview(panel_wc, "document.readyState")
                 .ExtractString() == "complete";
    }));

    // Now, we want to watch for a new tab.
    ui_test_utils::AllBrowserTabAddedWaiter tab_waiter;

    // Trigger window.open in the webview.
    GURL target_url = embedded_test_server()->GetURL("/title1.html");
    std::string open_script =
        base::StringPrintf("window.open('%s')", target_url.spec().c_str());

    ASSERT_TRUE(
        content::ExecJs(panel_wc, base::StringPrintf(R"(
      const app = document.querySelector('contextual-tasks-app');
      const webview = app ? app.shadowRoot.getElementById('threadFrame')
                          : null;
      webview.executeScript({code: "%s"});
    )",
                                                     open_script.c_str())));

    // Wait for the tab to be added.
    content::WebContents* new_tab = tab_waiter.Wait();
    ASSERT_TRUE(new_tab);

    // Verify the URL of the new tab.
    EXPECT_EQ(new_tab->GetVisibleURL(), target_url);
  }

  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LensSearchControllerUnificationBrowserTest,
                       RoutesToUnifiedPanel_SignedOut) {
  auto* mock_ui = GetMockUiService();
  ASSERT_TRUE(mock_ui);
  EXPECT_CALL(*mock_ui, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(testing::Return(false));

  auto* controller =
      LensSearchController::From(browser()->GetActiveTabInterface());
  ASSERT_TRUE(controller);

  controller->OpenLensOverlay(
      lens::LensOverlayInvocationSource::kContextualTasksComposebox);

  EXPECT_TRUE(controller->should_route_to_contextual_tasks());
}

IN_PROC_BROWSER_TEST_F(LensSearchControllerUnificationBrowserTest,
                       RoutesToUnifiedPanel_SignedOut_WhenCobrowseIneligible) {
  auto* mock_aim = GetMockAimService();
  ASSERT_TRUE(mock_aim);
  EXPECT_CALL(*mock_aim, IsAimEligible()).WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_aim, IsCobrowseEligible())
      .WillRepeatedly(testing::Return(false));

  auto* controller =
      LensSearchController::From(browser()->GetActiveTabInterface());
  ASSERT_TRUE(controller);

  controller->OpenLensOverlay(
      lens::LensOverlayInvocationSource::kContextualTasksComposebox);

  EXPECT_TRUE(controller->should_route_to_contextual_tasks());
}

IN_PROC_BROWSER_TEST_F(LensSearchControllerUnificationBrowserTest,
                       RoutesToUnifiedPanel_SignedIn) {
  auto* mock_ui = GetMockUiService();
  ASSERT_TRUE(mock_ui);
  EXPECT_CALL(*mock_ui, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_ui, CookieJarContainsPrimaryAccount())
      .WillRepeatedly(testing::Return(true));

  auto* controller =
      LensSearchController::From(browser()->GetActiveTabInterface());
  ASSERT_TRUE(controller);

  controller->OpenLensOverlay(
      lens::LensOverlayInvocationSource::kContextualTasksComposebox);

  EXPECT_TRUE(controller->should_route_to_contextual_tasks());
}

IN_PROC_BROWSER_TEST_F(LensSearchControllerUnificationBrowserTest,
                       RoutesToUnifiedPanel_SignedIn_CobrowseIneligible) {
  auto* mock_ui = GetMockUiService();
  ASSERT_TRUE(mock_ui);
  EXPECT_CALL(*mock_ui, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_ui, CookieJarContainsPrimaryAccount())
      .WillRepeatedly(testing::Return(true));

  auto* mock_aim = GetMockAimService();
  ASSERT_TRUE(mock_aim);
  EXPECT_CALL(*mock_aim, IsAimEligible()).WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_aim, IsCobrowseEligible())
      .WillRepeatedly(testing::Return(false));

  auto* controller =
      LensSearchController::From(browser()->GetActiveTabInterface());
  ASSERT_TRUE(controller);

  controller->OpenLensOverlay(
      lens::LensOverlayInvocationSource::kContextualTasksComposebox);

  EXPECT_TRUE(controller->should_route_to_contextual_tasks());
}

class LensSearchControllerUnificationSignOutDisabledTest
    : public LensSearchControllerUnificationBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{contextual_tasks::kContextualTasks, {}},
         {lens::features::kLensSidePanelUnification,
          {{"allow-signed-out", "false"}}}},
        {});
    InProcessBrowserTest::SetUp();
  }
};

IN_PROC_BROWSER_TEST_F(LensSearchControllerUnificationSignOutDisabledTest,
                       RoutesToLegacyPanel_SignedOut_WithOverrideFlagDisabled) {
  auto* mock_ui = GetMockUiService();
  ASSERT_TRUE(mock_ui);
  EXPECT_CALL(*mock_ui, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(testing::Return(false));

  auto* controller =
      LensSearchController::From(browser()->GetActiveTabInterface());
  ASSERT_TRUE(controller);

  controller->OpenLensOverlay(
      lens::LensOverlayInvocationSource::kContextualTasksComposebox);

  EXPECT_FALSE(controller->should_route_to_contextual_tasks());
}

IN_PROC_BROWSER_TEST_F(LensSearchControllerUnificationSignOutDisabledTest,
                       RoutesToUnifiedPanel_SignedIn_WithOverrideFlagDisabled) {
  auto* mock_ui = GetMockUiService();
  ASSERT_TRUE(mock_ui);
  EXPECT_CALL(*mock_ui, IsSignedInToBrowserWithValidCredentials())
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(*mock_ui, CookieJarContainsPrimaryAccount())
      .WillRepeatedly(testing::Return(true));

  auto* controller =
      LensSearchController::From(browser()->GetActiveTabInterface());
  ASSERT_TRUE(controller);

  controller->OpenLensOverlay(
      lens::LensOverlayInvocationSource::kContextualTasksComposebox);
}

IN_PROC_BROWSER_TEST_F(LensSearchControllerUnificationBrowserTest,
                       IsWebUIEnabledInIncognito_WithUnificationEnabled) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  Profile* incognito_profile = incognito_browser->GetProfile();
  EXPECT_TRUE(incognito_profile->IsOffTheRecord());

  ContextualTasksUIConfig config;
  EXPECT_TRUE(config.IsWebUIEnabled(incognito_profile));
}

class LensSearchControllerUnificationDisabledTest
    : public InProcessBrowserTest {
 public:
  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{contextual_tasks::kContextualTasks, {}}},
        {{lens::features::kLensSidePanelUnification}});
    InProcessBrowserTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LensSearchControllerUnificationDisabledTest,
                       IsWebUIEnabledInIncognito_WithUnificationDisabled) {
  Browser* incognito_browser = CreateIncognitoBrowser();
  Profile* incognito_profile = incognito_browser->GetProfile();
  EXPECT_TRUE(incognito_profile->IsOffTheRecord());

  ContextualTasksUIConfig config;
  EXPECT_FALSE(config.IsWebUIEnabled(incognito_profile));
}

IN_PROC_BROWSER_TEST_F(LensSearchControllerUnificationBrowserTest,
                       LinkClickInPanelOpensNewTabWhenCobrowseIneligible) {
  VerifyLinkClickInPanelOpensNewTab(/*aim_eligible=*/true,
                                    /*cobrowse_eligible=*/false);
}

IN_PROC_BROWSER_TEST_F(LensSearchControllerUnificationBrowserTest,
                       LinkClickInPanelOpensNewTabWhenAimIneligible) {
  VerifyLinkClickInPanelOpensNewTab(/*aim_eligible=*/false,
                                    /*cobrowse_eligible=*/false);
}
