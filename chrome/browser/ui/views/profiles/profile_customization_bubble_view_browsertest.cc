// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_customization_bubble_view.h"

#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/feature_list.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/signin/signin_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/signin_view_controller.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/user_education/browser_feature_promo_controller.h"
#include "chrome/browser/ui/webui/signin/login_ui_test_utils.h"
#include "chrome/browser/ui/webui/signin/profile_customization_handler.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/test_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/user_education/test/feature_promo_test_util.h"
#include "content/public/test/browser_test.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace {

std::unique_ptr<KeyedService> CreateTestTracker(content::BrowserContext*) {
  return feature_engagement::CreateTestTracker();
}

}  // namespace

// TODO(https://crbug.com/1459176): Rename the file to match the class once
// `ProfileCustomizationBubbleView` is deleted.
class ProfileCustomizationBrowserTest : public DialogBrowserTest {
 public:
  ProfileCustomizationBrowserTest() {
    feature_list_.InitAndEnableFeatures(
        {feature_engagement::kIPHProfileSwitchFeature});
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ProfileCustomizationBrowserTest::RegisterTestTracker));
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ProfileCustomizationBubbleView::CreateBubble(browser(), GetAvatarButton());
  }

  // Returns the avatar button, which is the anchor view for the customization
  // bubble.
  views::View* GetAvatarButton() {
    BrowserView* browser_view =
        BrowserView::GetBrowserViewForBrowser(browser());
    views::View* avatar_button =
        browser_view->toolbar_button_provider()->GetAvatarToolbarButton();
    DCHECK(avatar_button);
    return avatar_button;
  }

 private:
  static void RegisterTestTracker(content::BrowserContext* context) {
    feature_engagement::TrackerFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating(&CreateTestTracker));
  }

  feature_engagement::test::ScopedIphFeatureList feature_list_;
  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(ProfileCustomizationBrowserTest, IPH) {
  AvatarToolbarButton::SetIPHMinDelayAfterCreationForTesting(base::Seconds(0));
  auto lock = BrowserFeaturePromoController::BlockActiveWindowCheckForTesting();
  // Create the customization dialog, owned by the view hierarchy.
  ProfileCustomizationBubbleView::CreateBubble(browser(), GetAvatarButton());

  EXPECT_TRUE(browser()->signin_view_controller()->ShowsModalDialog());

  feature_engagement::Tracker* tracker =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->GetFeaturePromoController()
          ->feature_engagement_tracker();

  EXPECT_NE(
      tracker->GetTriggerState(feature_engagement::kIPHProfileSwitchFeature),
      feature_engagement::Tracker::TriggerState::HAS_BEEN_DISPLAYED);

  ASSERT_TRUE(
      login_ui_test_utils::CompleteProfileCustomizationDialog(browser()));

  ASSERT_TRUE(user_education::test::WaitForFeatureEngagementReady(tracker));
  EXPECT_EQ(
      tracker->GetTriggerState(feature_engagement::kIPHProfileSwitchFeature),
      feature_engagement::Tracker::TriggerState::HAS_BEEN_DISPLAYED);
}
