// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_switches.h"

namespace switches {

// Clears the token service before using it. This allows simulating the
// expiration of credentials during testing.
const char kClearTokenService[] = "clear-token-service";

// Disables sending signin scoped device id to LSO with refresh token request.
const char kDisableSigninScopedDeviceId[] = "disable-signin-scoped-device-id";

#if !BUILDFLAG(ENABLE_MIRROR)
// Command line flag for enabling account consistency. Default mode is disabled.
// Mirror is a legacy mode in which Google accounts are always addded to Chrome,
// and Chrome then adds them to the Google authentication cookies.
// Dice is a new experiment in which Chrome is aware of the accounts in the
// Google authentication cookies.
const char kAccountConsistency[] = "account-consistency";

// Values for the kAccountConsistency flag.
const char kAccountConsistencyMirror[] = "mirror";
const char kAccountConsistencyDice[] = "dice";
#endif

}  // namespace switches
