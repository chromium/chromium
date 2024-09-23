// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "starboard_event_source.h"

#include "base/test/task_environment.h"
#include "chromecast/starboard/chromecast/starboard_adapter/src/cast_starboard_api_adapter_impl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/owned_window_anchor.h"
#include "ui/events/event.h"
#include "ui/events/keycodes/dom/dom_code.h"
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
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  MOCK_METHOD(void,
              OnFullscreenTypeChanged,
              (ui::PlatformFullscreenType old_type,
               ui::PlatformFullscreenType new_type),
              (override));
  MOCK_METHOD(void, OnOverviewModeChanged, (bool overview), (override));
#endif  // BUILDFLAG(IS_CHROMEOS_LACROS)
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
  MOCK_METHOD(void, OnMouseEnter, (), (override));
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

constexpr float kXPosClick = 1;
constexpr float kYPosClick = 2;

void DispatchSbEvent(const SbEvent* event) {
  // The |source_| should be subscribed to this and receive the event.
  CastStarboardApiAdapterImpl::SbEventHandle(event);
}

void EmulateKey(SbKey key, bool press) {
  SbInputData input_data;
  input_data.type = press ? kSbInputEventTypePress : kSbInputEventTypeUnpress;
  input_data.key = key;

  // Normally invalid for keys other than mouse presses, but okay for test.
  input_data.position.x = kXPosClick;
  input_data.position.y = kYPosClick;

  SbEvent event;
  event.type = kSbEventTypeInput;
  event.data = &input_data;

  DispatchSbEvent(&event);
}

// A test fixture is used to manage the global mock state and to handle the
// lifetime of the SingleThreadTaskEnvironment.
class StarboardEventSourceTest : public ::testing::Test {
 protected:
  StarboardEventSourceTest() : last_ui_event_(nullptr), source_(&delegate_) {
    ON_CALL(delegate_, DispatchEvent(_)).WillByDefault([&](ui::Event* event) {
      last_ui_event_ = event->Clone();
    });
  }

  ~StarboardEventSourceTest() override = default;

  // This should be destructed last.
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ui::Event> last_ui_event_;
  MockPlatformWindowDelegate delegate_;
  StarboardEventSource source_;
};

TEST_F(StarboardEventSourceTest, SupportedKeysArepropagated) {
  EmulateKey(kSbKeyMediaPlayPause, /*press=*/true);
  ASSERT_NE(last_ui_event_, nullptr);
  EXPECT_TRUE(last_ui_event_->IsKeyEvent());
  EXPECT_EQ(last_ui_event_->AsKeyEvent()->code(),
            ui::DomCode::MEDIA_PLAY_PAUSE);
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kKeyPressed);

  EmulateKey(kSbKeyMediaPlayPause, /*press=*/false);
  EXPECT_EQ(last_ui_event_->AsKeyEvent()->code(),
            ui::DomCode::MEDIA_PLAY_PAUSE);
  EXPECT_EQ(last_ui_event_->type(), ui::EventType::kKeyReleased);
}

TEST_F(StarboardEventSourceTest, UnsupportedKeyIsNotPropagated) {
  EmulateKey(kSbKeyCancel, /*press=*/true);
  EXPECT_EQ(last_ui_event_, nullptr);

  EmulateKey(kSbKeyCancel, /*press=*/false);
  EXPECT_EQ(last_ui_event_, nullptr);
}

TEST_F(StarboardEventSourceTest, UnsupportedClickIsNotPropagated) {
  EmulateKey(kSbKeyMouse2, /*press=*/true);
  EXPECT_EQ(last_ui_event_, nullptr);

  EmulateKey(kSbKeyMouse2, /*press=*/false);
  EXPECT_EQ(last_ui_event_, nullptr);
}

}  // namespace
}  // namespace chromecast
