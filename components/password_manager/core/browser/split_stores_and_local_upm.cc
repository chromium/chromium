// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/split_stores_and_local_upm.h"

#include "base/android/device_info.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "build/buildflag.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"

namespace password_manager {

namespace {

// Do not expose these constants! Use GetSplitStoresUpmMinVersion() instead.
const int kSplitStoresUpmMinVersionForNonAuto = 240212000;
const int kSplitStoresUpmMinVersionForAuto = 241512000;

}  // namespace

bool IsGmsCoreUpdateRequired() {
#if BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  return false;
#else
  const std::string& gms_version_str =
      base::android::device_info::gms_version_code();
  int gms_version;
  // GMSCore version could not be parsed, probably no GMSCore installed.
  if (!base::StringToInt(gms_version_str, &gms_version)) {
    return true;
  }
  // Returns whether GMSCore version is pre account/local password separation.
  return gms_version < GetSplitStoresUpmMinVersion();
#endif  //  BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
}

int GetSplitStoresUpmMinVersion() {
  return base::android::device_info::is_automotive()
             ? kSplitStoresUpmMinVersionForAuto
             : kSplitStoresUpmMinVersionForNonAuto;
}

}  // namespace password_manager
