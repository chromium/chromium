// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_DEVICE_TRUST_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_DEVICE_TRUST_UTILS_H_

#include <optional>

#include "build/build_config.h"
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals.mojom.h"
#include "components/enterprise/buildflags/buildflags.h"

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/upload_client_error.h"
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

namespace enterprise_connectors::utils {

// Retrieves the KeyInfo containing any information about the currently loaded
// key.
connectors_internals::mojom::KeyInfoPtr GetKeyInfo();

// Returns true if the current Chrome build is allowed to delete Device Trust
// keys.
bool CanDeleteDeviceTrustKey();

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

// Converts `identity` into a format that can be used by the connectors
// internals page. `key_upload_code` represents the upload code for the
// identity's private key, if available.
connectors_internals::mojom::ClientIdentityPtr ConvertIdentity(
    const client_certificates::ClientIdentity& identity,
    const std::optional<client_certificates::HttpCodeOrClientError>&
        key_upload_code);

#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

}  // namespace enterprise_connectors::utils

#endif  // CHROME_BROWSER_UI_WEBUI_CONNECTORS_INTERNALS_DEVICE_TRUST_UTILS_H_
