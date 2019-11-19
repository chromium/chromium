// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/manifest_handlers/app_display_mode_info.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "extensions/common/install_warning.h"
#include "extensions/common/manifest.h"
#include "extensions/common/manifest_constants.h"

namespace extensions {

namespace keys = manifest_keys;
namespace errors = manifest_errors;

AppDisplayModeInfo::AppDisplayModeInfo() = default;

AppDisplayModeInfo::~AppDisplayModeInfo() = default;

// static
blink::mojom::DisplayMode AppDisplayModeInfo::GetDisplayMode(
    const Extension* extension) {
  auto* info = static_cast<AppDisplayModeInfo*>(
      extension->GetManifestData(keys::kAppDisplayMode));
  return info ? info->display_mode : blink::mojom::DisplayMode::kStandalone;
}

AppDisplayModeHandler::AppDisplayModeHandler() = default;

AppDisplayModeHandler::~AppDisplayModeHandler() = default;

bool AppDisplayModeHandler::Parse(Extension* extension, base::string16* error) {
  std::string display_mode_string;
  blink::mojom::DisplayMode display_mode =
      blink::mojom::DisplayMode::kUndefined;
  if (extension->manifest()->GetString(keys::kAppDisplayMode,
                                       &display_mode_string)) {
    display_mode = blink::DisplayModeFromString(display_mode_string);
  }

  if (display_mode == blink::mojom::DisplayMode::kUndefined) {
    *error = base::UTF8ToUTF16(errors::kInvalidAppDisplayMode);
    return false;
  }

  // Currently, only allow the display_mode key for bookmark apps. We'll add
  // an install warning in Validate().
  if (!extension->from_bookmark()) {
    extension->AddInstallWarning(
        InstallWarning(errors::kInvalidDisplayModeAppType));
    return true;
  }

  auto app_display_mode_info = std::make_unique<AppDisplayModeInfo>();
  app_display_mode_info->display_mode = display_mode;
  extension->SetManifestData(keys::kAppDisplayMode,
                             std::move(app_display_mode_info));

  return true;
}

base::span<const char* const> AppDisplayModeHandler::Keys() const {
  static constexpr const char* kKeys[] = {keys::kAppDisplayMode};
  return kKeys;
}

}  // namespace extensions
