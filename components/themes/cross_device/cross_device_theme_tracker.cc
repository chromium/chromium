// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/themes/cross_device/cross_device_theme_tracker.h"

namespace themes {

PlatformThemeInfo::PlatformThemeInfo() = default;
PlatformThemeInfo::PlatformThemeInfo(const PlatformThemeInfo&) = default;
PlatformThemeInfo& PlatformThemeInfo::operator=(const PlatformThemeInfo&) =
    default;
PlatformThemeInfo::~PlatformThemeInfo() = default;

bool PlatformThemeInfo::operator==(const PlatformThemeInfo& other) const {
  return device_name == other.device_name && os_type == other.os_type &&
         form_factor == other.form_factor && color == other.color &&
         color_variant == other.color_variant &&
         background == other.background && extension == other.extension;
}

PlatformThemeInfo::Background::Background() = default;
PlatformThemeInfo::Background::Background(const Background&) = default;
PlatformThemeInfo::Background& PlatformThemeInfo::Background::operator=(
    const Background&) = default;
PlatformThemeInfo::Background::~Background() = default;

PlatformThemeInfo::Extension::Extension() = default;
PlatformThemeInfo::Extension::Extension(const Extension&) = default;
PlatformThemeInfo::Extension& PlatformThemeInfo::Extension::operator=(
    const Extension&) = default;
PlatformThemeInfo::Extension::~Extension() = default;

}  // namespace themes
