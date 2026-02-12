// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/ios/certificate_provisioning_service_ios.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/ios/client_identity_ios_error.h"
#include "components/policy/core/common/policy_logger.h"
#include "net/cert/x509_certificate.h"

namespace client_certificates {

CertificateProvisioningServiceIOS::~CertificateProvisioningServiceIOS() =
    default;

class CertificateProvisioningServiceIOSImpl
    : public CertificateProvisioningServiceIOS {
 public:
  explicit CertificateProvisioningServiceIOSImpl(
      std::unique_ptr<CertificateProvisioningService> core_service);

  ~CertificateProvisioningServiceIOSImpl() override;

  // CertificateProvisioningService:
  void GetManagedIdentity(GetManagedIdentityCallback callback) override;
  void DeleteManagedIdentities(
      base::OnceCallback<void(bool)> callback) override;
  Status GetCurrentStatus() const override;

  // CertificateProvisioningServiceIOS:
  void GetManagedIdentityIOS(GetManagedIdentityIOSCallback callback) override;

 private:
  void UpdateIOSCache(const std::optional<ClientIdentity>& core_identity);

  void OnCoreIdentityLoaded(GetManagedIdentityIOSCallback callback,
                            std::optional<ClientIdentity> core_identity);

  std::optional<ClientIdentityIOS> cached_ios_identity_;
  std::unique_ptr<CertificateProvisioningService> core_service_;

  base::WeakPtrFactory<CertificateProvisioningServiceIOSImpl> weak_factory_{
      this};
};

// static
std::unique_ptr<CertificateProvisioningServiceIOS>
CertificateProvisioningServiceIOS::Create(
    std::unique_ptr<CertificateProvisioningService> core_service) {
  return std::make_unique<CertificateProvisioningServiceIOSImpl>(
      std::move(core_service));
}

CertificateProvisioningServiceIOSImpl::CertificateProvisioningServiceIOSImpl(
    std::unique_ptr<CertificateProvisioningService> core_service)
    : core_service_(std::move(core_service)) {
  CHECK(core_service_);
}

CertificateProvisioningServiceIOSImpl::
    ~CertificateProvisioningServiceIOSImpl() = default;

void CertificateProvisioningServiceIOSImpl::GetManagedIdentity(
    GetManagedIdentityCallback callback) {
  GetManagedIdentityIOS(base::BindOnce(
      [](GetManagedIdentityCallback callback,
         std::optional<ClientIdentityIOS> ios_identity) {
        if (!ios_identity.has_value() || !ios_identity->is_valid()) {
          std::move(callback).Run(std::nullopt);
          return;
        }
        std::move(callback).Run(ios_identity->identity);
      },
      std::move(callback)));
}

void CertificateProvisioningServiceIOSImpl::DeleteManagedIdentities(
    base::OnceCallback<void(bool)> callback) {
  core_service_->DeleteManagedIdentities(std::move(callback));
}

CertificateProvisioningService::Status
CertificateProvisioningServiceIOSImpl::GetCurrentStatus() const {
  Status status = core_service_->GetCurrentStatus();
  if (cached_ios_identity_.has_value()) {
    status.identity = cached_ios_identity_->identity;
  }
  return status;
}

void CertificateProvisioningServiceIOSImpl::GetManagedIdentityIOS(
    GetManagedIdentityIOSCallback callback) {
  core_service_->GetManagedIdentity(base::BindOnce(
      &CertificateProvisioningServiceIOSImpl::OnCoreIdentityLoaded,
      weak_factory_.GetWeakPtr(), std::move(callback)));
}

void CertificateProvisioningServiceIOSImpl::UpdateIOSCache(
    const std::optional<ClientIdentity>& core_identity) {
  if (!core_identity.has_value()) {
    cached_ios_identity_.reset();
    return;
  }

  // Only base values are being compared (e.g. certificate and private key)
  if (cached_ios_identity_.has_value() &&
      cached_ios_identity_.value().Equals(core_identity.value())) {
    return;
  }

  auto create_result = ClientIdentityIOS::TryCreate(core_identity.value());
  if (create_result.has_value()) {
    LOG_POLICY(INFO, DEVICE_TRUST)
        << "Successfully generated SecIdentityRef. Updating cache for iOS.";
    cached_ios_identity_ = std::move(create_result.value());
  } else {
    LOG_POLICY(ERROR, DEVICE_TRUST)
        << "Failed to create SecIdentityRef: "
        << ClientIdentityIOSErrorToString(create_result.error())
        << ". Resetting cached identity for iOS.";
    cached_ios_identity_.reset();
  }
}

void CertificateProvisioningServiceIOSImpl::OnCoreIdentityLoaded(
    GetManagedIdentityIOSCallback callback,
    std::optional<ClientIdentity> core_identity) {
  UpdateIOSCache(core_identity);
  std::move(callback).Run(cached_ios_identity_);
}

}  // namespace client_certificates
