// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/importer/ie_importer_utils_win.h"

#include "chrome/common/importer/importer_test_registry_overrider_win.h"

namespace {

const base::char16 kIEFavoritesOrderKey[] = STRING16_LITERAL(
    "Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\MenuOrder\\Favorit"
    "es");

const base::char16 kIEStorage2Key[] = STRING16_LITERAL(
    "Software\\Microsoft\\Internet Explorer\\IntelliForms\\Storage2");

const base::char16 kIESettingsMainKey[] =
    STRING16_LITERAL("Software\\Microsoft\\Internet Explorer\\Main");

base::string16 GetPotentiallyOverridenIEKey(
    const base::string16& desired_key_path) {
  base::string16 test_reg_override(
      ImporterTestRegistryOverrider::GetTestRegistryOverride());
  return test_reg_override.empty() ? desired_key_path : test_reg_override;
}

}  // namespace

namespace importer {

base::string16 GetIEFavoritesOrderKey() {
  // Return kIEFavoritesOrderKey unless an override has been set for tests.
  return GetPotentiallyOverridenIEKey(kIEFavoritesOrderKey);
}

base::string16 GetIE7PasswordsKey() {
  // Return kIEStorage2Key unless an override has been set for tests.
  return GetPotentiallyOverridenIEKey(kIEStorage2Key);
}

base::string16 GetIESettingsKey() {
  // Return kIESettingsMainKey unless an override has been set for tests.
  return GetPotentiallyOverridenIEKey(kIESettingsMainKey);
}

}  // namespace importer

