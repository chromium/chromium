// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/extensions/extensions_menu_view_model.h"
#include "chrome/browser/ui/views/extensions/extensions_menu_handler.h"

#ifndef CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSIONS_MENU_DELEGATE_ANDROID_H_
#define CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSIONS_MENU_DELEGATE_ANDROID_H_

// Implements Android-specific extensions menu UI logic.
class ExtensionsMenuDelegateAndroid : public ExtensionsMenuViewModel::Delegate,
                                      public ExtensionsMenuViewModel::Observer,
                                      public ExtensionsMenuHandler {
 public:
  explicit ExtensionsMenuDelegateAndroid(BrowserWindowInterface* browser);
  ExtensionsMenuDelegateAndroid(const ExtensionsMenuDelegateAndroid&) = delete;
  const ExtensionsMenuDelegateAndroid& operator=(
      const ExtensionsMenuDelegateAndroid&) = delete;
  ~ExtensionsMenuDelegateAndroid() override;

  // ExtensionsMenuViewModel::Delegate:
  std::unique_ptr<ExtensionActionViewModel> CreateActionViewModel(
      const extensions::ExtensionId& extension_id) override;

  // ExtensionsMenuViewModel::Observer:
  void OnActiveWebContentsChanged(content::WebContents* web_contents) override;
  void OnActionAdded(ExtensionActionViewModel* action_model,
                     int index) override;
  void OnActionRemoved(const ToolbarActionsModel::ActionId& action_id,
                       int index) override;
  void OnActionUpdated() override;
  void OnActionsInitialized() override;
  void OnHostAccessRequestAddedOrUpdated(
      const extensions::ExtensionId& extension_id,
      content::WebContents* web_contents) override;
  void OnHostAccessRequestsCleared() override;
  void OnHostAccessRequestDismissedByUser(
      const extensions::ExtensionId& extension_id) override;
  void OnHostAccessRequestRemoved(
      const extensions::ExtensionId& extension_id) override;
  void OnShowHostAccessRequestsInToolbarChanged(
      const extensions::ExtensionId& extension_id,
      bool can_show_requests) override;
  void OnToolbarPinnedActionsChanged() override;
  void OnUserPermissionsSettingsChanged() override;

  // ExtensionsMenuHandler:
  void CloseBubble() override;
  void OnAllowExtensionClicked(
      const extensions::ExtensionId& extension_id) override;
  void OnDismissExtensionClicked(
      const extensions::ExtensionId& extension_id) override;
  void OnExtensionToggleSelected(const extensions::ExtensionId& extension_id,
                                 bool is_on) override;
  void OnReloadPageButtonClicked() override;
  void OnShowRequestsTogglePressed(const extensions::ExtensionId& extension_id,
                                   bool is_on) override;
  void OnSiteAccessSelected(
      const extensions::ExtensionId& extension_id,
      extensions::PermissionsManager::UserSiteAccess site_access) override;
  void OnSiteSettingsToggleButtonPressed(bool is_on) override;
  void OpenMainPage() override;
  void OpenSitePermissionsPage(
      const extensions::ExtensionId& extension_id) override;

 private:
  // The platform-agnostic menu view model.
  std::unique_ptr<ExtensionsMenuViewModel> menu_model_;
  base::ScopedObservation<ExtensionsMenuViewModel,
                          ExtensionsMenuViewModel::Observer>
      menu_model_observation_{this};
};

#endif  // CHROME_BROWSER_UI_ANDROID_EXTENSIONS_EXTENSIONS_MENU_DELEGATE_ANDROID_H_
