// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view.h"

#include <algorithm>
#include <memory>

#include "ash/constants/app_types.h"
#include "ash/frame/non_client_frame_view_ash.h"
#include "chrome/browser/sharesheet/sharesheet_metrics.h"
#include "chrome/browser/sharesheet/sharesheet_service.h"
#include "chrome/browser/sharesheet/sharesheet_service_factory.h"
#include "chrome/browser/sharesheet/sharesheet_types.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_bubble_view_delegate.h"
#include "chrome/browser/ui/ash/sharesheet/sharesheet_header_view.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chrome/test/base/testing_profile.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

namespace ash {
namespace sharesheet {

class TestWidgetDelegate : public views::WidgetDelegateView {
 public:
  TestWidgetDelegate() = default;
  ~TestWidgetDelegate() override = default;

  // views::WidgetDelegateView:
  bool CanActivate() const override { return true; }

  std::unique_ptr<views::NonClientFrameView> CreateNonClientFrameView(
      views::Widget* widget) override {
    return std::make_unique<NonClientFrameViewAsh>(widget);
  }
};

class SharesheetBubbleViewTest : public ChromeAshTestBase {
 public:
  SharesheetBubbleViewTest() = default;

  // ChromeAshTestBase:
  void SetUp() override {
    ChromeAshTestBase::SetUp();

    profile_ = std::make_unique<TestingProfile>();

    // Set up parent window for sharesheet to anchor to.
    auto* widget = new views::Widget();
    views::Widget::InitParams params;
    params.delegate = new TestWidgetDelegate();
    params.context = GetContext();
    widget->Init(std::move(params));
    widget->Show();
    widget->GetNativeWindow()->SetProperty(aura::client::kShowStateKey,
                                           ui::SHOW_STATE_FULLSCREEN);
    gfx::Size window_size = widget->GetWindowBoundsInScreen().size();
    auto* content_view = new views::NativeViewHost();
    content_view->SetBounds(0, 0, window_size.width(), window_size.height());
    widget->GetContentsView()->AddChildView(content_view);

    parent_window_ = widget->GetNativeWindow();
  }

  void ShowAndVerifyBubble(apps::mojom::IntentPtr intent) {
    ::sharesheet::SharesheetService* const sharesheet_service =
        ::sharesheet::SharesheetServiceFactory::GetForProfile(profile_.get());
    sharesheet_service->ShowBubbleForTesting(
        parent_window_, std::move(intent),
        /*contains_hosted_document=*/false,
        ::sharesheet::SharesheetMetrics::LaunchSource::kUnknown,
        /*delivered_callback=*/base::DoNothing(),
        /*close_callback=*/base::DoNothing());
    bubble_delegate_ = static_cast<SharesheetBubbleViewDelegate*>(
        sharesheet_service->GetUiDelegateForTesting(parent_window_));
    EXPECT_NE(bubble_delegate_, nullptr);
    sharesheet_bubble_view_ = bubble_delegate_->GetBubbleViewForTesting();
    EXPECT_NE(sharesheet_bubble_view_, nullptr);

    ASSERT_TRUE(bubble_delegate_->IsBubbleVisible());
    sharesheet_widget_ = sharesheet_bubble_view_->GetWidget();
    ASSERT_EQ(sharesheet_widget_->GetName(), "SharesheetBubbleView");
  }

  void CloseBubble() {
    bubble_delegate_->CloseBubble(::sharesheet::SharesheetResult::kCancel);
    // |bubble_delegate_| and |sharesheet_bubble_view_| destruct on close.
    bubble_delegate_ = nullptr;
    sharesheet_bubble_view_ = nullptr;

    ASSERT_FALSE(sharesheet_widget_->IsVisible());
  }

  apps::mojom::IntentPtr CreateDefaultTextIntent() {
    auto intent = apps_util::CreateShareIntentFromText("text", "title");
    intent->action = apps_util::kIntentActionSend;
    return intent;
  }

  void CloseBubbleWithEscKey() {
    GetEventGenerator()->PressAndReleaseKey(ui::VKEY_ESCAPE);
    // |bubble_delegate_| and |sharesheet_bubble_view_| destruct on close.
    bubble_delegate_ = nullptr;
    sharesheet_bubble_view_ = nullptr;

    ASSERT_FALSE(IsSharesheetVisible());
  }

  bool IsSharesheetVisible() { return sharesheet_widget_->IsVisible(); }

  SharesheetBubbleView* sharesheet_bubble_view() {
    return sharesheet_bubble_view_;
  }

  SharesheetHeaderView* header_view() {
    return sharesheet_bubble_view_->GetHeaderViewForTesting();
  }

  views::View* body_view() {
    return sharesheet_bubble_view_->GetBodyViewForTesting();
  }

  views::View* footer_view() {
    return sharesheet_bubble_view_->GetFooterViewForTesting();
  }

 private:
  gfx::NativeWindow parent_window_;
  std::unique_ptr<TestingProfile> profile_;
  SharesheetBubbleViewDelegate* bubble_delegate_;
  SharesheetBubbleView* sharesheet_bubble_view_;
  views::Widget* sharesheet_widget_;
};

TEST_F(SharesheetBubbleViewTest, BubbleDoesOpenAndClose) {
  ShowAndVerifyBubble(CreateDefaultTextIntent());
  CloseBubble();
}

TEST_F(SharesheetBubbleViewTest, EmptyState) {
  ShowAndVerifyBubble(CreateDefaultTextIntent());

  // Header should contain Share label.
  ASSERT_TRUE(header_view()->GetVisible());
  ASSERT_EQ(header_view()->children().size(), 1u);

  // Body view should contain 3 children, an image and 2 labels.
  ASSERT_TRUE(body_view()->GetVisible());
  ASSERT_EQ(body_view()->children().size(), 3u);

  // Footer should be an empty view that just acts as padding.
  ASSERT_TRUE(footer_view()->GetVisible());
  ASSERT_EQ(footer_view()->children().size(), 0);
}

TEST_F(SharesheetBubbleViewTest, CloseWithEscKey) {
  ShowAndVerifyBubble(apps_util::CreateShareIntentFromText("text", "title"));
  CloseBubbleWithEscKey();
}

TEST_F(SharesheetBubbleViewTest, CloseMultipleTimes) {
  ShowAndVerifyBubble(apps_util::CreateShareIntentFromText("text", "title"));
  CloseBubbleWithEscKey();
  CloseBubbleWithEscKey();
}

TEST_F(SharesheetBubbleViewTest, HoldEscapeKey) {
  GetEventGenerator()->PressKey(ui::VKEY_ESCAPE, ui::EventFlags::EF_NONE);
  ShowAndVerifyBubble(apps_util::CreateShareIntentFromText("text", "title"));
  GetEventGenerator()->ReleaseKey(ui::VKEY_ESCAPE, ui::EventFlags::EF_NONE);
  CloseBubbleWithEscKey();
}

}  // namespace sharesheet
}  // namespace ash
