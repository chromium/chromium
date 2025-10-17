// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/extensions/extension_action_platform_delegate_views.h"

#include <utility>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/extensions/extension_view_host.h"
#include "chrome/browser/extensions/extension_view_host_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/extensions/accelerator_priority.h"
#include "chrome/browser/ui/extensions/extension_action_view_controller.h"
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/tabs/tab_list_interface.h"
#include "chrome/browser/ui/views/extensions/extension_popup.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/toolbar/toolbar_action_view_delegate_views.h"
#include "chrome/browser/ui/views/toolbar/toolbar_view.h"
#include "components/sessions/content/session_tab_helper.h"
#include "extensions/browser/extension_action.h"
#include "extensions/common/api/extension_action/action_info.h"
#include "extensions/common/command.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "ui/views/view.h"

using extensions::ActionInfo;

ExtensionActionPlatformDelegateViews::ExtensionActionPlatformDelegateViews(
    BrowserWindowInterface* browser,
    ExtensionsContainer* extensions_container)
    : browser_(browser), extensions_container_(extensions_container) {}

ExtensionActionPlatformDelegateViews::~ExtensionActionPlatformDelegateViews() {
  // Should have already unregistered.
  DCHECK(!action_keybinding_);
}

ExtensionActionPlatformDelegateViews*
ExtensionActionPlatformDelegateViews::GetPopupOwnerDelegate() {
  ExtensionActionViewController* owner_controller =
      static_cast<ExtensionActionViewController*>(
          extensions_container_->GetActionForId(controller_->GetId()));
  return static_cast<ExtensionActionPlatformDelegateViews*>(
      owner_controller->platform_delegate());
}

void ExtensionActionPlatformDelegateViews::DoTriggerPopup(
    std::unique_ptr<extensions::ExtensionViewHost> host,
    PopupShowAction show_action,
    bool by_user,
    ShowPopupCallback callback) {
  DCHECK_EQ(this, GetPopupOwnerDelegate());

  // Always hide the current popup, even if it's not owned by this extension.
  // Only one popup should be visible at a time.
  extensions_container_->HideActivePopup();

  extensions_container_->CloseOverflowMenuIfOpen();

  popup_host_ = host.get();
  popup_host_observation_.Observe(popup_host_.get());
  extensions_container_->SetPopupOwner(controller_);

  extensions_container_->PopOutAction(
      controller_->GetId(),
      base::BindOnce(&ExtensionActionPlatformDelegateViews::ShowPopup,
                     weak_factory_.GetWeakPtr(), std::move(host), show_action,
                     by_user, std::move(callback)));
}

void ExtensionActionPlatformDelegateViews::ShowPopup(
    std::unique_ptr<extensions::ExtensionViewHost> host,
    PopupShowAction show_action,
    bool by_user,
    ShowPopupCallback callback) {
  // It's possible that the popup should be closed before it finishes opening
  // (since it can open asynchronously). Check before proceeding.
  if (!popup_host_) {
    if (callback) {
      std::move(callback).Run(nullptr);
    }
    return;
  }

  // NOTE: Today, ShowPopup() always synchronously creates the platform-specific
  // popup class, which is what we care most about (since `has_opened_popup_`
  // is used to determine whether we need to manually close the
  // ExtensionViewHost). This doesn't necessarily mean that the popup has
  // completed rendering on the screen.
  has_opened_popup_ = true;

  // TOP_RIGHT is correct for both RTL and LTR, because the views platform
  // performs the flipping in RTL cases.
  views::BubbleBorder::Arrow arrow = views::BubbleBorder::TOP_RIGHT;
  ExtensionPopup::ShowPopup(browser_->GetBrowserForMigrationOnly(),
                            std::move(host),
                            GetDelegateViews()->GetReferenceButtonForPopup(),
                            arrow, show_action, std::move(callback));

  extensions_container_->OnPopupShown(controller_->GetId(), by_user);
}

void ExtensionActionPlatformDelegateViews::OnPopupClosed() {
  DCHECK(popup_host_observation_.IsObservingSource(popup_host_.get()));
  popup_host_observation_.Reset();
  popup_host_ = nullptr;
  has_opened_popup_ = false;
  extensions_container_->SetPopupOwner(nullptr);
  if (extensions_container_->GetPoppedOutActionId() == controller_->GetId()) {
    extensions_container_->UndoPopOut();
  }
  extensions_container_->OnPopupClosed(controller_->GetId());
}

void ExtensionActionPlatformDelegateViews::AttachToController(
    ExtensionActionViewController* controller) {
  CHECK(controller);
  CHECK(!controller_);
  controller_ = controller;
}

void ExtensionActionPlatformDelegateViews::DetachFromController() {
  CHECK(controller_);
  controller_ = nullptr;
}

void ExtensionActionPlatformDelegateViews::RegisterCommand() {
  // If we've already registered, do nothing.
  if (action_keybinding_) {
    return;
  }

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
  if (!action_keybinding_) {
    return;
  }

  views::FocusManager* focus_manager =
      GetDelegateViews()->GetFocusManagerForAccelerator();
  if (focus_manager) {
    focus_manager->UnregisterAccelerator(*action_keybinding_, this);
    action_keybinding_.reset();
  }
}

bool ExtensionActionPlatformDelegateViews::IsShowingPopup() const {
  return popup_host_ != nullptr;
}

void ExtensionActionPlatformDelegateViews::HidePopup() {
  if (!IsShowingPopup()) {
    return;
  }

  // Only call Close() on the popup if it's been shown; otherwise, the popup
  // will be cleaned up in ShowPopup().
  if (has_opened_popup_) {
    popup_host_->Close();
  }

  // We need to do these actions synchronously (instead of closing and then
  // performing the rest of the cleanup in OnExtensionHostDestroyed()) because
  // the extension host may close asynchronously, and we need to keep the view
  // delegate up to date.
  if (popup_host_) {
    OnPopupClosed();
  }
}

gfx::NativeView ExtensionActionPlatformDelegateViews::GetPopupNativeView() {
  return popup_host_ ? popup_host_->view()->GetNativeView() : gfx::NativeView();
}

void ExtensionActionPlatformDelegateViews::TriggerPopup(
    std::unique_ptr<extensions::ExtensionViewHost> host,
    PopupShowAction show_action,
    bool by_user,
    ShowPopupCallback callback) {
  GetPopupOwnerDelegate()->DoTriggerPopup(std::move(host), show_action, by_user,
                                          std::move(callback));
}

void ExtensionActionPlatformDelegateViews::ShowContextMenuAsFallback() {
  extensions_container_->ShowContextMenuAsFallback(controller_->GetId());
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

void ExtensionActionPlatformDelegateViews::OnExtensionHostDestroyed(
    extensions::ExtensionHost* host) {
  OnPopupClosed();
}

ToolbarActionViewDelegateViews*
ExtensionActionPlatformDelegateViews::GetDelegateViews() const {
  return static_cast<ToolbarActionViewDelegateViews*>(
      controller_->view_delegate());
}
