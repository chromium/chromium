// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_THEME_HANDLER_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_THEME_HANDLER_H_

#include <memory>

#include "base/macros.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"

namespace base {
class DictionaryValue;
}

namespace extensions {

// A structure to hold the parsed theme data.
struct ThemeInfo : public Extension::ManifestData {
  // Define out of line constructor/destructor to please Clang.
  ThemeInfo();
  ~ThemeInfo() override;

  static const base::DictionaryValue* GetImages(const Extension* extension);
  static const base::DictionaryValue* GetColors(const Extension* extension);
  static const base::DictionaryValue* GetTints(const Extension* extension);
  static const base::DictionaryValue* GetDisplayProperties(
      const Extension* extension);

  // A map of resource id's to relative file paths.
  std::unique_ptr<base::DictionaryValue> theme_images_;

  // A map of color names to colors.
  std::unique_ptr<base::DictionaryValue> theme_colors_;

  // A map of color names to colors.
  std::unique_ptr<base::DictionaryValue> theme_tints_;

  // A map of display properties.
  std::unique_ptr<base::DictionaryValue> theme_display_properties_;
};

// Parses the "theme" manifest key.
class ThemeHandler : public ManifestHandler {
 public:
  ThemeHandler();
  ~ThemeHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;
  bool Validate(const Extension* extension,
                std::string* error,
                std::vector<InstallWarning>* warnings) const override;

 private:
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(ThemeHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_THEME_HANDLER_H_
