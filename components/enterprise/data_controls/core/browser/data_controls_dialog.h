// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_DATA_CONTROLS_DIALOG_H_
#define COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_DATA_CONTROLS_DIALOG_H_

#include <vector>

#include "base/functional/callback_forward.h"

namespace data_controls {

class DataControlsDialogFactory;

// Platform-agnostic dialog used to warn the user their action is interrupted by
// a Data Controls rule. The dialog looks like this (exact strings vary
// depending on what action is blocked):
//
// +---------------------------------------------------+
// |                                                    |
// |  Pasting this content to this site is not allowed  |
// |  +---+                                             |
// |  | E | Your administrator has blocked this action  |
// |  +---+                                             |
// |                                                    |
// |                           +--------+  +---------+  |
// |                           | Cancel |  | Proceed |  |
// |                           +--------+  +---------+  |
// +----------------------------------------------------+
//
// * The "E" box represents the enterprise logo that looks like a building.
// * The "Cancel"/"Proceed" choice is only available for warnings, blocked
//   actions only have a "Close" button.
class DataControlsDialog {
 public:
  // Represents the type of dialog, based on the action that triggered it and
  // severity of the triggered rule. This will change the strings in the dialog,
  // available buttons, etc.
  enum class Type {
    kClipboardPasteBlock,
    kClipboardPasteWarn,
    kClipboardCopyBlock,
    kClipboardCopyWarn,
  };

  ~DataControlsDialog();

  Type type() const;

  // Accessors for `callbacks_`.
  void AddCallback(base::OnceCallback<void(bool bypassed)> callback);
  void ClearCallbacks();

 protected:
  // Shows the dialog asynchronously. `on_destructed` is expected to be called
  // at the start of the destructor to let the caller of `Show()` know they
  // should cleanup any reference to this dialog.
  virtual void Show(base::OnceClosure on_destructed) = 0;

  friend DataControlsDialogFactory;

  DataControlsDialog(Type type,
                     base::OnceCallback<void(bool bypassed)> callback);

  // Calls `callbacks_` with the value in `bypasses`. "true" represents the user
  // ignoring a warning and proceeding with the action, "false" corresponds to
  // the user cancelling their action after seeing a warning. This should not be
  // called for blocking dialogs.
  void OnDialogButtonClicked(bool bypassed);

  Type type_;

  // Called when the dialog closes, with `true` in the case of a bypassed
  // warning.
  std::vector<base::OnceCallback<void(bool bypassed)>> callbacks_;
};

}  // namespace data_controls

#endif  // COMPONENTS_ENTERPRISE_DATA_CONTROLS_CORE_BROWSER_DATA_CONTROLS_DIALOG_H_
