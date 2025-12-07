// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_DIALOG_UTILS_H_
#define CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_DIALOG_UTILS_H_

#include <memory>
#include <vector>

#include "build/build_config.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_id.h"
#include "ui/gfx/native_ui_types.h"

namespace content {
class WebContents;
}  // namespace content

namespace ui {
class DialogModel;
}  // namespace ui

class Browser;
class ExtensionsToolbarContainer;

// Shows the dialog constructed from `dialog_model` for a single extension. This
// may be anchored to the extension's UI in the browser if available or
// shown as a modal dialog. This is similar to the overload taking a
// `std::vector<extensions::ExtensionId>`, but for a single extension.
void ShowDialog(gfx::NativeWindow parent,
                const extensions::ExtensionId& extension_id,
                std::unique_ptr<ui::DialogModel> dialog_model);

// Shows the dialog constructed from `dialog_model` for a set of extensions.
// This may be anchored to the extensions' UI in the browser if
// available or shown as a modal dialog.
void ShowDialog(gfx::NativeWindow parent,
                const std::vector<extensions::ExtensionId>& extension_ids,
                std::unique_ptr<ui::DialogModel> dialog_model);

// Shows a modal dialog constructed from `dialog_model` on the `parent` window.
void ShowModalDialog(gfx::NativeWindow parent,
                     std::unique_ptr<ui::DialogModel> dialog_model);

// Shows a modal dialog constructed from `dialog_model` on `web_contents`.
void ShowWebModalDialog(content::WebContents* web_contents,
                        std::unique_ptr<ui::DialogModel> dialog_model);

#if defined(TOOLKIT_VIEWS)
// Shows the dialog constructed from `dialog_model` for `extension_ids` and
// is anchored to `container`.
void ShowDialog(ExtensionsToolbarContainer* container,
                const std::vector<extensions::ExtensionId>& extension_ids,
                std::unique_ptr<ui::DialogModel> dialog_model);

// Shows the dialog constructed from `dialog_model` in `browser`.
void ShowDialog(Browser* browser,
                std::unique_ptr<ui::DialogModel> dialog_model);
#endif  // defined(TOOLKIT_VIEWS)

#endif  // CHROME_BROWSER_UI_EXTENSIONS_EXTENSION_DIALOG_UTILS_H_
