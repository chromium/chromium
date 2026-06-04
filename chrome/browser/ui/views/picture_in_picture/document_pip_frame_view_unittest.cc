// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_frame_view.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "build/build_config.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_widget_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/picture_in_picture_window_options/picture_in_picture_window_options.mojom.h"
#include "ui/base/hit_test.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_manager.h"
#include "google_apis/gaia/gaia_id.h"
#endif

namespace {

#if BUILDFLAG(IS_CHROMEOS)
constexpr char kTestUserEmail[] = "user@example.com";
#endif

blink::mojom::PictureInPictureWindowOptions MakePipOptions(
    bool disallow_return_to_opener) {
  blink::mojom::PictureInPictureWindowOptions opts;
  opts.width = 400;
  opts.height = 300;
  opts.disallow_return_to_opener = disallow_return_to_opener;
  opts.prefer_initial_window_placement = false;
  return opts;
}

}  // namespace

class DocumentPipFrameViewTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    test_views_delegate()->set_use_desktop_native_widgets(true);

#if BUILDFLAG(IS_CHROMEOS)
    ASSERT_TRUE(user_manager::UserManager::IsInitialized());
    auto* fake_user_manager = static_cast<ash::FakeChromeUserManager*>(
        user_manager::UserManager::Get());

    const GaiaId kTestUserGaiaId("1111111111");
    auto account_id =
        AccountId::FromUserEmailGaiaId(kTestUserEmail, kTestUserGaiaId);
    fake_user_manager->AddUserWithAffiliationAndTypeAndProfile(
        account_id, /*is_affiliated=*/true, user_manager::UserType::kRegular,
        &profile_);
    fake_user_manager->LoginUser(account_id);
#endif  // BUILDFLAG(IS_CHROMEOS)

    opener_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    ASSERT_TRUE(opener_web_contents_);
    // The thin origin chip reads the opener's security state.
    ChromeSecurityStateTabHelper::CreateForWebContents(
        opener_web_contents_.get());

    opener_host_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET);
    auto* web_view = opener_host_widget_->SetContentsView(
        std::make_unique<views::WebView>(&profile_));
    web_view->SetWebContents(opener_web_contents_.get());
    opener_host_widget_->Show();
  }

  void TearDown() override {
    opener_web_contents_.reset();
    opener_host_widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

  content::WebContents* opener() { return opener_web_contents_.get(); }

  // Creates a DocumentPipHost and opens a PiP widget with the given option.
  // Returns the frame view from the widget.
  DocumentPipFrameView* CreatePipAndGetFrameView(
      bool disallow_return_to_opener) {
    DocumentPipHost::CreateForWebContents(opener());
    auto* host = DocumentPipHost::FromWebContents(opener());
    auto child =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    host->CreatePipWidget(std::move(child),
                          MakePipOptions(disallow_return_to_opener));

    views::Widget* widget = host->GetWidget();
    EXPECT_TRUE(widget);
    auto* frame_view = widget->non_client_view()->frame_view();
    EXPECT_TRUE(frame_view);
    return static_cast<DocumentPipFrameView*>(frame_view);
  }

  views::ImageButton* GetBackToTabButton(DocumentPipFrameView* frame_view) {
    return frame_view->back_to_tab_button_;
  }

  views::ImageButton* GetCloseButton(DocumentPipFrameView* frame_view) {
    return frame_view->close_image_button_;
  }

  views::Button* GetOriginChip(DocumentPipFrameView* frame_view) {
    return frame_view->origin_chip_;
  }

  views::ImageView* GetSecurityIcon(DocumentPipFrameView* frame_view) {
    return frame_view->security_icon_;
  }

  views::Label* GetOriginLabel(DocumentPipFrameView* frame_view) {
    return frame_view->origin_label_;
  }

  bool GetRenderActive(DocumentPipFrameView* frame_view) {
    return frame_view->render_active_;
  }

  views::FlexLayoutView* GetButtonContainer(DocumentPipFrameView* frame_view) {
    return frame_view->button_container_view_;
  }

  bool ShowPageInfo(DocumentPipFrameView* frame_view) {
    return frame_view->ShowPageInfo();
  }

  void OnMouseEnteredOrExitedWindow(DocumentPipFrameView* frame_view,
                                    bool entered) {
    frame_view->OnMouseEnteredOrExitedWindow(entered);
  }

 protected:
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  std::unique_ptr<views::Widget> opener_host_widget_;
  std::unique_ptr<content::WebContents> opener_web_contents_;
};

// Back-to-tab button is present when disallow_return_to_opener is false.
TEST_F(DocumentPipFrameViewTest, BackToTabButtonPresent_WhenAllowed) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_TRUE(GetBackToTabButton(frame_view));
}

// Back-to-tab button is absent when disallow_return_to_opener is true.
TEST_F(DocumentPipFrameViewTest, BackToTabButtonAbsent_WhenDisallowed) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/true);

  EXPECT_FALSE(GetBackToTabButton(frame_view));
}

// Close button is always present regardless of disallow_return_to_opener.
TEST_F(DocumentPipFrameViewTest, CloseButtonAlwaysPresent) {
  auto* frame_view_allowed =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);
  EXPECT_TRUE(GetCloseButton(frame_view_allowed));

  // Reset to create a second host.
  opener_web_contents_.reset();
  opener_web_contents_ =
      content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
  ChromeSecurityStateTabHelper::CreateForWebContents(
      opener_web_contents_.get());
  auto* web_view =
      static_cast<views::WebView*>(opener_host_widget_->GetContentsView());
  web_view->SetWebContents(opener_web_contents_.get());

  auto* frame_view_disallowed =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/true);
  EXPECT_TRUE(GetCloseButton(frame_view_disallowed));
}

// Origin label exists.
TEST_F(DocumentPipFrameViewTest, OriginLabelExists) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_TRUE(GetOriginLabel(frame_view));
}

// NonClientHitTest returns HTCLIENT for the close button bounds.
TEST_F(DocumentPipFrameViewTest, HitTestCloseButton_ReturnsClient) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  // Force layout so buttons have valid bounds.
  auto* widget = frame_view->GetWidget();
  widget->SetBounds(gfx::Rect(0, 0, 400, 300));
  widget->LayoutRootViewIfNecessary();

  views::ImageButton* close_btn = GetCloseButton(frame_view);
  ASSERT_TRUE(close_btn);
  ASSERT_FALSE(close_btn->bounds().IsEmpty());

  // Convert the center of the close button to frame-view coordinates.
  gfx::Point center = close_btn->bounds().CenterPoint();
  views::View::ConvertPointToTarget(close_btn->parent(), frame_view, &center);

  EXPECT_EQ(HTCLIENT, frame_view->NonClientHitTest(center));
}

// NonClientHitTest returns HTCLIENT for the back-to-tab button bounds.
TEST_F(DocumentPipFrameViewTest, HitTestBackToTabButton_ReturnsClient) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  auto* widget = frame_view->GetWidget();
  widget->SetBounds(gfx::Rect(0, 0, 400, 300));
  widget->LayoutRootViewIfNecessary();

  views::ImageButton* back_btn = GetBackToTabButton(frame_view);
  ASSERT_TRUE(back_btn);
  ASSERT_FALSE(back_btn->bounds().IsEmpty());

  gfx::Point center = back_btn->bounds().CenterPoint();
  views::View::ConvertPointToTarget(back_btn->parent(), frame_view, &center);

  EXPECT_EQ(HTCLIENT, frame_view->NonClientHitTest(center));
}

// NonClientHitTest returns HTCAPTION for the top bar area outside of buttons.
TEST_F(DocumentPipFrameViewTest, HitTestTopBar_ReturnsCaption) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  auto* widget = frame_view->GetWidget();
  widget->SetBounds(gfx::Rect(0, 0, 400, 300));
  widget->LayoutRootViewIfNecessary();

  // Use the button container's left edge as the right boundary of the spacer
  // (not the close button, which is further right inside the container).
  views::Button* chip = GetOriginChip(frame_view);
  ASSERT_TRUE(chip);
  gfx::Point chip_right_pt(chip->GetMirroredBounds().right(),
                           chip->GetMirroredBounds().CenterPoint().y());
  views::View::ConvertPointToTarget(chip->parent(), frame_view, &chip_right_pt);

  views::FlexLayoutView* btn_container = GetButtonContainer(frame_view);
  ASSERT_TRUE(btn_container);
  gfx::Point container_left_pt(
      btn_container->GetMirroredBounds().x(),
      btn_container->GetMirroredBounds().CenterPoint().y());
  views::View::ConvertPointToTarget(btn_container->parent(), frame_view,
                                    &container_left_pt);

  // Pick the midpoint of the gap between the chip and the button container,
  // vertically centered in the 34px top bar.
  int spacer_x = (chip_right_pt.x() + container_left_pt.x()) / 2;
  ASSERT_GT(spacer_x, chip_right_pt.x());
  ASSERT_LT(spacer_x, container_left_pt.x());
  gfx::Point top_bar_point(spacer_x, 17);

  EXPECT_EQ(HTCAPTION, frame_view->NonClientHitTest(top_bar_point));
}

// GetMinimumSize returns a positive size.
TEST_F(DocumentPipFrameViewTest, GetMinimumSize_IsPositive) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  gfx::Size min_size = frame_view->GetMinimumSize();
  EXPECT_GT(min_size.width(), 0);
  EXPECT_GT(min_size.height(), 0);
}

// GetMaximumSize is at least as large as GetMinimumSize.
TEST_F(DocumentPipFrameViewTest, GetMaximumSize_AtLeastMinimum) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  auto* widget = frame_view->GetWidget();
  widget->SetBounds(gfx::Rect(0, 0, 400, 300));
  widget->Show();

  gfx::Size min_size = frame_view->GetMinimumSize();
  gfx::Size max_size = frame_view->GetMaximumSize();
  EXPECT_GE(max_size.width(), min_size.width());
  EXPECT_GE(max_size.height(), min_size.height());
}

// CloseReason UMA defaults to kOther when no reason is set.
TEST_F(DocumentPipFrameViewTest, CloseReasonHistogram_DefaultsToOther) {
  base::HistogramTester histogram_tester;
  CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  // Closing without setting a reason records kOther.
  auto* host = DocumentPipHost::FromWebContents(opener());
  host->CloseContents(host->GetChildWebContents());

  histogram_tester.ExpectUniqueSample(
      "Media.DocumentPictureInPicture.CloseReason",
      DocumentPipFrameView::CloseReason::kOther, 1);
}

// CloseReason UMA records kCloseButton when set via set_close_reason().
TEST_F(DocumentPipFrameViewTest, CloseReasonHistogram_RecordsCloseButton) {
  base::HistogramTester histogram_tester;
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  frame_view->set_close_reason(DocumentPipFrameView::CloseReason::kCloseButton);
  auto* host = DocumentPipHost::FromWebContents(opener());
  host->CloseContents(host->GetChildWebContents());

  histogram_tester.ExpectUniqueSample(
      "Media.DocumentPictureInPicture.CloseReason",
      DocumentPipFrameView::CloseReason::kCloseButton, 1);
}

// CloseReason UMA records kBackToTabButton when set.
TEST_F(DocumentPipFrameViewTest, CloseReasonHistogram_RecordsBackToTabButton) {
  base::HistogramTester histogram_tester;
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  frame_view->set_close_reason(
      DocumentPipFrameView::CloseReason::kBackToTabButton);
  auto* host = DocumentPipHost::FromWebContents(opener());
  host->CloseContents(host->GetChildWebContents());

  histogram_tester.ExpectUniqueSample(
      "Media.DocumentPictureInPicture.CloseReason",
      DocumentPipFrameView::CloseReason::kBackToTabButton, 1);
}

// The origin chip (lock + origin label) is present.
TEST_F(DocumentPipFrameViewTest, OriginChipPresent) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_TRUE(GetOriginChip(frame_view));
  EXPECT_TRUE(GetSecurityIcon(frame_view));
  EXPECT_TRUE(GetOriginLabel(frame_view));
}

// The origin label shows the opener origin in security-display form (scheme
// omitted, path omitted, subdomain preserved).
TEST_F(DocumentPipFrameViewTest, OriginLabelShowsSecurityDisplayOrigin) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("https://www.example.com/some/path?query=1"));

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_EQ(u"www.example.com", GetOriginLabel(frame_view)->GetText());
}

// The security icon is populated from the opener's security state when the
// frame view is constructed.
TEST_F(DocumentPipFrameViewTest, SecurityIconHasImage) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("https://www.example.com/"));

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_FALSE(GetSecurityIcon(frame_view)->GetImageModel().IsEmpty());
}

// NonClientHitTest returns HTCLIENT for the origin chip bounds.
TEST_F(DocumentPipFrameViewTest, HitTestOriginChip_ReturnsClient) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("https://example.com/"));

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  auto* widget = frame_view->GetWidget();
  widget->SetBounds(gfx::Rect(0, 0, 400, 300));
  widget->LayoutRootViewIfNecessary();

  views::ImageView* security_icon = GetSecurityIcon(frame_view);
  ASSERT_TRUE(security_icon);
  ASSERT_FALSE(security_icon->bounds().IsEmpty());

  gfx::Point center = security_icon->bounds().CenterPoint();
  views::View::ConvertPointToTarget(security_icon->parent(), frame_view,
                                    &center);

  EXPECT_EQ(HTCLIENT, frame_view->NonClientHitTest(center));
}

// The frame renders the active state by default.
TEST_F(DocumentPipFrameViewTest, RenderActiveByDefault) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_TRUE(GetRenderActive(frame_view));
}

// Widget activation changes toggle the rendered active state.
TEST_F(DocumentPipFrameViewTest, WidgetActivationTogglesRenderState) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  frame_view->OnWidgetActivationChanged(frame_view->GetWidget(),
                                        /*active=*/false);
  EXPECT_FALSE(GetRenderActive(frame_view));

  frame_view->OnWidgetActivationChanged(frame_view->GetWidget(),
                                        /*active=*/true);
  EXPECT_TRUE(GetRenderActive(frame_view));
}

// The frame stays rendered active when the widget deactivates while the mouse
// is still inside the PiP window.
TEST_F(DocumentPipFrameViewTest, MouseInsideKeepsRenderActive) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  OnMouseEnteredOrExitedWindow(frame_view, /*entered=*/true);
  frame_view->OnWidgetActivationChanged(frame_view->GetWidget(),
                                        /*active=*/false);
  EXPECT_TRUE(GetRenderActive(frame_view));

  // OnMouseEnteredOrExitedWindow reads Widget::IsActive() directly, so the
  // following expectation only holds if the widget really is inactive at the
  // OS level (not just that the observer was notified above).
  ASSERT_FALSE(frame_view->GetWidget()->IsActive());
  OnMouseEnteredOrExitedWindow(frame_view, /*entered=*/false);
  EXPECT_FALSE(GetRenderActive(frame_view));
}

// Clicking the origin chip opens a Page Info bubble anchored to the PiP window.
TEST_F(DocumentPipFrameViewTest, ShowPageInfoOpensBubble) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("https://www.example.com/"));

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);
  auto* widget = frame_view->GetWidget();
  widget->SetBounds(gfx::Rect(0, 0, 400, 300));
  widget->Show();

  ASSERT_FALSE(PageInfoBubbleViewBase::GetPageInfoBubbleForTesting());
  EXPECT_TRUE(ShowPageInfo(frame_view));

  views::BubbleDialogDelegateView* bubble =
      PageInfoBubbleViewBase::GetPageInfoBubbleForTesting();
  ASSERT_TRUE(bubble);
  EXPECT_EQ(GetOriginChip(frame_view), bubble->GetAnchorView());

  bubble->GetWidget()->CloseNow();
}
