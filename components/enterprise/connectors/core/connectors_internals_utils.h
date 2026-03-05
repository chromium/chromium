// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_INTERNALS_UTILS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_INTERNALS_UTILS_H_

#include <optional>
#include <string>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/connectors_internals.mojom.h"
#include "crypto/signature_verifier.h"

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/upload_client_error.h"
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

namespace client_certificates {
class CertificateProvisioningService;
}  // namespace client_certificates

namespace enterprise_connectors::utils {

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

// Fetches the identity from `provisioning_service` if the policy is enabled.
// If enabled, `enabled_level` is added to `enabled_levels`.
connectors_internals::mojom::ClientIdentityPtr GetIdentity(
    client_certificates::CertificateProvisioningService* provisioning_service,
    std::vector<std::string>& enabled_levels,
    const std::string& enabled_level);

// Converts `identity` into a format that can be used by the connectors
// internals page. `key_upload_code` represents the upload code for the
// identity's private key, if available.
connectors_internals::mojom::ClientIdentityPtr ConvertIdentity(
    const client_certificates::ClientIdentity& identity,
    const std::optional<client_certificates::HttpCodeOrClientError>&
        key_upload_code);

#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

// Maps a signature algorithm to a Mojo key type.
connectors_internals::mojom::KeyType AlgorithmToType(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm);

// Hashes the given SPKI bytes and encodes them into a Base64Url string.
std::string HashAndEncodeString(const std::string& spki_bytes);

// Wraps an optional integer into a Mojo Int32Value.
connectors_internals::mojom::Int32ValuePtr ToMojomValue(
    std::optional<int> integer_value);

}  // namespace enterprise_connectors::utils

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_INTERNALS_UTILS_H_
