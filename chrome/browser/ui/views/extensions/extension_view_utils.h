// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_UTILS_H_

#include "ui/gfx/native_ui_types.h"

class Browser;
class ExtensionsToolbarContainer;
class ToolbarActionViewModel;

namespace content {
class WebContents;
}

namespace ui {
class ImageModel;
}

// Returns the extensions toolbar container in `browser` or `parent`, if
// existent.
ExtensionsToolbarContainer* GetExtensionsToolbarContainer(Browser* browser);
ExtensionsToolbarContainer* GetExtensionsToolbarContainer(
    gfx::NativeWindow parent);

// Returns the icon corresponding to `action` for the given `web_contents`.
ui::ImageModel GetIcon(ToolbarActionViewModel* action,
                       content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_UTILS_H_
