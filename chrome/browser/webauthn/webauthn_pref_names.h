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

}  // namespace webauthn::pref_names

#endif  // CHROME_BROWSER_WEBAUTHN_WEBAUTHN_PREF_NAMES_H_
