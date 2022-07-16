// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/compat_mode/touch_mode_mouse_rewriter.h"

#include "ui/events/test/event_generator.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

namespace arc {

namespace {

class LongPressReceiverView : public views::View {
 public:
  bool OnMousePressed(const ui::MouseEvent& event) override {
    if (event.IsLeftMouseButton()) {
      left_pressed_ = true;
      ++press_count_;
      return true;
    } else if (event.IsRightMouseButton()) {
      right_pressed_ = true;
      ++press_count_;
      return true;
    }
    return false;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (event.IsLeftMouseButton()) {
      left_pressed_ = false;
      ++release_count_;
    } else if (event.IsRightMouseButton()) {
      right_pressed_ = false;
      ++release_count_;
    }
  }

  bool left_pressed() const { return left_pressed_; }
  bool right_pressed() const { return right_pressed_; }
  int press_count() const { return press_count_; }
  int release_count() const { return release_count_; }

 private:
  bool left_pressed_ = false;
  bool right_pressed_ = false;
  int press_count_ = 0;
  int release_count_ = 0;
};

}  // namespace

class TouchModeMouseRewriterTest : public views::ViewsTestBase {
 public:
  TouchModeMouseRewriterTest()
      : views::ViewsTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~TouchModeMouseRewriterTest() override = default;
};

TEST_F(TouchModeMouseRewriterTest, RightClickConvertedToLongPress) {
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  LongPressReceiverView* view =
      widget->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  host()->GetEventSource()->AddEventRewriter(&touch_mode_mouse_rewriter);
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_FALSE(view->left_pressed());
  EXPECT_FALSE(view->right_pressed());

  // Press the right button. It will immediately generate a synthesized left
  // press event.
  generator.PressRightButton();
  EXPECT_TRUE(view->left_pressed());
  EXPECT_FALSE(view->right_pressed());

  // Immediately release the right button. It will not generate any event.
  generator.ReleaseRightButton();
  EXPECT_TRUE(view->left_pressed());
  EXPECT_FALSE(view->right_pressed());

  // After a while, the synthesized left press will be released.
  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_FALSE(view->left_pressed());
  EXPECT_FALSE(view->right_pressed());

  host()->GetEventSource()->RemoveEventRewriter(&touch_mode_mouse_rewriter);
}

TEST_F(TouchModeMouseRewriterTest, LeftPressedBeforeRightClick) {
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  LongPressReceiverView* view =
      widget->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  host()->GetEventSource()->AddEventRewriter(&touch_mode_mouse_rewriter);
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_EQ(0, view->press_count());
  EXPECT_EQ(0, view->release_count());

  generator.PressLeftButton();
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  // This right click should be ignored.
  generator.PressRightButton();
  generator.ReleaseRightButton();

  task_environment()->FastForwardBy(base::Milliseconds(200));
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  generator.ReleaseLeftButton();
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(1, view->release_count());

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(1, view->release_count());

  host()->GetEventSource()->RemoveEventRewriter(&touch_mode_mouse_rewriter);
}

TEST_F(TouchModeMouseRewriterTest, RightClickDuringLeftPress) {
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  LongPressReceiverView* view =
      widget->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  host()->GetEventSource()->AddEventRewriter(&touch_mode_mouse_rewriter);
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_EQ(0, view->press_count());
  EXPECT_EQ(0, view->release_count());

  generator.PressLeftButton();
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  // This right click should be ignored.
  generator.PressRightButton();
  generator.ReleaseRightButton();

  task_environment()->FastForwardBy(base::Seconds(1));
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  generator.ReleaseLeftButton();
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(1, view->release_count());

  host()->GetEventSource()->RemoveEventRewriter(&touch_mode_mouse_rewriter);
}

TEST_F(TouchModeMouseRewriterTest, LeftClickedAfterRightClick) {
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  LongPressReceiverView* view =
      widget->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  host()->GetEventSource()->AddEventRewriter(&touch_mode_mouse_rewriter);
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_EQ(0, view->press_count());
  EXPECT_EQ(0, view->release_count());

  generator.PressRightButton();
  generator.ReleaseRightButton();

  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  task_environment()->FastForwardBy(base::Milliseconds(200));
  // This left click should be ignored.
  generator.PressLeftButton();
  generator.ReleaseLeftButton();
  task_environment()->FastForwardBy(base::Seconds(1));

  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(1, view->release_count());

  host()->GetEventSource()->RemoveEventRewriter(&touch_mode_mouse_rewriter);
}

TEST_F(TouchModeMouseRewriterTest, LeftLongPressedAfterRightClick) {
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  LongPressReceiverView* view =
      widget->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  host()->GetEventSource()->AddEventRewriter(&touch_mode_mouse_rewriter);
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_EQ(0, view->press_count());
  EXPECT_EQ(0, view->release_count());

  generator.PressRightButton();
  generator.ReleaseRightButton();
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  task_environment()->FastForwardBy(base::Milliseconds(200));

  // This left long press should be ignored.
  generator.PressLeftButton();
  task_environment()->FastForwardBy(base::Seconds(1));
  generator.ReleaseLeftButton();

  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(1, view->release_count());

  host()->GetEventSource()->RemoveEventRewriter(&touch_mode_mouse_rewriter);
}

TEST_F(TouchModeMouseRewriterTest, RightClickedTwice) {
  std::unique_ptr<views::Widget> widget = CreateTestWidget();
  LongPressReceiverView* view =
      widget->SetContentsView(std::make_unique<LongPressReceiverView>());
  widget->Show();

  TouchModeMouseRewriter touch_mode_mouse_rewriter;
  host()->GetEventSource()->AddEventRewriter(&touch_mode_mouse_rewriter);
  ui::test::EventGenerator generator(GetContext(), widget->GetNativeWindow());
  EXPECT_EQ(0, view->press_count());
  EXPECT_EQ(0, view->release_count());

  generator.PressRightButton();
  generator.ReleaseRightButton();
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  task_environment()->FastForwardBy(base::Milliseconds(200));

  // This right click should be ignored.
  generator.PressRightButton();
  generator.ReleaseRightButton();
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(0, view->release_count());

  task_environment()->FastForwardBy(base::Seconds(2));
  EXPECT_EQ(1, view->press_count());
  EXPECT_EQ(1, view->release_count());

  host()->GetEventSource()->RemoveEventRewriter(&touch_mode_mouse_rewriter);
}

}  // namespace arc
