// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check_deref.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/ui/webui/new_tab_page/composebox/variations/composebox_fieldtrial.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> BuildMockAimServiceEligibilityServiceInstance(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<MockAimEligibilityService> mock_aim_eligibility_service =
      std::make_unique<MockAimEligibilityService>(
          CHECK_DEREF(profile->GetPrefs()), /*template_url_service=*/nullptr,
          /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr);

  ON_CALL(*mock_aim_eligibility_service, IsAimEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_aim_eligibility_service, IsAimLocallyEligible())
      .WillByDefault(testing::Return(true));
  ON_CALL(*mock_aim_eligibility_service, IsServerEligibilityEnabled())
      .WillByDefault(testing::Return(true));

  return std::move(mock_aim_eligibility_service);
}

}  // namespace

class NewTabPageFocusTest : public WebUIMochaFocusTest {
 public:
  void SetUpBrowserContextKeyedServices(
      content::BrowserContext* context) override {
    WebUIMochaFocusTest::SetUpBrowserContextKeyedServices(context);
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindOnce(BuildMockAimServiceEligibilityServiceInstance));
  }

 protected:
  NewTabPageFocusTest() {
    set_test_loader_host(chrome::kChromeUINewTabPageHost);
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ntp_composebox::kNtpComposebox,
                              ntp_realbox::kNtpRealboxNext},
        /*disabled_features=*/{});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NewTabPageFocusTest, DoodleShareDialogFocus) {
  RunTest("new_tab_page/doodle_share_dialog_focus_test.js", "mocha.run()");
}

IN_PROC_BROWSER_TEST_F(NewTabPageFocusTest, AppFocus) {
  RunTest("new_tab_page/app_focus_test.js", "mocha.run()");
}
