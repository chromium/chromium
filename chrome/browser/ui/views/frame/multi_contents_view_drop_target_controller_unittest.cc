// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"

#include <memory>

#include "base/test/task_environment.h"
#include "content/public/common/drop_data.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/views/view_class_properties.h"

namespace {

static constexpr gfx::Size kMultiContentsViewSize(500, 500);

static constexpr gfx::PointF kDragPointForDropTargetShow(450, 450);

content::DropData ValidUrlDropData() {
  content::DropData valid_url_data;
  valid_url_data.url = GURL("https://mail.google.com");
  return valid_url_data;
}

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

  // Fast forwards by an arbitrary time to ensure timed events are executed.
  void FastForward() { task_environment_.FastForwardBy(base::Seconds(60)); }

  void TriggerDropTargetShowTimer() {
    controller().OnWebContentsDragUpdate(ValidUrlDropData(),
                                         kDragPointForDropTargetShow);
  }

 private:
  std::unique_ptr<MultiContentsViewDropTargetController> controller_;
  std::unique_ptr<views::View> multi_contents_view_;
  raw_ptr<views::View> drop_target_view_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests that the drop target is shown when a drag reaches enters the "drop
// area" and a valid url is being dragged.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_ShowDropTarget) {
  TriggerDropTargetShowTimer();
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());
}

// Tests that the drop target is not shown when an invalid url is being dragged.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_HideDropTargetOnInvalidURL) {
  controller().OnWebContentsDragUpdate(content::DropData(),
                                       kDragPointForDropTargetShow);

  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target timer is cancelled when a drag is not in the
// "drop area".
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_HideDropTargetOnOutOfBounds) {
  TriggerDropTargetShowTimer();
  EXPECT_FALSE(drop_target_view().GetVisible());

  controller().OnWebContentsDragUpdate(ValidUrlDropData(), gfx::PointF(0, 0));
  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target timer is cancelled when a drag exits the contents
// view.
TEST_F(MultiContentsViewDropTargetControllerTest, OnWebContentsDragExit) {
  TriggerDropTargetShowTimer();
  EXPECT_FALSE(drop_target_view().GetVisible());

  controller().OnWebContentsDragExit();
  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

}  // namespace
