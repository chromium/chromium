// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/keyboard.h"

#include <string_view>

#include "ash/accelerators/accelerator_controller_impl.h"
#include "ash/accessibility/accessibility_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/keyboard/keyboard_controller_impl.h"
#include "ash/public/cpp/external_arc/overlay/arc_overlay_manager.h"
#include "ash/shell.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test/test_widget_builder.h"
#include "ash/test/test_window_builder.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller_test_api.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"
#include "components/exo/buffer.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/keyboard_device_configuration_delegate.h"
#include "components/exo/keyboard_modifiers.h"
#include "components/exo/keyboard_observer.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "components/exo/test/shell_surface_builder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/accelerators/test_accelerator_target.h"
#include "ui/base/ime/constants.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/base/ime/events.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/events/ozone/events_ozone.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"
#include "ui/views/controls/textfield/textfield.h"

namespace exo {
namespace {

// XKB mod masks for the default keymap.
constexpr uint32_t kShiftMask = 1 << 0;
constexpr uint32_t kControlMask = 1 << 2;
constexpr uint32_t kAltMask = 1 << 3;
constexpr uint32_t kNumLockMask = 1 << 4;

class KeyboardTest : public test::ExoTestBase {
 public:
  KeyboardTest()
      : test::ExoTestBase(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~KeyboardTest() override = default;
};

class MockKeyboardDelegate : public KeyboardDelegate {
 public:
  MockKeyboardDelegate() = default;

  // Overridden from KeyboardDelegate:
  MOCK_METHOD(bool, CanAcceptKeyboardEventsForSurface, (Surface*), (const));
  MOCK_METHOD(
      void,
      OnKeyboardEnter,
      (Surface*,
       (const base::flat_map<PhysicalCode, base::flat_set<KeyState>>&)));
  MOCK_METHOD(void, OnKeyboardLeave, (Surface*));
  MOCK_METHOD(uint32_t, OnKeyboardKey, (base::TimeTicks, ui::DomCode, bool));
  MOCK_METHOD(void, OnKeyboardModifiers, (const KeyboardModifiers&));
  MOCK_METHOD(void,
              OnKeyRepeatSettingsChanged,
              (bool, base::TimeDelta, base::TimeDelta));
  MOCK_METHOD(void, OnKeyboardLayoutUpdated, (std::string_view));
};
using NiceMockKeyboardDelegate = ::testing::NiceMock<MockKeyboardDelegate>;

class MockKeyboardDeviceConfigurationDelegate
    : public KeyboardDeviceConfigurationDelegate {
 public:
  MockKeyboardDeviceConfigurationDelegate() = default;

  // Overridden from KeyboardDeviceConfigurationDelegate:
  MOCK_METHOD(void, OnKeyboardTypeChanged, (bool));
};

class MockKeyboardObserver : public KeyboardObserver {
 public:
  MockKeyboardObserver() = default;

  // Overridden from KeyboardObserver:
  MOCK_METHOD(void, OnKeyboardDestroying, (Keyboard*));
  MOCK_METHOD(void, OnKeyboardKey, (base::TimeTicks, ui::DomCode, bool));
};
using NiceMockKeyboardObserver = ::testing::NiceMock<MockKeyboardObserver>;

class TestShellSurface : public ShellSurface {
 public:
  explicit TestShellSurface(Surface* surface) : ShellSurface(surface) {}

  MOCK_METHOD(bool, AcceleratorPressed, (const ui::Accelerator& accelerator));
};

// This event handler moves the focus to the given window when receiving a key
// event.
class TestEventHandler : public ui::EventHandler {
 public:
  explicit TestEventHandler(aura::Window* focus_window)
      : focus_window_(focus_window) {}
  TestEventHandler(const TestEventHandler&) = delete;
  TestEventHandler& operator=(const TestEventHandler&) = delete;

  void OnKeyEvent(ui::KeyEvent* event) override {
    aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow())
        ->FocusWindow(focus_window_);
  }

  raw_ptr<aura::Window> focus_window_;
};

// Verifies that switching desks via alt-tab doesn't prevent Seat from receiving
// key events. https://crbug.com/1008574.
TEST_F(KeyboardTest, CorrectSeatPressedKeysOnSwitchingDesks) {
  Seat seat;
  Keyboard keyboard(std::make_unique<NiceMockKeyboardDelegate>(), &seat);

  // Create 2 desks.
  auto* desks_controller = ash::DesksController::Get();
  desks_controller->NewDesk(ash::DesksCreationRemovalSource::kButton);
  ASSERT_EQ(2u, desks_controller->desks().size());
  ash::Desk* desk_1 = desks_controller->desks()[0].get();
  const ash::Desk* desk_2 = desks_controller->desks()[1].get();
  // Desk 1 has a normal window.
  auto win0 = CreateAppWindow(gfx::Rect(0, 0, 250, 100));

  // Desk 2 has an exo surface window.
  ash::ActivateDesk(desk_2);
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();

  // Go back to desk 1, and trigger an alt-tab (releasing alt first). This would
  // trigger activating the exo surface window on desk 2, which would lead to a
  // desk switch animation. During the animation, expect that Seat gets all the
  // keys in `OnKeyEvent()`, and the |pressed_keys_| map is correctly updated.
  ash::ActivateDesk(desk_1);
  auto displatch_key_event = [&](ui::EventType type, ui::KeyboardCode key_code,
                                 ui::DomCode code, int flags) {
    ui::KeyEvent key_event{type, key_code, code, flags};
    seat.WillProcessEvent(&key_event);
    GetEventGenerator()->Dispatch(&key_event);

    EXPECT_EQ(type != ui::EventType::kKeyReleased,
              seat.pressed_keys().count(PhysicalCode(code)));

    seat.DidProcessEvent(&key_event);
  };

  ash::DeskSwitchAnimationWaiter waiter;
  displatch_key_event(ui::EventType::kKeyPressed, ui::VKEY_MENU,
                      ui::DomCode::ALT_LEFT,
                      /*flags=*/0);
  displatch_key_event(ui::EventType::kKeyPressed, ui::VKEY_TAB,
                      ui::DomCode::TAB,
                      /*flags=*/ui::EF_ALT_DOWN);
  displatch_key_event(ui::EventType::kKeyReleased, ui::VKEY_MENU,
                      ui::DomCode::ALT_LEFT,
                      /*flags=*/0);
  displatch_key_event(ui::EventType::kKeyReleased, ui::VKEY_TAB,
                      ui::DomCode::TAB,
                      /*flags=*/0);

  EXPECT_TRUE(seat.pressed_keys().empty());
  EXPECT_EQ(desk_2, desks_controller->GetTargetActiveDesk());
  waiter.Wait();
}

TEST_F(KeyboardTest, OnKeyboardEnter) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->surface_for_testing();

  Seat seat;
  // Pressing key before Keyboard instance is created and surface has
  // received focus.
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_A);
  generator.PressKey(ui::VKEY_A, ui::EF_SHIFT_DOWN);

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(surface->window());

  // Keyboard should try to set initial focus to surface.
  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(false));
  Keyboard keyboard(std::move(delegate), &seat);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Set up expectation for the key release.
  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardEnter(
                  surface,
                  base::flat_map<PhysicalCode, base::flat_set<KeyState>>(
                      {{ui::DomCode::US_A, base::flat_set<KeyState>{
                                               {ui::DomCode::US_A, false}}}})));
  focus_client->FocusWindow(nullptr);
  focus_client->FocusWindow(surface->window());
  // Surface should maintain keyboard focus when moved to top-level window.
  focus_client->FocusWindow(surface->window()->GetToplevelWindow());

  // Release key after surface lost focus.
  focus_client->FocusWindow(nullptr);
  generator.ReleaseKey(ui::VKEY_A, ui::EF_SHIFT_DOWN);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Key should no longer be pressed when focus returns.
  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr, OnKeyboardModifiers(KeyboardModifiers{
                                 kShiftMask | kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(
      *delegate_ptr,
      OnKeyboardEnter(
          surface, base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window()->GetToplevelWindow());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, OnKeyboardLeave) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->surface_for_testing();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  Seat seat;
  auto keyboard = std::make_unique<Keyboard>(std::move(delegate), &seat);
  ON_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillByDefault(testing::Return(true));

  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(
      *delegate_ptr,
      OnKeyboardEnter(
          surface, base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  EXPECT_CALL(*delegate_ptr, OnKeyboardLeave(surface));
  focus_client->FocusWindow(nullptr);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(
      *delegate_ptr,
      OnKeyboardEnter(
          surface, base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  EXPECT_CALL(*delegate_ptr, OnKeyboardLeave(surface));
  shell_surface.reset();
  // Verify before destroying keyboard to make sure the expected call
  // is made on the methods above, rather than in the destructor.
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, OnKeyboardKeyMultipleRewrites) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->surface_for_testing();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  NiceMockKeyboardObserver observer;
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
  keyboard.AddObserver(&observer);

  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(
      *delegate_ptr,
      OnKeyboardEnter(
          surface, base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  // Test Rewriting A -> Ctrl + B
  {
    testing::InSequence s;
    // Presses:
    EXPECT_CALL(observer,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, true));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, true));
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_B, true));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_B, true));

    // Releases:
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_B, false));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_B, false));
    EXPECT_CALL(observer,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, false));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, false));
  }
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_A);
  generator.PressKey(ui::VKEY_LCONTROL,
                     ui::EF_CONTROL_DOWN | ui::EF_IS_CUSTOMIZED_FROM_BUTTON);
  generator.PressKey(ui::VKEY_B,
                     ui::EF_CONTROL_DOWN | ui::EF_IS_CUSTOMIZED_FROM_BUTTON);
  generator.ReleaseKey(ui::VKEY_B,
                       ui::EF_CONTROL_DOWN | ui::EF_IS_CUSTOMIZED_FROM_BUTTON);
  generator.ReleaseKey(ui::VKEY_LCONTROL, ui::EF_IS_CUSTOMIZED_FROM_BUTTON);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, OnKeyboardKeyMultipleRewritesReleaseAllPressed) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->surface_for_testing();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  NiceMockKeyboardObserver observer;
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
  keyboard.AddObserver(&observer);

  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(
      *delegate_ptr,
      OnKeyboardEnter(
          surface, base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  // Test Rewriting A -> Ctrl + B, release all events when we get non-matching
  // release event.
  {
    testing::InSequence s;
    // Presses:
    EXPECT_CALL(observer,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, true));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, true));
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_B, true));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_B, true));

    // Releases:
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_B, false));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_B, false));
    EXPECT_CALL(observer,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, false));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, false));
  }
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_A);
  generator.PressKey(ui::VKEY_LCONTROL,
                     ui::EF_CONTROL_DOWN | ui::EF_IS_CUSTOMIZED_FROM_BUTTON);
  generator.PressKey(ui::VKEY_B,
                     ui::EF_CONTROL_DOWN | ui::EF_IS_CUSTOMIZED_FROM_BUTTON);
  // Invalid release event, therefore we should release all pressed keys from
  // the currently held physical key.
  generator.ReleaseKey(ui::VKEY_A, 0);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, OnKeyboardKeyMultipleRewritesInvalid) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->surface_for_testing();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  NiceMockKeyboardObserver observer;
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
  keyboard.AddObserver(&observer);

  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(
      *delegate_ptr,
      OnKeyboardEnter(
          surface, base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  // Test Rewriting A -> Ctrl + B, Only ctrl events should be emitted since
  // these events are not allowlisted.
  {
    testing::InSequence s;
    // Presses:
    EXPECT_CALL(observer,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, true));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, true));
    EXPECT_CALL(observer,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, false));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, false));
  }
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_A);
  generator.PressKey(ui::VKEY_LCONTROL, ui::EF_CONTROL_DOWN);
  generator.PressKey(ui::VKEY_B, ui::EF_CONTROL_DOWN);
  generator.ReleaseKey(ui::VKEY_B, ui::EF_CONTROL_DOWN);
  generator.ReleaseKey(ui::VKEY_LCONTROL, 0);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, OnKeyboardKey) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->surface_for_testing();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  NiceMockKeyboardObserver observer;
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
  keyboard.AddObserver(&observer);

  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(
      *delegate_ptr,
      OnKeyboardEnter(
          surface, base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  // This should only generate a press event for KEY_A.
  {
    testing::InSequence s;
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_A, true));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_A, true));
  }
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_A);
  generator.PressKey(ui::VKEY_A, 0);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // This should not generate another press event for KEY_A.
  generator.PressKey(ui::VKEY_A, 0);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // This should only generate a single release event for KEY_A.
  {
    testing::InSequence s;
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_A, false));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_A, false));
  }
  generator.ReleaseKey(ui::VKEY_A, 0);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Test key event rewriting. In this case, ARROW_DOWN is rewritten to KEY_END
  // as a result of ALT being pressed.
  {
    testing::InSequence s;
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::END, true));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::END, true));
  }
  EXPECT_CALL(*delegate_ptr, OnKeyboardModifiers(KeyboardModifiers{
                                 kAltMask | kNumLockMask, 0, 0, 0}));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::ARROW_DOWN);
  generator.PressKey(ui::VKEY_END, ui::EF_ALT_DOWN);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // This should generate a release event for KEY_END as that is the key
  // associated with the key press.
  {
    testing::InSequence s;
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::END, false));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::END, false));
  }
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  generator.ReleaseKey(ui::VKEY_DOWN, 0);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Press accelerator after surface lost focus.
  EXPECT_CALL(*delegate_ptr, OnKeyboardLeave(surface));
  focus_client->FocusWindow(nullptr);
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_W);
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Key should be pressed when focus returns.
  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr, OnKeyboardModifiers(KeyboardModifiers{
                                 kControlMask | kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardEnter(
                  surface,
                  base::flat_map<PhysicalCode, base::flat_set<KeyState>>(
                      {{ui::DomCode::US_W, base::flat_set<KeyState>{
                                               {ui::DomCode::US_W, false}}}})));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Releasing accelerator when surface has focus should generate event.
  {
    testing::InSequence s;
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_W, false));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_W, false));
  }
  generator.ReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Key events should be ignored when the focused window is not an
  // exo::Surface.
  std::unique_ptr<aura::Window> window =
      ash::ChildTestWindowBuilder(shell_surface->GetWidget()->GetNativeWindow(),
                                  surface->window()->bounds())
          .Build();
  // Moving the focus away will reset the focused surface.
  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .Times(0);
  focus_client->FocusWindow(window.get());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
  EXPECT_FALSE(seat.GetFocusedSurface());
  EXPECT_FALSE(keyboard.focused_surface_for_testing());

  EXPECT_CALL(observer,
              OnKeyboardKey(testing::_, ui::DomCode::ARROW_LEFT, true))
      .Times(0);
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::ARROW_LEFT, true))
      .Times(0);
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::ARROW_LEFT);
  generator.PressKey(ui::VKEY_LEFT, 0);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  EXPECT_CALL(observer,
              OnKeyboardKey(testing::_, ui::DomCode::ARROW_LEFT, false))
      .Times(0);
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::ARROW_LEFT, false))
      .Times(0);
  generator.ReleaseKey(ui::VKEY_LEFT, 0);
  // Verify before destroying keyboard to make sure the expected call
  // is made on the methods above, rather than in the destructor.
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, OnKeyboardKey_MousePhysicalEvent) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->surface_for_testing();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  NiceMockKeyboardObserver observer;
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
  keyboard.AddObserver(&observer);

  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(
      *delegate_ptr,
      OnKeyboardEnter(
          surface, base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  // This should only generate a press event for KEY_A.
  {
    testing::InSequence s;
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_A, true));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_A, true));
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_A, false));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_A, false));
  }
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ash::mojom::CustomizableButton::kExtra);
  generator.PressKey(ui::VKEY_A, 0);
  generator.ReleaseKey(ui::VKEY_A, 0);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Test Rewriting A -> Ctrl + B
  {
    testing::InSequence s;
    // Presses:
    EXPECT_CALL(observer,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, true));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, true));
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_B, true));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_B, true));

    // Releases:
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_B, false));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_B, false));
    EXPECT_CALL(observer,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, false));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, false));
  }
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ash::mojom::CustomizableButton::kMiddle);
  generator.PressKey(ui::VKEY_LCONTROL,
                     ui::EF_CONTROL_DOWN | ui::EF_IS_CUSTOMIZED_FROM_BUTTON);
  generator.PressKey(ui::VKEY_B,
                     ui::EF_CONTROL_DOWN | ui::EF_IS_CUSTOMIZED_FROM_BUTTON);
  generator.ReleaseKey(ui::VKEY_B,
                       ui::EF_CONTROL_DOWN | ui::EF_IS_CUSTOMIZED_FROM_BUTTON);
  generator.ReleaseKey(ui::VKEY_LCONTROL, ui::EF_IS_CUSTOMIZED_FROM_BUTTON);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, OnKeyboardKey_NotSendKeyIfConsumedByIme) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->surface_for_testing();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  NiceMockKeyboardObserver observer;
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
  keyboard.AddObserver(&observer);

  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(
      *delegate_ptr,
      OnKeyboardEnter(
          surface, base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  views::Widget* widget =
      views::Widget::GetTopLevelWidgetForNativeView(surface->window());
  ui::InputMethod* input_method = widget->GetInputMethod();
  ui::DummyTextInputClient client{ui::TEXT_INPUT_TYPE_TEXT};
  input_method->SetFocusedTextInputClient(&client);

  // If a text field is focused, a pressed key event is not sent to a client
  // because a key event should be consumed by the IME.
  // However, the observer should receive OnKeyboardKey, always.
  EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_A, true));
  EXPECT_CALL(*delegate_ptr, OnKeyboardKey(testing::_, ui::DomCode::US_A, true))
      .Times(0);
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_A);

  {
    ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_A, 0);
    ui::SetKeyboardImeFlags(&event, ui::kPropertyKeyboardImeHandledFlag);
    event.set_source_device_id(0);
    generator.Dispatch(&event);
  }
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // TODO(yhanada): The below EXPECT_CALL fails because exo::Keyboard currently
  // sends a key release event for the keys which exo::Keyboard sent a pressed
  // event for. It might causes a never-ending key repeat in the client.
  // EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_A, false));
  EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_A, false));
  generator.ReleaseKey(ui::VKEY_A, 0);
  testing::Mock::VerifyAndClearExpectations(&observer);

  // Any key event should be sent to a client if a key event skips IME.
  surface->window()->SetProperty(aura::client::kSkipImeProcessing, true);
  {
    testing::InSequence s;
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_C, true));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_C, true));
  }
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_C);
  generator.PressKey(ui::VKEY_C, 0);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  {
    testing::InSequence s;
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_C, false));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_C, false));
  }
  generator.ReleaseKey(ui::VKEY_C, 0);
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  input_method->SetFocusedTextInputClient(nullptr);
}

TEST_F(KeyboardTest, OnKeyboardKey_KeyboardInhibit) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->surface_for_testing();

  // Set lacros attribute now for testing. This can be removed, when
  // all clients are migrated into this model.
  surface->window()->SetProperty(chromeos::kAppTypeKey,
                                 chromeos::AppType::LACROS);

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  // Register accelerator to be triggered.
  ui::TestAcceleratorTarget accelerator_target;
  {
    ui::Accelerator accelerator(ui::VKEY_P,
                                ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
    ash::AcceleratorControllerImpl* controller =
        ash::Shell::Get()->accelerator_controller();
    controller->Register({accelerator}, &accelerator_target);
  }

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  NiceMockKeyboardObserver observer;
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
  keyboard.AddObserver(&observer);
  keyboard.SetNeedKeyboardKeyAcks(true);

  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(
      *delegate_ptr,
      OnKeyboardEnter(
          surface, base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  // This should only generate a press event for KEY_P.
  accelerator_target.ResetCounts();
  EXPECT_CALL(observer,
              OnKeyboardKey(testing::_, ui::DomCode::US_P, testing::_))
      .Times(0);
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::US_P, testing::_))
      .Times(0);
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_P);
  generator.PressKey(ui::VKEY_P, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(1, accelerator_target.accelerator_count());
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Set keyboard-shortcut-inhibited, so the key event should be sent to app.
  surface->SetKeyboardShortcutsInhibited(true);
  accelerator_target.ResetCounts();
  {
    testing::InSequence s;
    EXPECT_CALL(observer, OnKeyboardKey(testing::_, ui::DomCode::US_P, true));
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_P, true));
  }
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_P);
  generator.PressKey(ui::VKEY_P, ui::EF_ALT_DOWN | ui::EF_SHIFT_DOWN);
  EXPECT_EQ(0, accelerator_target.accelerator_count());
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, KeyboardKey_SuppressAutoRepeat) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->surface_for_testing();

  // Set lacros attribute now for testing. This can be removed, when
  // all clients are migrated into this model.
  surface->window()->SetProperty(chromeos::kAppTypeKey,
                                 chromeos::AppType::LACROS);

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  NiceMockKeyboardObserver observer;
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
  keyboard.AddObserver(&observer);
  keyboard.SetNeedKeyboardKeyAcks(true);

  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(true));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  // Send KeyEvent annotated the auto repeat suppression.
  {
    testing::InSequence s;
    EXPECT_CALL(*delegate_ptr,
                OnKeyRepeatSettingsChanged(false, testing::_, testing::_))
        .Times(1);
    EXPECT_CALL(observer,
                OnKeyboardKey(testing::_, ui::DomCode::US_X, testing::_))
        .Times(1);
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_X, testing::_))
        .Times(1);
  }
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_X);
  {
    ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_X, 0);
    event.set_source_device_id(ui::ED_UNKNOWN_DEVICE);
    {
      ui::Event::Properties properties;
      ui::SetKeyboardImeFlagProperty(&properties,
                                     ui::kPropertyKeyboardImeIgnoredFlag);
      ui::SetKeyEventSuppressAutoRepeat(properties);
      event.SetProperties(properties);
    }
    generator.Dispatch(&event);
  }
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Following KeyEvent without the annotation will re-enable
  // auto-repeat.
  {
    testing::InSequence s;
    EXPECT_CALL(*delegate_ptr,
                OnKeyRepeatSettingsChanged(true, testing::_, testing::_))
        .Times(1);
    EXPECT_CALL(observer,
                OnKeyboardKey(testing::_, ui::DomCode::US_Y, testing::_))
        .Times(1);
    EXPECT_CALL(*delegate_ptr,
                OnKeyboardKey(testing::_, ui::DomCode::US_Y, testing::_))
        .Times(1);
  }
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_Y);
  {
    ui::KeyEvent event(ui::EventType::kKeyPressed, ui::VKEY_Y, 0);
    event.set_source_device_id(ui::ED_UNKNOWN_DEVICE);
    {
      ui::Event::Properties properties;
      ui::SetKeyboardImeFlagProperty(&properties,
                                     ui::kPropertyKeyboardImeIgnoredFlag);
      event.SetProperties(properties);
    }
    generator.Dispatch(&event);
  }
  testing::Mock::VerifyAndClearExpectations(&observer);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, FocusWithArcOverlay) {
  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  // Just allow any surface to receive focus.
  EXPECT_CALL(*delegate, CanAcceptKeyboardEventsForSurface(::testing::_))
      .WillRepeatedly(testing::Return(true));
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);

  // TODO(oshima): Create a TestExoWindowBuilder.
  class TestPropertyResolver : public exo::WMHelper::AppPropertyResolver {
   public:
    TestPropertyResolver() = default;
    ~TestPropertyResolver() override = default;
    void PopulateProperties(
        const Params& params,
        ui::PropertyHandler& out_properties_container) override {
      out_properties_container.SetProperty(chromeos::kAppTypeKey,
                                           chromeos::AppType::ARC_APP);
    }
  };
  WMHelper::GetInstance()->RegisterAppPropertyResolver(
      std::make_unique<TestPropertyResolver>());

  ash::ArcOverlayManager arc_overlay_manager_;

  auto* widget1 = ash::TestWidgetBuilder()
                      .SetBounds(gfx::Rect(200, 200))
                      .BuildOwnedByNativeWidget();
  views::Textfield* textfield1 = new views::Textfield();
  widget1->GetContentsView()->AddChildView(textfield1);
  textfield1->SetBounds(0, 0, 100, 100);

  auto* widget2 = ash::TestWidgetBuilder()
                      .SetBounds(gfx::Rect(200, 200))
                      .BuildOwnedByNativeWidget();

  auto hold = arc_overlay_manager_.RegisterHostWindow(
      "test", widget1->GetNativeWindow());

  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<ShellSurface>(surface.get());
  gfx::Size buffer_size(10, 10);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  surface->SetClientSurfaceId("billing_id:test");
  surface->Attach(buffer.get());
  surface->Commit();
  EXPECT_TRUE(shell_surface->GetWidget());

  // The overlay should have the focus when created.
  EXPECT_EQ(keyboard.focused_surface_for_testing(), surface.get());

  widget2->Activate();
  EXPECT_FALSE(keyboard.focused_surface_for_testing());

  // Activating the host widget should set the focus back to the overlay.
  widget1->Activate();
  EXPECT_EQ(keyboard.focused_surface_for_testing(), surface.get());

  constexpr char kFocusedViewClassName[] = "OverlayNativeViewHost";
  EXPECT_STREQ(kFocusedViewClassName,
               widget1->GetFocusManager()->GetFocusedView()->GetClassName());

  // Tabbing should not move the focus away from the overlay.
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  generator.PressKey(ui::VKEY_TAB, 0);

  EXPECT_STREQ(kFocusedViewClassName,
               widget1->GetFocusManager()->GetFocusedView()->GetClassName());
  EXPECT_EQ(keyboard.focused_surface_for_testing(), surface.get());

  hold.RunAndReset();
  widget1->CloseNow();
}

TEST_F(KeyboardTest, OnKeyboardModifiers) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->surface_for_testing();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);

  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(
      *delegate_ptr,
      OnKeyboardEnter(
          surface, base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  // This should generate a modifier event.
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::US_A, true));
  EXPECT_CALL(*delegate_ptr, OnKeyboardModifiers(KeyboardModifiers{
                                 kShiftMask | kNumLockMask, 0, 0, 0}));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_A);
  generator.PressKey(ui::VKEY_A, ui::EF_SHIFT_DOWN);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // This should generate another modifier event.
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::US_B, true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{
                  kShiftMask | kAltMask | kNumLockMask, 0, 0, 0}));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_B);
  generator.PressKey(ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // This should generate a third modifier event.
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::US_B, false));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  generator.ReleaseKey(ui::VKEY_B, 0);
  // Verify before destroying keyboard to make sure the expected call
  // is made on the methods above, rather than in the destructor.
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, OnKeyboardTypeChanged) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  ui::DeviceHotplugEventObserver* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  ASSERT_TRUE(device_data_manager != nullptr);
  // Make sure that DeviceDataManager has one external keyboard...
  const std::vector<ui::KeyboardDevice> keyboards{
      ui::KeyboardDevice(2, ui::InputDeviceType::INPUT_DEVICE_USB, "keyboard")};
  device_data_manager->OnKeyboardDevicesUpdated(keyboards);
  // and a touch screen.
  const std::vector<ui::TouchscreenDevice> touch_screen{
      ui::TouchscreenDevice(3, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "touch", gfx::Size(600, 400), 1)};
  device_data_manager->OnTouchscreenDevicesUpdated(touch_screen);

  ash::TabletModeControllerTestApi().EnterTabletMode();

  Seat seat;
  auto keyboard = std::make_unique<Keyboard>(
      std::make_unique<NiceMockKeyboardDelegate>(), &seat);

  MockKeyboardDeviceConfigurationDelegate configuration_delegate;

  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(true));
  keyboard->SetDeviceConfigurationDelegate(&configuration_delegate);
  EXPECT_TRUE(keyboard->HasDeviceConfigurationDelegate());
  testing::Mock::VerifyAndClearExpectations(&configuration_delegate);

  // Removing all keyboard devices in tablet mode calls
  // OnKeyboardTypeChanged() with false.
  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(false));
  device_data_manager->OnKeyboardDevicesUpdated(
      std::vector<ui::KeyboardDevice>({}));
  testing::Mock::VerifyAndClearExpectations(&configuration_delegate);

  // Re-adding keyboards calls OnKeyboardTypeChanged() with true.
  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(true));
  device_data_manager->OnKeyboardDevicesUpdated(keyboards);
  testing::Mock::VerifyAndClearExpectations(&configuration_delegate);

  keyboard.reset();

  ash::TabletModeControllerTestApi().LeaveTabletMode();
}

TEST_F(KeyboardTest, OnKeyboardTypeChanged_AccessibilityKeyboard) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  ui::DeviceHotplugEventObserver* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  ASSERT_TRUE(device_data_manager != nullptr);
  // Make sure that DeviceDataManager has one external keyboard.
  const std::vector<ui::KeyboardDevice> keyboards{
      ui::KeyboardDevice(2, ui::InputDeviceType::INPUT_DEVICE_USB, "keyboard")};
  device_data_manager->OnKeyboardDevicesUpdated(keyboards);

  Seat seat;
  Keyboard keyboard(std::make_unique<NiceMockKeyboardDelegate>(), &seat);
  MockKeyboardDeviceConfigurationDelegate configuration_delegate;

  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(true));
  keyboard.SetDeviceConfigurationDelegate(&configuration_delegate);
  EXPECT_TRUE(keyboard.HasDeviceConfigurationDelegate());
  testing::Mock::VerifyAndClearExpectations(&configuration_delegate);

  ash::AccessibilityController* accessibility_controller =
      ash::Shell::Get()->accessibility_controller();

  // Enable a11y keyboard calls OnKeyboardTypeChanged() with false.
  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(false));
  accessibility_controller->virtual_keyboard().SetEnabled(true);
  testing::Mock::VerifyAndClearExpectations(&configuration_delegate);

  // Disable a11y keyboard calls OnKeyboardTypeChanged() with true.
  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(true));
  accessibility_controller->virtual_keyboard().SetEnabled(false);
  // Verify before destroying keyboard to make sure the expected call
  // is made on the methods above, rather than in the destructor.
  testing::Mock::VerifyAndClearExpectations(&configuration_delegate);
}

constexpr base::TimeDelta kDelta50Ms = base::Milliseconds(50);
constexpr base::TimeDelta kDelta500Ms = base::Milliseconds(500);
constexpr base::TimeDelta kDelta1000Ms = base::Milliseconds(1000);

TEST_F(KeyboardTest, KeyRepeatSettingsUninitialized) {
  Seat seat;

  // Simulate unsigned login state.
  auto* keyboard_controller = ash::Shell::Get()->keyboard_controller();
  keyboard_controller->OnSigninScreenPrefServiceInitialized(nullptr);

  // If KeyboardController is not initialized with prefs,
  // no key-repeat setting event should be triggered.
  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  EXPECT_CALL(*delegate_ptr,
              OnKeyRepeatSettingsChanged(testing::_, testing::_, testing::_))
      .Times(0);
  Keyboard keyboard(std::move(delegate), &seat);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Then, when the pref is initialized, key repeat setting event should be
  // triggered.
  TestingPrefServiceSimple pref_service;
  ash::KeyboardControllerImpl::RegisterProfilePrefs(pref_service.registry(),
                                                    /*country=*/"");

  EXPECT_CALL(*delegate_ptr,
              OnKeyRepeatSettingsChanged(testing::_, testing::_, testing::_));
  keyboard_controller->OnSigninScreenPrefServiceInitialized(&pref_service);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Unset the pref_service before its destruction just for tearing down.
  keyboard_controller->OnSigninScreenPrefServiceInitialized(nullptr);
}

TEST_F(KeyboardTest, KeyRepeatSettingsLoadDefaults) {
  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  EXPECT_CALL(*delegate,
              OnKeyRepeatSettingsChanged(true, kDelta500Ms, kDelta50Ms));

  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
}

TEST_F(KeyboardTest, KeyRepeatSettingsLoadInitially) {
  std::string email = "user0@tray";
  SetUserPref(email, ash::prefs::kXkbAutoRepeatEnabled, base::Value(true));
  SetUserPref(email, ash::prefs::kXkbAutoRepeatDelay, base::Value(1000));
  SetUserPref(email, ash::prefs::kXkbAutoRepeatInterval, base::Value(1000));

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  EXPECT_CALL(*delegate_ptr,
              OnKeyRepeatSettingsChanged(true, kDelta1000Ms, kDelta1000Ms));
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
  // Verify before destroying keyboard to make sure the expected call
  // is made on the methods above, rather than in the destructor.
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, KeyRepeatSettingsUpdateAtRuntime) {
  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  // Initially load defaults.
  EXPECT_CALL(*delegate_ptr,
              OnKeyRepeatSettingsChanged(testing::_, testing::_, testing::_));
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Make sure that setting prefs triggers the corresponding delegate calls.
  const std::string email = "user0@tray";

  EXPECT_CALL(*delegate_ptr,
              OnKeyRepeatSettingsChanged(false, testing::_, testing::_));
  SetUserPref(email, ash::prefs::kXkbAutoRepeatEnabled, base::Value(false));
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  EXPECT_CALL(*delegate_ptr,
              OnKeyRepeatSettingsChanged(false, kDelta1000Ms, testing::_));
  SetUserPref(email, ash::prefs::kXkbAutoRepeatDelay, base::Value(1000));
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  EXPECT_CALL(*delegate_ptr,
              OnKeyRepeatSettingsChanged(false, kDelta1000Ms, kDelta1000Ms));
  SetUserPref(email, ash::prefs::kXkbAutoRepeatInterval, base::Value(1000));
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, KeyRepeatSettingsIgnoredForNonActiveUser) {
  // Simulate two users, with the first user as active.
  CreateUserSessions(2);

  // Key repeat settings should be sent exactly once, for the default values.
  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  EXPECT_CALL(*delegate_ptr,
              OnKeyRepeatSettingsChanged(true, kDelta500Ms, kDelta50Ms));
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Set prefs for non-active user; no calls should result.
  EXPECT_CALL(*delegate_ptr,
              OnKeyRepeatSettingsChanged(testing::_, testing::_, testing::_))
      .Times(0);
  const std::string email = "user1@tray";
  SetUserPref(email, ash::prefs::kXkbAutoRepeatEnabled, base::Value(true));
  SetUserPref(email, ash::prefs::kXkbAutoRepeatDelay, base::Value(1000));
  SetUserPref(email, ash::prefs::kXkbAutoRepeatInterval, base::Value(1000));
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, KeyRepeatSettingsUpdateOnProfileChange) {
  // Simulate two users, with the first user as active.
  CreateUserSessions(2);

  // Second user has different preferences.
  std::string email = "user1@tray";
  SetUserPref(email, ash::prefs::kXkbAutoRepeatEnabled, base::Value(true));
  SetUserPref(email, ash::prefs::kXkbAutoRepeatDelay, base::Value(1000));
  SetUserPref(email, ash::prefs::kXkbAutoRepeatInterval, base::Value(1000));

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  // Initially, load default prefs for first user.
  EXPECT_CALL(*delegate_ptr,
              OnKeyRepeatSettingsChanged(true, kDelta500Ms, kDelta50Ms));
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Switching user should load new prefs.
  EXPECT_CALL(*delegate_ptr,
              OnKeyRepeatSettingsChanged(true, kDelta1000Ms, kDelta1000Ms));
  SimulateUserLogin(email, user_manager::UserType::kRegular);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, KeyboardLayout) {
  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  // Initially, update to the current keyboard layout.
  EXPECT_CALL(*delegate_ptr, OnKeyboardLayoutUpdated(testing::_));
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Updating the keyboard layout should trigger the delegate call.
  EXPECT_CALL(*delegate_ptr, OnKeyboardLayoutUpdated(testing::_));
  keyboard.OnKeyboardLayoutNameChanged("ja-jp");
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, KeyboardObserver) {
  // Declare before the keyboard so the mock verification happens
  // after the keyboard destruction.
  MockKeyboardObserver observer1;
  MockKeyboardObserver observer2;

  Seat seat;
  Keyboard keyboard(std::make_unique<NiceMockKeyboardDelegate>(), &seat);

  keyboard.AddObserver(&observer1);
  keyboard.AddObserver(&observer2);
  EXPECT_TRUE(keyboard.HasObserver(&observer1));
  EXPECT_TRUE(keyboard.HasObserver(&observer2));
  testing::Mock::VerifyAndClearExpectations(&observer1);
  testing::Mock::VerifyAndClearExpectations(&observer2);

  keyboard.RemoveObserver(&observer1);
  EXPECT_FALSE(keyboard.HasObserver(&observer1));
  EXPECT_TRUE(keyboard.HasObserver(&observer2));
  testing::Mock::VerifyAndClearExpectations(&observer1);
  testing::Mock::VerifyAndClearExpectations(&observer2);

  // Called from the destructor of Keyboard.
  EXPECT_CALL(observer1, OnKeyboardDestroying(&keyboard)).Times(0);
  EXPECT_CALL(observer2, OnKeyboardDestroying(&keyboard));
}

TEST_F(KeyboardTest, NeedKeyboardKeyAcks) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  Seat seat;
  Keyboard keyboard(std::make_unique<NiceMockKeyboardDelegate>(), &seat);

  EXPECT_FALSE(keyboard.AreKeyboardKeyAcksNeeded());
  keyboard.SetNeedKeyboardKeyAcks(true);
  EXPECT_TRUE(keyboard.AreKeyboardKeyAcksNeeded());
  keyboard.SetNeedKeyboardKeyAcks(false);
  EXPECT_FALSE(keyboard.AreKeyboardKeyAcksNeeded());
}

TEST_F(KeyboardTest, AckKeyboardKey) {
  auto surface = std::make_unique<Surface>();
  auto shell_surface = std::make_unique<TestShellSurface>(surface.get());
  gfx::Size buffer_size(10, 10);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  surface->Attach(buffer.get());
  surface->Commit();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardEnter(
                  surface.get(),
                  base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // If we don't set NeedKeyboardAckKeys to true, accelerators are always passed
  // to ShellSurface.
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  // Press KEY_W with Ctrl.
  EXPECT_CALL(*shell_surface.get(), AcceleratorPressed(ui::Accelerator(
                                        ui::VKEY_W, ui::EF_CONTROL_DOWN,
                                        ui::Accelerator::KeyState::PRESSED)))
      .WillOnce(testing::Return(true));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_W);
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);

  // Release KEY_W.
  generator.ReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
  testing::Mock::VerifyAndClearExpectations(shell_surface.get());

  // If we set NeedKeyboardAckKeys to true, only unhandled accelerators are
  // passed to ShellSurface.
  keyboard.SetNeedKeyboardKeyAcks(true);

  // Press KEY_W with Ctrl.
  EXPECT_CALL(*delegate_ptr, OnKeyboardKey(testing::_, ui::DomCode::US_W, true))
      .WillOnce(testing::Return(1));
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Send ack for the key press.
  EXPECT_CALL(*shell_surface.get(), AcceleratorPressed(ui::Accelerator(
                                        ui::VKEY_W, ui::EF_CONTROL_DOWN,
                                        ui::Accelerator::KeyState::PRESSED)))
      .WillOnce(testing::Return(true));
  keyboard.AckKeyboardKey(1, false /* handled */);
  testing::Mock::VerifyAndClearExpectations(shell_surface.get());

  // Release KEY_W.
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::US_W, false))
      .WillOnce(testing::Return(2));
  generator.ReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Send ack for the key release.
  keyboard.AckKeyboardKey(2, false /* handled */);

  // Press KEY_W with Ctrl again.
  EXPECT_CALL(*delegate_ptr, OnKeyboardKey(testing::_, ui::DomCode::US_W, true))
      .WillOnce(testing::Return(3));
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Send ack for the key press.
  // AcceleratorPressed is not called when the accelerator is already handled.
  keyboard.AckKeyboardKey(3, true /* handled */);

  // A repeat key event should not be sent to the client and also should not
  // invoke the accelerator.
  EXPECT_CALL(*shell_surface.get(), AcceleratorPressed(ui::Accelerator(
                                        ui::VKEY_W, ui::EF_CONTROL_DOWN,
                                        ui::Accelerator::KeyState::PRESSED)))
      .Times(0);
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN | ui::EF_IS_REPEAT);
  testing::Mock::VerifyAndClearExpectations(shell_surface.get());

  // Another key press event while holding the key is also ignored and should
  // not invoke the accelerator.
  EXPECT_CALL(*shell_surface.get(), AcceleratorPressed(ui::Accelerator(
                                        ui::VKEY_W, ui::EF_CONTROL_DOWN,
                                        ui::Accelerator::KeyState::PRESSED)))
      .Times(0);
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(shell_surface.get());

  // Release the key and reset modifier_flags.
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::US_W, false));
  generator.ReleaseKey(ui::VKEY_W, 0);
  // Verify before destroying keyboard to make sure the expected call
  // is made on the methods above, rather than in the destructor.
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, AckKeyboardKeyMoveFocus) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->surface_for_testing();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);

  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(
      *delegate_ptr,
      OnKeyboardEnter(
          surface, base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  keyboard.SetNeedKeyboardKeyAcks(true);

  // Press KEY_W with Ctrl.
  EXPECT_CALL(*delegate_ptr, OnKeyboardModifiers(KeyboardModifiers{
                                 kControlMask | kNumLockMask, 0, 0, 0}))
      .Times(1);
  EXPECT_CALL(*delegate_ptr, OnKeyboardKey(testing::_, ui::DomCode::US_W, true))
      .WillOnce(testing::Return(1));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_W);
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Move focus from the window
  EXPECT_CALL(*delegate_ptr, OnKeyboardLeave(surface));
  focus_client->FocusWindow(nullptr);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Send ack for the key press. |AcceleratorPressed()| should not be called.
  keyboard.AckKeyboardKey(1, false /* handled */);
}

TEST_F(KeyboardTest, AckKeyboardKeyExpired) {
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface = std::make_unique<TestShellSurface>(surface.get());
  gfx::Size buffer_size(10, 10);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  surface->Attach(buffer.get());
  surface->Commit();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);

  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardEnter(
                  surface.get(),
                  base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  keyboard.SetNeedKeyboardKeyAcks(true);

  // Press KEY_W with Ctrl.
  EXPECT_CALL(*delegate_ptr, OnKeyboardModifiers(KeyboardModifiers{
                                 kControlMask | kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(*delegate_ptr, OnKeyboardKey(testing::_, ui::DomCode::US_W, true))
      .WillOnce(testing::Return(1));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_W);
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Keyboard processes pending events as if it is handled when it expires,
  // so |AcceleratorPressed()| should not be called.
  EXPECT_CALL(*shell_surface.get(), AcceleratorPressed(ui::Accelerator(
                                        ui::VKEY_W, ui::EF_CONTROL_DOWN,
                                        ui::Accelerator::KeyState::PRESSED)))
      .Times(0);

  // Wait until |ProcessExpiredPendingKeyAcks| is fired.
  task_environment()->FastForwardBy(base::Milliseconds(1000));

  // Send ack for the key press as if it was not handled. In the normal case,
  // |AcceleratorPressed()| should be called, but since the timeout passed, the
  // key should have been treated as handled already and removed from the
  // pending_key_acks_ map. Since the event is no longer in the map,
  // |AcceleratorPressed()| should not be called.
  keyboard.AckKeyboardKey(1, false /* handled */);

  // Release the key and reset modifier_flags.
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::US_W, false));
  generator.ReleaseKey(ui::VKEY_W, 0);
  // Verify before destroying keyboard to make sure the expected call
  // is made on the methods above, rather than in the destructor.
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, AckKeyboardKeyAcceleratorOnRelease) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->surface_for_testing();

  // Set lacros attribute now for testing. This can be removed, when
  // all clients are migrated into this model.
  surface->window()->SetProperty(chromeos::kAppTypeKey,
                                 chromeos::AppType::LACROS);

  // Register accelerator to be triggered.
  ui::TestAcceleratorTarget accelerator_target;
  {
    ui::Accelerator accelerator(ui::VKEY_CONTROL, 0,
                                ui::Accelerator::KeyState::RELEASED);
    ash::AcceleratorControllerImpl* controller =
        ash::Shell::Get()->accelerator_controller();
    controller->Register({accelerator}, &accelerator_target);
  }

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);

  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(
      *delegate_ptr,
      OnKeyboardEnter(
          surface, base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  keyboard.SetNeedKeyboardKeyAcks(true);

  // Press CONTROL key.
  EXPECT_CALL(*delegate_ptr, OnKeyboardModifiers(KeyboardModifiers{
                                 kControlMask | kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, true))
      .WillOnce(testing::Return(1));

  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::CONTROL_LEFT);
  generator.PressKey(ui::VKEY_CONTROL, ui::EF_CONTROL_DOWN);
  // SEARCH key can be used as a modifier, so it is handled in release event.
  // Thus accelerator handler should not be triggered.
  EXPECT_EQ(0, accelerator_target.accelerator_count());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Send ack for the key press as if it was not handled.
  keyboard.AckKeyboardKey(1, false /* handled */);

  // Wait until |ProcessExpiredPendingKeyAcks| is fired.
  task_environment()->FastForwardBy(base::Milliseconds(1000));

  // Release the key and reset modifier_flags.
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::CONTROL_LEFT, false))
      .WillOnce(testing::Return(2));
  generator.ReleaseKey(ui::VKEY_CONTROL, 0);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
  // Now the accelerator should be handled.
  EXPECT_EQ(1, accelerator_target.accelerator_count());

  // Then, on ack key, even if application does not process the key event,
  // accelerator key should not be handled (because it is already done).
  keyboard.AckKeyboardKey(2, false /* handled */);
  EXPECT_EQ(1, accelerator_target.accelerator_count());

  // Verify before destroying keyboard to make sure the expected call
  // is made on the methods above, rather than in the destructor.
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

// Test for crbug.com/753539. If action for an accelerator moves the focus to
// another window, it causes clearing the map of pending key acks in Keyboard.
// We can't assume that an iterator of the map is valid after processing an
// accelerator.
class TestShellSurfaceWithMovingFocusAccelerator : public ShellSurface {
 public:
  explicit TestShellSurfaceWithMovingFocusAccelerator(Surface* surface)
      : ShellSurface(surface) {}

  bool AcceleratorPressed(const ui::Accelerator& accelerator) override {
    aura::client::FocusClient* focus_client =
        aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
    focus_client->FocusWindow(nullptr);
    return true;
  }
};

TEST_F(KeyboardTest, AckKeyboardKeyExpiredWithMovingFocusAccelerator) {
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface =
      std::make_unique<TestShellSurfaceWithMovingFocusAccelerator>(
          surface.get());
  gfx::Size buffer_size(10, 10);
  auto buffer = test::ExoTestHelper::CreateBuffer(buffer_size);
  surface->Attach(buffer.get());
  surface->Commit();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);

  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardEnter(
                  surface.get(),
                  base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  keyboard.SetNeedKeyboardKeyAcks(true);

  // Press KEY_W with Ctrl.
  EXPECT_CALL(*delegate_ptr, OnKeyboardModifiers(KeyboardModifiers{
                                 kControlMask | kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(*delegate_ptr, OnKeyboardKey(testing::_, ui::DomCode::US_W, true))
      .WillOnce(testing::Return(1));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_W);
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  EXPECT_CALL(*delegate_ptr, OnKeyboardLeave(surface.get()));

  // Send ack as unhandled. This will call |AcceleratorPressed| and move the
  // focus.
  keyboard.AckKeyboardKey(1, false /* handled */);

  // Wait until |ProcessExpiredPendingKeyAcks| is fired.
  task_environment()->FastForwardBy(base::Milliseconds(1000));

  // Verify before destroying keyboard to make sure the expected call
  // is made on the methods above, rather than in the destructor.
  testing::Mock::VerifyAndClearExpectations(&delegate);
}

TEST_F(KeyboardTest, OnKeyboardKey_ChangeFocusInPreTargetHandler) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->surface_for_testing();
  auto normal_window = CreateAppWindow(gfx::Rect(0, 0, 100, 100));
  TestEventHandler handler{shell_surface->GetWidget()->GetNativeView()};

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  NiceMockKeyboardObserver observer;
  Seat seat;
  Keyboard keyboard(std::move(delegate), &seat);
  keyboard.AddObserver(&observer);

  // Focus the non-exo window.
  focus_client->FocusWindow(normal_window.get());

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());

  // Keyboard should not get a key event sent to the non-exo window.
  generator.PressKey(ui::VKEY_A, 0);
  generator.ReleaseKey(ui::VKEY_A, 0);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Sending a key event causes a focus change.
  // It calls OnKeyboardEnter, but OnKeyboardKey should not be called because
  // the event's target is |normal_window|.
  wm_helper()->AddPreTargetHandler(&handler);

  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(
      *delegate_ptr,
      OnKeyboardEnter(
          surface, base::flat_map<PhysicalCode, base::flat_set<KeyState>>()));

  generator.PressKey(ui::VKEY_A, 0);
  EXPECT_EQ(shell_surface->GetWidget()->GetNativeView(),
            focus_client->GetFocusedWindow());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  wm_helper()->RemovePreTargetHandler(&handler);
}

TEST_F(KeyboardTest, SystemKeysNotSentAsPressedKeys) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  Seat seat;

  // Pressing keys before Keyboard instance is created and surface has
  // received focus.
  ui::test::EventGenerator* generator = GetEventGenerator();
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_A);
  generator->PressKey(ui::VKEY_A, ui::EF_SHIFT_DOWN);
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::LAUNCH_APP1);
  generator->PressKey(ui::VKEY_MEDIA_LAUNCH_APP1, 0);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  auto keyboard = std::make_unique<Keyboard>(std::move(delegate), &seat);
  ON_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillByDefault(testing::Return(true));

  // LAUNCH_APP1 should be filtered out before sending OnKeyboardEnter.
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardEnter(
                  surface,
                  base::flat_map<PhysicalCode, base::flat_set<KeyState>>(
                      {{ui::DomCode::US_A, base::flat_set<KeyState>{
                                               {ui::DomCode::US_A, false}}}})));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, CanConsumeSystemKeysSentAsPressedKeys) {
  auto shell_surface = test::ShellSurfaceBuilder({10, 10}).BuildShellSurface();
  auto* surface = shell_surface->root_surface();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);
  ash::WindowState::Get(surface->window()->GetToplevelWindow())
      ->SetCanConsumeSystemKeys(true);
  Seat seat;

  // Pressing keys before Keyboard instance is created and surface has
  // received focus.
  ui::test::EventGenerator* generator = GetEventGenerator();
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_A);
  generator->PressKey(ui::VKEY_A, ui::EF_SHIFT_DOWN);
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::LAUNCH_APP1);
  generator->PressKey(ui::VKEY_MEDIA_LAUNCH_APP1, 0);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  auto keyboard = std::make_unique<Keyboard>(std::move(delegate), &seat);
  ON_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface))
      .WillByDefault(testing::Return(true));

  // LAUNCH_APP1 should not be filtered out before sending OnKeyboardEnter.
  EXPECT_CALL(
      *delegate_ptr,
      OnKeyboardEnter(
          surface,
          base::flat_map<PhysicalCode, base::flat_set<KeyState>>({
              {ui::DomCode::US_A,
               base::flat_set<KeyState>{{ui::DomCode::US_A, false}}},
              {ui::DomCode::LAUNCH_APP1,
               base::flat_set<KeyState>{{ui::DomCode::LAUNCH_APP1, false}}},
          })));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}
}  // namespace
}  // namespace exo
