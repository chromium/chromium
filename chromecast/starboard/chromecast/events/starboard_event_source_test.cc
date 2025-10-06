// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/starboard/chromecast/events/starboard_event_source.h"

#include <cmath>
#include <memory>
#include <optional>

#include "base/test/task_environment.h"
#include "chromecast/starboard/chromecast/starboard_adapter/src/cast_starboard_api_adapter_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/events/event.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/pointer_details.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/gfx/geometry/vector2d.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace chromecast {
namespace {

// Copied from ui/ozone/test/mock_platform_window_delegate.h because of
// the visibility restriction.
class MockPlatformWindowDelegate : public ui::PlatformWindowDelegate {
 public:
  MockPlatformWindowDelegate() = default;

  MockPlatformWindowDelegate(const MockPlatformWindowDelegate&) = delete;
  MockPlatformWindowDelegate& operator=(const MockPlatformWindowDelegate&) =
      delete;

  ~MockPlatformWindowDelegate() override = default;

  MOCK_METHOD(gfx::Insets,
              CalculateInsetsInDIP,
              (ui::PlatformWindowState window_state),
              (const, override));
  MOCK_METHOD(void, OnBoundsChanged, (const BoundsChange& change), (override));
  MOCK_METHOD(void,
              OnDamageRect,
              (const gfx::Rect& damaged_region),
              (override));
  MOCK_METHOD(void, DispatchEvent, (ui::Event * event), (override));
  MOCK_METHOD(void, OnCloseRequest, (), (override));
  MOCK_METHOD(void, OnClosed, (), (override));
  MOCK_METHOD(void,
              OnWindowStateChanged,
              (ui::PlatformWindowState old_state,
               ui::PlatformWindowState new_state),
              (override));
#if BUILDFLAG(IS_LINUX)
  MOCK_METHOD(void,
              OnWindowTiledStateChanged,
              (ui::WindowTiledEdges new_tiled_edges),
              (override));
#endif
  MOCK_METHOD(void, OnLostCapture, (), (override));
  MOCK_METHOD(void,
              OnAcceleratedWidgetAvailable,
              (gfx::AcceleratedWidget widget),
              (override));
  MOCK_METHOD(void, OnWillDestroyAcceleratedWidget, (), (override));
  MOCK_METHOD(void, OnAcceleratedWidgetDestroyed, (), (override));
  MOCK_METHOD(void, OnActivationChanged, (bool active), (override));
  MOCK_METHOD(std::optional<gfx::Size>,
              GetMinimumSizeForWindow,
              (),
              (const, override));
  MOCK_METHOD(std::optional<gfx::Size>,
              GetMaximumSizeForWindow,
              (),
              (const, override));
  MOCK_METHOD(std::optional<ui::OwnedWindowAnchor>,
              GetOwnedWindowAnchorAndRectInDIP,
              (),
              (override));
  MOCK_METHOD(void, OnCursorUpdate, (), (override));
  MOCK_METHOD(void,
              OnOcclusionStateChanged,
              (ui::PlatformWindowOcclusionState occlusion_state),
              (override));
  MOCK_METHOD(int64_t,
              OnStateUpdate,
              (const State& old, const State& latest),
              (override));
  MOCK_METHOD(bool,
              OnRotateFocus,
              (ui::PlatformWindowDelegate::RotateDirection, bool),
              (override));
  MOCK_METHOD(bool, CanMaximize, (), (const, override));
  MOCK_METHOD(bool, CanFullscreen, (), (const, override));
};

using ::testing::_;
using ::testing::SaveArg;

constexpr float kXPos = 1.0f;
constexpr float kYPos = 2.0f;
constexpr float kDeltaX = 3.0f;
constexpr float kDeltaY = 4.0f;
constexpr float kPressure = 0.5f;
constexpr float kSizeX = 10.0f;
constexpr float kSizeY = 12.0f;
constexpr float kTiltX = 30.0f;
constexpr float kTiltY = 40.0f;

void DispatchSbEvent(const SbEvent* event) {
  // The |source_| should be subscribed to this and receive the event.
  CastStarboardApiAdapterImpl::SbEventHandle(event);
}

void EmulateKey(SbKey key,
                bool press,
                SbInputDeviceType device_type = kSbInputDeviceTypeKeyboard,
                unsigned int key_modifiers = kSbKeyModifiersNone) {
  SbInputData input_data = {};
  input_data.type = press ? kSbInputEventTypePress : kSbInputEventTypeUnpress;
  input_data.device_type = device_type;
  input_data.key = key;
  input_data.key_modifiers = key_modifiers;

  // Position is mostly for mouse/touch, but harmless for keys.
  input_data.position.x = kXPos;
  input_data.position.y = kYPos;

  SbEvent event;
  event.type = kSbEventTypeInput;
  event.data = &input_data;

  DispatchSbEvent(&event);
}

void EmulateMouseMove(float x,
                      float y,
                      unsigned int key_modifiers = kSbKeyModifiersNone) {
  SbInputData input_data = {};
  input_data.type = kSbInputEventTypeMove;
  input_data.device_type = kSbInputDeviceTypeMouse;
  input_data.position.x = x;
  input_data.position.y = y;
  input_data.key_modifiers = key_modifiers;

  SbEvent event;
  event.type = kSbEventTypeInput;
  event.data = &input_data;

  DispatchSbEvent(&event);
}

void EmulateMouseWheel(float delta_x,
                       float delta_y,
                       unsigned int key_modifiers = kSbKeyModifiersNone) {
  SbInputData input_data = {};
  input_data.type = kSbInputEventTypeWheel;
  input_data.device_type = kSbInputDeviceTypeMouse;
  input_data.position.x = kXPos;
  input_data.position.y = kYPos;
  input_data.delta.x = delta_x;
  input_data.delta.y = delta_y;
  input_data.key_modifiers = key_modifiers;

  SbEvent event;
  event.type = kSbEventTypeInput;
  event.data = &input_data;

  DispatchSbEvent(&event);
}

void EmulateTouch(SbInputEventType type,
                  float x,
                  float y,
                  unsigned int key_modifiers = kSbKeyModifiersNone) {
  SbInputData input_data = {};
  input_data.type = type;
  input_data.device_type = kSbInputDeviceTypeTouchScreen;
  input_data.position.x = x;
  input_data.position.y = y;
  input_data.key_modifiers = key_modifiers;
  input_data.pressure = kPressure;
  input_data.size.x = kSizeX;
  input_data.size.y = kSizeY;
  input_data.tilt.x = kTiltX;
  input_data.tilt.y = kTiltY;

  SbEvent event;
  event.type = kSbEventTypeInput;
  event.data = &input_data;

  DispatchSbEvent(&event);
}

// A test fixture is used to manage the global mock state and to handle the
// lifetime of the SingleThreadTaskEnvironment.
class StarboardEventSourceTest : public ::testing::Test {
 protected:
  StarboardEventSourceTest() : source_(&delegate_) {
    ON_CALL(delegate_, DispatchEvent(_))
        .WillByDefault(
            [this](ui::Event* event) { last_ui_event_ = event->Clone(); });
  }

  ~StarboardEventSourceTest() override = default;

  // This should be destructed last.
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ui::Event> last_ui_event_;
  MockPlatformWindowDelegate delegate_;
  StarboardEventSource source_;
};

TEST_F(StarboardEventSourceTest, SupportedKeysArePropagated) {
  EmulateKey(kSbKeyMediaPlayPause, /*press=*/true);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_TRUE(last_ui_event_->IsKeyEvent());
  EXPECT_EQ(last_ui_event_->AsKeyEvent()->code(),
            ui::DomCode::MEDIA_PLAY_PAUSE);
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kKeyPressed);

  EmulateKey(kSbKeyMediaPlayPause, /*press=*/false);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_EQ(last_ui_event_->AsKeyEvent()->code(),
            ui::DomCode::MEDIA_PLAY_PAUSE);
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kKeyReleased);
}

TEST_F(StarboardEventSourceTest, LetterKey) {
  EmulateKey(kSbKeyA, /*press=*/true);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_TRUE(last_ui_event_->IsKeyEvent());
  EXPECT_EQ(last_ui_event_->AsKeyEvent()->code(), ui::DomCode::US_A);
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kKeyPressed);
  EXPECT_FALSE(last_ui_event_->flags() & ui::EF_SHIFT_DOWN);
}

TEST_F(StarboardEventSourceTest, LetterKeyWithShift) {
  EmulateKey(kSbKeyA, /*press=*/true, kSbInputDeviceTypeKeyboard,
             kSbKeyModifiersShift);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_TRUE(last_ui_event_->IsKeyEvent());
  EXPECT_EQ(last_ui_event_->AsKeyEvent()->code(), ui::DomCode::US_A);
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kKeyPressed);
  EXPECT_TRUE(last_ui_event_->flags() & ui::EF_SHIFT_DOWN);
}

TEST_F(StarboardEventSourceTest, DigitKey) {
  EmulateKey(kSbKey1, /*press=*/true);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_EQ(last_ui_event_->AsKeyEvent()->code(), ui::DomCode::DIGIT1);
}

TEST_F(StarboardEventSourceTest, NumpadKey) {
  EmulateKey(kSbKeyNumpad5, /*press=*/true);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_EQ(last_ui_event_->AsKeyEvent()->code(), ui::DomCode::NUMPAD5);
}

TEST_F(StarboardEventSourceTest, OemSemicolonKey) {
  EmulateKey(kSbKeyOem1, /*press=*/true);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_EQ(last_ui_event_->AsKeyEvent()->code(), ui::DomCode::SEMICOLON);
}

TEST_F(StarboardEventSourceTest, UnsupportedKeyIsNotPropagated) {
  EmulateKey(kSbKeyCancel, /*press=*/true);
  EXPECT_EQ(last_ui_event_, nullptr);

  EmulateKey(kSbKeyCancel, /*press=*/false);
  EXPECT_EQ(last_ui_event_, nullptr);
}

TEST_F(StarboardEventSourceTest, MouseMove) {
  EmulateMouseMove(kDeltaX, kDeltaY);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_TRUE(last_ui_event_->IsMouseEvent());
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kMouseMoved);
  EXPECT_EQ(last_ui_event_->AsLocatedEvent()->location_f().x(), kDeltaX);
  EXPECT_EQ(last_ui_event_->AsLocatedEvent()->location_f().y(), kDeltaY);
  EXPECT_EQ(last_ui_event_->flags(), 0);
}

TEST_F(StarboardEventSourceTest, MouseLeftButtonClick) {
  EmulateKey(kSbKeyMouse1, true, kSbInputDeviceTypeMouse);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_TRUE(last_ui_event_->IsMouseEvent());
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kMousePressed);
  EXPECT_EQ(last_ui_event_->AsLocatedEvent()->location_f().x(), kXPos);
  EXPECT_EQ(last_ui_event_->AsLocatedEvent()->location_f().y(), kYPos);
  EXPECT_TRUE(last_ui_event_->AsMouseEvent()->IsOnlyLeftMouseButton());
  EXPECT_TRUE(last_ui_event_->flags() & ui::EF_LEFT_MOUSE_BUTTON);

  EmulateKey(kSbKeyMouse1, false, kSbInputDeviceTypeMouse);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_TRUE(last_ui_event_->IsMouseEvent());
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kMouseReleased);
  EXPECT_TRUE(last_ui_event_->AsMouseEvent()->IsOnlyLeftMouseButton());
}

TEST_F(StarboardEventSourceTest, MouseRightButtonClick) {
  EmulateKey(kSbKeyMouse2, true, kSbInputDeviceTypeMouse);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kMousePressed);
  EXPECT_TRUE(last_ui_event_->AsMouseEvent()->IsOnlyRightMouseButton());

  EmulateKey(kSbKeyMouse2, false, kSbInputDeviceTypeMouse);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kMouseReleased);
  EXPECT_TRUE(last_ui_event_->AsMouseEvent()->IsOnlyRightMouseButton());
}

TEST_F(StarboardEventSourceTest, MouseMiddleButtonClick) {
  EmulateKey(kSbKeyMouse3, true, kSbInputDeviceTypeMouse);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kMousePressed);
  EXPECT_TRUE(last_ui_event_->AsMouseEvent()->IsOnlyMiddleMouseButton());
}

TEST_F(StarboardEventSourceTest, MouseDrag) {
  // Left Mouse + Move creates Drag
  EmulateKey(kSbKeyMouse1, true, kSbInputDeviceTypeMouse);
  EmulateMouseMove(kDeltaX, kDeltaY);
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kMouseDragged);
  EXPECT_TRUE(last_ui_event_->flags() & ui::EF_LEFT_MOUSE_BUTTON);

  // Releasing Left Mouse removes Drag
  EmulateKey(kSbKeyMouse1, false, kSbInputDeviceTypeMouse);
  EmulateMouseMove(kDeltaX, kDeltaY);
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kMouseMoved);
}

TEST_F(StarboardEventSourceTest, MouseDragMulti) {
  // Left Mouse + Move creates Drag
  EmulateKey(kSbKeyMouse1, true, kSbInputDeviceTypeMouse);
  EmulateMouseMove(kDeltaX, kDeltaY);
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kMouseDragged);
  EXPECT_TRUE(last_ui_event_->flags() & ui::EF_LEFT_MOUSE_BUTTON);

  // Adding Right Mouse creates Drag
  EmulateKey(kSbKeyMouse2, true, kSbInputDeviceTypeMouse);
  EmulateMouseMove(kDeltaX, kDeltaY);
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kMouseDragged);
  EXPECT_TRUE(last_ui_event_->flags() & ui::EF_LEFT_MOUSE_BUTTON);
  EXPECT_TRUE(last_ui_event_->flags() & ui::EF_RIGHT_MOUSE_BUTTON);

  // Only releasing Left Mouse preserves Drag
  EmulateKey(kSbKeyMouse1, false, kSbInputDeviceTypeMouse);
  EmulateMouseMove(kDeltaX, kDeltaY);
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kMouseDragged);

  // Also releasing Right Mouse removes Drag
  EmulateKey(kSbKeyMouse2, false, kSbInputDeviceTypeMouse);
  EmulateMouseMove(kDeltaX, kDeltaY);
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kMouseMoved);
}

TEST_F(StarboardEventSourceTest, MouseWheelScroll) {
  EmulateMouseWheel(kDeltaX, kDeltaY);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_TRUE(last_ui_event_->IsMouseWheelEvent());
  EXPECT_EQ(last_ui_event_->AsMouseWheelEvent()->x_offset(),
            static_cast<int>(-kDeltaX * ui::MouseWheelEvent::kWheelDelta));
  EXPECT_EQ(last_ui_event_->AsMouseWheelEvent()->y_offset(),
            static_cast<int>(-kDeltaY * ui::MouseWheelEvent::kWheelDelta));
}

TEST_F(StarboardEventSourceTest, TouchPress) {
  EmulateTouch(kSbInputEventTypePress, kXPos, kYPos);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_TRUE(last_ui_event_->IsTouchEvent());
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kTouchPressed);
  auto* touch_event = last_ui_event_->AsTouchEvent();
  EXPECT_EQ(touch_event->location_f().x(), kXPos);
  EXPECT_EQ(touch_event->location_f().y(), kYPos);
  EXPECT_EQ(touch_event->pointer_details().id, 0);
  EXPECT_FLOAT_EQ(touch_event->pointer_details().force, kPressure);
  EXPECT_FLOAT_EQ(touch_event->pointer_details().radius_x, kSizeX / 2.0f);
  EXPECT_FLOAT_EQ(touch_event->pointer_details().radius_y, kSizeY / 2.0f);
  EXPECT_FLOAT_EQ(touch_event->pointer_details().tilt_x, kTiltX);
  EXPECT_FLOAT_EQ(touch_event->pointer_details().tilt_y, kTiltY);
}

TEST_F(StarboardEventSourceTest, TouchRelease) {
  EmulateTouch(kSbInputEventTypeUnpress, kXPos, kYPos);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_TRUE(last_ui_event_->IsTouchEvent());
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kTouchReleased);
  EXPECT_EQ(last_ui_event_->AsTouchEvent()->pointer_details().id, 0);
}

TEST_F(StarboardEventSourceTest, TouchMove) {
  EmulateTouch(kSbInputEventTypeMove, kDeltaX, kDeltaY);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_TRUE(last_ui_event_->IsTouchEvent());
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kTouchMoved);
  EXPECT_EQ(last_ui_event_->AsTouchEvent()->location_f().x(), kDeltaX);
  EXPECT_EQ(last_ui_event_->AsTouchEvent()->location_f().y(), kDeltaY);
  EXPECT_EQ(last_ui_event_->AsTouchEvent()->pointer_details().id, 0);
}

}  // namespace
}  // namespace chromecast
