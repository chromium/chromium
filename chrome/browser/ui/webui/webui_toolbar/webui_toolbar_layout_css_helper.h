// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_LAYOUT_CSS_HELPER_H_
#define CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_LAYOUT_CSS_HELPER_H_

#include <string>
#include <string_view>

#include "content/public/browser/web_ui_data_source.h"
#include "third_party/blink/public/mojom/loader/local_resource_loader_config.mojom.h"

namespace gfx {
class Insets;
}  // namespace gfx

namespace views {
class TypographyProvider;
}  // namespace views

/* Provides a CSS file defining CSS variables corresponding to all the
 * LayoutConstants, and some fonts. This should normally be hooked up as a
 * request filter. */
class WebUIToolbarLayoutCssHelper {
 public:
  // Generates the CSS content for layout constants and fonts.
  static std::string GenerateLayoutConstantsCss();

  static bool ShouldHandleRequest(const std::string& path);
  static void HandleRequest(const std::string& path,
                            content::WebUIDataSource::GotDataCallback callback);

  static void SetAsRequestFilter(content::WebUIDataSource* source);

  // Configures local resource loading for the layout CSS.
  static void PopulateLocalResourceLoaderConfig(
      blink::mojom::LocalResourceLoaderConfig* config);

  // Escapes name of CSS font, trying to preserve the original codepoint
  // sequence. Assumes valid utf-8.
  //
  // The output should be wrapped in double quotes.
  static std::string EscapeCssFontName(std::string_view in);

 private:
  static void AddFontVariables(
      std::string_view prefix,
      int context,
      int style,
      const views::TypographyProvider& typography_provider,
      std::string& out);

  static void AddInsets(std::string_view prefix,
                        const gfx::Insets& insets,
                        std::string& css_string);
};

#endif  // CHROME_BROWSER_UI_WEBUI_WEBUI_TOOLBAR_WEBUI_TOOLBAR_LAYOUT_CSS_HELPER_H_
