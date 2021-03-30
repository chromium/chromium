// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_APP_THEME_COLOR_INFO_H_
#define CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_APP_THEME_COLOR_INFO_H_

#include "base/macros.h"
#include "base/optional.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handler.h"
#include "third_party/skia/include/core/SkColor.h"

namespace extensions {

// A structure to hold the parsed app theme color data.
struct AppThemeColorInfo : public Extension::ManifestData {
  AppThemeColorInfo();
  ~AppThemeColorInfo() override;

  static base::Optional<SkColor> GetThemeColor(const Extension* extension);

  // The color to use for the browser frame.
  base::Optional<SkColor> theme_color;
};

// Parses the "app.theme_color" manifest key.
class AppThemeColorHandler : public ManifestHandler {
 public:
  AppThemeColorHandler();
  ~AppThemeColorHandler() override;

  bool Parse(Extension* extension, std::u16string* error) override;

 private:
  base::span<const char* const> Keys() const override;

  DISALLOW_COPY_AND_ASSIGN(AppThemeColorHandler);
};

}  // namespace extensions

#endif  // CHROME_COMMON_EXTENSIONS_MANIFEST_HANDLERS_APP_THEME_COLOR_INFO_H_
