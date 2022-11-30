// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/platform_ui_input_delegate.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AllOf;
using ::testing::Property;
using ::testing::Ref;

namespace vr {

class PlatformUiInputDelegateTest : public PlatformUiInputDelegate {
 public:
  MOCK_METHOD1(FwdSendGestureToTarget, void(const InputEvent& event));

  void SendGestureToTarget(std::unique_ptr<InputEvent> event) override;
};

void PlatformUiInputDelegateTest::SendGestureToTarget(
    std::unique_ptr<InputEvent> event) {
  FwdSendGestureToTarget(*event);
}

TEST(PlatformUiInputDelegateTest, OnHoverEvents) {
  PlatformUiInputDelegateTest delegate;
  delegate.SetSize(40.0f, 30.0f);

  EXPECT_CALL(delegate,
              FwdSendGestureToTarget(
                  AllOf(Property(&InputEvent::type, InputEvent::kHoverEnter),
                        Property(&InputEvent::position_in_widget,
                                 gfx::PointF(20.0f, 15.0f)))));
  delegate.OnHoverEnter({0.5f, 0.5f}, base::TimeTicks());

  EXPECT_CALL(
      delegate,
      FwdSendGestureToTarget(AllOf(
          Property(&InputEvent::type, InputEvent::kHoverMove),
          Property(&InputEvent::position_in_widget, gfx::PointF(4.0f, 6.0f)))));
  delegate.OnHoverMove({0.1f, 0.2f}, base::TimeTicks());

  EXPECT_CALL(delegate, FwdSendGestureToTarget(Property(
                            &InputEvent::type, InputEvent::kHoverLeave)));
  delegate.OnHoverLeave(base::TimeTicks());
}

TEST(PlatformUiInputDelegateTest, OnButtonEvents) {
  PlatformUiInputDelegateTest delegate;
  delegate.SetSize(40.0f, 30.0f);

  EXPECT_CALL(
      delegate,
      FwdSendGestureToTarget(AllOf(
          Property(&InputEvent::type, InputEvent::kButtonDown),
          Property(&InputEvent::position_in_widget, gfx::PointF(4.0f, 3.0f)))));
  delegate.OnButtonDown({0.1f, 0.1f}, base::TimeTicks());

  EXPECT_CALL(
      delegate,
      FwdSendGestureToTarget(AllOf(
          Property(&InputEvent::type, InputEvent::kMove),
          Property(&InputEvent::position_in_widget, gfx::PointF(8.0f, 3.0f)))));
  delegate.OnTouchMove({0.2f, 0.1f}, base::TimeTicks());

  EXPECT_CALL(
      delegate,
      FwdSendGestureToTarget(AllOf(
          Property(&InputEvent::type, InputEvent::kButtonUp),
          Property(&InputEvent::position_in_widget, gfx::PointF(0.0f, 3.0f)))));
  delegate.OnButtonUp({0.0f, 0.1f}, base::TimeTicks());
}

TEST(PlatformUiInputDelegateTest, OnInputEvent) {
  PlatformUiInputDelegateTest delegate;
  delegate.SetSize(40.0f, 30.0f);

  auto event = std::make_unique<InputEvent>(InputEvent::kTypeUndefined);
  EXPECT_CALL(delegate,
              FwdSendGestureToTarget(
                  AllOf(Ref(*event), Property(&InputEvent::position_in_widget,
                                              gfx::PointF(4.0f, 3.0f)))));
  delegate.OnInputEvent(std::move(event), {0.1f, 0.1f});
}

}  // namespace vr
