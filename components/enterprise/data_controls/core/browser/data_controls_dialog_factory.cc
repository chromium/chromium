// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/data_controls/core/browser/data_controls_dialog_factory.h"

#include <map>

#include "base/memory/singleton.h"

namespace data_controls {

namespace {

// Helper that keeps track of dialogs currently showing for given
// WebContents-type pair.  These are used to determine if a call to
// `DataControlsDialog::Show` is redundant or not. Keyed with `void*` instead of
// `content::WebContents*` to avoid accidental bugs from dereferencing that
// pointer.
std::map<std::pair<void*, DataControlsDialog::Type>, DataControlsDialog*>&
CurrentDialogsStorage() {
  static std::map<std::pair<void*, DataControlsDialog::Type>,
                  DataControlsDialog*>
      dialogs;
  return dialogs;
}

// Returns null if no dialog is currently shown on `web_contents` for `type`.
DataControlsDialog* GetCurrentDialog(content::WebContents* web_contents,
                                     DataControlsDialog::Type type) {
  if (CurrentDialogsStorage().count({web_contents, type})) {
    return CurrentDialogsStorage().at({web_contents, type});
  }
  return nullptr;
}

}  // namespace

void DataControlsDialogFactory::ShowDialogIfNeeded(
    content::WebContents* web_contents,
    DataControlsDialog::Type type,
    base::OnceCallback<void(bool bypassed)> callback) {
  DCHECK(web_contents);

  // Don't show a new dialog if there is already an existing dialog of the same
  // type showing in `web_contents` already. If `callback` is non-null, we add
  // it to the currently showing dialog.
  if (auto* dialog = GetCurrentDialog(web_contents, type)) {
    if (callback) {
      dialog->callbacks_.push_back(std::move(callback));
    }
    return;
  }

  auto* dialog = CreateDialog(type, web_contents, std::move(callback));

  std::pair<void*, DataControlsDialog::Type> key = {web_contents, type};
  CurrentDialogsStorage()[key] = dialog;
  dialog->Show(base::BindOnce(
      [](std::pair<void*, DataControlsDialog::Type> key) {
        CurrentDialogsStorage().erase(key);
      },
      key));
}

}  // namespace data_controls
