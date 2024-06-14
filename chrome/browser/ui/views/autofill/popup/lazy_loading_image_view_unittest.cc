// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/lazy_loading_image_view.h"

#include "base/functional/callback_helpers.h"
#include "base/test/mock_callback.h"
#include "chrome/app/vector_icons/vector_icons.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/vector_icons/vector_icons.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/resources/grit/ui_resources.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/test/views_drawing_test_utils.h"
#include "ui/views/test/views_test_utils.h"

namespace autofill {

class LazyLoadingImageViewTest : public ChromeViewsTestBase {
 public:
  LazyLoadingImageViewTest() = default;
  LazyLoadingImageViewTest(LazyLoadingImageViewTest&) = delete;
  LazyLoadingImageViewTest& operator=(LazyLoadingImageViewTest&) = delete;
  ~LazyLoadingImageViewTest() override = default;

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    widget_ =
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  }

  void TearDown() override {
    widget_.reset();
    ChromeViewsTestBase::TearDown();
  }

 protected:
  views::Widget& widget() const { return *widget_; }

  void Paint() { views::test::PaintViewToBitmap(widget().GetRootView()); }

 private:
  std::unique_ptr<views::Widget> widget_;
};

TEST_F(LazyLoadingImageViewTest, ImageLoaderProvidesContent) {
  base::MockCallback<LazyLoadingImageView::ImageLoader> mock_loader;
  gfx::Image image =
      ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          IDR_DEFAULT_FAVICON);
  EXPECT_CALL(mock_loader, Run)
      .WillOnce(
          [&image](base::CancelableTaskTracker*,
                   LazyLoadingImageView::ImageLoaderOnLoadSuccess on_success,
                   LazyLoadingImageView::ImageLoaderOnLoadFail) {
            std::move(on_success).Run(image);
          });

  auto* view = widget().SetContentsView(std::make_unique<LazyLoadingImageView>(
      image.Size(), ui::ImageModel::FromVectorIcon(kGlobeIcon),
      mock_loader.Get()));

  ASSERT_FALSE(gfx::test::AreImagesEqual(image, view->GetImageForTesting()));
  Paint();
  EXPECT_TRUE(gfx::test::AreImagesEqual(image, view->GetImageForTesting()));
}

TEST_F(LazyLoadingImageViewTest, ImageLoaderIsTriggerredForVisibleViewsOnly) {
  base::MockCallback<LazyLoadingImageView::ImageLoader> mock_loader;
  ON_CALL(mock_loader, Run)
      .WillByDefault(
          [](base::CancelableTaskTracker*,
             LazyLoadingImageView::ImageLoaderOnLoadSuccess on_success,
             LazyLoadingImageView::ImageLoaderOnLoadFail) {
            std::move(on_success).Run(gfx::Image());
          });

  testing::MockFunction<void()> check;
  {
    testing::InSequence s;
    EXPECT_CALL(mock_loader, Run).Times(0);
    EXPECT_CALL(check, Call);
    EXPECT_CALL(mock_loader, Run);
  }

  gfx::Size visible_area_size(100, 100);
  auto* container =
      widget().SetContentsView(views::Builder<views::BoxLayoutView>()
                                   .SetPreferredSize(visible_area_size)
                                   .Build());

  // `push_view` is a big enough view that pushes the image view out of
  // the `container` viewport and thus makes it invisible.
  auto* push_view =
      container->AddChildView(views::Builder<views::View>()
                                  .SetPreferredSize(visible_area_size)
                                  .Build());
  container->AddChildView(std::make_unique<LazyLoadingImageView>(
      gfx::Size(10, 10), ui::ImageModel::FromVectorIcon(kGlobeIcon),
      mock_loader.Get()));

  Paint();

  check.Call();

  // Removing `push_view` makes the image view visible.
  auto push_view_owned = container->RemoveChildViewT(push_view);
  views::test::RunScheduledLayout(&widget());
  Paint();
}

}  // namespace autofill
