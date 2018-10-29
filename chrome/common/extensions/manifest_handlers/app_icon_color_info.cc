// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/app_icon_color_info.h"

#include <memory>

#include "base/no_destructor.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "extensions/common/image_util.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

namespace {

const AppIconColorInfo& GetAppIconColorInfo(const Extension* extension) {
  static const base::NoDestructor<AppIconColorInfo> fallback;

  AppIconColorInfo* info = static_cast<AppIconColorInfo*>(
      extension->GetManifestData(keys::kAppIconColor));
  return info ? *info : *fallback;
}

}  // namespace

AppIconColorInfo::AppIconColorInfo() : icon_color_(SK_ColorTRANSPARENT) {
}

AppIconColorInfo::~AppIconColorInfo() {
}

// static
SkColor AppIconColorInfo::GetIconColor(const Extension* extension) {
  return GetAppIconColorInfo(extension).icon_color_;
}

// static
const std::string& AppIconColorInfo::GetIconColorString(
    const Extension* extension) {
  return GetAppIconColorInfo(extension).icon_color_string_;
}

AppIconColorHandler::AppIconColorHandler() {
}

AppIconColorHandler::~AppIconColorHandler() {
}

bool AppIconColorHandler::Parse(Extension* extension, base::string16* error) {
  std::unique_ptr<AppIconColorInfo> app_icon_color_info(new AppIconColorInfo);

  const base::Value* temp = NULL;
  if (extension->manifest()->Get(keys::kAppIconColor, &temp)) {
    if (!temp->GetAsString(&app_icon_color_info->icon_color_string_)) {
      *error =
          base::UTF8ToUTF16(extensions::manifest_errors::kInvalidAppIconColor);
      return false;
    }

    if (!image_util::ParseHexColorString(
            app_icon_color_info->icon_color_string_,
            &app_icon_color_info->icon_color_)) {
      *error =
          base::UTF8ToUTF16(extensions::manifest_errors::kInvalidAppIconColor);
      return false;
    }
  }

  extension->SetManifestData(keys::kAppIconColor,
                             std::move(app_icon_color_info));
  return true;
}

base::span<const char* const> AppIconColorHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kAppIconColor};
  return kKeys;
}

}  // namespace extensions
