// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/glic/border/border_view.h"

#include "base/test/bind.h"
#include "base/test/test_future.h"
#include "cc/test/pixel_test_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/viz/common/frame_timing_details.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/snapshot/snapshot.h"

namespace glic {

namespace {
using BorderViewBrowserTest = InProcessBrowserTest;
}

// Exercise that, the border is resized correctly whenever the browser's size
// changes.
IN_PROC_BROWSER_TEST_F(BorderViewBrowserTest, BorderResize) {
  // TODO(crbug.com/385828490): We should exercise the proper closing flow.
  // Currently the BookmarkModel has a dangling observer during destruction, if
  // the glic UI is toggled.
  auto* border =
      browser()->window()->AsBrowserView()->contents_web_view()->glic_border();
  border->StartAnimation();
  auto* contents_web_view = browser()->GetBrowserView().contents_web_view();
  EXPECT_EQ(border->GetVisibleBounds(), contents_web_view->GetVisibleBounds());

  // Note: there is a minimal size that the desktop window can be. It seems to
  // be around 500px by 500px.
  const gfx::Size new_size(600, 600);
  auto* browser_window = browser()->window();
  const gfx::Rect new_bounds(browser_window->GetBounds().origin(), new_size);
  EXPECT_NE(browser_window->GetBounds(), new_bounds);

  {
    SCOPED_TRACE("resizing");
    browser_window->SetBounds(new_bounds);
    content::RunAllPendingInMessageLoop();
  }

  // Resized correctly.
  EXPECT_EQ(browser_window->GetBounds(), new_bounds);
  EXPECT_EQ(border->GetVisibleBounds(), contents_web_view->GetVisibleBounds());
}

namespace {

class BorderViewZOrderBrowserTest : public InProcessBrowserTest,
                                    public ::testing::WithParamInterface<bool> {
 public:
  BorderViewZOrderBrowserTest() = default;
  ~BorderViewZOrderBrowserTest() override = default;
};

// Another View that also fights to be the z-topmost child.
class CompetingSibling : public views::View, public views::ViewObserver {
  METADATA_HEADER(CompetingSibling, View)
 public:
  explicit CompetingSibling(views::View* parent) : parent_(parent) {
    parent_->AddObserver(this);
  }
  CompetingSibling(const CompetingSibling&) = delete;
  CompetingSibling& operator=(const CompetingSibling&) = delete;
  ~CompetingSibling() override { parent_->RemoveObserver(this); }

  // `views::ViewObserver`:
  void OnChildViewReordered(views::View* observed_view,
                            views::View* child) override {
    ASSERT_EQ(observed_view, parent_);
    parent_->ReorderChildView(this, parent_->children().size());
  }

 private:
  const raw_ptr<views::View> parent_ = nullptr;
};

BEGIN_METADATA(CompetingSibling)
END_METADATA

}  // namespace

// The glic border should always be the z-topmost child of the contents web
// view, unless there is a competing sibling.
IN_PROC_BROWSER_TEST_P(BorderViewZOrderBrowserTest, TopMostChild) {
  auto* parent = browser()->GetBrowserView().contents_web_view();
  ASSERT_TRUE(parent);
  auto* border = static_cast<views::View*>(parent->glic_border());
  ASSERT_TRUE(border);

  EXPECT_EQ(border, parent->children().back());
  EXPECT_EQ(parent->children().size(), 2u);

  views::View* existing_sibling = parent->children().at(0u);
  views::View* new_sibling = nullptr;

  bool competing_sibling = GetParam();

  EXPECT_EQ(parent->children().size(), 2u);
  new_sibling = parent->AddChildViewAt(
      competing_sibling ? std::make_unique<CompetingSibling>(parent)
                        : std::make_unique<views::View>(),
      2u);
  if (competing_sibling) {
    EXPECT_THAT(parent->children(),
                ::testing::ElementsAre(existing_sibling, border, new_sibling));
  } else {
    // If the border view has another sibling that also competes to be the
    // z-topmost child, the border view should back down and not lock up the UI
    // thread.
    EXPECT_THAT(parent->children(),
                ::testing::ElementsAre(existing_sibling, new_sibling, border));
  }

  EXPECT_EQ(parent->children().size(), 3u);
  views::View* reorder_target = parent->children().at(1u);
  parent->ReorderChildView(reorder_target, 3u);
  if (competing_sibling) {
    EXPECT_THAT(parent->children(),
                ::testing::ElementsAre(existing_sibling, border, new_sibling));
  } else {
    EXPECT_THAT(parent->children(),
                ::testing::ElementsAre(existing_sibling, new_sibling, border));
  }
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    BorderViewZOrderBrowserTest,
    ::testing::Bool(),
    [](const ::testing::TestParamInfo<BorderViewZOrderBrowserTest::ParamType>&
           info) {
      return info.param ? "CompetingSibling" : "RegularSibling";
    });

namespace {

class BorderViewPixelTest : public InProcessBrowserTest {
 public:
  BorderViewPixelTest() = default;
  ~BorderViewPixelTest() override = default;

  void SetUp() override {
    EnablePixelOutput();
    InProcessBrowserTest::SetUp();
  }

  static SkBitmap ConstructExpectedBitmap(const gfx::Size& size,
                                          SkColor border_color,
                                          SkColor center_color,
                                          int border_width) {
    SkBitmap bitmap;
    SkImageInfo info =
        SkImageInfo::Make(size.width(), size.height(), kRGBA_8888_SkColorType,
                          kUnpremul_SkAlphaType);
    bitmap.allocPixels(info);
    bitmap.eraseColor(center_color);
    SkCanvas canvas(bitmap, SkSurfaceProps{});
    SkPaint border;
    border.setColor(border_color);
    border.setStyle(SkPaint::Style::kStroke_Style);
    border.setStrokeWidth(border_width);
    canvas.drawRect(SkRect::MakeXYWH(0, 0, size.width(), size.height()),
                    border);
    return bitmap;
  }

  // TODO(crbug.com/386220498): Look into the feasibility of Skia gold tests.
  static void ExpectTwoBitmapsAreEqual(const SkBitmap& actual,
                                       const SkBitmap& expected,
                                       int bottom_rows_to_exclude) {
    ASSERT_EQ(actual.dimensions(), expected.dimensions())
        << actual.dimensions().width() << "x" << actual.dimensions().height()
        << " vs " << expected.dimensions().width() << "x"
        << expected.dimensions().height();

    int num_pixel_mismatch = 0;
    gfx::Rect err_bounding_box;

    for (int r = 0; r < actual.height() - bottom_rows_to_exclude; ++r) {
      for (int c = 0; c < actual.width(); ++c) {
        if (actual.getColor(c, r) != expected.getColor(c, r)) {
          ++num_pixel_mismatch;
          err_bounding_box.Union(gfx::Rect(c, r, 1, 1));
        }
      }
    }
    if (num_pixel_mismatch != 0) {
      EXPECT_TRUE(false) << "Number of pixel mismatches: " << num_pixel_mismatch
                         << "; error bounding box: "
                         << err_bounding_box.ToString() << "; actual bitmap "
                         << cc::GetPNGDataUrl(actual);
    }
  }

  static void WaitForCompositorFlush(content::WebContents* web_contents) {
    ForceNewCompositorFrameFromBrowser(web_contents);
    WaitForBrowserCompositorFramePresented(web_contents);
  }
};
}  // namespace

IN_PROC_BROWSER_TEST_F(BorderViewPixelTest, Basic) {
  // TODO(crbug.com/385828490): We should exercise the proper closing flow.
  // Currently the BookmarkModel has a dangling observer during destruction, if
  // the glic UI is toggled.
  auto* border =
      browser()->window()->AsBrowserView()->contents_web_view()->glic_border();

  ASSERT_EQ(browser()->tab_strip_model()->GetTabCount(), 1);
  auto* web_contents =
      browser()->tab_strip_model()->GetTabAtIndex(0)->GetContents();

  {
    SCOPED_TRACE("Start animation");
    border->StartAnimation();
    // Makes sure the pixels are on the screen.
    WaitForCompositorFlush(web_contents);
  }

  auto* browser_view = browser()->window()->AsBrowserView();

  gfx::Rect capture_rect = web_contents->GetViewBounds();
  const auto& browser_view_screen_bounds = browser_view->GetBoundsInScreen();
  const auto& browser_view_bounds = browser_view->bounds();
  // TODO(liuwilliam): We should only use the screen coordinates, instead mixing
  // screen and view coords. Currently the WebContents's screen coordinates are
  // not exposed (see `ui::NativeViewHost`).
  //
  // Example:
  // web_contents->GetViewBounds()     14,97 1042x689
  // browser_view->GetBoundsInScreen() 14,10 1042x776
  // browser_view->bounds()            4,0 1042x776
  // capture_rect                      4,87 1042x689
  capture_rect.Offset(-browser_view_screen_bounds.OffsetFromOrigin());
  capture_rect.Offset(browser_view_bounds.OffsetFromOrigin());

  int bottom_rows_to_exclude = 0;
#if BUILDFLAG(IS_MAC)
  // Exclude the bottom 15 rows from the comparison because the rounded bottom
  // corners.
  bottom_rows_to_exclude = 15;
#endif

  gfx::NativeWindow native_window = browser()->window()->GetNativeWindow();

  {
    SCOPED_TRACE("Assert border");
    base::test::TestFuture<gfx::Image> result;
    ui::GrabWindowSnapshot(native_window, capture_rect, result.GetCallback());
    // Makes sure our CopyOutputRequest is served. We won't get the results back
    // if Viz doesn't activate a frame.
    WaitForCompositorFlush(web_contents);

    SkBitmap expected = ConstructExpectedBitmap(
        capture_rect.size(),
        /*border_color=*/
        browser()->GetBrowserView().GetColorProvider()->GetColor(
            ui::kColorSysPrimary),
        /*center_color=*/SkColors::kWhite.toSkColor(), /*border_width=*/10);
    ExpectTwoBitmapsAreEqual(result.Get().AsBitmap(), expected,
                             bottom_rows_to_exclude);
  }
  {
    SCOPED_TRACE("Cancel animation");
    // TODO(crbug.com/385828490): Ditto.
    border->CancelAnimation();
    WaitForCompositorFlush(web_contents);
  }
  {
    SCOPED_TRACE("Assert no border");
    base::test::TestFuture<gfx::Image> result;
    ui::GrabWindowSnapshot(native_window, capture_rect, result.GetCallback());
    WaitForCompositorFlush(web_contents);

    SkBitmap expected = ConstructExpectedBitmap(
        capture_rect.size(),
        /*border_color=*/SkColors::kWhite.toSkColor(),
        /*center_color=*/SkColors::kWhite.toSkColor(), /*border_width=*/10);
    ExpectTwoBitmapsAreEqual(result.Get().AsBitmap(), expected,
                             bottom_rows_to_exclude);
  }
}

}  // namespace glic
