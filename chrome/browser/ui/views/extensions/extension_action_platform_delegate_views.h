// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_VIEWS_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/ui/extensions/extension_action_platform_delegate.h"
#include "extensions/browser/extension_host_observer.h"
#include "ui/base/accelerators/accelerator.h"

class BrowserWindowInterface;
class ExtensionsContainerViews;

namespace extensions {
class ExtensionViewHost;
}  // namespace extensions

// Implements Views-specific extension action UI logic, such as showing the
// action's popup and the context menu.
//
// This class doesn't subclass View directly, as the implementations for the
// action button in the toolbar and one in the extensions menu are different
// types of views.
class ExtensionActionPlatformDelegateViews
    : public ExtensionActionPlatformDelegate,
      public ui::AcceleratorTarget,
      public extensions::ExtensionHostObserver {
 public:
  ExtensionActionPlatformDelegateViews(
      BrowserWindowInterface* browser,
      ExtensionsContainerViews* extensions_container);

  ExtensionActionPlatformDelegateViews(
      const ExtensionActionPlatformDelegateViews&) = delete;
  ExtensionActionPlatformDelegateViews& operator=(
      const ExtensionActionPlatformDelegateViews&) = delete;

  ~ExtensionActionPlatformDelegateViews() override;

 private:
  // Returns the ExtensionActionPlatformDelegateViews instance that should own
  // the action popup, i.e. the one tied to the action button in the toolbar.
  // TODO(crbug.com/448199168): Remove this method. It is confusing that we have
  // two platform delegates per action that maintain popup states separately but
  // only one of them are actually used.
  ExtensionActionPlatformDelegateViews* GetPopupOwnerDelegate();

  // Begins the process of showing the popup for the extension action on the
  // current web contents. This function must be called on the
  void DoTriggerPopup(std::unique_ptr<extensions::ExtensionViewHost> host,
                      PopupShowAction show_action,
                      bool by_user,
                      ShowPopupCallback callback);

  // Shows the popup with the given |host|.
  void ShowPopup(std::unique_ptr<extensions::ExtensionViewHost> host,
                 PopupShowAction show_action,
                 bool by_user,
                 ShowPopupCallback callback);

  // Handles cleanup after the popup closes.
  void OnPopupClosed();

  // ExtensionActionPlatformDelegate:
  void AttachToModel(ExtensionActionViewModel* model) override;
  void DetachFromModel() override;
  void RegisterCommand() override;
  void UnregisterCommand() override;
  bool IsShowingPopup() const override;
  void HidePopup() override;
  gfx::NativeView GetPopupNativeView() override;
  void TriggerPopup(std::unique_ptr<extensions::ExtensionViewHost> host,
                    PopupShowAction show_action,
                    bool by_user,
                    ShowPopupCallback callback) override;
  void ShowContextMenuAsFallback() override;
  bool CloseOverflowMenuIfOpen() override;

  // ui::AcceleratorTarget:
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;

  // ExtensionHostObserver:
  void OnExtensionHostDestroyed(extensions::ExtensionHost* host) override;

  // The corresponding browser window.
  const raw_ptr<BrowserWindowInterface> browser_;

  // The corresponding ExtensionsContainerViews on the toolbar.
  const raw_ptr<ExtensionsContainerViews> extensions_container_;

  // The platform-agnostic view model.
  raw_ptr<ExtensionActionViewModel> model_{nullptr};

  // The extension popup's host if the popup is visible; null otherwise.
  raw_ptr<extensions::ExtensionViewHost> popup_host_;

  // Whether the toolbar action has opened an active popup. This is unique from
  // `popup_host_` since `popup_host_` may be non-null even if the popup hasn't
  // opened yet if we're waiting on other UI to be ready (e.g. the action to
  // slide out in the toolbar).
  bool has_opened_popup_ = false;

  // The extension key binding accelerator this extension action is listening
  // for (to show the popup).
  std::unique_ptr<ui::Accelerator> action_keybinding_;

  base::ScopedObservation<extensions::ExtensionHost,
                          extensions::ExtensionHostObserver>
      popup_host_observation_{this};

  base::WeakPtrFactory<ExtensionActionPlatformDelegateViews> weak_factory_{
      this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_ACTION_PLATFORM_DELEGATE_VIEWS_H_
