// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/ui/omnibox/omnibox_next_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_aim_popup_webui_content.h"
#include "chrome/browser/ui/views/omnibox/omnibox_popup_aim_presenter.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/omnibox/browser/mock_aim_eligibility_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace {

std::unique_ptr<KeyedService> BuildMockAimEligibilityService(
    content::BrowserContext* context) {
  auto service = std::make_unique<testing::NiceMock<MockAimEligibilityService>>(
      *Profile::FromBrowserContext(context)->GetPrefs(),
      /*template_url_service=*/nullptr,
      /*url_loader_factory=*/nullptr, /*identity_manager=*/nullptr,
      AimEligibilityService::Configuration{});
  ON_CALL(*service, GetCountryCode()).WillByDefault(testing::Return("US"));
  ON_CALL(*service, GetLocaleImpl()).WillByDefault(testing::Return("en-US"));
  return service;
}

}  // namespace

class OmniboxAimPopupBrowserTest : public InProcessBrowserTest {
 public:
  OmniboxAimPopupBrowserTest() {
    feature_list_.InitWithFeatures({omnibox::internal::kWebUIOmniboxAimPopup,
                                    omnibox::internal::kWebUIOmniboxPopup},
                                   {});
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &OmniboxAimPopupBrowserTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    AimEligibilityServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&BuildMockAimEligibilityService));
  }

 protected:
  LocationBarView* location_bar() {
    return BrowserView::GetBrowserViewForBrowser(browser())
        ->toolbar()
        ->location_bar_view();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(OmniboxAimPopupBrowserTest, ClearEventuallyDetaches) {
  // Ensure we are in AIM state so presenter is created.
  location_bar()->GetOmniboxController()->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kAim);

  auto* presenter = location_bar()->GetOmniboxPopupAimPresenter();
  ASSERT_TRUE(presenter);

  presenter->Show();
  auto* content =
      static_cast<OmniboxAimPopupWebUIContent*>(presenter->GetWebUIContent());
  ASSERT_TRUE(content);

  // Wait for the WebContents to be ready so the handler can be created.
  content::WaitForLoadStop(content->GetWebContents());

  base::test::TestFuture<void> future;
  auto subscription =
      content->AddWebContentsDetachedCallback(base::BindLambdaForTesting(
          [&](views::WebView* view) { future.SetValue(); }));

  // Trigger `OmniboxPopupAimPresenter::Hide()` by setting state to `kNone`.
  // This ensures `in_popup_state_transition_` is handled correctly by
  // `LocationBarView` and the state matches widget visibility.
  location_bar()->GetOmniboxController()->popup_state_manager()->SetPopupState(
      OmniboxPopupState::kNone);

  // This should eventually call `OmniboxPopupWebUIBaseContent::Detach`,
  // either immediately if there's no handler, or after the Mojo callback if
  // there is.
  EXPECT_TRUE(future.Wait());
}
