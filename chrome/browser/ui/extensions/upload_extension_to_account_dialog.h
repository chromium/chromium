// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_UPLOAD_EXTENSION_TO_ACCOUNT_DIALOG_H_
#define CHROME_BROWSER_UI_EXTENSIONS_UPLOAD_EXTENSION_TO_ACCOUNT_DIALOG_H_

#include "base/functional/callback_forward.h"
#include "ui/gfx/native_ui_types.h"

class Profile;

namespace extensions {

class Extension;

// Shows a dialog when the user tries to upload an extension to their account.
void ShowUploadExtensionToAccountDialog(Profile* profile,
                                        gfx::NativeWindow parent,
                                        const Extension& extension,
                                        base::OnceClosure accept_callback,
                                        base::OnceClosure cancel_callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_UPLOAD_EXTENSION_TO_ACCOUNT_DIALOG_H_
