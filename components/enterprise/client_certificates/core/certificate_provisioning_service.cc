// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/certificate_provisioning_service.h"

#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "components/enterprise/client_certificates/core/certificate_store.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/key_upload_client.h"
#include "components/enterprise/client_certificates/core/prefs.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/store_error.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "net/cert/x509_certificate.h"

namespace client_certificates {

namespace {

constexpr int kDaysBeforeExpiration = 7;

// Returns true if `certificate` expires within the next `kDaysBeforeExpiration`
// days.
bool IsCertExpiringSoon(const net::X509Certificate& certificate) {
  return (base::Time::Now() + base::Days(kDaysBeforeExpiration)) >
         certificate.valid_expiry();
}

}  // namespace

CertificateProvisioningService::Status::Status(bool is_provisioning)
    : is_provisioning(is_provisioning) {}

CertificateProvisioningService::Status::Status(const Status&) = default;
CertificateProvisioningService::Status&
CertificateProvisioningService::Status::operator=(const Status&) = default;

CertificateProvisioningService::Status::~Status() = default;

CertificateProvisioningService::~CertificateProvisioningService() = default;

class CertificateProvisioningServiceImpl
    : public CertificateProvisioningService {
 public:
  CertificateProvisioningServiceImpl(
      PrefService* profile_prefs,
      CertificateStore* certificate_store,
      std::unique_ptr<KeyUploadClient> upload_client);
  ~CertificateProvisioningServiceImpl() override;

  // CertificateProvisioningService:
  void GetManagedIdentity(GetManagedIdentityCallback callback) override;
  Status GetCurrentStatus() const override;

 private:
  bool IsPolicyEnabled() const;

  void OnPolicyUpdated();

  void OnPermanentIdentityLoaded(
      StoreErrorOr<std::optional<ClientIdentity>> expected_permanent_identity);

  void OnPrivateKeyCreated(
      StoreErrorOr<scoped_refptr<PrivateKey>> expected_private_key);

  void OnCertificateCreatedResponse(
      bool is_permanent_identity,
      scoped_refptr<PrivateKey> private_key,
      HttpCodeOrClientError upload_code,
      scoped_refptr<net::X509Certificate> certificate);

  void OnKeyUploadResponse(HttpCodeOrClientError upload_code);

  void OnCertificateCommitted(scoped_refptr<PrivateKey> private_key,
                              scoped_refptr<net::X509Certificate> certificate,
                              std::optional<StoreError> commit_error);

  void OnProvisioningError();

  void OnFinishedProvisioning();

  PrefChangeRegistrar pref_observer_;
  raw_ptr<PrefService> profile_prefs_;
  raw_ptr<CertificateStore> certificate_store_;
  std::unique_ptr<KeyUploadClient> upload_client_;

  bool is_provisioning_{false};

  // Callbacks waiting for an identity to be available.
  std::vector<GetManagedIdentityCallback> pending_callbacks_;

  std::optional<ClientIdentity> cached_identity_ = std::nullopt;
  std::optional<HttpCodeOrClientError> last_upload_code_;

  base::WeakPtrFactory<CertificateProvisioningServiceImpl> weak_factory_{this};
};

// static
std::unique_ptr<CertificateProvisioningService>
CertificateProvisioningService::Create(
    PrefService* profile_prefs,
    CertificateStore* certificate_store,
    std::unique_ptr<KeyUploadClient> upload_client) {
  return std::make_unique<CertificateProvisioningServiceImpl>(
      profile_prefs, certificate_store, std::move(upload_client));
}

CertificateProvisioningServiceImpl::CertificateProvisioningServiceImpl(
    PrefService* profile_prefs,
    CertificateStore* certificate_store,
    std::unique_ptr<KeyUploadClient> upload_client)
    : profile_prefs_(profile_prefs),
      certificate_store_(certificate_store),
      upload_client_(std::move(upload_client)) {
  CHECK(profile_prefs_);
  CHECK(certificate_store_);
  CHECK(upload_client_);

  pref_observer_.Init(profile_prefs_);
  pref_observer_.Add(
      prefs::kProvisionManagedClientCertificateForUserPrefs,
      base::BindRepeating(&CertificateProvisioningServiceImpl::OnPolicyUpdated,
                          weak_factory_.GetWeakPtr()));

  // Call once to initialize the watcher with the current pref's values.
  OnPolicyUpdated();
}

CertificateProvisioningServiceImpl::~CertificateProvisioningServiceImpl() =
    default;

void CertificateProvisioningServiceImpl::GetManagedIdentity(
    GetManagedIdentityCallback callback) {
  if (!IsPolicyEnabled()) {
    std::move(callback).Run(std::nullopt);
    return;
  }

  if (!is_provisioning_ && cached_identity_ && cached_identity_->is_valid() &&
      !IsCertExpiringSoon(*cached_identity_->certificate)) {
    // A valid identity is already cached, just return it.
    std::move(callback).Run(cached_identity_);
    return;
  }

  pending_callbacks_.push_back(std::move(callback));

  if (!is_provisioning_) {
    OnPolicyUpdated();
  }
}

CertificateProvisioningService::Status
CertificateProvisioningServiceImpl::GetCurrentStatus() const {
  Status status(is_provisioning_);

  status.is_policy_enabled = IsPolicyEnabled();
  status.identity = cached_identity_;
  status.last_upload_code = last_upload_code_;
  return status;
}

bool CertificateProvisioningServiceImpl::IsPolicyEnabled() const {
  return profile_prefs_->IsManagedPreference(
             prefs::kProvisionManagedClientCertificateForUserPrefs) &&
         profile_prefs_->GetInteger(
             prefs::kProvisionManagedClientCertificateForUserPrefs) == 1;
}

void CertificateProvisioningServiceImpl::OnPolicyUpdated() {
  if (IsPolicyEnabled() && !is_provisioning_) {
    // Start by trying to load the current identity.
    is_provisioning_ = true;
    certificate_store_->GetIdentity(
        kManagedProfileIdentityName,
        base::BindOnce(
            &CertificateProvisioningServiceImpl::OnPermanentIdentityLoaded,
            weak_factory_.GetWeakPtr()));
  }
}

void CertificateProvisioningServiceImpl::OnPermanentIdentityLoaded(
    StoreErrorOr<std::optional<ClientIdentity>> expected_permanent_identity) {
  if (!expected_permanent_identity.has_value()) {
    // TODO(b:324077611): Log the error.
    OnProvisioningError();
    return;
  }

  std::optional<ClientIdentity>& permanent_identity_optional =
      expected_permanent_identity.value();
  if (permanent_identity_optional.has_value()) {
    if (permanent_identity_optional->is_valid()) {
      // Already have a full identity, so cache it.
      cached_identity_ = permanent_identity_optional.value();

      // If the certificate has expired (or is close to), then update it before
      // responding to pending callbacks.
      if (!IsCertExpiringSoon(*permanent_identity_optional->certificate)) {
        OnFinishedProvisioning();
        upload_client_->SyncKey(
            cached_identity_->private_key,
            base::BindOnce(
                &CertificateProvisioningServiceImpl::OnKeyUploadResponse,
                weak_factory_.GetWeakPtr()));
        return;
      }
    }

    if (permanent_identity_optional->private_key) {
      // Identity is only missing a valid certificate, skip the key creation
      // step.
      upload_client_->CreateCertificate(
          permanent_identity_optional->private_key,
          base::BindOnce(
              &CertificateProvisioningServiceImpl::OnCertificateCreatedResponse,
              weak_factory_.GetWeakPtr(), /*is_permanent_identity=*/true,
              permanent_identity_optional->private_key));
      return;
    }

    if (permanent_identity_optional->certificate) {
      // TODO(b:319627471): Figure out what to do with this edge-case after
      // playing around with the E2E feature a bit.
      OnProvisioningError();
      return;
    }
  }

  // There's no identity, so create a new key in the temporary location
  // and try to provision a certificate for it.
  certificate_store_->CreatePrivateKey(
      kTemporaryManagedProfileIdentityName,
      base::BindOnce(&CertificateProvisioningServiceImpl::OnPrivateKeyCreated,
                     weak_factory_.GetWeakPtr()));
}

void CertificateProvisioningServiceImpl::OnPrivateKeyCreated(
    StoreErrorOr<scoped_refptr<PrivateKey>> expected_private_key) {
  if (!expected_private_key.has_value()) {
    OnProvisioningError();
    return;
  }

  upload_client_->CreateCertificate(
      expected_private_key.value(),
      base::BindOnce(
          &CertificateProvisioningServiceImpl::OnCertificateCreatedResponse,
          weak_factory_.GetWeakPtr(), /*is_permanent_identity=*/false,
          expected_private_key.value()));
}

void CertificateProvisioningServiceImpl::OnCertificateCreatedResponse(
    bool is_permanent_identity,
    scoped_refptr<PrivateKey> private_key,
    HttpCodeOrClientError upload_code,
    scoped_refptr<net::X509Certificate> certificate) {
  last_upload_code_ = upload_code;

  if (!certificate) {
    OnProvisioningError();
    return;
  }

  if (is_permanent_identity) {
    // For some reason, the permanent identity only had a private key, so store
    // the newly created certificate along with it.
    certificate_store_->CommitCertificate(
        kManagedProfileIdentityName, certificate,
        base::BindOnce(
            &CertificateProvisioningServiceImpl::OnCertificateCommitted,
            weak_factory_.GetWeakPtr(), std::move(private_key), certificate));
  } else {
    // Typical flow where the private key was created in the temporary location,
    // and will be moved to the permanent location along with its newly created
    // certificate.
    certificate_store_->CommitIdentity(
        kTemporaryManagedProfileIdentityName, kManagedProfileIdentityName,
        certificate,
        base::BindOnce(
            &CertificateProvisioningServiceImpl::OnCertificateCommitted,
            weak_factory_.GetWeakPtr(), std::move(private_key), certificate));
  }
}

void CertificateProvisioningServiceImpl::OnKeyUploadResponse(
    HttpCodeOrClientError upload_code) {
  last_upload_code_ = upload_code;
}

void CertificateProvisioningServiceImpl::OnCertificateCommitted(
    scoped_refptr<PrivateKey> private_key,
    scoped_refptr<net::X509Certificate> certificate,
    std::optional<StoreError> commit_error) {
  if (commit_error.has_value()) {
    OnProvisioningError();
    return;
  }

  cached_identity_.emplace(kManagedProfileIdentityName, std::move(private_key),
                           std::move(certificate));
  OnFinishedProvisioning();
}

void CertificateProvisioningServiceImpl::OnProvisioningError() {
  // TODO(b:322837073): Record failure histogram.
  OnFinishedProvisioning();
}

void CertificateProvisioningServiceImpl::OnFinishedProvisioning() {
  is_provisioning_ = false;

  std::optional<ClientIdentity> identity =
      cached_identity_ && cached_identity_->is_valid() ? cached_identity_
                                                       : std::nullopt;
  for (auto& pending_callback : pending_callbacks_) {
    std::move(pending_callback).Run(identity);
  }
  pending_callbacks_.clear();
}

}  // namespace client_certificates
