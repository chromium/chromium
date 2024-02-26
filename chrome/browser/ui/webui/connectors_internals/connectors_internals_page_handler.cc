// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/connectors_internals/connectors_internals_page_handler.h"

#include "base/check.h"
#include "base/containers/span.h"
#include "base/json/json_writer.h"
#include "base/values.h"
#include "build/build_config.h"
#include "chrome/browser/enterprise/client_certificates/certificate_provisioning_service_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/common/common_types.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_connector_service_factory.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service.h"
#include "chrome/browser/enterprise/connectors/device_trust/device_trust_service_factory.h"
#include "chrome/browser/enterprise/signals/user_permission_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/connectors_internals/connectors_internals.mojom.h"
#include "chrome/browser/ui/webui/connectors_internals/device_trust_utils.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "components/enterprise/buildflags/buildflags.h"
#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/cert/x509_certificate.h"

#if BUILDFLAG(IS_MAC)
#include "chrome/browser/enterprise/connectors/device_trust/key_management/core/mac/secure_enclave_client.h"
#endif  // BUILDFLAG(IS_MAC)

#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#endif

namespace enterprise_connectors {

namespace {

std::string ConvertPolicyLevelToString(DTCPolicyLevel level) {
  switch (level) {
    case DTCPolicyLevel::kBrowser:
      return "Browser";
    case DTCPolicyLevel::kUser:
      return "User";
  }
}

}  // namespace

ConnectorsInternalsPageHandler::ConnectorsInternalsPageHandler(
    mojo::PendingReceiver<connectors_internals::mojom::PageHandler> receiver,
    Profile* profile)
    : receiver_(this, std::move(receiver)), profile_(profile) {
  DCHECK(profile_);
}

ConnectorsInternalsPageHandler::~ConnectorsInternalsPageHandler() = default;

void ConnectorsInternalsPageHandler::GetDeviceTrustState(
    GetDeviceTrustStateCallback callback) {
  auto* device_trust_service =
      DeviceTrustServiceFactory::GetForProfile(profile_);

  // The factory will not return a service if the profile is off-the-record, or
  // if the current management configuration is not supported.
  if (!device_trust_service) {
    auto state = connectors_internals::mojom::DeviceTrustState::New(
        false, std::vector<std::string>(),
        connectors_internals::mojom::KeyInfo::New(
            connectors_internals::mojom::KeyManagerInitializedValue::
                UNSUPPORTED,
            nullptr,
            connectors_internals::mojom::KeyManagerPermanentFailure::
                UNSPECIFIED),
        std::string(), nullptr);
    std::move(callback).Run(std::move(state));
    return;
  }

  // Since this page is used for debugging purposes, show the signals regardless
  // of the policy value (i.e. even if service->IsEnabled is false).
  device_trust_service->GetSignals(
      base::BindOnce(&ConnectorsInternalsPageHandler::OnSignalsCollected,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback),
                     device_trust_service->IsEnabled()));
}

void ConnectorsInternalsPageHandler::DeleteDeviceTrustKey(
    DeleteDeviceTrustKeyCallback callback) {
#if BUILDFLAG(IS_MAC)
  auto client = SecureEnclaveClient::Create();

  // Delete both the permanent and temporary keys.
  client->DeleteKey(SecureEnclaveClient::KeyType::kTemporary);
  client->DeleteKey(SecureEnclaveClient::KeyType::kPermanent);
  std::move(callback).Run();
#else
  NOTIMPLEMENTED();
#endif  // BUILDFLAG(IS_MAC)
}

void ConnectorsInternalsPageHandler::GetClientCertificateState(
    GetClientCertificateStateCallback callback) {
#if BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
  auto* certificate_provisioning_service =
      client_certificates::CertificateProvisioningServiceFactory::GetForProfile(
          profile_);
  if (!certificate_provisioning_service) {
    std::move(callback).Run(
        connectors_internals::mojom::ClientCertificateState::New(
            std::vector<std::string>(), nullptr, nullptr));
    return;
  }

  const auto& status = certificate_provisioning_service->GetCurrentStatus();
  std::vector<std::string> enabled_levels;
  if (status.is_policy_enabled) {
    enabled_levels.push_back("Profile");
  }

  connectors_internals::mojom::ClientIdentityPtr managed_profile_identity =
      nullptr;
  if (status.identity.has_value()) {
    managed_profile_identity = utils::ConvertIdentity(status.identity.value(),
                                                      status.last_upload_code);
  }

  std::move(callback).Run(
      connectors_internals::mojom::ClientCertificateState::New(
          std::move(enabled_levels), std::move(managed_profile_identity),
          nullptr));

#else
  std::move(callback).Run(
      connectors_internals::mojom::ClientCertificateState::New(
          std::vector<std::string>(), nullptr, nullptr));
#endif  // BUILDFLAG(ENTERPRISE_CLIENT_CERTIFICATES)
}

void ConnectorsInternalsPageHandler::OnSignalsCollected(
    GetDeviceTrustStateCallback callback,
    bool is_device_trust_enabled,
    const base::Value::Dict signals) {
  std::string signals_json;
  base::JSONWriter::WriteWithOptions(
      signals, base::JSONWriter::OPTIONS_PRETTY_PRINT, &signals_json);

  const auto* user_permission_service =
      enterprise_signals::UserPermissionServiceFactory::GetForProfile(profile_);
  connectors_internals::mojom::ConsentMetadataPtr consent_metadata = nullptr;
  if (user_permission_service) {
    consent_metadata = connectors_internals::mojom::ConsentMetadata::New(
        user_permission_service->CanCollectSignals() ==
            device_signals::UserPermission::kGranted,
        user_permission_service->HasUserConsented());
  }

  std::vector<std::string> policy_enabled_levels;
  auto* device_trust_connector_service =
      DeviceTrustConnectorServiceFactory::GetForProfile(profile_);
  if (device_trust_connector_service) {
    for (const auto& level :
         device_trust_connector_service->GetEnabledInlinePolicyLevels()) {
      policy_enabled_levels.push_back(ConvertPolicyLevelToString(level));
    }
  }

  auto state = connectors_internals::mojom::DeviceTrustState::New(
      is_device_trust_enabled, policy_enabled_levels, utils::GetKeyInfo(),
      signals_json, std::move(consent_metadata));
  std::move(callback).Run(std::move(state));
}

}  // namespace enterprise_connectors
