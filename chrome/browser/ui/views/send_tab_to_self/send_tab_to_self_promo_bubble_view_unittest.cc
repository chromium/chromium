// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_promo_bubble_view.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/send_tab_to_self/features.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_frame_view.h"

namespace send_tab_to_self {

namespace {

class StubSendTabToSelfBubbleController : public SendTabToSelfBubbleController {
 public:
  explicit StubSendTabToSelfBubbleController(content::WebContents* web_contents)
      : SendTabToSelfBubbleController(web_contents) {}

  ~StubSendTabToSelfBubbleController() override = default;

  AccountInfo GetSharingAccountInfo() override {
    AccountInfo info;
    info.email = "user@host.com";
    return info;
  }
};

}  // namespace

class SendTabToSelfPromoBubbleViewTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    // Create an anchor for the bubble.
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);

    web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    // Owned by WebContents.
    auto controller = std::make_unique<StubSendTabToSelfBubbleController>(
        web_contents_.get());
    controller_ = controller.get();
    web_contents_->SetUserData(StubSendTabToSelfBubbleController::UserDataKey(),
                               std::move(controller));
  }

  void CreateBubble(SendTabToSelfPromoBubbleView::PromoType promo_type) {
    bubble_ = new SendTabToSelfPromoBubbleView(
        views::BubbleAnchor(anchor_widget_->GetContentsView()),
        web_contents_.get(), promo_type);
    views::BubbleDialogDelegateView::CreateBubble(bubble_);
  }

  void TearDown() override {
    if (bubble_) {
      views::Widget* widget = bubble_->GetWidget();
      bubble_ = nullptr;
      widget->CloseNow();
    }
    anchor_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  TestingProfile profile_;
  base::test::ScopedFeatureList feature_list_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<SendTabToSelfPromoBubbleView> bubble_ = nullptr;
  // Owned by WebContents.
  raw_ptr<StubSendTabToSelfBubbleController> controller_;
};

TEST_F(SendTabToSelfPromoBubbleViewTest,
       InitLayout_EnhancedUiEnabled_LoadsModernizedDesign) {
  feature_list_.InitAndEnableFeature(kSendTabToSelfEnhancedDesktopUI);

  CreateBubble(SendTabToSelfPromoBubbleView::PromoType::kSignInPromo);

  // Title should match modernized title strictly.
  EXPECT_EQ(
      bubble_->GetWindowTitle(),
      l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_TITLE));

  // Ok button (Sign In) should be visible and use modernized label.
  ASSERT_TRUE(bubble_->GetOkButton());
  EXPECT_EQ(bubble_->GetOkButton()->GetText(),
            l10n_util::GetStringUTF16(
                IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_BUTTON_LABEL));

  // Frame view should have a header view (illustration).
  EXPECT_NE(nullptr, bubble_->GetBubbleFrameView()->GetHeaderViewForTesting());
}

TEST_F(SendTabToSelfPromoBubbleViewTest,
       InitLayout_EnhancedUiDisabled_LoadsLegacyDesign) {
  feature_list_.InitAndDisableFeature(kSendTabToSelfEnhancedDesktopUI);

  CreateBubble(SendTabToSelfPromoBubbleView::PromoType::kSignInPromo);

  // Title should match legacy title strictly.
  EXPECT_EQ(bubble_->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF));

  // Ok button (Sign In) should be visible and use legacy label.
  ASSERT_TRUE(bubble_->GetOkButton());
  EXPECT_EQ(
      bubble_->GetOkButton()->GetText(),
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_ACCOUNT_CHOOSER_SIGN_IN));

  // Frame view should not have a header view (legacy design has no header).
  EXPECT_EQ(nullptr, bubble_->GetBubbleFrameView()->GetHeaderViewForTesting());
}

TEST_F(SendTabToSelfPromoBubbleViewTest,
       InitLayout_NoTargetDevices_LoadsDeviceActivityDesign) {
  feature_list_.InitAndEnableFeature(kSendTabToSelfEnhancedDesktopUI);

  CreateBubble(SendTabToSelfPromoBubbleView::PromoType::kNoTargetDevice);

  // Ok button should not be visible.
  EXPECT_FALSE(bubble_->GetOkButton());

  // Frame view should not have a header view.
  EXPECT_EQ(nullptr, bubble_->GetBubbleFrameView()->GetHeaderViewForTesting());
}

}  // namespace send_tab_to_self
