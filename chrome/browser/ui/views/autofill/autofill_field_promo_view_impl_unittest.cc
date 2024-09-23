// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_field_promo_view_impl.h"

#include <optional>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/picture_in_picture/picture_in_picture_window_manager.h"
#include "chrome/browser/ui/autofill/autofill_field_promo_view.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "content/public/browser/picture_in_picture_window_controller.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/widget.h"
#include "url/origin.h"

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

  std::optional<gfx::Rect> GetWindowBounds() override { return bounds_; }
  void SetWindowBounds(gfx::Rect bounds) { bounds_ = bounds; }

 private:
  gfx::Rect bounds_;
};

AutofillFieldPromoViewImpl* GetViewRawPtr(
    base::WeakPtr<AutofillFieldPromoView> view) {
  return static_cast<AutofillFieldPromoViewImpl*>(view.get());
}

class AutofillFieldPromoViewImplTest : public TestWithBrowserView {
 public:
  void SetUp() override {
    TestWithBrowserView::SetUp();
    // Create the first tab so that `web_contents()` exists.
    AddTab(browser(), GURL(chrome::kChromeUINewTabURL));
  }

  void TearDown() override {
    if (view_) {
      view_->Close();
    }
    TestWithBrowserView::TearDown();
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
  // This works based on the assumption that the top chrome UI bounds do not
  // change. There are only a few operations which can do that (ex: toggling the
  // bookmark bar) and none of them occur in this test fixture and they should
  // never occur in the future.
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
      kAutofillStandaloneCvcSuggestionElementId;
  base::WeakPtr<AutofillFieldPromoView> view_;
};

TEST_F(AutofillFieldPromoViewImplTest, BoundsAreCorrect) {
  // Set custom web contents bounds.
#if BUILDFLAG(IS_MAC)
  ChangeBrowserWindowBoundsForDesiredWebContentsBounds(
      gfx::Rect(300, 300, 1000, 1000));
#else
  web_contents()->GetNativeView()->SetBoundsInScreen(
      gfx::Rect(300, 300, 1000, 1000),
      display::Screen::GetScreen()->GetDisplayForNewWindows());
#endif  // BUILDFLAG(IS_MAC)

  // Element is within the boundaries of `web_contents()`.
  EXPECT_EQ(GetViewRawPtr(CreateView(gfx::RectF(400, 400, 300, 300)))->bounds(),
            gfx::Rect(400, 699, 300, 1));

  // Element partially exceeds the upper limit of `web_contents()`.
  EXPECT_EQ(GetViewRawPtr(CreateView(gfx::RectF(800, 800, 300, 300)))->bounds(),
            gfx::Rect(800, 999, 200, 1));

  // Element partially exceeds the lower limit of `web_contents()`.
  EXPECT_EQ(
      GetViewRawPtr(CreateView(gfx::RectF(-100, -100, 300, 300)))->bounds(),
      gfx::Rect(0, 199, 200, 1));
}

TEST_F(AutofillFieldPromoViewImplTest, LifetimeIsManagedCorrectlyOnClose) {
  base::WeakPtr<AutofillFieldPromoView> view = CreateView();
  AutofillFieldPromoViewImpl* view_ptr = GetViewRawPtr(view);

  EXPECT_TRUE(BrowserView::GetBrowserViewForBrowser(browser())
                  ->contents_web_view()
                  ->Contains(view_ptr));

  view->Close();
  EXPECT_FALSE(view);
}

TEST_F(AutofillFieldPromoViewImplTest, OverlapsWithPictureInPictureWindow) {
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
}

TEST_F(AutofillFieldPromoViewImplTest, ElementIdForIphIsCorrect) {
  EXPECT_EQ(
      GetViewRawPtr(CreateView())->GetProperty(views::kElementIdentifierKey),
      element_identifier());
}

}  // namespace
}  // namespace autofill
