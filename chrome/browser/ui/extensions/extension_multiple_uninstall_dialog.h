// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MULTIPLE_UNINSTALL_DIALOG_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MULTIPLE_UNINSTALL_DIALOG_H_

#include <vector>

#include "base/functional/callback_forward.h"
#include "extensions/common/extension_id.h"
#include "ui/gfx/native_ui_types.h"

class Profile;

namespace extensions {

// Shows a modal dialog to users when they uninstall multiple extensions.
// When the dialog is accepted, `accept_callback` is invoked.
// When the dialog is canceled, `cancel_callback` is invoked.
void ShowExtensionMultipleUninstallDialog(
    Profile* profile,
    gfx::NativeWindow parent,
    const std::vector<ExtensionId>& extension_ids,
    base::OnceClosure accept_callback,
    base::OnceClosure cancel_callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_MULTIPLE_UNINSTALL_DIALOG_H_
