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
#include "ui/gfx/image/image.h"

class Browser;

namespace extensions {

enum class MV2ExperimentStage;

// Per-browser-window class responsible for showing the disabled extensions
// dialog for the Manifest V2 deprecation.
class Mv2DisabledDialogController {
 public:
  explicit Mv2DisabledDialogController(Browser* browser);
  ~Mv2DisabledDialogController();

  // Information for the extensions that should be included in the dialog.
  struct ExtensionInfo {
    ExtensionId id;
    std::string name;
    gfx::Image icon;
  };

  // Cleans up this class. Should be called before destruction.
  void TearDown();

  // Testing:
  void MaybeShowDisabledDialogForTesting();
  std::vector<ExtensionInfo> GetAffectedExtensionsForTesting() {
    return affected_extensions_info_;
  }

 private:
  // Populates `affected_extensions_info_` with the extensions that should be
  // included in the disabled dialog.
  void ComputeAffectedExtensions();

  // Called when extension with `extension_id` and `extension_name` has finished
  // loading its `icon`. Calls `done_callback` to keep track of number of icons
  // loaded.
  void OnExtensionIconLoaded(const ExtensionId& extension_id,
                             const std::string& extension_name,
                             base::OnceClosure done_callback,
                             const gfx::Image& icon);

  // Shows a disabled dialog with `affected_extensions_info_`, removing the ones
  // that are no longer affected (since this method is called asynchronously).
  void MaybeShowDisabledDialog();

  // Uninstalls extensions corresponding to `affected_extensions_info_` when the
  // remove option is selected.
  void OnRemoveSelected();

  // Opens the management page when the manage option is selected.
  void OnManageSelected();

  // Updates the pref that stores whether the user acknowledged the dialog for
  // each of the extensions corresponding to `affected_extensions_info_`. This
  // should be called when the user takes any action on the dialog.
  void UserAcknowledgedDialog();

  raw_ptr<Browser> browser_;
  base::CallbackListSubscription show_dialog_subscription_;

  // The current stage of the MV2 deprecation experiment.
  MV2ExperimentStage experiment_stage_;

  // Extensions information to display in the disabled dialog.
  std::vector<ExtensionInfo> affected_extensions_info_;

  base::WeakPtrFactory<Mv2DisabledDialogController> weak_ptr_factory_{this};
};

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_MV2_DISABLED_DIALOG_CONTROLLER_H_
