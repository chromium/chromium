// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/multi_contents_view_drop_target_controller.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/frame/multi_contents_drop_target_view.h"
#include "content/public/common/drop_data.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/view_class_properties.h"

namespace {

static constexpr gfx::Size kMultiContentsViewSize(500, 500);

static constexpr gfx::PointF kDragPointForStartDropTargetShow(1, 250);
static constexpr gfx::PointF kDragPointForEndDropTargetShow(499, 250);
static constexpr gfx::PointF kDragPointForHiddenTargets(250, 250);

content::DropData ValidUrlDropData() {
  content::DropData valid_url_data;
  valid_url_data.url = GURL("https://mail.google.com");
  return valid_url_data;
}

void SetRTL(bool rtl) {
  // Override the current locale/direction.
  base::i18n::SetICUDefaultLocale(rtl ? "he" : "en");
  ASSERT_EQ(rtl, base::i18n::IsRTL());
}

class MockDropDelegate : public MultiContentsDropTargetView::DropDelegate {
 public:
  MOCK_METHOD(void,
              HandleLinkDrop,
              (MultiContentsDropTargetView::DropSide, const std::vector<GURL>&),
              (override));
};

class MultiContentsViewDropTargetControllerTest : public testing::Test {
 public:
  MultiContentsViewDropTargetControllerTest() = default;
  ~MultiContentsViewDropTargetControllerTest() override = default;

  void SetUp() override {
    SetRTL(false);
    multi_contents_view_ = std::make_unique<views::View>();
    drop_target_view_ = multi_contents_view_->AddChildView(
        std::make_unique<MultiContentsDropTargetView>(drop_delegate_));

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

  MultiContentsDropTargetView& drop_target_view() { return *drop_target_view_; }

  // Fast forwards by an arbitrary time to ensure timed events are executed.
  void FastForward(double progress = 1.0) {
    task_environment_.FastForwardBy(base::Milliseconds(1000 * progress));
  }

  void DragURLTo(const gfx::PointF& point) {
    controller().OnWebContentsDragUpdate(ValidUrlDropData(), point, false);
  }

 private:
  MockDropDelegate drop_delegate_;
  std::unique_ptr<MultiContentsViewDropTargetController> controller_;
  std::unique_ptr<views::View> multi_contents_view_;
  raw_ptr<MultiContentsDropTargetView> drop_target_view_;

  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

// Tests that the start drop target is shown when a drag reaches enters the
// "drop area" and a valid url is being dragged.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_ShowStartDropTarget) {
  DragURLTo(kDragPointForStartDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().side().has_value());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::START);
}

// Tests that the end drop target is shown when a drag reaches enters the
// "drop area" and a valid url is being dragged.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_ShowEndDropTarget) {
  DragURLTo(kDragPointForEndDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().side().has_value());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::END);
}

// With RTL enabled, tests that the "end" area's drag coordinates will show
// the "start" drop target.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_ShowStartDropTarget_RTL) {
  SetRTL(true);
  DragURLTo(kDragPointForEndDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().side().has_value());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::START);
}

// With RTL enabled, tests that the "start" area's drag coordinates will show
// the "end" drop target.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_ShowEndDropTarget_RTL) {
  SetRTL(true);
  DragURLTo(kDragPointForStartDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward();
  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().side().has_value());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::END);
}

// Tests that the drop target is shown even if the timer was started from a drag
// in a different region.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_DragMovedBetweenDropTargets) {
  DragURLTo(kDragPointForEndDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  FastForward(0.25);
  EXPECT_FALSE(drop_target_view().GetVisible());

  DragURLTo(kDragPointForStartDropTargetShow);
  FastForward(0.25);
  EXPECT_FALSE(drop_target_view().GetVisible());

  // Fast forward to the end of the animtion. The start-side drop target should
  // be shown, even though the timer started with a drag to the end-side.
  FastForward(0.50);

  EXPECT_TRUE(drop_target_view().GetVisible());
  ASSERT_TRUE(drop_target_view().side().has_value());
  EXPECT_EQ(drop_target_view().side().value(),
            MultiContentsDropTargetView::DropSide::START);
}

// Tests that the drop target is not shown when an invalid url is being dragged.
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_HideDropTargetOnInvalidURL) {
  controller().OnWebContentsDragUpdate(content::DropData(),
                                       kDragPointForStartDropTargetShow, false);

  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target timer is cancelled when a drag is not in the
// "drop area".
TEST_F(MultiContentsViewDropTargetControllerTest,
       OnWebContentsDragUpdate_HideDropTargetOnOutOfBounds) {
  DragURLTo(kDragPointForStartDropTargetShow);
  EXPECT_FALSE(drop_target_view().GetVisible());

  controller().OnWebContentsDragUpdate(ValidUrlDropData(),
                                       kDragPointForHiddenTargets, false);
  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

// Tests that the drop target timer is cancelled when a drag exits the contents
// view.
TEST_F(MultiContentsViewDropTargetControllerTest, OnWebContentsDragExit) {
  DragURLTo(kDragPointForStartDropTargetShow);

  controller().OnWebContentsDragExit();
  FastForward();
  EXPECT_FALSE(drop_target_view().GetVisible());
}

}  // namespace
