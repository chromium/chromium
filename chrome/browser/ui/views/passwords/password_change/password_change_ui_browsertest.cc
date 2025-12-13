// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/affiliations/affiliation_service_factory.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
#include "chrome/browser/password_manager/password_change_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

using ::affiliations::AffiliationService;
using ::affiliations::MockAffiliationService;
using ::testing::NiceMock;
using ::testing::Return;

std::unique_ptr<KeyedService> CreateTestAffiliationService(
    content::BrowserContext* context) {
  return std::make_unique<NiceMock<MockAffiliationService>>();
}

std::unique_ptr<KeyedService> CreateMockOptimizationService(
    content::BrowserContext* context) {
  return std::make_unique<NiceMock<MockOptimizationGuideKeyedService>>();
}

}  // namespace

class PasswordChangeUiBrowserTest : public DialogBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating([](content::BrowserContext* context) {
                  AffiliationServiceFactory::GetInstance()->SetTestingFactory(
                      context,
                      base::BindRepeating(&CreateTestAffiliationService));
                  OptimizationGuideKeyedServiceFactory::GetInstance()
                      ->SetTestingFactory(
                          context,
                          base::BindRepeating(&CreateMockOptimizationService));
                }));
  }

  MockAffiliationService* affiliation_service() {
    return static_cast<MockAffiliationService*>(
        AffiliationServiceFactory::GetForProfile(browser()->profile()));
  }

  MockOptimizationGuideKeyedService* mock_optimization_guide_keyed_service() {
    return static_cast<MockOptimizationGuideKeyedService*>(
        OptimizationGuideKeyedServiceFactory::GetForProfile(
            browser()->profile()));
  }

 private:
  void ShowUi(const std::string& name) override {
    GURL main_url = GURL("https://example.com/");
    GURL password_change_url = GURL("https://example.com/password");
    ON_CALL(*affiliation_service(), GetChangePasswordURL(main_url))
        .WillByDefault(Return(password_change_url));
    if (StartsWith(name, "LeakBubble", base::CompareCase::SENSITIVE)) {
      ON_CALL(*mock_optimization_guide_keyed_service(),
              ShouldFeatureBeCurrentlyEnabledForUser(
                  optimization_guide::UserVisibleFeatureKey::
                      kPasswordChangeSubmission))
          .WillByDefault(Return(true));
    }

    password_manager::PasswordForm form;
    form.url = main_url;
    form.signon_realm = main_url.GetWithEmptyPath().spec();
    form.username_value = u"username";
    form.password_value = u"password";
    ManagePasswordsUIController::FromWebContents(
        browser()->tab_strip_model()->GetActiveWebContents())
        ->OnCredentialLeak(password_manager::LeakedPasswordDetails(
            password_manager::CreateLeakType(
                password_manager::IsSaved(true),
                password_manager::IsReused(false),
                password_manager::IsSyncing(true),
                password_manager::HasChangePasswordUrl(true)),
            std::move(form),
            /*in_account_store=*/false));
  }

  ChromePasswordChangeService* password_change_service() {
    return PasswordChangeServiceFactory::GetForProfile(browser()->profile());
  }

  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(PasswordChangeUiBrowserTest, InvokeUi_PrivacyNotice) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(PasswordChangeUiBrowserTest, InvokeUi_LeakBubble) {
  ShowAndVerifyUi();
}
