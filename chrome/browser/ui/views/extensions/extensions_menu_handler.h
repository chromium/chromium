// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_HANDLER_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_HANDLER_H_

#include "extensions/browser/permissions_manager.h"
#include "extensions/common/extension_id.h"

// An interface that provides callbacks to the extensions menu pages.
class ExtensionsMenuHandler {
 public:
  virtual ~ExtensionsMenuHandler() = default;

  // Creates and opens the main page in the menu, if it exists.
  virtual void OpenMainPage() = 0;

  // Creates and opens the site permissions page for `extension_id` in the menu,
  // if it exists.
  virtual void OpenSitePermissionsPage(
      const extensions::ExtensionId& extension_id) = 0;

  // Closes the currently-showing extensions menu, if it exists.
  virtual void CloseBubble() = 0;

  // Updates the user site setting whether toggle `is_on`.
  virtual void OnSiteSettingsToggleButtonPressed(bool is_on) = 0;

  // Updates the user site access for `extension_id` to `site_access`.
  virtual void OnSiteAccessSelected(
      const extensions::ExtensionId& extension_id,
      extensions::PermissionsManager::UserSiteAccess site_access) = 0;

  // Grants or withhelds site access for `extension_id` depending on
  // `site_access_toggle`.
  virtual void OnExtensionToggleSelected(
      const extensions::ExtensionId& extension_id,
      bool is_on) = 0;

  // Reload the current web contents.
  virtual void OnReloadPageButtonClicked() = 0;

  // Grants one time site access to `extension_id` on the current web contents.
  virtual void OnAllowExtensionClicked(
      const extensions::ExtensionId& extension_id) = 0;

  // Dismiss the `extension_id` requests access in the menu and toolbar one time
  // on the current web contents.
  virtual void OnDismissExtensionClicked(
      const extensions::ExtensionId& extension_id) = 0;

  // Sets whether `extension_id` can show site access requests in the toolbar
  // according to `is_on`.
  virtual void OnShowRequestsTogglePressed(
      const extensions::ExtensionId& extension_id,
      bool is_on) = 0;
};

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_MENU_HANDLER_H_
