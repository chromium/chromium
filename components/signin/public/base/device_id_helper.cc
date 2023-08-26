// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/device_id_helper.h"

#include "base/check.h"
#include "base/command_line.h"
#include "base/uuid.h"
#include "build/chromeos_buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"

namespace signin {

#if !BUILDFLAG(IS_CHROMEOS_ASH)

std::string GetSigninScopedDeviceId(PrefService* prefs) {
  std::string signin_scoped_device_id =
      prefs->GetString(prefs::kGoogleServicesSigninScopedDeviceId);
  if (signin_scoped_device_id.empty()) {
    // If device_id doesn't exist then generate new and save in prefs.
    signin_scoped_device_id = RecreateSigninScopedDeviceId(prefs);
  }
  return signin_scoped_device_id;
}

std::string RecreateSigninScopedDeviceId(PrefService* prefs) {
  std::string signin_scoped_device_id = GenerateSigninScopedDeviceId();
  DCHECK(!signin_scoped_device_id.empty());
  prefs->SetString(prefs::kGoogleServicesSigninScopedDeviceId,
                   signin_scoped_device_id);
  return signin_scoped_device_id;
}

std::string GenerateSigninScopedDeviceId() {
  return base::Uuid::GenerateRandomV4().AsLowercaseString();
}

#endif

}  // namespace signin
