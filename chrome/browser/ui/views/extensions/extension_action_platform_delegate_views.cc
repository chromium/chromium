// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_action_platform_delegate_views.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/accelerator_priority.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view_delegate_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "extensions/browser/extension_action.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/command.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "ui/views/view.h"

using extensions::ActionInfo;

// static
std::unique_ptr<ExtensionActionPlatformDelegate>
ExtensionActionPlatformDelegate::Create(
    ExtensionActionViewController* controller) {
  return std::make_unique<ExtensionActionPlatformDelegateViews>(controller);
}

ExtensionActionPlatformDelegateViews::ExtensionActionPlatformDelegateViews(
    ExtensionActionViewController* controller)
    : controller_(controller) {
  command_service_observation_.Observe(
      extensions::CommandService::Get(controller_->browser()->profile()));
}

ExtensionActionPlatformDelegateViews::~ExtensionActionPlatformDelegateViews() {
  // Should have already unregistered.
  DCHECK(!action_keybinding_);
}

void ExtensionActionPlatformDelegateViews::RegisterCommand() {
  // If we've already registered, do nothing.
  if (action_keybinding_)
    return;

  extensions::Command extension_command;
  views::FocusManager* focus_manager =
      GetDelegateViews()->GetFocusManagerForAccelerator();
  if (focus_manager && controller_->GetExtensionCommand(&extension_command)) {
    action_keybinding_ =
        std::make_unique<ui::Accelerator>(extension_command.accelerator());
    focus_manager->RegisterAccelerator(*action_keybinding_,
                                       kExtensionAcceleratorPriority, this);
  }
}

void ExtensionActionPlatformDelegateViews::UnregisterCommand() {
  // If we've already unregistered, do nothing.
  if (!action_keybinding_)
    return;

  views::FocusManager* focus_manager =
      GetDelegateViews()->GetFocusManagerForAccelerator();
  if (focus_manager) {
    focus_manager->UnregisterAccelerator(*action_keybinding_, this);
    action_keybinding_.reset();
  }
}

void ExtensionActionPlatformDelegateViews::ShowPopup(
    std::unique_ptr<extensions::ExtensionViewHost> host,
    PopupShowAction show_action,
    ShowPopupCallback callback) {
  // TOP_RIGHT is correct for both RTL and LTR, because the views platform
  // performs the flipping in RTL cases.
  views::BubbleBorder::Arrow arrow = views::BubbleBorder::TOP_RIGHT;

  ExtensionPopup::ShowPopup(std::move(host),
                            GetDelegateViews()->GetReferenceButtonForPopup(),
                            arrow, show_action, std::move(callback));
}

void ExtensionActionPlatformDelegateViews::OnExtensionCommandAdded(
    const std::string& extension_id,
    const extensions::Command& command) {
  if (extension_id != controller_->extension()->id())
    return;  // Not this action's extension.

  if (!extensions::Command::IsActionRelatedCommand(command.command_name()))
    return;

  RegisterCommand();
}

void ExtensionActionPlatformDelegateViews::OnExtensionCommandRemoved(
    const std::string& extension_id,
    const extensions::Command& command) {
  if (extension_id != controller_->extension()->id())
    return;

  if (!extensions::Command::IsActionRelatedCommand(command.command_name()))
    return;

  extensions::Command extension_command;
  if (controller_->GetExtensionCommand(&extension_command))
    return;  // Command has not been removed.

  UnregisterCommand();
}

void ExtensionActionPlatformDelegateViews::OnCommandServiceDestroying() {
  DCHECK(command_service_observation_.IsObserving());
  command_service_observation_.Reset();
}

bool ExtensionActionPlatformDelegateViews::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  DCHECK(controller_->CanHandleAccelerators());

  if (controller_->IsShowingPopup()) {
    controller_->HidePopup();
  } else {
    controller_->ExecuteUserAction(
        ToolbarActionViewController::InvocationSource::kCommand);
  }

  return true;
}

bool ExtensionActionPlatformDelegateViews::CanHandleAccelerators() const {
  return controller_->CanHandleAccelerators();
}

ToolbarActionViewDelegateViews*
ExtensionActionPlatformDelegateViews::GetDelegateViews() const {
  return static_cast<ToolbarActionViewDelegateViews*>(
      controller_->view_delegate());
}
