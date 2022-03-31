// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_DEPRECATED_APPS_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_DEPRECATED_APPS_DIALOG_VIEW_H_

#include <memory>
#include <set>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_system.h"
#include "extensions/common/extension_id.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/table/table_view.h"
#include "ui/views/window/dialog_delegate.h"

namespace content {
class WebContents;
}

namespace views {
class Label;
class TableView;
}  // namespace views

namespace ui {
class TableModel;
}

class DeprecatedAppsDialogView : public views::DialogDelegateView {
 public:
  METADATA_HEADER(DeprecatedAppsDialogView);
  DeprecatedAppsDialogView(const DeprecatedAppsDialogView&) = delete;
  DeprecatedAppsDialogView& operator=(const DeprecatedAppsDialogView&) = delete;
  ~DeprecatedAppsDialogView() override;

  // Create the dialog metadata and show it.
  static DeprecatedAppsDialogView* CreateAndShowDialog(
      const std::set<extensions::ExtensionId>& deprecated_app_ids,
      content::WebContents* web_contents);

  base::WeakPtr<DeprecatedAppsDialogView> AsWeakPtr();

  // views::DialogDelegateView overrides:
  std::u16string GetWindowTitle() const override;

  enum ViewID { DEPRECATED_APPS_TABLE = 1 };

 private:
  class DeprecatedAppsTableModel;
  DeprecatedAppsDialogView(
      const std::set<extensions::ExtensionId>& deprecated_app_ids,
      content::WebContents* web_contents);

  // Initialize the dialog when the object is instantiated.
  void InitDialog();

  // Reset all information from Tablemodel and hide the dialog.
  void CloseDialog();

  // Callback that runs when the icon images are updated.
  void OnIconsLoadedForTable();

  // Callback that runs when accept button is clicked to
  // uninstall all extensions.
  void UninstallExtensions();

  // Controls the table view within the dialog box.
  raw_ptr<views::TableView> deprecated_apps_table_view_;

  // TableModel object that stores the app information.
  std::unique_ptr<DeprecatedAppsTableModel> deprecated_apps_table_model_;

  raw_ptr<views::Label> info_label_;

  std::set<extensions::ExtensionId> deprecated_app_ids_;

  raw_ptr<content::WebContents> web_contents_;

  base::WeakPtrFactory<DeprecatedAppsDialogView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_DEPRECATED_APPS_DIALOG_VIEW_H_
