// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_GPM_USER_VERIFICATION_POLICY_H_
#define CHROME_BROWSER_WEBAUTHN_GPM_USER_VERIFICATION_POLICY_H_

namespace device {
enum class UserVerificationRequirement;
}

// Returns whether Google Password Manager policy is to do user verification for
// the given requirement.
bool GpmWillDoUserVerification(device::UserVerificationRequirement requirement,
                               bool platform_has_biometrics);

#endif  // CHROME_BROWSER_WEBAUTHN_GPM_USER_VERIFICATION_POLICY_H_
