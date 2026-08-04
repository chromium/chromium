// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/picture_in_picture/document_pip_frame_view.h"

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback_helpers.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/content_settings/page_specific_content_settings_delegate.h"
#include "chrome/browser/picture_in_picture/auto_pip_setting_overlay_view.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ssl/chrome_security_state_tab_helper.h"
#include "chrome/browser/ssl/chrome_security_state_util.h"
#include "chrome/browser/ui/color/chrome_color_id.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/views/location_bar/content_setting_image_view.h"
#include "chrome/browser/ui/views/location_bar/icon_label_bubble_view.h"
#include "chrome/browser/ui/views/page_info/page_info_bubble_view_base.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_host.h"
#include "chrome/browser/ui/views/picture_in_picture/document_pip_widget_delegate.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/permissions/permission_recovery_success_rate_tracker.h"
#include "components/security_state/content/security_state_tab_helper.h"
#include "components/security_state/core/security_state.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/mojom/picture_in_picture_window_options/picture_in_picture_window_options.mojom.h"
#include "ui/base/hit_test.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/animation_test_api.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_delegate.h"
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

gfx::Rect MakeDefaultInitialBounds() {
  return gfx::Rect(20, 30, 400, 300);
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
    // Seed the PictureInPictureWindowManager's opener display, which production
    // sets via CalculateInitialPictureInPictureWindowBounds() before the host
    // is created (see
    // PictureInPictureWindowManager::EnterDocumentPictureInPicture).
    // DocumentPipFrameView::UpdateWindowBoundsForRequestedInnerSize(), run
    // during CreateAndShowPipWindow() below, CHECK()s that it is set.
    PictureInPictureWindowManager::GetInstance()
        ->CalculateInitialPictureInPictureWindowBounds(
            MakePipOptions(disallow_return_to_opener),
            display::Screen::Get()->GetPrimaryDisplay());
    host->CreateAndShowPipWindow(std::move(child),
                                 MakePipOptions(disallow_return_to_opener),
                                 MakeDefaultInitialBounds());

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

  IconLabelBubbleView* GetOriginChip(DocumentPipFrameView* frame_view) {
    return frame_view->origin_chip_;
  }

  views::Label* GetWindowTitle(DocumentPipFrameView* frame_view) {
    return frame_view->window_title_;
  }

  int GetTopAreaHeight(DocumentPipFrameView* frame_view) {
    return frame_view->GetTopAreaHeight();
  }

  bool GetRenderActive(DocumentPipFrameView* frame_view) {
    return frame_view->animation_controller_->is_top_bar_active();
  }

  views::FlexLayoutView* GetButtonContainer(DocumentPipFrameView* frame_view) {
    return frame_view->button_container_view_;
  }

  const std::vector<raw_ptr<ContentSettingImageView, VectorExperimental>>&
  GetContentSettingViews(DocumentPipFrameView* frame_view) {
    return frame_view->content_setting_views_;
  }

  // Whether the "fade all buttons together" animation (used when no content-
  // setting icon is visible) is currently running.
  bool AllButtonsAnimationRunning(DocumentPipFrameView* frame_view) {
    return frame_view->animation_controller_->show_all_buttons_animation_
               .is_animating() ||
           frame_view->animation_controller_->hide_all_buttons_animation_
               .is_animating();
  }

  // Whether the per-button fade animations (used when a content-setting icon is
  // visible, so the camera can slide into the freed space) are running.
  bool PerButtonAnimationsRunning(DocumentPipFrameView* frame_view) {
    return frame_view->animation_controller_->show_back_to_tab_button_animation_
               .is_animating() ||
           frame_view->animation_controller_->hide_back_to_tab_button_animation_
               .is_animating() ||
           frame_view->animation_controller_->show_close_button_animation_
               .is_animating() ||
           frame_view->animation_controller_->hide_close_button_animation_
               .is_animating();
  }

  bool ShowPageInfo(DocumentPipFrameView* frame_view) {
    return frame_view->ShowPageInfo();
  }

  void OnMouseEnteredOrExitedWindow(DocumentPipFrameView* frame_view,
                                    bool entered) {
    frame_view->OnMouseEnteredOrExitedWindow(entered);
  }

  // Drives every running top-bar animation on `frame_view` straight to its end
  // via gfx::AnimationTestApi, so the button opacities and foreground color
  // settle deterministically without a live compositor. Reaches the private
  // animation members on the frame view's PipTopBarAnimationController through
  // the test's friendship with both classes.
  void StepTopBarAnimationsToEnd(DocumentPipFrameView* frame_view) {
    std::vector<gfx::Animation*> animations = {
        &frame_view->animation_controller_->top_bar_color_animation_,
        &frame_view->animation_controller_
             ->move_camera_button_to_left_animation_,
        &frame_view->animation_controller_
             ->move_camera_button_to_right_animation_,
        &frame_view->animation_controller_->show_all_buttons_animation_,
        &frame_view->animation_controller_->hide_all_buttons_animation_,
    };
    if (frame_view->back_to_tab_button_) {
      animations.push_back(&frame_view->animation_controller_
                                ->show_back_to_tab_button_animation_);
      animations.push_back(&frame_view->animation_controller_
                                ->hide_back_to_tab_button_animation_);
      animations.push_back(
          &frame_view->animation_controller_->show_close_button_animation_);
      animations.push_back(
          &frame_view->animation_controller_->hide_close_button_animation_);
    }
    const base::TimeTicks now = base::TimeTicks::Now();
    for (gfx::Animation* animation : animations) {
      if (!animation->is_animating()) {
        continue;
      }
      gfx::AnimationTestApi api(animation);
      api.SetStartTime(now);
      // Step well past the total 250ms animation duration to guarantee every
      // part of the multi-part button animations completes.
      api.Step(now + base::Milliseconds(500));
    }
  }

  // Overrides the computed malicious-content status, so the chip is
  // computed at DANGEROUS level.
  void SeedFakeSecurityState(
      security_state::MaliciousContentStatus malicious_content_status) {
    scoped_malicious_content_status_.emplace(malicious_content_status);
  }

  AutoPipSettingOverlayView* GetAutoPipOverlay(
      DocumentPipFrameView* frame_view) {
    return frame_view->auto_pip_setting_overlay_;
  }

  bool IsOverlayViewVisible(DocumentPipFrameView* frame_view) {
    return frame_view->IsOverlayViewVisible();
  }

  // Injects a real auto-PiP overlay onto the frame view the same way the
  // production ctor would (AddChildView + raw_ptr). The unit-test harness
  // creates the host directly (bypassing
  // EnterStandaloneDocumentPictureInPicture), so
  // PictureInPictureWindowManager::GetOverlayView() returns null; this lets the
  // overlay-dependent behaviors (layout, visibility coupling) be exercised
  // without the full auto-PiP embargo path, which is covered by browser tests.
  AutoPipSettingOverlayView* InjectAutoPipOverlay(
      DocumentPipFrameView* frame_view) {
    auto overlay = std::make_unique<AutoPipSettingOverlayView>(
        base::DoNothing(), GURL("https://example.com/"),
        frame_view->top_bar_container_view_, views::BubbleBorder::TOP_CENTER);
    frame_view->auto_pip_setting_overlay_ =
        frame_view->AddChildView(std::move(overlay));
    return frame_view->auto_pip_setting_overlay_;
  }

 protected:
  content::RenderViewHostTestEnabler test_render_host_factories_;
  TestingProfile profile_;
  std::unique_ptr<views::Widget> opener_host_widget_;
  std::optional<chrome_security_state::ScopedMaliciousContentStatusForTesting>
      scoped_malicious_content_status_;
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

// Window title exists.
TEST_F(DocumentPipFrameViewTest, WindowTitleExists) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_TRUE(GetWindowTitle(frame_view));
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
  IconLabelBubbleView* chip = GetOriginChip(frame_view);
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

TEST_F(DocumentPipFrameViewTest, ChildDialogObserverResizesAndRestores) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);
  views::Widget* pip_widget = frame_view->GetWidget();
  ASSERT_TRUE(pip_widget);
  pip_widget->SetBounds(gfx::Rect(20, 30, 120, 80));
  const gfx::Rect original_bounds = pip_widget->GetWindowBoundsInScreen();

  auto child_contents = std::make_unique<views::View>();
  child_contents->SetPreferredSize(
      gfx::Size(original_bounds.width() + 50, original_bounds.height() + 50));
  auto child_delegate = std::make_unique<views::DialogDelegate>();
  child_delegate->SetModalType(ui::mojom::ModalType::kWindow);
  child_delegate->SetOwnershipOfNewWidget(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  child_delegate->SetContentsView(std::move(child_contents));
  std::unique_ptr<views::Widget> child_dialog(
      views::DialogDelegate::CreateDialogWidget(child_delegate.get(),
                                                gfx::NativeWindow(),
                                                pip_widget->GetNativeView()));

  child_dialog->Show();
  auto* host = DocumentPipHost::FromWebContents(opener());
  EXPECT_FALSE(host->IsChildResizePendingForTesting());

  EXPECT_TRUE(
      child_dialog->GetContentsView()->GetCanProcessEventsWithinSubtree());
  const gfx::Rect grown_bounds = pip_widget->GetWindowBoundsInScreen();
  EXPECT_GE(grown_bounds.width(), original_bounds.width());
  EXPECT_GE(grown_bounds.height(), original_bounds.height());
  EXPECT_NE(grown_bounds.size(), original_bounds.size());

  child_dialog->CloseNow();
  EXPECT_EQ(original_bounds.size(),
            pip_widget->GetWindowBoundsInScreen().size());
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

// The origin chip and window title are present.
TEST_F(DocumentPipFrameViewTest, OriginChipPresent) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_TRUE(GetOriginChip(frame_view));
  EXPECT_TRUE(GetWindowTitle(frame_view));
}

// The window title shows the opener URL formatted the omnibox way: scheme and
// trivial subdomain omitted, path preserved.
TEST_F(DocumentPipFrameViewTest, WindowTitleShowsFormattedUrl) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("https://www.example.com/some/path?query=1"));

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_EQ(std::u16string(u"example.com/some/path?query=1"),
            GetWindowTitle(frame_view)->GetText());
}

// HTTPS URLs head-elide so a long origin cannot be pushed out of view.
TEST_F(DocumentPipFrameViewTest, WindowTitleElidesHeadForHttps) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("https://www.example.com/some/path"));

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_EQ(gfx::ELIDE_HEAD, GetWindowTitle(frame_view)->GetElideBehavior());
}

// File URLs tail-elide (the file name/query is the spoofable part) and the chip
// shows the "File" security text, matching the browser-backed frame.
TEST_F(DocumentPipFrameViewTest, FileUrlTailElidesAndChipSaysFile) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("file:///home/user/movie.html"));

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_EQ(gfx::ELIDE_TAIL, GetWindowTitle(frame_view)->GetElideBehavior());
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_OMNIBOX_FILE),
            GetOriginChip(frame_view)->GetText());
}

// A secure HTTPS page shows no security chip text (the lock icon alone conveys
// the secure state), mirroring the omnibox.
TEST_F(DocumentPipFrameViewTest, HttpsSecureUrlHasEmptyChipText) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("https://www.example.com/"));

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_TRUE(GetOriginChip(frame_view)->GetText().empty());
}

// A plain HTTP page reports the WARNING security level, so the chip shows the
// "Not secure" verbose text.
TEST_F(DocumentPipFrameViewTest, HttpWarningUrlChipSaysNotSecure) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("http://www.example.com/"));

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_NOT_SECURE_VERBOSE_STATE),
            GetOriginChip(frame_view)->GetText());
}

// A DANGEROUS page that fails the malware check shows the "Dangerous" verbose
// text on the chip.
TEST_F(DocumentPipFrameViewTest, DangerousMalwareUrlChipSaysDangerous) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("https://malware.test/"));
  SeedFakeSecurityState(security_state::MALICIOUS_CONTENT_STATUS_MALWARE);

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_DANGEROUS_VERBOSE_STATE),
            GetOriginChip(frame_view)->GetText());
}

// A DANGEROUS page on the billing interstitial list carries its own UI, so the
// chip stays empty.
TEST_F(DocumentPipFrameViewTest, DangerousBillingUrlHasEmptyChipText) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("https://billing.test/"));
  SeedFakeSecurityState(security_state::MALICIOUS_CONTENT_STATUS_BILLING);

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_TRUE(GetOriginChip(frame_view)->GetText().empty());
}

// A DANGEROUS page blocked by enterprise policy carries its own UI, so the chip
// stays empty.
TEST_F(DocumentPipFrameViewTest, DangerousManagedPolicyUrlHasEmptyChipText) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("https://policy.test/"));
  SeedFakeSecurityState(
      security_state::MALICIOUS_CONTENT_STATUS_MANAGED_POLICY_BLOCK);

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_TRUE(GetOriginChip(frame_view)->GetText().empty());
}

// The recompute after Widget::Init() grows the outer window by the non-client
// area (top bar + frame insets) so the requested inner web-contents size is
// preserved, rather than leaving the window one top-bar height too short.
TEST_F(DocumentPipFrameViewTest, OuterBoundsAccountForTopBar) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  views::Widget* widget = frame_view->GetWidget();
  ASSERT_TRUE(widget);

  // MakePipOptions() requests a 400x300 inner (web-contents) area.
  const gfx::Size outer = widget->GetWindowBoundsInScreen().size();
  EXPECT_GE(outer.width(), 400);
  EXPECT_GE(outer.height(), 300 + GetTopAreaHeight(frame_view));
}

// The security icon is populated from the opener's security state when the
// frame view is constructed.
TEST_F(DocumentPipFrameViewTest, OriginChipHasSecurityImage) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("https://www.example.com/"));

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  auto* chip = GetOriginChip(frame_view);
  ASSERT_TRUE(chip);
  EXPECT_FALSE(chip->GetImageContainerView()->GetPreferredSize().IsEmpty());
}

// NonClientHitTest returns HTCLIENT for the origin chip bounds (opens Page
// Info on click).
TEST_F(DocumentPipFrameViewTest, HitTestOriginChip_ReturnsClient) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("https://example.com/"));

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  auto* widget = frame_view->GetWidget();
  widget->SetBounds(gfx::Rect(0, 0, 400, 300));
  widget->LayoutRootViewIfNecessary();

  IconLabelBubbleView* chip = GetOriginChip(frame_view);
  ASSERT_TRUE(chip);
  ASSERT_FALSE(chip->bounds().IsEmpty());

  gfx::Point center = chip->bounds().CenterPoint();
  views::View::ConvertPointToTarget(chip->parent(), frame_view, &center);

  EXPECT_EQ(HTCLIENT, frame_view->NonClientHitTest(center));
}

// NonClientHitTest returns HTCAPTION for the window title bounds (draggable,
// matching browser-backed PiP behavior where the window title is draggable).
TEST_F(DocumentPipFrameViewTest, HitTestWindowTitle_ReturnsCaption) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("https://example.com/"));

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  auto* widget = frame_view->GetWidget();
  widget->SetBounds(gfx::Rect(0, 0, 400, 300));
  widget->LayoutRootViewIfNecessary();

  views::Label* window_title = GetWindowTitle(frame_view);
  ASSERT_TRUE(window_title);
  ASSERT_FALSE(window_title->bounds().IsEmpty());

  gfx::Point center = window_title->bounds().CenterPoint();
  views::View::ConvertPointToTarget(window_title->parent(), frame_view,
                                    &center);

  EXPECT_EQ(HTCAPTION, frame_view->NonClientHitTest(center));
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
  // OS level (not just that the observer was notified above). The widget is
  // shown (and thus activated) when it is created; hiding it deactivates it
  // (NativeWidgetMac::Deactivate() is a no-op, so Hide() is used here).
  frame_view->GetWidget()->Hide();
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

// The camera/microphone content-setting icon is created and parented under the
// button container, to the left of the window-control buttons.
TEST_F(DocumentPipFrameViewTest, ContentSettingViewsPresent) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  const auto& content_setting_views = GetContentSettingViews(frame_view);
  ASSERT_EQ(1u, content_setting_views.size());
  EXPECT_EQ(GetButtonContainer(frame_view), content_setting_views[0]->parent());
}

// UpdateContentSettingsIcons() refreshes without crashing and leaves the media
// icon hidden when there is no active capture.
//
// TODO(crbug.com/515252142): The icon currently stays hidden because standalone
// PiP cannot yet grant media permissions (DocumentPipHost::
// RequestMediaAccessPermission returns PERMISSION_DENIED, so there is never an
// active capture). This GetVisible() expectation is temporary; once the
// standalone permission dialog is wired up, the icon should become
// visible while capture is active and this test should be updated accordingly.
TEST_F(DocumentPipFrameViewTest, UpdateContentSettingsIconsHidesIdleIcon) {
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("https://www.example.com/"));
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  frame_view->UpdateContentSettingsIcons();

  const auto& content_setting_views = GetContentSettingViews(frame_view);
  ASSERT_EQ(1u, content_setting_views.size());
  EXPECT_FALSE(content_setting_views[0]->GetVisible());
}

// The content-setting icon's delegate reads from the opener WebContents and
// surfaces no Browser-backed bubble-model delegate (standalone PiP).
TEST_F(DocumentPipFrameViewTest, ContentSettingDelegateUsesOpener) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_EQ(opener(), frame_view->GetContentSettingWebContents());
  EXPECT_EQ(nullptr, frame_view->GetContentSettingBubbleModelDelegate());
  EXPECT_FALSE(frame_view->ShouldHideContentSettingImage());
}

// Deactivating the window fades the window-control buttons out (opacity 0)
// while keeping them laid out (their fixed-size wrappers reserve the space),
// and reactivating fades them back in. Mirrors
// PictureInPictureBrowserFrameView, which animates opacity rather than toggling
// visibility.
TEST_F(DocumentPipFrameViewTest, InactiveFadesOutWindowControlButtons) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  views::ImageButton* close_btn = GetCloseButton(frame_view);
  views::ImageButton* back_btn = GetBackToTabButton(frame_view);
  ASSERT_TRUE(close_btn);
  ASSERT_TRUE(back_btn);
  // The buttons paint to layers so their opacity can be animated.
  ASSERT_TRUE(close_btn->layer());
  ASSERT_TRUE(back_btn->layer());

  // Deactivating fades the buttons to opacity 0; they stay laid out/visible.
  frame_view->OnWidgetActivationChanged(frame_view->GetWidget(),
                                        /*active=*/false);
  StepTopBarAnimationsToEnd(frame_view);
  EXPECT_EQ(0.0f, close_btn->layer()->opacity());
  EXPECT_EQ(0.0f, back_btn->layer()->opacity());
  EXPECT_TRUE(close_btn->GetVisible());
  EXPECT_TRUE(back_btn->GetVisible());

  // Reactivating fades them back in to opacity 1.
  frame_view->OnWidgetActivationChanged(frame_view->GetWidget(),
                                        /*active=*/true);
  StepTopBarAnimationsToEnd(frame_view);
  EXPECT_EQ(1.0f, close_btn->layer()->opacity());
  EXPECT_EQ(1.0f, back_btn->layer()->opacity());
}

// The top-bar foreground color fades between the active and inactive colors as
// the window activation state changes. Once an animation settles, the
// steady-state color surfaced to the origin chip matches the active/inactive
// state.
TEST_F(DocumentPipFrameViewTest, TopBarForegroundColorTracksActiveState) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);
  auto* widget = frame_view->GetWidget();
  const auto* color_provider = widget->GetColorProvider();
  const SkColor active = color_provider->GetColor(kColorPipWindowForeground);
  const SkColor inactive =
      color_provider->GetColor(kColorPipWindowForegroundInactive);
  ASSERT_NE(active, inactive);

  // Active by default.
  EXPECT_EQ(active, frame_view->GetIconLabelBubbleSurroundingForegroundColor());

  // Deactivating settles on the inactive foreground.
  frame_view->OnWidgetActivationChanged(widget, /*active=*/false);
  StepTopBarAnimationsToEnd(frame_view);
  EXPECT_EQ(inactive,
            frame_view->GetIconLabelBubbleSurroundingForegroundColor());

  // Reactivating settles back on the active foreground.
  frame_view->OnWidgetActivationChanged(widget, /*active=*/true);
  StepTopBarAnimationsToEnd(frame_view);
  EXPECT_EQ(active, frame_view->GetIconLabelBubbleSurroundingForegroundColor());
}

// With no active media capture the content-setting icon is hidden, so the
// button container carries the symmetric "no camera" horizontal margin that
// keeps the window controls spaced consistently. Mirrors
// PictureInPictureBrowserFrameView::UpdateContentSettingsIcons.
TEST_F(DocumentPipFrameViewTest, ContentSettingsMarginWhenIconHidden) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  const auto& content_setting_views = GetContentSettingViews(frame_view);
  ASSERT_EQ(1u, content_setting_views.size());
  ASSERT_FALSE(content_setting_views[0]->GetVisible());

  const gfx::Insets* margins =
      GetButtonContainer(frame_view)->GetProperty(views::kMarginsKey);
  ASSERT_TRUE(margins);
  EXPECT_EQ(gfx::Insets::VH(
                0, GetLayoutConstant(LayoutConstant::kTabAfterTitlePadding)),
            *margins);
}

// When a content-setting (camera) icon is visible, the button container carries
// the asymmetric "with camera" margin, and activation changes drive the
// per-button fade animations (plus the camera slide) rather than the
// fade-all-buttons path used when no content-setting icon is shown. Mirrors
// PictureInPictureBrowserFrameView.
TEST_F(DocumentPipFrameViewTest,
       ContentSettingVisibleUsesPerButtonAnimationsAndMargin) {
  // Register PageSpecificContentSettings before navigating so it attaches to
  // the committed page, then seed active camera capture on the opener. This is
  // the only way to make the media content-setting icon visible in a unit test
  // (standalone PiP cannot grant media permissions itself yet).
  content_settings::PageSpecificContentSettings::CreateForWebContents(
      opener(),
      std::make_unique<PageSpecificContentSettingsDelegate>(opener()));
  // The delegate's UpdateLocationBar() records usage through this tracker, so
  // it must exist on the opener before any media-stream permission is set.
  permissions::PermissionRecoverySuccessRateTracker::CreateForWebContents(
      opener());
  content::WebContentsTester::For(opener())->NavigateAndCommit(
      GURL("https://www.example.com/"));

  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  content_settings::PageSpecificContentSettings* pscs =
      content_settings::PageSpecificContentSettings::GetForFrame(
          opener()->GetPrimaryMainFrame());
  ASSERT_TRUE(pscs);
  // Seed camera access that is also blocked at the site level. This test only
  // needs the icon to be visible so it can exercise the per-button animation +
  // margin path; it does not care whether the camera was allowed or blocked.
  //
  // Both flags are required, and they play different roles in
  // ContentSettingMediaImageModel::UpdateAndGetVisibility():
  //   - kCameraAccessed is the gate: the model only enters its camera display
  //     branch when IsCamAccessed() is true, so without it the icon stays
  //     hidden no matter what else is set.
  //   - kCameraBlocked is what makes visibility deterministic on macOS. There
  //     the system-level camera permission is kNotDetermined in unit tests,
  //     and an accessed-but-not-blocked camera is treated as a pending system
  //     prompt and hidden (IsCameraAccessPendingOnSystemLevelPrompt(), which
  //     fires only when the camera is *not* blocked at the site level). Adding
  //     kCameraBlocked sidesteps that macOS-only gate.
  pscs->OnMediaStreamPermissionSet(
      GURL("https://www.example.com/"),
      {content_settings::PageSpecificContentSettings::kCameraAccessed,
       content_settings::PageSpecificContentSettings::kCameraBlocked});

  // Refresh the icons now that camera capture is active; the icon becomes
  // visible and the button container adopts the "with camera" margin.
  frame_view->UpdateContentSettingsIcons();

  const auto& content_setting_views = GetContentSettingViews(frame_view);
  ASSERT_EQ(1u, content_setting_views.size());
  ASSERT_TRUE(content_setting_views[0]->GetVisible());

  const gfx::Insets* margins =
      GetButtonContainer(frame_view)->GetProperty(views::kMarginsKey);
  ASSERT_TRUE(margins);
  EXPECT_EQ(
      gfx::Insets::TLBR(
          0, 0, 0, GetLayoutConstant(LayoutConstant::kTabAfterTitlePadding)),
      *margins);

  views::ImageButton* close_btn = GetCloseButton(frame_view);
  views::ImageButton* back_btn = GetBackToTabButton(frame_view);
  ASSERT_TRUE(close_btn->layer());
  ASSERT_TRUE(back_btn->layer());

  // Deactivating drives the per-button hide animations (not the fade-all path),
  // because a content-setting icon is visible.
  frame_view->OnWidgetActivationChanged(frame_view->GetWidget(),
                                        /*active=*/false);
  EXPECT_FALSE(AllButtonsAnimationRunning(frame_view));
  EXPECT_TRUE(PerButtonAnimationsRunning(frame_view));
  StepTopBarAnimationsToEnd(frame_view);
  EXPECT_EQ(0.0f, close_btn->layer()->opacity());
  EXPECT_EQ(0.0f, back_btn->layer()->opacity());
  // The camera icon slid right into the space vacated by the hidden buttons.
  EXPECT_GT(content_setting_views[0]->x(), 0);

  // Reactivating drives the per-button show animations and slides the camera
  // icon back to its resting position.
  frame_view->OnWidgetActivationChanged(frame_view->GetWidget(),
                                        /*active=*/true);
  EXPECT_FALSE(AllButtonsAnimationRunning(frame_view));
  EXPECT_TRUE(PerButtonAnimationsRunning(frame_view));
  StepTopBarAnimationsToEnd(frame_view);
  EXPECT_EQ(1.0f, close_btn->layer()->opacity());
  EXPECT_EQ(1.0f, back_btn->layer()->opacity());
  EXPECT_EQ(0, content_setting_views[0]->x());
}

// must not report HTCLIENT over their (now hidden) bounds; the area falls
// through to the draggable caption instead.
TEST_F(DocumentPipFrameViewTest, HitTestCloseButton_InactiveReturnsCaption) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  auto* widget = frame_view->GetWidget();
  widget->SetBounds(gfx::Rect(0, 0, 400, 300));
  widget->LayoutRootViewIfNecessary();

  views::ImageButton* close_btn = GetCloseButton(frame_view);
  ASSERT_TRUE(close_btn);
  ASSERT_FALSE(close_btn->bounds().IsEmpty());
  gfx::Point center = close_btn->bounds().CenterPoint();
  views::View::ConvertPointToTarget(close_btn->parent(), frame_view, &center);

  // Active: hits the close button.
  EXPECT_EQ(HTCLIENT, frame_view->NonClientHitTest(center));

  // Inactive: buttons hidden, so the same point is caption (draggable).
  frame_view->OnWidgetActivationChanged(frame_view->GetWidget(),
                                        /*active=*/false);
  EXPECT_EQ(HTCAPTION, frame_view->NonClientHitTest(center));
}

// Without an AutoPictureInPictureTabHelper on the opener (the default harness),
// no auto-PiP overlay is created, so the accessor is null and the overlay is
// not reported visible.
TEST_F(DocumentPipFrameViewTest, AutoPipOverlayAbsent_WhenNoAutoPip) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);

  EXPECT_FALSE(GetAutoPipOverlay(frame_view));
  EXPECT_FALSE(IsOverlayViewVisible(frame_view));
}

// When present, the overlay is laid out over the contents area below the top
// bar (content_area - top_bar), matching PictureInPictureBrowserFrameView.
TEST_F(DocumentPipFrameViewTest, AutoPipOverlayLaidOutBelowTopBar) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);
  AutoPipSettingOverlayView* overlay = InjectAutoPipOverlay(frame_view);
  ASSERT_TRUE(overlay);

  auto* widget = frame_view->GetWidget();
  widget->SetBounds(gfx::Rect(0, 0, 400, 300));
  widget->LayoutRootViewIfNecessary();

  // FrameBorderInsets() is empty, so the content area equals the local bounds
  // and the top bar occupies the top GetTopAreaHeight() px.
  const gfx::Rect local_bounds = frame_view->GetLocalBounds();
  const int top_height = GetTopAreaHeight(frame_view);
  const gfx::Rect expected(local_bounds.x(), local_bounds.y() + top_height,
                           local_bounds.width(),
                           local_bounds.height() - top_height);
  EXPECT_EQ(expected, overlay->bounds());
}

// IsOverlayViewVisible() reflects the overlay's own visibility.
TEST_F(DocumentPipFrameViewTest, IsOverlayViewVisible_TracksOverlayVisibility) {
  auto* frame_view =
      CreatePipAndGetFrameView(/*disallow_return_to_opener=*/false);
  AutoPipSettingOverlayView* overlay = InjectAutoPipOverlay(frame_view);
  ASSERT_TRUE(overlay);

  // A freshly added child view is visible by default.
  EXPECT_TRUE(IsOverlayViewVisible(frame_view));

  overlay->SetVisible(false);
  EXPECT_FALSE(IsOverlayViewVisible(frame_view));
}
