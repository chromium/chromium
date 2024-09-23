// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/shell_init/ash_shell_init.h"

#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/quick_pair/keyed_service/quick_pair_mediator.h"
#include "ash/shell.h"
#include "ash/shell_init_params.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/ash/keyboard/chrome_keyboard_ui_factory.h"
#include "chrome/browser/ui/ash/shell_delegate/chrome_shell_delegate.h"
#include "chrome/browser/ui/ui_features.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "content/public/browser/context_factory.h"
#include "ui/aura/window_tree_host.h"

namespace {

void CreateShell() {
  ash::ShellInitParams shell_init_params;
  shell_init_params.delegate = std::make_unique<ChromeShellDelegate>();
  shell_init_params.context_factory = content::GetContextFactory();
  shell_init_params.local_state = g_browser_process->local_state();
  shell_init_params.keyboard_ui_factory =
      std::make_unique<ChromeKeyboardUIFactory>();
  shell_init_params.quick_pair_mediator_factory =
      std::make_unique<ash::quick_pair::Mediator::FactoryImpl>();
  shell_init_params.dbus_bus = ash::DBusThreadManager::Get()->GetSystemBus();

  ash::Shell::CreateInstance(std::move(shell_init_params));
}

}  // namespace

AshShellInit::AshShellInit() {
  CreateShell();
  ash::Shell::GetPrimaryRootWindow()->GetHost()->Show();
}

AshShellInit::~AshShellInit() {
  ash::Shell::DeleteInstance();
}
