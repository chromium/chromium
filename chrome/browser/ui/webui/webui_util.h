// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_H_

#include <string>

#include "base/containers/span.h"
#include "base/strings/string_piece.h"
#include "build/build_config.h"
#include "ui/base/webui/resource_path.h"

namespace content {
class WebContents;
class WebUIDataSource;
}

namespace ui {
class NativeTheme;
}

namespace webui {

// Performs common setup steps for a |source| using JS modules: enable i18n
// string replacements, adding test resources, and updating CSP/trusted types to
// allow tests to work.
// UIs that don't have a dedicated grd file should generally use this utility.
void SetJSModuleDefaults(content::WebUIDataSource* source);

// Calls SetJSModuleDefaults(), and additionally adds all resources in the
// resource map to |source| and sets |default_resource| as the default resource.
// UIs that have a dedicated grd file should generally use this utility.
void SetupWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const ResourcePath> resources,
                          int default_resource);

// Returns whether the device is enterprise managed. Note that on Linux, there's
// no good way of detecting whether the device is managed, so always return
// false.
bool IsEnterpriseManaged();

#if defined(TOOLKIT_VIEWS)
// Returns whether WebContents should use dark mode colors depending on the
// theme.
ui::NativeTheme* GetNativeTheme(content::WebContents* web_contents);
#endif

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_H_
