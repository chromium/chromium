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
#include "components/enterprise/client_certificates/core/context_delegate.h"
#include "components/enterprise/client_certificates/core/key_upload_client.h"
#include "components/enterprise/client_certificates/core/metrics_util.h"
#include "components/enterprise/client_certificates/core/prefs.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/store_error.h"
#include "components/policy/core/common/policy_logger.h"
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
      std::unique_ptr<ContextDelegate> context_delegate,
      std::unique_ptr<KeyUploadClient> upload_client);
  ~CertificateProvisioningServiceImpl() override;

  // CertificateProvisioningService:
  void GetManagedIdentity(GetManagedIdentityCallback callback) override;
  Status GetCurrentStatus() const override;

 private:
  bool IsPolicyEnabled() const;

  bool IsProvisioning() const;

  void OnPolicyUpdated();

  void OnPermanentIdentityLoaded(
      StoreErrorOr<std::optional<ClientIdentity>> expected_permanent_identity);

  void OnTemporaryIdentityLoaded(
      StoreErrorOr<std::optional<ClientIdentity>> expected_temporary_identity);

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

  void OnProvisioningError(
      ProvisioningError error,
      std::optional<StoreError> store_error = std::nullopt);

  void OnFinishedProvisioning(bool success);

  PrefChangeRegistrar pref_observer_;
  raw_ptr<PrefService> profile_prefs_;
  raw_ptr<CertificateStore> certificate_store_;
  std::unique_ptr<ContextDelegate> context_delegate_;
  std::unique_ptr<KeyUploadClient> upload_client_;

  std::optional<ProvisioningContext> provisioning_context_{std::nullopt};

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
    std::unique_ptr<ContextDelegate> context_delegate,
    std::unique_ptr<KeyUploadClient> upload_client) {
  return std::make_unique<CertificateProvisioningServiceImpl>(
      profile_prefs, certificate_store, std::move(context_delegate),
      std::move(upload_client));
}

CertificateProvisioningServiceImpl::CertificateProvisioningServiceImpl(
    PrefService* profile_prefs,
    CertificateStore* certificate_store,
    std::unique_ptr<ContextDelegate> context_delegate,
    std::unique_ptr<KeyUploadClient> upload_client)
    : profile_prefs_(profile_prefs),
      certificate_store_(certificate_store),
      context_delegate_(std::move(context_delegate)),
      upload_client_(std::move(upload_client)) {
  CHECK(profile_prefs_);
  CHECK(certificate_store_);
  CHECK(context_delegate_);
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

  if (!IsProvisioning() && cached_identity_ && cached_identity_->is_valid() &&
      !IsCertExpiringSoon(*cached_identity_->certificate)) {
    // A valid identity is already cached, just return it.
    std::move(callback).Run(cached_identity_);
    return;
  }

  pending_callbacks_.push_back(std::move(callback));

  if (!IsProvisioning()) {
    OnPolicyUpdated();
  }
}

CertificateProvisioningService::Status
CertificateProvisioningServiceImpl::GetCurrentStatus() const {
  Status status(IsProvisioning());

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

bool CertificateProvisioningServiceImpl::IsProvisioning() const {
  return provisioning_context_.has_value();
}

void CertificateProvisioningServiceImpl::OnPolicyUpdated() {
  if (IsPolicyEnabled() && !IsProvisioning()) {
    // Start by trying to load the current identity.
    LOG_POLICY(INFO, DEVICE_TRUST)
        << "Managed identity provisioning started for: "
        << kManagedProfileIdentityName;
    provisioning_context_.emplace();
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
    LOG_POLICY(ERROR, DEVICE_TRUST)
        << "Permanent identity loading failed: "
        << StoreErrorToString(expected_permanent_identity.error());
    OnProvisioningError(ProvisioningError::kIdentityLoadingFailed,
                        expected_permanent_identity.error());
    return;
  }

  // Setting as certificate creation by default, more specific scenarios will
  // overwrite this value later.
  provisioning_context_->scenario = ProvisioningScenario::kCertificateCreation;

  std::optional<ClientIdentity>& permanent_identity_optional =
      expected_permanent_identity.value();
  if (permanent_identity_optional.has_value()) {
    if (permanent_identity_optional->is_valid()) {
      // Already have a full identity, so cache it.
      cached_identity_ = permanent_identity_optional.value();

      // If the certificate has expired (or is close to), then update it before
      // responding to pending callbacks.
      if (!IsCertExpiringSoon(*permanent_identity_optional->certificate)) {
        // No need to block on key syncs, the scenario can therefore be
        // automatically completed.
        provisioning_context_->scenario = ProvisioningScenario::kPublicKeySync;
        OnFinishedProvisioning(/*success=*/true);
        upload_client_->SyncKey(
            cached_identity_->private_key,
            base::BindOnce(
                &CertificateProvisioningServiceImpl::OnKeyUploadResponse,
                weak_factory_.GetWeakPtr()));
        return;
      }

      LOG_POLICY(INFO, DEVICE_TRUST)
          << "Certificate expiring soon, renewing...";
      provisioning_context_->scenario =
          ProvisioningScenario::kCertificateRenewal;
    }

    if (permanent_identity_optional->private_key) {
      // Identity is only missing a valid certificate, skip the key creation
      // step.
      LOG_POLICY(INFO, DEVICE_TRUST)
          << "Private key found in permanent storage, fetching a certificate "
             "from the server...";
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
      LOG_POLICY(ERROR, DEVICE_TRUST)
          << "Permanent identity has a certificate, but no corresponding "
             "private key.";
      OnProvisioningError(ProvisioningError::kMissingPrivateKey);
      return;
    }
  }

  // There's no identity, so create a new key in the temporary location
  // and try to provision a certificate for it.
  LOG_POLICY(INFO, DEVICE_TRUST)
      << "Creating a private key in temporary storage...";
  certificate_store_->CreatePrivateKey(
      kTemporaryManagedProfileIdentityName,
      base::BindOnce(&CertificateProvisioningServiceImpl::OnPrivateKeyCreated,
                     weak_factory_.GetWeakPtr()));
}

void CertificateProvisioningServiceImpl::OnTemporaryIdentityLoaded(
    StoreErrorOr<std::optional<ClientIdentity>> expected_temporary_identity) {
  if (!expected_temporary_identity.has_value()) {
    // At this point, we failed to create a new private key due to a conflict,
    // and failed to get the conflicting identity; so just give up.
    LOG_POLICY(ERROR, DEVICE_TRUST)
        << "Temporary identity loading failed: "
        << StoreErrorToString(expected_temporary_identity.error());
    OnProvisioningError(ProvisioningError::kTemporaryIdentityLoadingFailed,
                        expected_temporary_identity.error());
    return;
  }

  if (!expected_temporary_identity->has_value() ||
      !expected_temporary_identity->value().private_key) {
    // This means that the database operations were successful, but the
    // temporary identity is simply empty. Since, in theory, this shouldn't
    // happen, log a metric.
    LOG_POLICY(ERROR, DEVICE_TRUST)
        << "Temporary identity loaded without a private key while it was "
           "expected to be present.";
    OnProvisioningError(ProvisioningError::kMissingTemporaryPrivateKey);
    return;
  }

  LOG_POLICY(INFO, DEVICE_TRUST) << "Resuming provisioning flow using private "
                                    "key from temporary storage...";
  OnPrivateKeyCreated(
      std::move(expected_temporary_identity->value().private_key));
}

void CertificateProvisioningServiceImpl::OnPrivateKeyCreated(
    StoreErrorOr<scoped_refptr<PrivateKey>> expected_private_key) {
  if (!expected_private_key.has_value()) {
    // If there is a conflict, it simply means a Temporary key already exists,
    // which can happen if we failed to fetch a certificate for it.
    if (expected_private_key.error() == StoreError::kConflictingIdentity) {
      LOG_POLICY(INFO, DEVICE_TRUST)
          << "Private key creation conflict, attempting resolution...";
      certificate_store_->GetIdentity(
          kTemporaryManagedProfileIdentityName,
          base::BindOnce(
              &CertificateProvisioningServiceImpl::OnTemporaryIdentityLoaded,
              weak_factory_.GetWeakPtr()));
      return;
    }

    LOG_POLICY(ERROR, DEVICE_TRUST)
        << "Failed to create a private key: "
        << StoreErrorToString(expected_private_key.error());
    OnProvisioningError(ProvisioningError::kPrivateKeyCreationFailed,
                        expected_private_key.error());
    return;
  }

  scoped_refptr<PrivateKey> private_key =
      std::move(expected_private_key.value());
  if (private_key) {
    LogPrivateKeyCreationSource(private_key->GetSource());
  }

  LOG_POLICY(INFO, DEVICE_TRUST) << "Fetching a certificate from the server...";
  upload_client_->CreateCertificate(
      private_key,
      base::BindOnce(
          &CertificateProvisioningServiceImpl::OnCertificateCreatedResponse,
          weak_factory_.GetWeakPtr(), /*is_permanent_identity=*/false,
          private_key));
}

void CertificateProvisioningServiceImpl::OnCertificateCreatedResponse(
    bool is_permanent_identity,
    scoped_refptr<PrivateKey> private_key,
    HttpCodeOrClientError upload_code,
    scoped_refptr<net::X509Certificate> certificate) {
  last_upload_code_ = upload_code;
  LogCertificateCreationResponse(upload_code, !!certificate);

  if (!certificate) {
    if (last_upload_code_->has_value()) {
      int http_status_code = last_upload_code_->value();
      bool is_success_code = http_status_code / 100 == 2;

      // If the status code shows a successful request but there is no
      // certificate in the response, it may simply be an indication of a bad
      // server configuration - nothing the client can do about it (except retry
      // later). Therefore, treat as warning instead of error.
      (is_success_code ? LOG_POLICY(WARNING, DEVICE_TRUST)
                       : LOG_POLICY(ERROR, DEVICE_TRUST))
          << "Certificate creation response received from the server without a "
             "certificate, status code: "
          << http_status_code;
    } else {
      LOG_POLICY(ERROR, DEVICE_TRUST)
          << "Failed to send a certificate creation request to the server: "
          << UploadClientErrorToString(last_upload_code_->error());
    }

    OnProvisioningError(ProvisioningError::kCertificateCreationFailed);
    return;
  }

  LOG_POLICY(INFO, DEVICE_TRUST) << "Certificate received from the server...";

  if (is_permanent_identity) {
    // For some reason, the permanent identity only had a private key, so store
    // the newly created certificate along with it.
    LOG_POLICY(INFO, DEVICE_TRUST)
        << "Committing the certificate to storage...";
    certificate_store_->CommitCertificate(
        kManagedProfileIdentityName, certificate,
        base::BindOnce(
            &CertificateProvisioningServiceImpl::OnCertificateCommitted,
            weak_factory_.GetWeakPtr(), std::move(private_key), certificate));
  } else {
    // Typical flow where the private key was created in the temporary location,
    // and will be moved to the permanent location along with its newly created
    // certificate.
    LOG_POLICY(INFO, DEVICE_TRUST)
        << "Committing the certificate to storage as an identity...";
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
  LogKeySyncResponse(upload_code);
}

void CertificateProvisioningServiceImpl::OnCertificateCommitted(
    scoped_refptr<PrivateKey> private_key,
    scoped_refptr<net::X509Certificate> certificate,
    std::optional<StoreError> commit_error) {
  if (commit_error.has_value()) {
    OnProvisioningError(ProvisioningError::kCertificateCommitFailed,
                        commit_error.value());
    return;
  }

  if (cached_identity_ && cached_identity_->certificate) {
    // Notify old cert as deleted.
    context_delegate_->OnClientCertificateDeleted(
        cached_identity_->certificate);
  }

  LOG_POLICY(INFO, DEVICE_TRUST)
      << "Storage successfully updated, updating cached identity...";
  cached_identity_.emplace(kManagedProfileIdentityName, std::move(private_key),
                           std::move(certificate));

  OnFinishedProvisioning(/*success=*/true);
}

void CertificateProvisioningServiceImpl::OnProvisioningError(
    ProvisioningError provisioning_error,
    std::optional<StoreError> store_error) {
  LogProvisioningError(provisioning_error, std::move(store_error));
  OnFinishedProvisioning(/*success=*/false);
}

void CertificateProvisioningServiceImpl::OnFinishedProvisioning(bool success) {
  LogProvisioningContext(provisioning_context_.value(), success);
  provisioning_context_.reset();

  std::optional<ClientIdentity> identity =
      cached_identity_ && cached_identity_->is_valid() ? cached_identity_
                                                       : std::nullopt;

  LOG_POLICY(INFO, DEVICE_TRUST)
      << "Managed identity provisioning finished."
      << (identity.has_value() ? " A cached identity is available."
                               : " No cached identity is available.");

  for (auto& pending_callback : pending_callbacks_) {
    std::move(pending_callback).Run(identity);
  }
  pending_callbacks_.clear();
}

}  // namespace client_certificates
