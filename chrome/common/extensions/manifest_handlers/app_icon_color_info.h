// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_APP_ICON_COLOR_INFO_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_APP_ICON_COLOR_INFO_H_

#include "base/macros.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "third_party/skia/include/core/SkColor.h"

namespace extensions {

// A structure to hold the parsed app icon color data.
struct AppIconColorInfo : public Extension::ManifestData {
  AppIconColorInfo();
  ~AppIconColorInfo() override;

  static SkColor GetIconColor(const Extension* extension);
  static const std::string& GetIconColorString(const Extension* extension);

  // The color to use if icons need to be generated for the app.
  SkColor icon_color_;

  // The string representation of the icon color.
  std::string icon_color_string_;
};

// Parses the "app.icon_color" manifest key.
class AppIconColorHandler : public ManifestHandler {
 public:
  AppIconColorHandler();
  ~AppIconColorHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(AppIconColorHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_APP_ICON_COLOR_INFO_H_
