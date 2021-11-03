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
      return true;
    } else if (event.IsRightMouseButton()) {
      right_pressed_ = true;
      return true;
    }
    return false;
  }

  void OnMouseReleased(const ui::MouseEvent& event) override {
    if (event.IsLeftMouseButton())
      left_pressed_ = false;
    else if (event.IsRightMouseButton())
      right_pressed_ = false;
  }

  bool left_pressed() const { return left_pressed_; }
  bool right_pressed() const { return right_pressed_; }

 private:
  bool left_pressed_ = false;
  bool right_pressed_ = false;
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

}  // namespace arc
