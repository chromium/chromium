// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/base/view_event_test_platform_part.h"

#include <memory>
#include <utility>

#include "ash/session/test_session_controller_client.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "ash/test/ash_test_helper.h"
#include "ash/test_shell_delegate.h"
#include "base/command_line.h"
#include "base/macros.h"
#include "chromeos/audio/cras_audio_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/power_policy_controller.h"
#include "chromeos/network/network_handler.h"
#include "content/public/browser/gpu_interface_provider_factory.h"
#include "device/bluetooth/dbus/bluez_dbus_manager.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/ui_base_features.h"
#include "ui/display/display_switches.h"
#include "ui/wm/core/wm_state.h"

namespace {

// ViewEventTestPlatformPart implementation for ChromeOS (chromeos=1).
class ViewEventTestPlatformPartChromeOS : public ViewEventTestPlatformPart {
 public:
  ViewEventTestPlatformPartChromeOS(
      ui::ContextFactory* context_factory,
      ui::ContextFactoryPrivate* context_factory_private);
  ~ViewEventTestPlatformPartChromeOS() override;

  // Overridden from ViewEventTestPlatformPart:
  gfx::NativeWindow GetContext() override {
    return ash::Shell::GetPrimaryRootWindow();
  }

 private:
  wm::WMState wm_state_;
  std::unique_ptr<aura::Env> env_;

  DISALLOW_COPY_AND_ASSIGN(ViewEventTestPlatformPartChromeOS);
};

ViewEventTestPlatformPartChromeOS::ViewEventTestPlatformPartChromeOS(
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private) {
  chromeos::DBusThreadManager::Initialize();
  // ash::Shell::CreateInstance needs chromeos::PowerPolicyController
  // initialized. In classic ash, it is initialized in chrome process. In mash,
  // it is initialized by window manager service.
  chromeos::PowerPolicyController::Initialize(
      chromeos::DBusThreadManager::Get()->GetPowerManagerClient());
  bluez::BluezDBusManager::Initialize();
  chromeos::CrasAudioHandler::InitializeForTesting();
  chromeos::NetworkHandler::Initialize();

  env_ = aura::Env::CreateInstance(features::IsSingleProcessMash()
                                       ? aura::Env::Mode::MUS
                                       : aura::Env::Mode::LOCAL);
  ash::ShellInitParams init_params;
  init_params.delegate = std::make_unique<ash::TestShellDelegate>();
  init_params.context_factory = context_factory;
  init_params.context_factory_private = context_factory_private;
  init_params.gpu_interface_provider = content::CreateGpuInterfaceProvider();
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kHostWindowBounds, "0+0-1280x800");
  ash::Shell::CreateInstance(std::move(init_params));
  ash::TestSessionControllerClient session_controller_client(
      ash::Shell::Get()->session_controller());
  session_controller_client.CreatePredefinedUserSessions(1);
  GetContext()->GetHost()->Show();
}

ViewEventTestPlatformPartChromeOS::~ViewEventTestPlatformPartChromeOS() {
  ash::Shell::DeleteInstance();
  env_.reset();

  chromeos::NetworkHandler::Shutdown();
  chromeos::CrasAudioHandler::Shutdown();
  bluez::BluezDBusManager::Shutdown();
  chromeos::PowerPolicyController::Shutdown();
  chromeos::DBusThreadManager::Shutdown();
}

}  // namespace

// static
ViewEventTestPlatformPart* ViewEventTestPlatformPart::Create(
    ui::ContextFactory* context_factory,
    ui::ContextFactoryPrivate* context_factory_private) {
  return new ViewEventTestPlatformPartChromeOS(context_factory,
                                               context_factory_private);
}
