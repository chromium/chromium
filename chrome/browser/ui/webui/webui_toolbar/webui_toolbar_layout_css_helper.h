// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_LAYOUT_CSS_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_LAYOUT_CSS_HELPER_H_

#include <string>

#include "content/public/browser/web_ui_data_source.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"

/* Provides a CSS file defining CSS variables corresponding to all the
 * LayoutConstants. This should normally be hooked up as a request filter. */
class WebUIToolbarLayoutCssHelper {
 public:
  // Generates the CSS content for layout constants.
  static std::string GenerateLayoutConstantsCss();

  static bool ShouldHandleRequest(const std::string& path);
  static void HandleRequest(const std::string& path,
                            content::WebUIDataSource::GotDataCallback callback);

  static void SetAsRequestFilter(content::WebUIDataSource* source);

  // Configures local resource loading for the layout CSS.
  static void PopulateLocalResourceLoaderConfig(
      blink::mojom::LocalResourceLoaderConfig* config);
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_LAYOUT_CSS_HELPER_H_
