// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SITE_ACCESS_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SITE_ACCESS_COMBOBOX_MODEL_H_

#include <vector>

#include "chrome/browser/extensions/site_permissions_helper.h"
#include "ui/base/models/combobox_model.h"

class Browser;

// The model for the site access combobox in the extensions menu. This manages
// the user's manipulation of the combobox and offers the data to show on it.
// Since this class doesn't own the extension, be sure to check for validity
// using ExtensionIsValid() before using those members.
class ExtensionSiteAccessComboboxModel : public ui::ComboboxModel {
 public:
  ExtensionSiteAccessComboboxModel(Browser* browser,
                                   const extensions::Extension* extension);

  ExtensionSiteAccessComboboxModel(const ExtensionSiteAccessComboboxModel&) =
      delete;
  const ExtensionSiteAccessComboboxModel& operator=(
      const ExtensionSiteAccessComboboxModel&) = delete;

  ~ExtensionSiteAccessComboboxModel() override;

  // Handles the action corresponding to the `selected_index`.
  void HandleSelection(size_t selected_index);

  // Gets the combobox item index corresponding to the current site access.
  size_t GetCurrentSiteAccessIndex() const;

  // ui::ComboboxModel:
  size_t GetItemCount() const override;
  std::u16string GetItemAt(size_t index) const override;
  absl::optional<size_t> GetDefaultIndex() const override;
  bool IsItemEnabledAt(size_t index) const override;

 private:
  // Checks if `extension_` is still valid by checking its
  // status in the registry.
  bool ExtensionIsValid() const;

  // Logs a user action when `site_access` is selected using the combobox.
  void LogSiteAccessAction(
      extensions::SitePermissionsHelper::SiteAccess site_access) const;

  const raw_ptr<Browser> browser_;

  // The extension associated with the combobox.
  raw_ptr<const extensions::Extension> extension_;

  // Combobox drop down items.
  std::vector<extensions::SitePermissionsHelper::SiteAccess> items_;
};

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_SITE_ACCESS_COMBOBOX_MODEL_H_
