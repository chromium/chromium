// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_DIALOGS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_DIALOGS_H_

#include "base/callback_forward.h"
#include "extensions/common/extension_id.h"

class Browser;

namespace extensions {

// Shows a dialog when an extension requires a refresh after gaining access to
// the current site in order to run its blocked action. When the dialog is
// accepted, `callback` is invoked. with a booleand indication whether the
// checkbox (if shown) was checked by the user.
void ShowBlockedActionDialog(Browser* browser,
                             const ExtensionId& extension_id,
                             bool show_checkbox,
                             base::OnceCallback<void(bool)> callback);

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_DIALOGS_H_
