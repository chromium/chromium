// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_field_promo_view_impl.h"

#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_view.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/autofill/popup/popup_view_views.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/gurl.h"
#include "url/origin.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#endif

namespace autofill {
namespace {

class TestPictureInPictureWindowController
    : public content::PictureInPictureWindowController {
 public:
  void Show() override {}
  void FocusInitiator() override {}
  void Close(bool) override {}
  void CloseAndFocusInitiator() override {}
  void OnWindowDestroyed(bool) override {}
  content::WebContents* GetWebContents() override { return nullptr; }
  content::WebContents* GetChildWebContents() override { return nullptr; }
  std::optional<url::Origin> GetOrigin() override { return std::nullopt; }

  std::optional<gfx::Rect> GetWindowBoundsInScreen() override {
    return bounds_;
  }
  void SetWindowBounds(gfx::Rect bounds) { bounds_ = bounds; }

 private:
  gfx::Rect bounds_;
};

AutofillFieldPromoViewImpl* GetViewRawPtr(
    base::WeakPtr<AutofillFieldPromoView> view) {
  return static_cast<AutofillFieldPromoViewImpl*>(view.get());
}

class AutofillFieldPromoViewImplBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                             GURL(chrome::kChromeUINewTabURL)));
  }

  void TearDownOnMainThread() override {
    if (view_) {
      view_->Close();
    }
    InProcessBrowserTest::TearDownOnMainThread();
  }

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  const ui::ElementIdentifier& element_identifier() {
    return test_promo_element_identifier_;
  }

  base::WeakPtr<AutofillFieldPromoView> CreateView(
      gfx::RectF bounds = gfx::RectF(0, 0, 1, 1)) {
    // Always close previously created view in order to avoid dangling pointers.
    if (view_) {
      view_->Close();
    }
    view_ = AutofillFieldPromoView::CreateAndShow(
        web_contents(), bounds, test_promo_element_identifier_);
    return view_;
  }

#if BUILDFLAG(IS_MAC)
  // On Mac, web contents bounds cannot be easily modified. As an alternative,
  // the bounds of the containing widget are changed.
  void ChangeBrowserWindowBoundsForDesiredWebContentsBounds(
      gfx::Rect expected_web_contents_bounds) {
    views::Widget* widget =
        views::Widget::GetWidgetForNativeView(web_contents()->GetNativeView());

    gfx::Vector2d origin_offset_between_widget_and_content_area =
        widget->GetWindowBoundsInScreen().origin() -
        web_contents()->GetContainerBounds().origin();
    gfx::Size size_offset_between_widget_and_content_area =
        widget->GetWindowBoundsInScreen().size() -
        web_contents()->GetContainerBounds().size();

    gfx::Rect widget_bounds;
    widget_bounds.set_origin(expected_web_contents_bounds.origin() +
                             origin_offset_between_widget_and_content_area);
    widget_bounds.set_size(expected_web_contents_bounds.size() +
                           size_offset_between_widget_and_content_area);
    widget->SetBounds(widget_bounds);
  }
#endif  // BUILDFLAG(IS_MAC)

 private:
  const ui::ElementIdentifier test_promo_element_identifier_ =
      PopupViewViews::kAutofillStandaloneCvcSuggestionElementId;
  base::WeakPtr<AutofillFieldPromoView> view_;
};

IN_PROC_BROWSER_TEST_F(AutofillFieldPromoViewImplBrowserTest,
                       BoundsAreCorrect) {
  // Set custom web contents bounds.
#if BUILDFLAG(IS_MAC)
  ChangeBrowserWindowBoundsForDesiredWebContentsBounds(
      gfx::Rect(50, 50, 400, 400));
#else
  web_contents()->GetNativeView()->SetBoundsInScreen(
      gfx::Rect(50, 50, 400, 400),
      display::Screen::Get()->GetDisplayForNewWindows());
#endif  // BUILDFLAG(IS_MAC)

  // Element is within the boundaries of `web_contents()`.
  EXPECT_EQ(GetViewRawPtr(CreateView(gfx::RectF(100, 100, 100, 100)))->bounds(),
            gfx::Rect(100, 199, 100, 1));

  // Element partially exceeds the upper limit of `web_contents()`.
  EXPECT_EQ(GetViewRawPtr(CreateView(gfx::RectF(350, 350, 100, 100)))->bounds(),
            gfx::Rect(350, 399, 50, 1));

  // Element partially exceeds the lower limit of `web_contents()`.
  EXPECT_EQ(GetViewRawPtr(CreateView(gfx::RectF(-50, -50, 100, 100)))->bounds(),
            gfx::Rect(0, 49, 50, 1));
}

IN_PROC_BROWSER_TEST_F(AutofillFieldPromoViewImplBrowserTest,
                       LifetimeIsManagedCorrectlyOnClose) {
  base::WeakPtr<AutofillFieldPromoView> view = CreateView();
  AutofillFieldPromoViewImpl* view_ptr = GetViewRawPtr(view);

  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->contents_web_view()
                  ->Contains(view_ptr));

  view->Close();
  EXPECT_FALSE(view);
}

IN_PROC_BROWSER_TEST_F(AutofillFieldPromoViewImplBrowserTest,
                       OverlapsWithPictureInPictureWindow) {
  base::WeakPtr<AutofillFieldPromoView> view =
      CreateView(gfx::RectF(200, 200, 300, 300));
  TestPictureInPictureWindowController picture_in_picture_window_controller;

  PictureInPictureWindowManager::GetInstance()
      ->set_window_controller_for_testing(
          &picture_in_picture_window_controller);

  picture_in_picture_window_controller.SetWindowBounds(
      gfx::Rect(100, 100, 50, 50));
  EXPECT_FALSE(view->OverlapsWithPictureInPictureWindow());

  picture_in_picture_window_controller.SetWindowBounds(
      gfx::Rect(100, 100, 1000, 1000));
  EXPECT_TRUE(view->OverlapsWithPictureInPictureWindow());

  PictureInPictureWindowManager::GetInstance()
      ->set_window_controller_for_testing(nullptr);
}

IN_PROC_BROWSER_TEST_F(AutofillFieldPromoViewImplBrowserTest,
                       ElementIdForIphIsCorrect) {
  EXPECT_EQ(
      GetViewRawPtr(CreateView())->GetProperty(views::kElementIdentifierKey),
      element_identifier());
}

}  // namespace
}  // namespace autofill
