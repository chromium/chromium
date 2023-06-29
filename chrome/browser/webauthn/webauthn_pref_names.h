// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBAUTHN_WEBAUTHN_PREF_NAMES_H_
#define CHROME_BROWSER_WEBAUTHN_WEBAUTHN_PREF_NAMES_H_

namespace webauthn::pref_names {

// Maps to the WebAuthenticationRemoteProxiedRequestsAllowed enterprise
// policy.
extern const char kRemoteProxiedRequestsAllowed[];

// Maps to the AllowWebAuthnWithBrokenCerts enterprise policy.
extern const char kAllowWithBrokenCerts[];

// The most recently used phone pairing from sync, identified by its public key
// encoded in base64. If there is no last recently used phone, the preference
// will be an empty string.
extern const char kLastUsedPairingFromSyncPublicKey[];

}  // namespace webauthn::pref_names

#endif  // CHROME_BROWSER_WEBAUTHN_WEBAUTHN_PREF_NAMES_H_
