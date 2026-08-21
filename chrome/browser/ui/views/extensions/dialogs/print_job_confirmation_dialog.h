// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_DIALOGS_PRINT_JOB_CONFIRMATION_DIALOG_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_DIALOGS_PRINT_JOB_CONFIRMATION_DIALOG_H_

#include <string>

#include "base/functional/callback_forward.h"
#include "build/build_config.h"
#include "extensions/common/extension_id.h"
#include "ui/gfx/native_ui_types.h"

#if BUILDFLAG(IS_CHROMEOS)

namespace gfx {
class ImageSkia;
}

namespace extensions {

// Shows the print job confirmation dialog bubble anchored to the toolbar icon
// for the extension.  If there's no toolbar icon or parent, it will display a
// browser-modal dialog instead.
void ShowPrintJobConfirmationDialog(gfx::NativeWindow parent,
                                    const ExtensionId& extension_id,
                                    const std::u16string& extension_name,
                                    const gfx::ImageSkia& extension_icon,
                                    const std::u16string& print_job_title,
                                    const std::u16string& printer_name,
                                    base::OnceCallback<void(bool)> callback);

}  // namespace extensions

#endif  // BUILDFLAG(IS_CHROMEOS)

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_DIALOGS_PRINT_JOB_CONFIRMATION_DIALOG_H_
