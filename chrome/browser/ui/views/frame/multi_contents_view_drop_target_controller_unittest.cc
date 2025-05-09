// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"

#include <memory>

#include "content/public/common/drop_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view_class_properties.h"

namespace {

static constexpr gfx::Size kMultiContentsViewSize(500, 500);

static constexpr gfx::PointF kDragPointForDropTargetShow(450, 450);

class MultiContentsViewDropTargetControllerTest : public testing::Test {
 public:
  MultiContentsViewDropTargetControllerTest() = default;
  ~MultiContentsViewDropTargetControllerTest() override = default;

  void SetUp() override {
    multi_contents_view_ = std::make_unique<views::View>();
    drop_target_view_ =
        multi_contents_view_->AddChildView(std::make_unique<views::View>());
    drop_target_view_->SetVisible(false);
    controller_ = std::make_unique<MultiContentsViewDropTargetController>(
        *drop_target_view_);

    multi_contents_view_->SetSize(kMultiContentsViewSize);
  }

  void TearDown() override {
    controller_.reset();
    drop_target_view_ = nullptr;
    multi_contents_view_.reset();
  }

  MultiContentsViewDropTargetController& controller() { return *controller_; }

  views::View& drop_target_view() { return *drop_target_view_; }

 private:
  std::unique_ptr<MultiContentsViewDropTargetController> controller_;
  std::unique_ptr<views::View> multi_contents_view_;
  raw_ptr<views::View> drop_target_view_;
};

// Tests that the drop target is shown when a drag reaches enters the "drop
// area" and a valid url is being dragged.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_ShowDropTarget) {
  ASSERT_FALSE(drop_target_view().GetVisible());

  content::DropData valid_url_data;
  valid_url_data.url = GURL("https://mail.google.com");
  controller().OnWebContentsDragUpdate(valid_url_data,
                                       kDragPointForDropTargetShow);

  EXPECT_TRUE(drop_target_view().GetVisible());
}

// Tests that the drop target is hidden when an invalid url is being dragged.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_HideDropTargetOnInvalidURL) {
  drop_target_view().SetVisible(true);
  ASSERT_TRUE(drop_target_view().GetVisible());

  controller().OnWebContentsDragUpdate(content::DropData(),
                                       kDragPointForDropTargetShow);

  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target is hidden when a drag is not in the "drop area".
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_HideDropTargetOnOutOfBounds) {
  drop_target_view().SetVisible(true);
  ASSERT_TRUE(drop_target_view().GetVisible());

  content::DropData valid_url_data;
  valid_url_data.url = GURL("https://mail.google.com");
  controller().OnWebContentsDragUpdate(valid_url_data, gfx::PointF(0, 0));

  EXPECT_FALSE(drop_target_view().GetVisible());
}

}  // namespace
