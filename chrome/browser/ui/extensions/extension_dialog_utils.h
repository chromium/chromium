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
#include "ui/gfx/native_widget_types.h"

namespace ui {
class DialogModel;
}

class Browser;
class ExtensionsToolbarContainer;

// Shows the dialog constructed from `dialog_model` anchored to the view
// corresponding to `extension_id` in the extensions container. This is similar
// to the overload taking a `std::vector<extensions::ExtensionId>`, but for a
// single extension. If `parent` does not have an extensions container, it will
// display a browser-modal dialog instead.
void ShowDialog(gfx::NativeWindow parent,
                const extensions::ExtensionId& extension_id,
                std::unique_ptr<ui::DialogModel> dialog_model);

// Shows the dialog constructed from `dialog_model` anchored to the view
// corresponding to `extension_ids` in the extensions container. If `parent`
// does not have an extensions container, it will display a browser-modal dialog
// instead.
void ShowDialog(gfx::NativeWindow parent,
                const std::vector<extensions::ExtensionId>& extension_ids,
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
