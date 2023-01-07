// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view.h"

#include <algorithm>

#include "ash/shell.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/nearby_sharing/common/nearby_share_features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/components/sharesheet/constants.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/intent_test_util.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "ui/aura/window.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"

namespace ash {
namespace sharesheet {

class SharesheetBubbleViewBrowserTest
    : public ::testing::WithParamInterface<bool>,
      public InProcessBrowserTest {
 public:
  SharesheetBubbleViewBrowserTest() {
    if (GetParam()) {
      scoped_feature_list_.InitAndEnableFeature(::features::kNearbySharing);
    } else {
      scoped_feature_list_.InitAndDisableFeature(::features::kNearbySharing);
    }
  }

  void ShowUi() {
    views::Widget::Widgets old_widgets;
    for (aura::Window* root_window : Shell::GetAllRootWindows())
      views::Widget::GetAllChildWidgets(root_window, &old_widgets);

    ::sharesheet::SharesheetService* const sharesheet_service =
        ::sharesheet::SharesheetServiceFactory::GetForProfile(
            browser()->profile());

    auto intent = apps_util::MakeShareIntent("text", "");
    intent->action = apps_util::kIntentActionSend;
    sharesheet_service->ShowBubble(
        browser()->tab_strip_model()->GetActiveWebContents(), std::move(intent),
        ::sharesheet::LaunchSource::kUnknown, base::DoNothing(),
        base::DoNothing());

    views::Widget::Widgets new_widgets;
    for (aura::Window* root_window : Shell::GetAllRootWindows())
      views::Widget::GetAllChildWidgets(root_window, &new_widgets);

    views::Widget::Widgets added_widgets;
    std::set_difference(new_widgets.begin(), new_widgets.end(),
                        old_widgets.begin(), old_widgets.end(),
                        std::inserter(added_widgets, added_widgets.begin()));
    ASSERT_EQ(added_widgets.size(), 1u);
    sharesheet_widget_ = *added_widgets.begin();
    ASSERT_EQ(sharesheet_widget_->GetName(), "SharesheetBubbleView");
  }

  bool VerifyUi() {
    if (sharesheet_widget_) {
      return sharesheet_widget_->IsVisible();
    }
    return false;
  }

  void DismissUi() {
    ASSERT_TRUE(sharesheet_widget_);
    sharesheet_widget_->Close();
    ASSERT_FALSE(sharesheet_widget_->IsVisible());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  views::Widget* sharesheet_widget_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         SharesheetBubbleViewBrowserTest,
                         ::testing::Bool());

IN_PROC_BROWSER_TEST_P(SharesheetBubbleViewBrowserTest, InvokeUi_Default) {
  ShowUi();
  ASSERT_TRUE(VerifyUi());
  DismissUi();
}

}  // namespace sharesheet
}  // namespace ash
