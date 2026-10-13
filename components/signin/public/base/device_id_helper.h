// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_PUBLIC_BASE_DEVICE_ID_HELPER_H_
#define COMPONENTS_SIGNIN_PUBLIC_BASE_DEVICE_ID_HELPER_H_

#include <string>

#include "base/component_export.h"
#include "build/build_config.h"

class PrefService;

namespace signin {

#if !BUILDFLAG(IS_CHROMEOS)

// Returns device id that is scoped to single signin. This device id will be
// regenerated if user signs out and signs back in.
// When refresh token is requested for this user it will be annotated with
// this device id.
COMPONENT_EXPORT(SIGNIN_DEVICE_ID_HELPER)
std::string GetSigninScopedDeviceId(PrefService* prefs);

// Forces the generation of a new device ID, and stores it in the pref service.
COMPONENT_EXPORT(SIGNIN_DEVICE_ID_HELPER)
std::string RecreateSigninScopedDeviceId(PrefService* prefs);

// Helper method. The device ID should generally be obtained through
// GetSigninScopedDeviceId().
// Creates a new device ID value.
COMPONENT_EXPORT(SIGNIN_DEVICE_ID_HELPER)
std::string GenerateSigninScopedDeviceId();

#else

// Returns a signin-scoped device ID for a ChromeOS user session.
//
// For ephemeral users a fresh identifier is generated on every call, prefixed
// so that it can be recognized as ephemeral. Otherwise the identifier is
// generated once and cached for the lifetime of the process, so that every
// caller in a session observes the same value -- which is why this lives in a
// component rather than being linked into each consumer.
COMPONENT_EXPORT(SIGNIN_DEVICE_ID_HELPER)
std::string GenerateSigninScopedDeviceId(bool for_ephemeral);

#endif

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_PUBLIC_BASE_DEVICE_ID_HELPER_H_
