// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_UTILS_H_
#define CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_UTILS_H_

#include "base/memory/weak_ptr.h"
#include "ui/gfx/native_ui_types.h"

class ExtensionsContainerViews;
class ToolbarActionViewModel;

namespace content {
class WebContents;
}

namespace ui {
class ImageModel;
}

// Sets an override `ExtensionsContainerViews` for `web_contents`. When set,
// `GetExtensionsContainerViews(content::WebContents*)` returns this container
// instead of the browser toolbar's container.
void SetExtensionsContainerViewsForWebContents(
    content::WebContents* web_contents,
    base::WeakPtr<ExtensionsContainerViews> container);

// Returns the extensions toolbar container in `parent`, if existent.
ExtensionsContainerViews* GetExtensionsContainerViews(gfx::NativeWindow parent);

// Returns the extensions container associated with `web_contents`, using any
// override set via `SetExtensionsContainerViewsForWebContents()` or falling
// back to the container in `web_contents->GetTopLevelNativeWindow()`.
ExtensionsContainerViews* GetExtensionsContainerViews(
    content::WebContents* web_contents);

// Returns the icon corresponding to `action` for the given `web_contents`.
ui::ImageModel GetIcon(ToolbarActionViewModel* action,
                       content::WebContents* web_contents);

#endif  // CHROME_BROWSER_UI_VIEWS_EXTENSIONS_EXTENSION_VIEW_UTILS_H_
