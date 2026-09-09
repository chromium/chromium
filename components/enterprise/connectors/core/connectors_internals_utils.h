// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_INTERNALS_UTILS_H_
#define COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_INTERNALS_UTILS_H_

#include <memory>
#include <optional>
#include <queue>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/scoped_refptr.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/connectors/connectors_internals.mojom.h"
#include "crypto/sign.h"

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/upload_client_error.h"  // nogncheck
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

class PrefService;

namespace base {
class Time;
}  // namespace base

namespace client_certificates {
class CertificateProvisioningService;
}  // namespace client_certificates

#if !BUILDFLAG(IS_ANDROID)
namespace enterprise_connectors {
class DeviceTrustConnectorService;
enum class DTCPolicyLevel;
}  // namespace enterprise_connectors
#endif  // !BUILDFLAG(IS_ANDROID)

namespace enterprise_reporting {
class ReportRequest;
class ReportScheduler;
enum class ReportGenerationError;
using ReportRequestQueue = std::queue<std::unique_ptr<ReportRequest>>;
}  // namespace enterprise_reporting

namespace enterprise_connectors::utils {

inline constexpr char kBrowserLevel[] = "Browser";
inline constexpr char kProfileLevel[] = "Profile";
inline constexpr char kUserLevel[] = "User";

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

// Creates a ClientCertificateState from the given browser and profile
// provisioning services.
connectors_internals::mojom::ClientCertificateStatePtr
CreateClientCertificateState(
    client_certificates::CertificateProvisioningService*
        browser_certificate_provisioning_service,
    client_certificates::CertificateProvisioningService*
        profile_certificate_provisioning_service);

#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)

// Maps a signature algorithm to a Mojo key type.
connectors_internals::mojom::KeyType AlgorithmToType(
    crypto::sign::SignatureKind algorithm);

// Hashes the given SPKI bytes and encodes them into a Base64Url string.
std::string HashAndEncodeString(const std::string& spki_bytes);

// Wraps an optional integer into a Mojo Int32Value.
connectors_internals::mojom::Int32ValuePtr ToMojomValue(
    std::optional<int> integer_value);

// Returns a JSON string representation of the given `request`.
std::string GetJsonForReportRequest(
    const enterprise_reporting::ReportRequest& request);

// Returns a formatted date and time string from `timestamp`, or an empty string
// if `timestamp` is null.
std::string GetStringFromTimestamp(base::Time timestamp);

#if !BUILDFLAG(IS_ANDROID)
// Converts `level` to its string representation ("Browser" or "User").
std::string ConvertPolicyLevelToString(
    enterprise_connectors::DTCPolicyLevel level);

// Returns the list of policy-enabled level names ("Browser", "User") from
// `connector_service`.
std::vector<std::string> GetPolicyEnabledLevels(
    const enterprise_connectors::DeviceTrustConnectorService*
        connector_service);
#endif  // !BUILDFLAG(IS_ANDROID)

// Creates a DeviceTrustState indicating that Device Trust is unsupported.
connectors_internals::mojom::DeviceTrustStatePtr
CreateUnsupportedDeviceTrustState();

// Creates a DeviceTrustState for platforms (such as iOS) that do not manage or
// persist signing keys (reporting NO_KEY).
connectors_internals::mojom::DeviceTrustStatePtr
CreateDeviceTrustStateWithNoKey(
    bool is_device_trust_enabled,
    std::vector<std::string> policy_enabled_levels,
    std::string signals_json,
    connectors_internals::mojom::ConsentMetadataPtr consent_metadata = nullptr);

// Creates a SignalsReportingState populated with preference timestamps and
// scheduler status.
connectors_internals::mojom::SignalsReportingStatePtr
CreateSignalsReportingState(
    const PrefService* profile_prefs,
    const enterprise_reporting::ReportScheduler* report_scheduler,
    bool can_collect_all_signals,
    std::optional<std::string> error_info = std::nullopt);

// Processes the result of a profile report generation, returning a pair of
// (error_message, signals_json). Exactly one of the two will be populated.
std::pair<std::optional<std::string>, std::optional<std::string>>
ProcessReportGenerationResult(
    base::expected<enterprise_reporting::ReportRequestQueue,
                   enterprise_reporting::ReportGenerationError> result);

}  // namespace enterprise_connectors::utils

#endif  // COMPONENTS_ENTERPRISE_CONNECTORS_CORE_CONNECTORS_INTERNALS_UTILS_H_
