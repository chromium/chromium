// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEB_APPS_DEPRECATED_APPS_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEB_APPS_DEPRECATED_APPS_DIALOG_VIEW_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tab_dialogs.h"
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

// Creates a dialog with two buttons. "Accept" causes the extension to be
// uninstalled and closes the dialog. "Cancel" closes the dialog.
class DeprecatedAppsDialogView : public views::DialogDelegateView {
  METADATA_HEADER(DeprecatedAppsDialogView, views::DialogDelegateView)

 public:
  DeprecatedAppsDialogView(const DeprecatedAppsDialogView&) = delete;
  DeprecatedAppsDialogView& operator=(const DeprecatedAppsDialogView&) = delete;
  ~DeprecatedAppsDialogView() override;

  // Create the dialog metadata and show it. Some behavior specializations:
  // * If the `optional_launched_extension_id` is passed, then the dialog will
  //   show the name of that chrome app in the title.
  // * If `optional_launched_extension_id` is empty and `deprecated_app_ids`
  //   only has one entry, then the dialog will display the name of the one
  //   deprecated chrome app.
  // * If `optional_launched_extension_id` is empty and `deprecated_app_ids` has
  //   more than one entry, then the title will just contain the number of
  //   deprecated chrome apps.
  static DeprecatedAppsDialogView* CreateAndShowDialog(
      const extensions::ExtensionId& optional_launched_extension_id,
      const std::set<extensions::ExtensionId>& deprecated_app_ids,
      content::WebContents* web_contents);

  base::WeakPtr<DeprecatedAppsDialogView> AsWeakPtr();

  // views::DialogDelegateView overrides:
  std::u16string GetWindowTitle() const override;

  enum ViewID { DEPRECATED_APPS_TABLE = 1 };

 private:
  class DeprecatedAppsTableModel;
  DeprecatedAppsDialogView(
      const extensions::ExtensionId& optional_launched_extension_id,
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
  void OnAccept();
  void OnCancel();

  // Controls the table view within the dialog box.
  raw_ptr<views::TableView> deprecated_apps_table_view_;

  // TableModel object that stores the app information.
  std::unique_ptr<DeprecatedAppsTableModel> deprecated_apps_table_model_;

  raw_ptr<views::Label> info_label_;

  std::optional<std::u16string> launched_extension_name_;
  std::set<extensions::ExtensionId> deprecated_app_ids_;
  std::optional<std::u16string> single_app_name_;

  raw_ptr<content::WebContents, AcrossTasksDanglingUntriaged> web_contents_;

  base::WeakPtrFactory<DeprecatedAppsDialogView> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEB_APPS_DEPRECATED_APPS_DIALOG_VIEW_H_
