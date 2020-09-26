// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/keyboard.h"

#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/public/cpp/ash_pref_names.h"
#include "ash/shell.h"
#include "ash/test/ash_test_helper.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/desks_test_util.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/macros.h"
#include "base/run_loop.h"
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
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/client/focus_client.h"
#include "ui/base/ime/dummy_text_input_client.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/test/event_generator.h"
#include "ui/events/types/event_type.h"

namespace exo {
namespace {

// XKB mod masks for the default keymap.
constexpr uint32_t kShiftMask = 1 << 0;
constexpr uint32_t kControlMask = 1 << 2;
constexpr uint32_t kAltMask = 1 << 3;
constexpr uint32_t kNumLockMask = 1 << 4;

using KeyboardTest = test::ExoTestBase;

class MockKeyboardDelegate : public KeyboardDelegate {
 public:
  MockKeyboardDelegate() = default;

  // Overridden from KeyboardDelegate:
  MOCK_METHOD(bool, CanAcceptKeyboardEventsForSurface, (Surface*), (const));
  MOCK_METHOD(void,
              OnKeyboardEnter,
              (Surface*, (const base::flat_map<ui::DomCode, ui::DomCode>&)));
  MOCK_METHOD(void, OnKeyboardLeave, (Surface*));
  MOCK_METHOD(uint32_t, OnKeyboardKey, (base::TimeTicks, ui::DomCode, bool));
  MOCK_METHOD(void, OnKeyboardModifiers, (const KeyboardModifiers&));
  MOCK_METHOD(void,
              OnKeyRepeatSettingsChanged,
              (bool, base::TimeDelta, base::TimeDelta));
  MOCK_METHOD(void, OnKeyboardLayoutUpdated, (base::StringPiece));
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
};

class TestShellSurface : public ShellSurface {
 public:
  explicit TestShellSurface(Surface* surface) : ShellSurface(surface) {}

  MOCK_METHOD(bool, AcceleratorPressed, (const ui::Accelerator& accelerator));
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
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

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

    EXPECT_EQ(type != ui::ET_KEY_RELEASED, seat.pressed_keys().count(code));

    seat.DidProcessEvent(&key_event);
  };

  ash::DeskSwitchAnimationWaiter waiter;
  displatch_key_event(ui::ET_KEY_PRESSED, ui::VKEY_MENU, ui::DomCode::ALT_LEFT,
                      /*flags=*/0);
  displatch_key_event(ui::ET_KEY_PRESSED, ui::VKEY_TAB, ui::DomCode::TAB,
                      /*flags=*/ui::EF_ALT_DOWN);
  displatch_key_event(ui::ET_KEY_RELEASED, ui::VKEY_MENU, ui::DomCode::ALT_LEFT,
                      /*flags=*/0);
  displatch_key_event(ui::ET_KEY_RELEASED, ui::VKEY_TAB, ui::DomCode::TAB,
                      /*flags=*/0);

  EXPECT_TRUE(seat.pressed_keys().empty());
  EXPECT_EQ(desk_2, desks_controller->GetTargetActiveDesk());
  waiter.Wait();
}

TEST_F(KeyboardTest, OnKeyboardEnter) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

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
  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(false));
  Keyboard keyboard(std::move(delegate), &seat);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Set up expectation for the key release.
  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr, OnKeyboardModifiers(KeyboardModifiers{
                                 kShiftMask | kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>(
                                  {{ui::DomCode::US_A, ui::DomCode::US_A}})));
  focus_client->FocusWindow(nullptr);
  focus_client->FocusWindow(surface->window());
  // Surface should maintain keyboard focus when moved to top-level window.
  focus_client->FocusWindow(surface->window()->GetToplevelWindow());

  // Release key after surface lost focus.
  focus_client->FocusWindow(nullptr);
  generator.ReleaseKey(ui::VKEY_A, ui::EF_SHIFT_DOWN);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Key should no longer be pressed when focus returns.
  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr, OnKeyboardModifiers(KeyboardModifiers{
                                 kShiftMask | kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
  focus_client->FocusWindow(surface->window()->GetToplevelWindow());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, OnKeyboardLeave) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  auto delegate = std::make_unique<NiceMockKeyboardDelegate>();
  auto* delegate_ptr = delegate.get();
  Seat seat;
  auto keyboard = std::make_unique<Keyboard>(std::move(delegate), &seat);
  ON_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillByDefault(testing::Return(true));

  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  EXPECT_CALL(*delegate_ptr, OnKeyboardLeave(surface.get()));
  focus_client->FocusWindow(nullptr);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  EXPECT_CALL(*delegate_ptr, OnKeyboardLeave(surface.get()));
  shell_surface.reset();
  surface.reset();
  // Verify before destroying keyboard to make sure the expected call
  // is made on the methods above, rather than in the destructor.
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, OnKeyboardKey) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
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
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  // This should only generate a press event for KEY_A.
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::US_A, true));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_A);
  generator.PressKey(ui::VKEY_A, 0);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // This should not generate another press event for KEY_A.
  generator.PressKey(ui::VKEY_A, 0);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // This should only generate a single release event for KEY_A.
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::US_A, false));
  generator.ReleaseKey(ui::VKEY_A, 0);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Test key event rewriting. In this case, ARROW_DOWN is rewritten to KEY_END
  // as a result of ALT being pressed.
  EXPECT_CALL(*delegate_ptr, OnKeyboardKey(testing::_, ui::DomCode::END, true));
  EXPECT_CALL(*delegate_ptr, OnKeyboardModifiers(KeyboardModifiers{
                                 kAltMask | kNumLockMask, 0, 0, 0}));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::ARROW_DOWN);
  generator.PressKey(ui::VKEY_END, ui::EF_ALT_DOWN);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // This should generate a release event for KEY_END as that is the key
  // associated with the key press.
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::END, false));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardModifiers(KeyboardModifiers{kNumLockMask, 0, 0, 0}));
  generator.ReleaseKey(ui::VKEY_DOWN, 0);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Press accelerator after surface lost focus.
  EXPECT_CALL(*delegate_ptr, OnKeyboardLeave(surface.get()));
  focus_client->FocusWindow(nullptr);
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_W);
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Key should be pressed when focus returns.
  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(*delegate_ptr, OnKeyboardModifiers(KeyboardModifiers{
                                 kControlMask | kNumLockMask, 0, 0, 0}));
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>(
                                  {{ui::DomCode::US_W, ui::DomCode::US_W}})));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Releasing accelerator when surface has focus should generate event.
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::US_W, false));
  generator.ReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Key events should be ignored when the focused window is not an
  // exo::Surface.
  auto window = CreateChildWindow(shell_surface->GetWidget()->GetNativeWindow(),
                                  gfx::Rect(buffer_size));
  // Moving the focus away will trigger the fallback path in GetEffectiveFocus.
  // TODO(oshima): Consider removing the fallback path.
  EXPECT_CALL(*delegate_ptr, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  focus_client->FocusWindow(window.get());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::ARROW_LEFT, true))
      .Times(0);
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::ARROW_LEFT);
  generator.PressKey(ui::VKEY_LEFT, 0);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::ARROW_LEFT, false))
      .Times(0);
  generator.ReleaseKey(ui::VKEY_LEFT, 0);
  // Verify before destroying keyboard to make sure the expected call
  // is made on the methods above, rather than in the destructor.
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);
}

TEST_F(KeyboardTest, OnKeyboardKey_NotSendKeyIfConsumedByIme) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
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
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
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
  EXPECT_CALL(*delegate_ptr, OnKeyboardKey(testing::_, ui::DomCode::US_A, true))
      .Times(0);
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_A);
  generator.PressKey(ui::VKEY_A, 0);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // TODO(yhanada): The below EXPECT_CALL fails because exo::Keyboard currently
  // sends a key release event for the keys which exo::Keyboard sent a pressed
  // event for. It might causes a never-ending key repeat in the client.
  // EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_A, false));
  generator.ReleaseKey(ui::VKEY_A, 0);

  // Any key event should be sent to a client if the focused window is marked as
  // ImeBlocking.
  WMHelper::GetInstance()->SetImeBlocked(surface->window()->GetToplevelWindow(),
                                         true);
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::US_B, true));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_B);
  generator.PressKey(ui::VKEY_B, 0);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::US_B, false));
  generator.ReleaseKey(ui::VKEY_B, 0);
  WMHelper::GetInstance()->SetImeBlocked(surface->window()->GetToplevelWindow(),
                                         false);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Any key event should be sent to a client if a key event skips IME.
  surface->window()->SetProperty(aura::client::kSkipImeProcessing, true);
  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::US_C, true));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_C);
  generator.PressKey(ui::VKEY_C, 0);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  EXPECT_CALL(*delegate_ptr,
              OnKeyboardKey(testing::_, ui::DomCode::US_C, false));
  generator.ReleaseKey(ui::VKEY_C, 0);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  input_method->SetFocusedTextInputClient(nullptr);
}

TEST_F(KeyboardTest, OnKeyboardModifiers) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
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
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
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
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  ui::DeviceHotplugEventObserver* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  ASSERT_TRUE(device_data_manager != nullptr);
  // Make sure that DeviceDataManager has one external keyboard...
  const std::vector<ui::InputDevice> keyboards{
      ui::InputDevice(2, ui::InputDeviceType::INPUT_DEVICE_USB, "keyboard")};
  device_data_manager->OnKeyboardDevicesUpdated(keyboards);
  // and a touch screen.
  const std::vector<ui::TouchscreenDevice> touch_screen{
      ui::TouchscreenDevice(3, ui::InputDeviceType::INPUT_DEVICE_INTERNAL,
                            "touch", gfx::Size(600, 400), 1)};
  device_data_manager->OnTouchscreenDevicesUpdated(touch_screen);

  ash::TabletModeController* tablet_mode_controller =
      ash::Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->SetEnabledForTest(true);

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
      std::vector<ui::InputDevice>({}));
  testing::Mock::VerifyAndClearExpectations(&configuration_delegate);

  // Re-adding keyboards calls OnKeyboardTypeChanged() with true.
  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(true));
  device_data_manager->OnKeyboardDevicesUpdated(keyboards);
  testing::Mock::VerifyAndClearExpectations(&configuration_delegate);

  keyboard.reset();

  tablet_mode_controller->SetEnabledForTest(false);
}

TEST_F(KeyboardTest, OnKeyboardTypeChanged_AccessibilityKeyboard) {
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

  aura::client::FocusClient* focus_client =
      aura::client::GetFocusClient(ash::Shell::GetPrimaryRootWindow());
  focus_client->FocusWindow(nullptr);

  ui::DeviceHotplugEventObserver* device_data_manager =
      ui::DeviceDataManager::GetInstance();
  ASSERT_TRUE(device_data_manager != nullptr);
  // Make sure that DeviceDataManager has one external keyboard.
  const std::vector<ui::InputDevice> keyboards{
      ui::InputDevice(2, ui::InputDeviceType::INPUT_DEVICE_USB, "keyboard")};
  device_data_manager->OnKeyboardDevicesUpdated(keyboards);

  Seat seat;
  Keyboard keyboard(std::make_unique<NiceMockKeyboardDelegate>(), &seat);
  MockKeyboardDeviceConfigurationDelegate configuration_delegate;

  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(true));
  keyboard.SetDeviceConfigurationDelegate(&configuration_delegate);
  EXPECT_TRUE(keyboard.HasDeviceConfigurationDelegate());
  testing::Mock::VerifyAndClearExpectations(&configuration_delegate);

  ash::AccessibilityControllerImpl* accessibility_controller =
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

constexpr base::TimeDelta kDelta50Ms = base::TimeDelta::FromMilliseconds(50);
constexpr base::TimeDelta kDelta500Ms = base::TimeDelta::FromMilliseconds(500);
constexpr base::TimeDelta kDelta1000Ms =
    base::TimeDelta::FromMilliseconds(1000);

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
  SimulateUserLogin(email, user_manager::UserType::USER_TYPE_REGULAR);
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
  std::unique_ptr<Surface> surface(new Surface);
  std::unique_ptr<ShellSurface> shell_surface(new ShellSurface(surface.get()));
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
  surface->Attach(buffer.get());
  surface->Commit();

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
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface = std::make_unique<TestShellSurface>(surface.get());
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
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
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
  focus_client->FocusWindow(surface->window());
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // If we don't set NeedKeyboardAckKeys to true, accelerators are always passed
  // to ShellSurface.
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  // Press KEY_W with Ctrl.
  EXPECT_CALL(*delegate_ptr, OnKeyboardModifiers(KeyboardModifiers{
                                 kControlMask | kNumLockMask, 0, 0, 0}));
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
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface = std::make_unique<TestShellSurface>(surface.get());
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
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
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
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
  EXPECT_CALL(*delegate_ptr, OnKeyboardLeave(surface.get()));
  focus_client->FocusWindow(nullptr);
  testing::Mock::VerifyAndClearExpectations(delegate_ptr);

  // Send ack for the key press. |AcceleratorPressed()| should not be called.
  keyboard.AckKeyboardKey(1, false /* handled */);
}

TEST_F(KeyboardTest, AckKeyboardKeyExpired) {
  std::unique_ptr<Surface> surface(new Surface);
  auto shell_surface = std::make_unique<TestShellSurface>(surface.get());
  gfx::Size buffer_size(10, 10);
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
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
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
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
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(1000));
  run_loop.Run();
  base::RunLoop().RunUntilIdle();

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
  std::unique_ptr<Buffer> buffer(
      new Buffer(exo_test_helper()->CreateGpuMemoryBuffer(buffer_size)));
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
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
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
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(1000));
  run_loop.Run();
  base::RunLoop().RunUntilIdle();

  // Verify before destroying keyboard to make sure the expected call
  // is made on the methods above, rather than in the destructor.
  testing::Mock::VerifyAndClearExpectations(&delegate);
}
}  // namespace
}  // namespace exo
