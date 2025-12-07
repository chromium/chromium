// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/webauthn/webauthn_pref_names.h"

namespace webauthn::pref_names {

const char kAllowWithBrokenCerts[] = "webauthn.allow_with_broken_certs";

const char kEnclaveDeclinedGPMCredentialCreationCount[] =
    "webauthn.enclave_declined_gpm_credential_creation_count";

const char kEnclaveDeclinedGPMBootstrappingCount[] =
    "webauthn.enclave_declined_gpm_bootstrapping_count";

const char kEnclaveFailedPINAttemptsCount[] =
    "webauthn.enclave_failed_pin_attempts_count";

const char kRemoteProxiedRequestsAllowed[] =
    "webauthn.remote_proxied_requests_allowed";

const char kRemoteDesktopAllowedOrigins[] =
    "webauthn.remote_desktop_allowed_origins";

const char kWebAuthnTouchIdMetadataSecretPrefName[] =
    "webauthn.touchid.metadata_secret";

}  // namespace webauthn::pref_names
