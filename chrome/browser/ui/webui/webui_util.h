// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_H_

#include "base/containers/span.h"
#include "build/build_config.h"
#include "ui/base/webui/resource_path.h"

namespace content {
class WebUIDataSource;
}

namespace webui {

// Performs common setup steps for a |source| using JS modules: enable i18n
// string replacements, adding test resources, and configuring script-src CSP
// headers to allow tests to work.
// UIs that don't have a dedicated grd file should generally use this utility.
void SetJSModuleDefaults(content::WebUIDataSource* source);

// Calls SetJSModuleDefaults(), and additionally adds all resources in the
// resource map to |source| and sets |default_resource| as the default resource.
// UIs that have a dedicated grd file should generally use this utility.
void SetupWebUIDataSource(content::WebUIDataSource* source,
                          base::span<const ResourcePath> resources,
                          int default_resource);

// Enables the 'trusted-types' CSP for the given WebUIDataSource. This is the
// default behavior when calling SetupWebUIDataSource().
void EnableTrustedTypesCSP(content::WebUIDataSource* source);

// Adds string for |id| to |source| and removes & from the string to allow for
// reuse of generic strings.
void AddLocalizedString(content::WebUIDataSource* source,
                        const std::string& message,
                        int id);

}  // namespace webui

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_UTIL_H_
