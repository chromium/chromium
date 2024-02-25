// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/sharing_hub/preview_view.h"

#include "chrome/browser/share/share_attempt.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"

namespace {

using PreviewViewTest = ChromeViewsTestBase;

views::Label* FindLabelWithText(views::View* root, std::u16string text) {
  return static_cast<views::Label*>(views::test::AnyViewMatchingPredicate(
      root, [=](const views::View* candidate) -> bool {
        return (IsViewClass<views::Label>(candidate)) &&
               static_cast<const views::Label*>(candidate)->GetText() == text;
      }));
}

views::ImageView* FindImage(views::View* root) {
  return static_cast<views::ImageView*>(views::test::AnyViewMatchingPredicate(
      root, [](const views::View* candidate) -> bool {
        return IsViewClass<views::ImageView>(candidate);
      }));
}

ui::ImageModel BuildTestImage(SkColor color) {
  return ui::ImageModel::FromImageSkia(gfx::ImageSkia::CreateFromBitmap(
      gfx::test::CreateBitmap(/*size=*/32, color), 1.0));
}

SkColor ImageTopLeftColor(ui::ImageModel model) {
  gfx::ImageSkia skia = model.Rasterize(nullptr);
  return skia.bitmap()->getColor(0, 0);
}

std::unique_ptr<sharing_hub::PreviewView> BuildTestPreview() {
  auto view = std::make_unique<sharing_hub::PreviewView>(
      share::ShareAttempt(nullptr, u"Title", GURL("https://www.chromium.org/"),
                          BuildTestImage(SK_ColorRED)));
  return view;
}

TEST_F(PreviewViewTest, IncludesTitle) {
  auto view = BuildTestPreview();
  ASSERT_TRUE(FindLabelWithText(view.get(), u"Title"));
}

TEST_F(PreviewViewTest, IncludesURL) {
  auto view = BuildTestPreview();
  ASSERT_TRUE(FindLabelWithText(view.get(), u"https://www.chromium.org/"));
}

TEST_F(PreviewViewTest, IncludesImage) {
  auto view = BuildTestPreview();
  auto* image = FindImage(view.get());
  ASSERT_TRUE(image);
  EXPECT_EQ(ImageTopLeftColor(image->GetImageModel()), SK_ColorRED);
}

}  // namespace
