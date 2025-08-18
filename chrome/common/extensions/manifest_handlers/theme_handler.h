// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_THEME_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_THEME_HANDLER_H_

#include <string>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "extensions/common/extension.h"
#include "extensions/common/extension_resource.h"
#include "extensions/common/manifest_handler.h"

namespace extensions {

// A structure to hold the parsed theme data.
struct ThemeInfo : public Extension::ManifestData {
  // Define out of line constructor/destructor to please Clang.
  ThemeInfo();
  ~ThemeInfo() override;

  struct ThemeResource {
    ExtensionResource resource;
    std::string scale;
  };

  using ThemeImages = base::flat_map<std::string, std::vector<ThemeResource>>;

  static const ThemeImages* GetImages(const Extension* extension);
  static const base::Value::Dict* GetColors(const Extension* extension);
  static const base::Value::Dict* GetTints(const Extension* extension);
  static const base::Value::Dict* GetDisplayProperties(
      const Extension* extension);
  static const base::Value::Dict* GetTabGroupColorPalette(
      const Extension* extension);

  // A map of resource ids to ExtensionResource entries.
  ThemeImages theme_images_;

  // A map of color names to colors.
  base::Value::Dict theme_colors_;

  // A map of color names to colors.
  base::Value::Dict theme_tints_;

  // A map of display properties.
  base::Value::Dict theme_display_properties_;

  // Maps a palette color key to a hue value (range: -1 to 360).
  // Example:
  // {
  //   "grey_override": 230,
  //   "blue_override": 12,
  //   "green_override": 300,
  //   "cyan_override": -1   // -1 indicates a grey/black color.
  // }
  base::Value::Dict theme_tab_group_color_palette_;
};

// Parses the "theme" manifest key.
class ThemeHandler : public ManifestHandler {
 public:
  ThemeHandler();

  ThemeHandler(const ThemeHandler&) = delete;
  ThemeHandler& operator=(const ThemeHandler&) = delete;

  ~ThemeHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool Validate(const Extension& extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  base::span<const char* const> Keys() const override;
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_THEME_HANDLER_H_
