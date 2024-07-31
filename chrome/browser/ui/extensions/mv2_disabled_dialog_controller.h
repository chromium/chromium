// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_MV2_DISABLED_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_EXTENSIONS_MV2_DISABLED_DIALOG_CONTROLLER_H_

#include <vector>

#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "extensions/common/extension_id.h"

class Browser;

namespace extensions {

// Per-browser-window class responsible for showing the disabled extensions
// dialog for the Manifest V2 deprecation.
class Mv2DisabledDialogController {
 public:
  explicit Mv2DisabledDialogController(Browser* browser);
  ~Mv2DisabledDialogController();

  // Cleans up this class. Should be called before destruction.
  void TearDown();

  // Testing:
  void MaybeShowDisabledDialogForTesting();

 private:
  // Shows a dialog with disabled extensions due to the MV2 deprecation, if
  // there are any extensions affected.
  void MaybeShowDisabledDialog();

  // Removes `extension_ids` when the remove option is selected.
  void OnRemoveSelected(const std::vector<ExtensionId>& extension_ids);

  // Opens the management page when the manage option is selected.
  void OnManageSelected(const std::vector<ExtensionId>& extension_ids);

  // Updates the pref for each `extension_ids` that stores whether the user
  // acknowledged the dialog. This should be called when the user takes any
  // action on the dialog.
  void UserAcknowledgedDialog(const std::vector<ExtensionId>& extension_ids);

  raw_ptr<Browser> browser_;
  base::CallbackListSubscription show_dialog_subscription_;

  base::WeakPtrFactory<Mv2DisabledDialogController> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_MV2_DISABLED_DIALOG_CONTROLLER_H_
