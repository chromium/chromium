// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_WEBAUTHN_SWITCHES_H_
#define CHROME_BROWSER_WEBAUTHN_WEBAUTHN_SWITCHES_H_

namespace webauthn::switches {

// Allows specifying one additional origin for the
// WebAuthenticationRemoteProxiedRequestsAllowed enterprise policy. The origin
// must be ordinarily allowed to perform WebAuthn requests (i.e. it must either
// be secure or http://localhost).
extern const char kRemoteProxiedRequestsAllowedAdditionalOrigin[];

// A list of origins that are permitted to request enterprise attestation when
// creating a WebAuthn credential.
extern const char kPermitEnterpriseAttestationOriginList[];

// The reauth URL for changing the Password Manager PIN.
extern const char kGpmPinResetReauthUrlSwitch[];

}  // namespace webauthn::switches

#endif  // CHROME_BROWSER_WEBAUTHN_WEBAUTHN_SWITCHES_H_
