// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

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
      AimEligibilityService* aim_service)
      : ContextualTasksUiService(profile,
                                 /*delegate=*/nullptr,
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
      AimEligibilityServiceFactory::GetForProfile(profile));
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
            browser()->profile()));
  }

  MockContextualTasksUiServiceForAuth* GetMockUiService() {
    return static_cast<MockContextualTasksUiServiceForAuth*>(
        contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
            browser()->profile()));
  }

 protected:
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

  EXPECT_TRUE(controller->should_route_to_contextual_tasks());
}
