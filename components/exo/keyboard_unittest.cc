// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/keyboard.h"

#include "ash/accessibility/accessibility_controller.h"
#include "ash/shell.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "components/exo/buffer.h"
#include "components/exo/keyboard_delegate.h"
#include "components/exo/keyboard_device_configuration_delegate.h"
#include "components/exo/keyboard_observer.h"
#include "components/exo/seat.h"
#include "components/exo/shell_surface.h"
#include "components/exo/surface.h"
#include "components/exo/test/exo_test_base.h"
#include "components/exo/test/exo_test_helper.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/aura/client/focus_client.h"
#include "ui/events/devices/device_data_manager.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/test/event_generator.h"

namespace exo {
namespace {

using KeyboardTest = test::ExoTestBase;

class MockKeyboardDelegate : public KeyboardDelegate {
 public:
  MockKeyboardDelegate() {}

  // Overridden from KeyboardDelegate:
  MOCK_METHOD1(OnKeyboardDestroying, void(Keyboard*));
  MOCK_CONST_METHOD1(CanAcceptKeyboardEventsForSurface, bool(Surface*));
  MOCK_METHOD2(OnKeyboardEnter,
               void(Surface*, const base::flat_map<ui::DomCode, ui::DomCode>&));
  MOCK_METHOD1(OnKeyboardLeave, void(Surface*));
  MOCK_METHOD3(OnKeyboardKey, uint32_t(base::TimeTicks, ui::DomCode, bool));
  MOCK_METHOD1(OnKeyboardModifiers, void(int));
};

class MockKeyboardDeviceConfigurationDelegate
    : public KeyboardDeviceConfigurationDelegate {
 public:
  MockKeyboardDeviceConfigurationDelegate() {}

  // Overridden from KeyboardDeviceConfigurationDelegate:
  MOCK_METHOD1(OnKeyboardDestroying, void(Keyboard*));
  MOCK_METHOD1(OnKeyboardTypeChanged, void(bool));
};

class MockKeyboardObserver : public KeyboardObserver {
 public:
  MockKeyboardObserver() {}

  // Overridden from KeyboardObserver:
  MOCK_METHOD1(OnKeyboardDestroying, void(Keyboard*));
};

class TestShellSurface : public ShellSurface {
 public:
  explicit TestShellSurface(Surface* surface) : ShellSurface(surface) {}

  MOCK_METHOD1(AcceleratorPressed, bool(const ui::Accelerator& accelerator));
};

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
  MockKeyboardDelegate delegate;
  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(false));
  auto keyboard = std::make_unique<Keyboard>(&delegate, &seat);

  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(ui::EF_SHIFT_DOWN));
  EXPECT_CALL(delegate,
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

  // Key should no longer be pressed when focus returns.
  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(ui::EF_SHIFT_DOWN));
  EXPECT_CALL(delegate,
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
  focus_client->FocusWindow(surface->window()->GetToplevelWindow());

  keyboard.reset();
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

  MockKeyboardDelegate delegate;
  Seat seat;
  auto keyboard = std::make_unique<Keyboard>(&delegate, &seat);

  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillRepeatedly(testing::Return(true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  EXPECT_CALL(delegate,
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
  focus_client->FocusWindow(surface->window());

  EXPECT_CALL(delegate, OnKeyboardLeave(surface.get()));
  focus_client->FocusWindow(nullptr);

  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  EXPECT_CALL(delegate,
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
  focus_client->FocusWindow(surface->window());

  EXPECT_CALL(delegate, OnKeyboardLeave(surface.get()));
  shell_surface.reset();
  surface.reset();

  keyboard.reset();
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

  MockKeyboardDelegate delegate;
  Seat seat;
  auto keyboard = std::make_unique<Keyboard>(&delegate, &seat);

  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  EXPECT_CALL(delegate,
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
  focus_client->FocusWindow(surface->window());

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  // This should only generate a press event for KEY_A.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_A, true));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_A);
  generator.PressKey(ui::VKEY_A, 0);

  // This should not generate another press event for KEY_A.
  generator.PressKey(ui::VKEY_A, 0);

  // This should only generate a single release event for KEY_A.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_A, false));
  generator.ReleaseKey(ui::VKEY_A, 0);

  // Test key event rewriting. In this case, ARROW_DOWN is rewritten to KEY_END
  // as a result of ALT being pressed.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::END, true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(ui::EF_ALT_DOWN));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::ARROW_DOWN);
  generator.PressKey(ui::VKEY_END, ui::EF_ALT_DOWN);

  // This should generate a release event for KEY_END as that is the key
  // associated with the key press.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::END, false));
  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  generator.ReleaseKey(ui::VKEY_DOWN, 0);

  // Press accelerator after surface lost focus.
  EXPECT_CALL(delegate, OnKeyboardLeave(surface.get()));
  focus_client->FocusWindow(nullptr);
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_W);
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);

  // Key should be pressed when focus returns.
  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(ui::EF_CONTROL_DOWN));
  EXPECT_CALL(delegate,
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>(
                                  {{ui::DomCode::US_W, ui::DomCode::US_W}})));
  focus_client->FocusWindow(surface->window());

  // Releasing accelerator when surface has focus should generate event.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_W, false));
  generator.ReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);

  keyboard.reset();
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

  MockKeyboardDelegate delegate;
  Seat seat;
  auto keyboard = std::make_unique<Keyboard>(&delegate, &seat);

  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  EXPECT_CALL(delegate,
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
  focus_client->FocusWindow(surface->window());

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  // This should generate a modifier event.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_A, true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(ui::EF_SHIFT_DOWN));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_A);
  generator.PressKey(ui::VKEY_A, ui::EF_SHIFT_DOWN);

  // This should generate another modifier event.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_B, true));
  EXPECT_CALL(delegate,
              OnKeyboardModifiers(ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_B);
  generator.PressKey(ui::VKEY_B, ui::EF_SHIFT_DOWN | ui::EF_ALT_DOWN);

  // This should generate a third modifier event.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_B, false));
  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  generator.ReleaseKey(ui::VKEY_B, 0);

  keyboard.reset();
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
  tablet_mode_controller->EnableTabletModeWindowManager(true);

  MockKeyboardDelegate delegate;
  Seat seat;
  auto keyboard = std::make_unique<Keyboard>(&delegate, &seat);
  MockKeyboardDeviceConfigurationDelegate configuration_delegate;

  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(true));
  keyboard->SetDeviceConfigurationDelegate(&configuration_delegate);
  EXPECT_TRUE(keyboard->HasDeviceConfigurationDelegate());

  // Removing all keyboard devices in tablet mode calls
  // OnKeyboardTypeChanged() with false.
  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(false));
  device_data_manager->OnKeyboardDevicesUpdated(
      std::vector<ui::InputDevice>({}));

  // Re-adding keyboards calls OnKeyboardTypeChanged() with true.
  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(true));
  device_data_manager->OnKeyboardDevicesUpdated(keyboards);

  keyboard.reset();

  tablet_mode_controller->EnableTabletModeWindowManager(false);
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

  MockKeyboardDelegate delegate;
  Seat seat;
  auto keyboard = std::make_unique<Keyboard>(&delegate, &seat);
  MockKeyboardDeviceConfigurationDelegate configuration_delegate;

  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(true));
  keyboard->SetDeviceConfigurationDelegate(&configuration_delegate);
  EXPECT_TRUE(keyboard->HasDeviceConfigurationDelegate());

  ash::AccessibilityController* accessibility_controller =
      ash::Shell::Get()->accessibility_controller();

  // Enable a11y keyboard calls OnKeyboardTypeChanged() with false.
  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(false));
  accessibility_controller->SetVirtualKeyboardEnabled(true);

  // Disable a11y keyboard calls OnKeyboardTypeChanged() with true.
  EXPECT_CALL(configuration_delegate, OnKeyboardTypeChanged(true));
  accessibility_controller->SetVirtualKeyboardEnabled(false);

  keyboard.reset();
}

TEST_F(KeyboardTest, KeyboardObserver) {
  MockKeyboardDelegate delegate;
  Seat seat;
  auto keyboard = std::make_unique<Keyboard>(&delegate, &seat);
  MockKeyboardObserver observer1;
  MockKeyboardObserver observer2;

  keyboard->AddObserver(&observer1);
  keyboard->AddObserver(&observer2);
  EXPECT_TRUE(keyboard->HasObserver(&observer1));
  EXPECT_TRUE(keyboard->HasObserver(&observer2));

  keyboard->RemoveObserver(&observer1);
  EXPECT_FALSE(keyboard->HasObserver(&observer1));
  EXPECT_TRUE(keyboard->HasObserver(&observer2));

  EXPECT_CALL(observer1, OnKeyboardDestroying(keyboard.get())).Times(0);
  EXPECT_CALL(observer2, OnKeyboardDestroying(keyboard.get()));
  keyboard.reset();
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

  MockKeyboardDelegate delegate;
  Seat seat;
  auto keyboard = std::make_unique<Keyboard>(&delegate, &seat);

  EXPECT_FALSE(keyboard->AreKeyboardKeyAcksNeeded());
  keyboard->SetNeedKeyboardKeyAcks(true);
  EXPECT_TRUE(keyboard->AreKeyboardKeyAcksNeeded());
  keyboard->SetNeedKeyboardKeyAcks(false);
  EXPECT_FALSE(keyboard->AreKeyboardKeyAcksNeeded());

  keyboard.reset();
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

  MockKeyboardDelegate delegate;
  Seat seat;
  auto keyboard = std::make_unique<Keyboard>(&delegate, &seat);

  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  EXPECT_CALL(delegate,
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
  focus_client->FocusWindow(surface->window());

  // If we don't set NeedKeyboardAckKeys to true, accelerators are always passed
  // to ShellSurface.
  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  // Press KEY_W with Ctrl.
  EXPECT_CALL(delegate, OnKeyboardModifiers(4));
  EXPECT_CALL(*shell_surface.get(), AcceleratorPressed(ui::Accelerator(
                                        ui::VKEY_W, ui::EF_CONTROL_DOWN,
                                        ui::Accelerator::KeyState::PRESSED)))
      .WillOnce(testing::Return(true));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_W);
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);

  // Release KEY_W.
  generator.ReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);

  // If we set NeedKeyboardAckKeys to true, only unhandled accelerators are
  // passed to ShellSurface.
  keyboard->SetNeedKeyboardKeyAcks(true);

  // Press KEY_W with Ctrl.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_W, true))
      .WillOnce(testing::Return(1));
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);

  // Send ack for the key press.
  EXPECT_CALL(*shell_surface.get(), AcceleratorPressed(ui::Accelerator(
                                        ui::VKEY_W, ui::EF_CONTROL_DOWN,
                                        ui::Accelerator::KeyState::PRESSED)))
      .WillOnce(testing::Return(true));
  keyboard->AckKeyboardKey(1, false /* handled */);

  // Release KEY_W.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_W, false))
      .WillOnce(testing::Return(2));
  generator.ReleaseKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);

  // Send ack for the key release.
  keyboard->AckKeyboardKey(2, false /* handled */);

  // Press KEY_W with Ctrl again.
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_W, true))
      .WillOnce(testing::Return(3));
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);

  // Send ack for the key press.
  // AcceleratorPressed is not called when the accelerator is already handled.
  keyboard->AckKeyboardKey(3, true /* handled */);

  // Release the key and reset modifier_flags.
  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_W, false));
  generator.ReleaseKey(ui::VKEY_W, 0);

  keyboard.reset();
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

  MockKeyboardDelegate delegate;
  Seat seat;
  auto keyboard = std::make_unique<Keyboard>(&delegate, &seat);

  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(0)).Times(1);
  EXPECT_CALL(delegate,
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
  focus_client->FocusWindow(surface->window());

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  keyboard->SetNeedKeyboardKeyAcks(true);

  // Press KEY_W with Ctrl.
  EXPECT_CALL(delegate, OnKeyboardModifiers(4)).Times(1);
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_W, true))
      .WillOnce(testing::Return(1));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_W);
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);

  // Move focus from the window
  EXPECT_CALL(delegate, OnKeyboardLeave(surface.get()));
  focus_client->FocusWindow(nullptr);

  // Send ack for the key press. |AcceleratorPressed()| should not be called.
  keyboard->AckKeyboardKey(1, false /* handled */);

  keyboard.reset();
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

  MockKeyboardDelegate delegate;
  Seat seat;
  auto keyboard = std::make_unique<Keyboard>(&delegate, &seat);

  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  EXPECT_CALL(delegate,
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
  focus_client->FocusWindow(surface->window());

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  keyboard->SetNeedKeyboardKeyAcks(true);

  // Press KEY_W with Ctrl.
  EXPECT_CALL(delegate, OnKeyboardModifiers(4));
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_W, true))
      .WillOnce(testing::Return(1));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_W);
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);

  // Keyboard processes pending events as if it's not handled if ack isnt' sent.
  EXPECT_CALL(*shell_surface.get(), AcceleratorPressed(ui::Accelerator(
                                        ui::VKEY_W, ui::EF_CONTROL_DOWN,
                                        ui::Accelerator::KeyState::PRESSED)))
      .WillOnce(testing::Return(true));
  // Wait until |ProcessExpiredPendingKeyAcks| is fired.
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(1000));
  run_loop.Run();
  RunAllPendingInMessageLoop();

  // Release the key and reset modifier_flags.
  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_W, false));
  generator.ReleaseKey(ui::VKEY_W, 0);

  keyboard.reset();
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

  MockKeyboardDelegate delegate;
  Seat seat;
  auto keyboard = std::make_unique<Keyboard>(&delegate, &seat);

  EXPECT_CALL(delegate, CanAcceptKeyboardEventsForSurface(surface.get()))
      .WillOnce(testing::Return(true));
  EXPECT_CALL(delegate, OnKeyboardModifiers(0));
  EXPECT_CALL(delegate,
              OnKeyboardEnter(surface.get(),
                              base::flat_map<ui::DomCode, ui::DomCode>()));
  focus_client->FocusWindow(surface->window());

  ui::test::EventGenerator generator(ash::Shell::GetPrimaryRootWindow());
  keyboard->SetNeedKeyboardKeyAcks(true);

  // Press KEY_W with Ctrl.
  EXPECT_CALL(delegate, OnKeyboardModifiers(4));
  EXPECT_CALL(delegate, OnKeyboardKey(testing::_, ui::DomCode::US_W, true))
      .WillOnce(testing::Return(1));
  seat.set_physical_code_for_currently_processing_event_for_testing(
      ui::DomCode::US_W);
  generator.PressKey(ui::VKEY_W, ui::EF_CONTROL_DOWN);

  // Wait until |ProcessExpiredPendingKeyAcks| is fired.
  // |ProcessExpiredPendingKeyAcks| will call |AcceleratorPressed| and focus
  // will be moved from the surface.
  EXPECT_CALL(delegate, OnKeyboardLeave(surface.get()));
  base::RunLoop run_loop;
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE, run_loop.QuitClosure(),
      base::TimeDelta::FromMilliseconds(1000));
  run_loop.Run();
  RunAllPendingInMessageLoop();

  keyboard.reset();
}

}  // namespace
}  // namespace exo
