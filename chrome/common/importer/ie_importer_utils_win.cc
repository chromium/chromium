// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/importer/ie_importer_utils_win.h"

#include "chrome/common/importer/importer_test_registry_overrider_win.h"

namespace {

const wchar_t kIEFavoritesOrderKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\MenuOrder\\"
    L"Favorites";

const wchar_t kIESettingsMainKey[] =
    L"Software\\Microsoft\\Internet Explorer\\Main";

std::wstring GetPotentiallyOverridenIEKey(
    const std::wstring& desired_key_path) {
  std::wstring test_reg_override(
      ImporterTestRegistryOverrider::GetTestRegistryOverride());
  return test_reg_override.empty() ? desired_key_path : test_reg_override;
}

}  // namespace

namespace importer {

std::wstring GetIEFavoritesOrderKey() {
  // Return kIEFavoritesOrderKey unless an override has been set for tests.
  return GetPotentiallyOverridenIEKey(kIEFavoritesOrderKey);
}

std::wstring GetIESettingsKey() {
  // Return kIESettingsMainKey unless an override has been set for tests.
  return GetPotentiallyOverridenIEKey(kIESettingsMainKey);
}

}  // namespace importer

