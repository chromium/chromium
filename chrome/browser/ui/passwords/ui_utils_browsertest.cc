// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/ui_utils.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/password_manager/core/browser/manage_passwords_referrer.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;

class ManagePasswordsUiUtilsBrowserTest : public InProcessBrowserTest {
 public:
  ManagePasswordsUiUtilsBrowserTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {autofill::features::kManagePasswordsPerceptionSurvey,
         autofill::features::kYourSavedInfoSettingsPage},
        /*disabled_features=*/{});
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(), base::BindRepeating(&BuildMockHatsService)));
  }

  void TearDownOnMainThread() override {
    mock_hats_service_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  MockHatsService* mock_hats_service() { return mock_hats_service_; }

 private:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<MockHatsService> mock_hats_service_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(ManagePasswordsUiUtilsBrowserTest,
                       NavigateToManagePasswordsPageTriggersSurvey) {
  EXPECT_CALL(*mock_hats_service(),
              LaunchDelayedSurvey(kHatsSurveyTriggerManagePasswordsPerception,
                                  10000, _, _));

  NavigateToManagePasswordsPage(
      browser(), password_manager::ManagePasswordsReferrer::kChromeSettings);
}
