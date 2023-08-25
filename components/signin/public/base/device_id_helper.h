// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_DEVICE_ID_HELPER_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_DEVICE_ID_HELPER_H_

#include <string>

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

class PrefService;

namespace signin {

#if !BUILDFLAG(IS_CHROMEOS_ASH)

// Returns device id that is scoped to single signin. This device id will be
// regenerated if user signs out and signs back in.
// When refresh token is requested for this user it will be annotated with
// this device id.
std::string GetSigninScopedDeviceId(PrefService* prefs);

// Forces the generation of a new device ID, and stores it in the pref service.
std::string RecreateSigninScopedDeviceId(PrefService* prefs);

// Helper method. The device ID should generally be obtained through
// GetSigninScopedDeviceId().
// Creates a new device ID value.
std::string GenerateSigninScopedDeviceId();

#endif

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_DEVICE_ID_HELPER_H_
