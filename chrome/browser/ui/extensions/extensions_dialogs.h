// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_DIALOGS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_DIALOGS_H_

#include "base/callback_forward.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "extensions/common/extension_id.h"
#include "ui/gfx/native_widget_types.h"

class Browser;

namespace gfx {
class ImageSkia;
}  // namespace gfx

namespace extensions {

// Shows a dialog when extensions require a refresh for their action
// to be run or blocked. The dialog content is based on whether caller
// `is_updating_permissions`. When the dialog is accepted, `callback` is
// invoked.
void ShowReloadPageDialog(
    Browser* browser,
    const std::vector<extensions::ExtensionId>& extension_ids,
    bool is_updating_permissions,
    base::OnceClosure callback);

#if BUILDFLAG(IS_CHROMEOS)

// Shows the print job confirmation dialog bubble anchored to the toolbar icon
// for the extension.
// If there's no toolbar icon, shows a modal dialog using
// CreateBrowserModalDialogViews(). Note that this dialog is shown even if there
// is no `parent` window.
void ShowPrintJobConfirmationDialog(gfx::NativeWindow parent,
                                    const ExtensionId& extension_id,
                                    const std::u16string& extension_name,
                                    const gfx::ImageSkia& extension_icon,
                                    const std::u16string& print_job_title,
                                    const std::u16string& printer_name,
                                    base::OnceCallback<void(bool)> callback);

#endif  // BUILDFLAG(IS_CHROMEOS)

}  // namespace extensions

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSIONS_DIALOGS_H_
