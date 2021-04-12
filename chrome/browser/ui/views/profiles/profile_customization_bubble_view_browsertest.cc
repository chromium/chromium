// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/profiles/profile_customization_bubble_view.h"

#include <string>

#include "base/callback_list.h"
#include "base/optional.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/feature_engagement/tracker_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/toolbar_button_provider.h"
#include "chrome/browser/ui/views/profiles/avatar_toolbar_button.h"
#include "chrome/browser/ui/views/user_education/feature_promo_controller_views.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/feature_engagement/public/tracker.h"
#include "components/feature_engagement/test/test_tracker.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "content/public/test/browser_test.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace {

std::unique_ptr<KeyedService> CreateTestTracker(content::BrowserContext*) {
  return feature_engagement::CreateTestTracker();
}

}  // namespace

class ProfileCustomizationBubbleBrowserTest : public DialogBrowserTest {
 public:
  ProfileCustomizationBubbleBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        feature_engagement::kIPHProfileSwitchFeature);
    subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ProfileCustomizationBubbleBrowserTest::RegisterTestTracker));
  }

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    ProfileCustomizationBubbleView::CreateBubble(browser()->profile(),
                                                 GetAvatarButton());
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

  base::test::ScopedFeatureList scoped_feature_list_;
  base::CallbackListSubscription subscription_;
};

IN_PROC_BROWSER_TEST_F(ProfileCustomizationBubbleBrowserTest,
                       InvokeUi_default) {
  ShowAndVerifyUi();
}

IN_PROC_BROWSER_TEST_F(ProfileCustomizationBubbleBrowserTest, IPH) {
  AvatarToolbarButton::SetIPHMinDelayAfterCreationForTesting(
      base::TimeDelta::FromSeconds(0));
  // Create the customization bubble, owned by the view hierarchy.
  ProfileCustomizationBubbleView* bubble =
      ProfileCustomizationBubbleView::CreateBubble(browser()->profile(),
                                                   GetAvatarButton());

  feature_engagement::Tracker* tracker =
      BrowserView::GetBrowserViewForBrowser(browser())
          ->feature_promo_controller()
          ->feature_engagement_tracker();

  EXPECT_NE(
      tracker->GetTriggerState(feature_engagement::kIPHProfileSwitchFeature),
      feature_engagement::Tracker::TriggerState::HAS_BEEN_DISPLAYED);

  bubble->OnDoneButtonClicked();

  base::RunLoop loop;
  tracker->AddOnInitializedCallback(
      base::BindLambdaForTesting([&loop](bool success) {
        DCHECK(success);
        loop.Quit();
      }));
  loop.Run();

  ASSERT_TRUE(tracker->IsInitialized());
  EXPECT_EQ(
      tracker->GetTriggerState(feature_engagement::kIPHProfileSwitchFeature),
      feature_engagement::Tracker::TriggerState::HAS_BEEN_DISPLAYED);
}
