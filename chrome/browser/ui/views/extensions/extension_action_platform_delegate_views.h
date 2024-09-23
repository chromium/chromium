// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/extensions/commands/command_service.h"
#include "chrome/browser/ui/extensions/extension_action_platform_delegate.h"
#include "ui/base/accelerators/accelerator.h"

class ToolbarActionViewDelegateViews;

// An abstract "View" for an ExtensionAction (Action, BrowserAction, or a
// PageAction). This contains the logic for showing the action's popup and
// the context menu. This class doesn't subclass View directly, as the
// implementations for page actions/browser actions are different types of
// views.
// All common logic for executing extension actions should go in this class;
// ToolbarActionViewDelegate classes should only have knowledge relating to
// the views::View wrapper.
class ExtensionActionPlatformDelegateViews
    : public ExtensionActionPlatformDelegate,
      public extensions::CommandService::Observer,
      public ui::AcceleratorTarget {
 public:
  ExtensionActionPlatformDelegateViews(
      ExtensionActionViewController* controller);

  ExtensionActionPlatformDelegateViews(
      const ExtensionActionPlatformDelegateViews&) = delete;
  ExtensionActionPlatformDelegateViews& operator=(
      const ExtensionActionPlatformDelegateViews&) = delete;

  ~ExtensionActionPlatformDelegateViews() override;

 private:
  // ExtensionActionPlatformDelegate:
  void RegisterCommand() override;
  void UnregisterCommand() override;
  void ShowPopup(std::unique_ptr<extensions::ExtensionViewHost> host,
                 PopupShowAction show_action,
                 ShowPopupCallback callback) override;

  // extensions::CommandService::Observer:
  void OnExtensionCommandAdded(const std::string& extension_id,
                               const extensions::Command& command) override;
  void OnExtensionCommandRemoved(const std::string& extension_id,
                                 const extensions::Command& command) override;
  void OnCommandServiceDestroying() override;

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  ToolbarActionViewDelegateViews* GetDelegateViews() const;

  // The owning ExtensionActionViewController.
  raw_ptr<ExtensionActionViewController> controller_;

  // The extension key binding accelerator this extension action is listening
  // for (to show the popup).
  std::unique_ptr<ui::Accelerator> action_keybinding_;

  base::ScopedObservation<extensions::CommandService,
                          extensions::CommandService::Observer>
      command_service_observation_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_VIEWS_H_
