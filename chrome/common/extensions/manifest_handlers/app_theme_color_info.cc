// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/app_theme_color_info.h"

#include <memory>

#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/image_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

AppThemeColorInfo::AppThemeColorInfo() {}

AppThemeColorInfo::~AppThemeColorInfo() {}

// static
base::Optional<SkColor> AppThemeColorInfo::GetThemeColor(
    const Extension* extension) {
  AppThemeColorInfo* info = static_cast<AppThemeColorInfo*>(
      extension->GetManifestData(keys::kAppThemeColor));
  return info ? info->theme_color : base::Optional<SkColor>();
}

AppThemeColorHandler::AppThemeColorHandler() {}

AppThemeColorHandler::~AppThemeColorHandler() {}

bool AppThemeColorHandler::Parse(Extension* extension, std::u16string* error) {
  std::string theme_color_string;
  SkColor theme_color = SK_ColorTRANSPARENT;
  if (!extension->manifest()->GetString(keys::kAppThemeColor,
                                        &theme_color_string) ||
      !image_util::ParseRgbColorString(theme_color_string, &theme_color)) {
    *error = base::UTF8ToUTF16(errors::kInvalidAppThemeColor);
    return false;
  }

  // Currently, only allow the theme_color key for bookmark apps. We'll add
  // an install warning in Validate().
  if (!extension->from_bookmark()) {
    extension->AddInstallWarning(
        InstallWarning(errors::kInvalidThemeColorAppType));
    return true;
  }

  auto app_theme_color_info = std::make_unique<AppThemeColorInfo>();
  app_theme_color_info->theme_color = static_cast<SkColor>(theme_color);
  extension->SetManifestData(keys::kAppThemeColor,
                             std::move(app_theme_color_info));

  return true;
}

base::span<const char* const> AppThemeColorHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kAppThemeColor};
  return kKeys;
}

}  // namespace extensions
