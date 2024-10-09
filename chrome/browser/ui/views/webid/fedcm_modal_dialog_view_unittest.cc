// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_modal_dialog_view.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/test/test_web_contents_factory.h"

class FedCmModalDialogView;

namespace {

class FedCmModalDialogViewTest : public ChromeViewsTestBase {
 public:
  FedCmModalDialogViewTest() {
    web_contents_ = web_contents_factory_.CreateWebContents(&testing_profile_);
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

 protected:
  content::WebContents* web_contents() { return web_contents_; }

  std::unique_ptr<base::HistogramTester> histogram_tester_;

 private:
  TestingProfile testing_profile_;
  content::TestWebContentsFactory web_contents_factory_;
  raw_ptr<content::WebContents>
      web_contents_;  // Owned by `web_contents_factory_`.
};

class TestDelegate : public content::WebContentsDelegate {
 public:
  explicit TestDelegate(content::WebContents* contents) {
    contents->SetDelegate(this);
  }
  ~TestDelegate() override = default;

  // content::WebContentsDelegate:
  content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params,
      base::OnceCallback<void(content::NavigationHandle&)>
          navigation_handle_callback) override {
    if (should_return_null_popup_window_) {
      return nullptr;
    }

    opened_++;
    return source;
  }

  void SetContentsBounds(content::WebContents* source,
                         const gfx::Rect& bounds) override {
    bounds_ = bounds;
  }

  void SetShouldReturnNullPopupWindow(bool should_return_null_popup_window) {
    should_return_null_popup_window_ = should_return_null_popup_window;
  }

  int opened() const { return opened_; }
  gfx::Rect bounds() const { return bounds_; }

 private:
  int opened_ = 0;
  bool should_return_null_popup_window_{false};
  gfx::Rect bounds_;
};

}  // namespace

TEST_F(FedCmModalDialogViewTest, ShowPopupWindow) {
  // Override the delegate to test that OpenURLFromTab gets called.
  TestDelegate delegate(web_contents());

  std::unique_ptr<FedCmModalDialogView> popup_window_view =
      std::make_unique<FedCmModalDialogView>(web_contents(),
                                             /*observer=*/nullptr);
  histogram_tester_->ExpectTotalCount(
      "Blink.FedCm.IdpSigninStatus.ShowPopupWindowResult", 0);
  content::WebContents* web_contents =
      popup_window_view->ShowPopupWindow(GURL(u"https://example.com"));

  EXPECT_EQ(1, delegate.opened());
  ASSERT_TRUE(web_contents);
  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.IdpSigninStatus.ShowPopupWindowResult",
      static_cast<int>(FedCmModalDialogView::ShowPopupWindowResult::kSuccess),
      1);
}

TEST_F(FedCmModalDialogViewTest, ShowPopupWindowFailedByInvalidUrl) {
  // Override the delegate to test that OpenURLFromTab gets called.
  TestDelegate delegate(web_contents());

  std::unique_ptr<FedCmModalDialogView> popup_window_view =
      std::make_unique<FedCmModalDialogView>(web_contents(),
                                             /*observer=*/nullptr);
  histogram_tester_->ExpectTotalCount(
      "Blink.FedCm.IdpSigninStatus.ShowPopupWindowResult", 0);
  content::WebContents* web_contents =
      popup_window_view->ShowPopupWindow(GURL(u"invalid"));

  EXPECT_EQ(0, delegate.opened());
  ASSERT_FALSE(web_contents);
  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.IdpSigninStatus.ShowPopupWindowResult",
      static_cast<int>(
          FedCmModalDialogView::ShowPopupWindowResult::kFailedByInvalidUrl),
      1);
}

TEST_F(FedCmModalDialogViewTest, ShowPopupWindowFailedForOtherReasons) {
  // Override the delegate to test that OpenURLFromTab gets called.
  TestDelegate delegate(web_contents());

  // Set OpenURLFromTab to return nullptr to emulate showing pop-up window
  // failing for other reasons.
  delegate.SetShouldReturnNullPopupWindow(
      /*should_return_null_popup_window=*/true);

  std::unique_ptr<FedCmModalDialogView> popup_window_view =
      std::make_unique<FedCmModalDialogView>(web_contents(),
                                             /*observer=*/nullptr);
  histogram_tester_->ExpectTotalCount(
      "Blink.FedCm.IdpSigninStatus.ShowPopupWindowResult", 0);
  content::WebContents* web_contents =
      popup_window_view->ShowPopupWindow(GURL(u"https://example.com"));

  EXPECT_EQ(0, delegate.opened());
  ASSERT_FALSE(web_contents);
  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.IdpSigninStatus.ShowPopupWindowResult",
      static_cast<int>(
          FedCmModalDialogView::ShowPopupWindowResult::kFailedForOtherReasons),
      1);
}

TEST_F(FedCmModalDialogViewTest, IdpInitiatedCloseMetric) {
  // Override the delegate to test that OpenURLFromTab gets called.
  TestDelegate delegate(web_contents());

  std::unique_ptr<FedCmModalDialogView> popup_window =
      std::make_unique<FedCmModalDialogView>(web_contents(),
                                             /*observer=*/nullptr);
  content::WebContents* web_contents =
      popup_window->ShowPopupWindow(GURL(u"https://example.com"));

  EXPECT_EQ(1, delegate.opened());
  ASSERT_TRUE(web_contents);

  histogram_tester_->ExpectTotalCount(
      "Blink.FedCm.IdpSigninStatus.ClosePopupWindowReason", 0);

  // Emulate IDP closing the pop-up window.
  popup_window->ClosePopupWindow();

  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.IdpSigninStatus.ClosePopupWindowReason",
      static_cast<int>(
          FedCmModalDialogView::ClosePopupWindowReason::kIdpInitiatedClose),
      1);
}

TEST_F(FedCmModalDialogViewTest, PopupWindowDestroyedMetric) {
  // Override the delegate to test that OpenURLFromTab gets called.
  TestDelegate delegate(web_contents());

  std::unique_ptr<FedCmModalDialogView> popup_window =
      std::make_unique<FedCmModalDialogView>(web_contents(),
                                             /*observer=*/nullptr);
  content::WebContents* web_contents =
      popup_window->ShowPopupWindow(GURL(u"https://example.com"));

  EXPECT_EQ(1, delegate.opened());
  ASSERT_TRUE(web_contents);

  histogram_tester_->ExpectTotalCount(
      "Blink.FedCm.IdpSigninStatus.ClosePopupWindowReason", 0);

  // Emulate user closing the pop-up window.
  popup_window->WebContentsDestroyed();

  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.IdpSigninStatus.ClosePopupWindowReason",
      static_cast<int>(
          FedCmModalDialogView::ClosePopupWindowReason::kPopupWindowDestroyed),
      1);
}

TEST_F(FedCmModalDialogViewTest, ShowPopupWindowWithCustomYPosition) {
  // Override the delegate to test that OpenURLFromTab gets called.
  TestDelegate delegate(web_contents());

  std::unique_ptr<FedCmModalDialogView> popup_window_view =
      std::make_unique<FedCmModalDialogView>(web_contents(),
                                             /*observer=*/nullptr);

  int custom_y_position = 1;
  popup_window_view->SetCustomYPosition(custom_y_position);

  content::WebContents* web_contents =
      popup_window_view->ShowPopupWindow(GURL(u"https://example.com"));

  EXPECT_EQ(1, delegate.opened());
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(custom_y_position, delegate.bounds().y());
}

// Tests that Blink.FedCm.Button.LoadingStatePopupInteraction is recorded
// correctly.
TEST_F(FedCmModalDialogViewTest, LoadingStatePopupInteractionMetric) {
  // Override the delegate to test that OpenURLFromTab gets called.
  TestDelegate delegate(web_contents());
  std::unique_ptr<FedCmModalDialogView> popup_window;

  auto OpenLoadingStatePopupWindow([&]() {
    popup_window = std::make_unique<FedCmModalDialogView>(web_contents(),
                                                          /*observer=*/nullptr);
    popup_window->SetActiveModeSheetType(AccountSelectionView::LOADING);
    popup_window->ShowPopupWindow(GURL(u"https://example.com"));
  });

  auto CheckForSampleAndReset(
      [&](FedCmModalDialogView::PopupInteraction result) {
        histogram_tester_->ExpectUniqueSample(
            "Blink.FedCm.Button.LoadingStatePopupInteraction",
            static_cast<int>(result), 1);
        histogram_tester_ = std::make_unique<base::HistogramTester>();
      });

  auto UserClosesPopupWindow([&]() {
    popup_window->OnWebContentsLostFocus(/*render_widget_host=*/nullptr);
    popup_window->WebContentsDestroyed();
  });

  // Emulate IDP closing the pop-up window, without user losing focus.
  OpenLoadingStatePopupWindow();
  popup_window->ClosePopupWindow();
  CheckForSampleAndReset(FedCmModalDialogView::PopupInteraction::
                             kNeverLosesFocusAndIdpInitiatedClose);

  // Emulate IDP closing the pop-up window, with user losing focus.
  OpenLoadingStatePopupWindow();
  popup_window->OnWebContentsLostFocus(/*render_widget_host=*/nullptr);
  popup_window->ClosePopupWindow();
  CheckForSampleAndReset(
      FedCmModalDialogView::PopupInteraction::kLosesFocusAndIdpInitiatedClose);

  // Emulate user closing the pop-up window, without user losing focus.
  OpenLoadingStatePopupWindow();
  UserClosesPopupWindow();
  CheckForSampleAndReset(FedCmModalDialogView::PopupInteraction::
                             kNeverLosesFocusAndPopupWindowDestroyed);

  // Emulate user closing the pop-up window, with user losing focus.
  OpenLoadingStatePopupWindow();
  popup_window->OnWebContentsLostFocus(/*render_widget_host=*/nullptr);
  UserClosesPopupWindow();
  CheckForSampleAndReset(FedCmModalDialogView::PopupInteraction::
                             kLosesFocusAndPopupWindowDestroyed);
}

// Tests that Blink.FedCm.Button.UseOtherAccountPopupInteraction is recorded
// correctly.
TEST_F(FedCmModalDialogViewTest, UseOtherAccountPopupInteractionMetric) {
  // Override the delegate to test that OpenURLFromTab gets called.
  TestDelegate delegate(web_contents());
  std::unique_ptr<FedCmModalDialogView> popup_window;

  auto OpenUseOtherAccountPopupWindow([&]() {
    popup_window = std::make_unique<FedCmModalDialogView>(web_contents(),
                                                          /*observer=*/nullptr);
    popup_window->SetActiveModeSheetType(
        AccountSelectionView::ACCOUNT_SELECTION);
    popup_window->ShowPopupWindow(GURL(u"https://example.com"));
  });

  auto CheckForSampleAndReset(
      [&](FedCmModalDialogView::PopupInteraction result) {
        histogram_tester_->ExpectUniqueSample(
            "Blink.FedCm.Button.UseOtherAccountPopupInteraction",
            static_cast<int>(result), 1);
        histogram_tester_ = std::make_unique<base::HistogramTester>();
      });

  auto UserClosesPopupWindow([&]() {
    popup_window->OnWebContentsLostFocus(/*render_widget_host=*/nullptr);
    popup_window->WebContentsDestroyed();
  });

  // Emulate IDP closing the pop-up window, without user losing focus.
  OpenUseOtherAccountPopupWindow();
  popup_window->ClosePopupWindow();
  CheckForSampleAndReset(FedCmModalDialogView::PopupInteraction::
                             kNeverLosesFocusAndIdpInitiatedClose);

  // Emulate IDP closing the pop-up window, with user losing focus.
  OpenUseOtherAccountPopupWindow();
  popup_window->OnWebContentsLostFocus(/*render_widget_host=*/nullptr);
  popup_window->ClosePopupWindow();
  CheckForSampleAndReset(
      FedCmModalDialogView::PopupInteraction::kLosesFocusAndIdpInitiatedClose);

  // Emulate user closing the pop-up window, without user losing focus.
  OpenUseOtherAccountPopupWindow();
  UserClosesPopupWindow();
  CheckForSampleAndReset(FedCmModalDialogView::PopupInteraction::
                             kNeverLosesFocusAndPopupWindowDestroyed);

  // Emulate user closing the pop-up window, with user losing focus.
  OpenUseOtherAccountPopupWindow();
  popup_window->OnWebContentsLostFocus(/*render_widget_host=*/nullptr);
  UserClosesPopupWindow();
  CheckForSampleAndReset(FedCmModalDialogView::PopupInteraction::
                             kLosesFocusAndPopupWindowDestroyed);
}
