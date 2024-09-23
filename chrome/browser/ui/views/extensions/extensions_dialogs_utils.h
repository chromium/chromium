// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_DIALOGS_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_DIALOGS_UTILS_H_

#include "content/public/browser/web_contents.h"
#include "extensions/common/extension_id.h"
#include "ui/views/bubble/bubble_dialog_model_host.h"

namespace content {
class WebContents;
}  // namespace content

class Browser;
class ToolbarActionViewController;
class ExtensionsToolbarContainer;

// Returns the extensions toolbar container in `browser` or `parent`, if
// existent.
ExtensionsToolbarContainer* GetExtensionsToolbarContainer(Browser* browser);
ExtensionsToolbarContainer* GetExtensionsToolbarContainer(
    gfx::NativeWindow parent);

// Returns the icon corresponding to `action` for the given `web_contents`.
ui::ImageModel GetIcon(ToolbarActionViewController* action,
                       content::WebContents* web_contents);

// Returns the host of the `web_content`. This method should only be called when
// web contents are present.
std::u16string GetCurrentHost(content::WebContents* web_contents);

// Shows the dialog constructed from `dialog_model` anchored to the view
// corresponding to `extension_id` in the extensions container. If parent does
// not have an extensions container, it will display a browser-modal dialog
// instead.
void ShowDialog(gfx::NativeWindow parent,
                const extensions::ExtensionId& extension_id,
                std::unique_ptr<ui::DialogModel> dialog_model);

// Shows the dialog constructed from `dialog_model` for `extension_ids` and
// is anchored to `container`.
void ShowDialog(ExtensionsToolbarContainer* container,
                const std::vector<extensions::ExtensionId>& extension_ids,
                std::unique_ptr<ui::DialogModel> dialog_model);

// Shows the dialog constructed from `dialog_model` in `browser`.
void ShowDialog(Browser* browser,
                std::unique_ptr<ui::DialogModel> dialog_model);

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSIONS_DIALOGS_UTILS_H_
