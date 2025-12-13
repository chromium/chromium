// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/prefs_certificate_store.h"

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/values.h"
#include "components/enterprise/client_certificates/core/client_identity.h"
#include "components/enterprise/client_certificates/core/constants.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_factory.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/prefs/pref_service.h"
#include "net/cert/x509_certificate.h"

namespace client_certificates {

namespace {

// Called from `GetIdentity` when a private key was loaded into a
// PrivateKey instance from its dict format. `private_key` will contain the
// PrivateKey instance when successful, or nullptr when not. `identity_name` and
// `certificate` are forwarded along as they are required to construct a
// `ClientIdentity` object. `callback` is used to return the end result to the
// original caller.
void OnKeyLoaded(
    const std::string& identity_name,
    scoped_refptr<net::X509Certificate> certificate,
    base::OnceCallback<void(StoreErrorOr<std::optional<ClientIdentity>>)>
        callback,
    scoped_refptr<PrivateKey> private_key) {
  if (!private_key) {
    // Loading of the private key has failed, so return an overall failure.
    std::move(callback).Run(base::unexpected(StoreError::kLoadKeyFailed));
    return;
  }
  std::move(callback).Run(ClientIdentity(identity_name, std::move(private_key),
                                         std::move(certificate)));
}

std::string GetEncodedCertificate(net::X509Certificate& certificate) {
  std::string pem_encoded_certificate;
  if (!net::X509Certificate::GetPEMEncoded(certificate.cert_buffer(),
                                           &pem_encoded_certificate)) {
    return std::string();
  }

  return pem_encoded_certificate;
}

scoped_refptr<net::X509Certificate> GetX509CertFromPem(
    std::string pem_encoded_certificate) {
  net::CertificateList certs =
      net::X509Certificate::CreateCertificateListFromBytes(
          base::as_byte_span(pem_encoded_certificate),
          net::X509Certificate::FORMAT_AUTO);
  if (!certs.empty()) {
    return certs[0];
  }

  return nullptr;
}

}  // namespace

PrefsCertificateStore::PrefsCertificateStore(
    PrefService* pref_service,
    std::unique_ptr<PrivateKeyFactory> key_factory)
    : pref_service_(pref_service), key_factory_(std::move(key_factory)) {
  CHECK(pref_service_);
  CHECK(key_factory_);
}
PrefsCertificateStore::~PrefsCertificateStore() = default;

void PrefsCertificateStore::CreatePrivateKey(
    const std::string& identity_name,
    base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)>
        callback) {
  const base::Value::Dict& identity = pref_service_->GetDict(identity_name);
  if (identity.size() && identity.FindDict(kKeyDetails)->size()) {
    // A private key already exists, this request is therefore treated as a
    // conflict. Only check for the private key, as certificates can be
    // replaced as long as the trusted private key is not lost.
    std::move(callback).Run(base::unexpected(StoreError::kConflictingIdentity));
    return;
  }

  // In order:
  // 1) Create a private key,
  // 2) Serialize the private key,
  // 3) Save the private key,
  // 4) Reply to the original callback.
  key_factory_->CreatePrivateKey(base::BindOnce(
      &PrefsCertificateStore::OnPrivateKeyCreated, weak_factory_.GetWeakPtr(),
      identity_name, std::move(callback)));
}

void PrefsCertificateStore::CommitCertificate(
    const std::string& identity_name,
    scoped_refptr<net::X509Certificate> certificate,
    base::OnceCallback<void(std::optional<StoreError>)> callback) {
  if (!certificate) {
    std::move(callback).Run(StoreError::kInvalidCertificateInput);
    return;
  }

  auto identity = pref_service_->GetDict(identity_name).Clone();
  identity.Set(kCertificate, GetEncodedCertificate(*certificate));
  pref_service_->SetDict(identity_name, std::move(identity));

  std::move(callback).Run(std::nullopt);
}

void PrefsCertificateStore::CommitIdentity(
    const std::string& temporary_identity_name,
    const std::string& final_identity_name,
    scoped_refptr<net::X509Certificate> certificate,
    base::OnceCallback<void(std::optional<StoreError>)> callback) {
  // Typical flow where the private key was created in the temporary location,
  // and will be moved to the permanent location along with its newly created
  // certificate.
  if (!pref_service_->HasPrefPath(temporary_identity_name)) {
    std::move(callback).Run(StoreError::kIdentityNotFound);
    return;
  }

  if (!certificate) {
    std::move(callback).Run(StoreError::kInvalidCertificateInput);
    return;
  }

  if (final_identity_name.empty()) {
    std::move(callback).Run(StoreError::kInvalidFinalIdentityName);
    return;
  }

  // In order:
  // 1) Retrieve the identity from temporary storage,
  // 2) Serialize the certificate + Update identity with the new certificate,
  // 3) Save the identity in permanent storage,
  // 4) Clear the temporary storage,
  // 5) Reply to the original callback.
  auto identity = pref_service_->GetDict(temporary_identity_name).Clone();
  identity.Set(kCertificate, GetEncodedCertificate(*certificate));
  pref_service_->SetDict(final_identity_name, std::move(identity));
  pref_service_->ClearPref(temporary_identity_name);
  std::move(callback).Run(std::nullopt);
}

void PrefsCertificateStore::GetIdentity(
    const std::string& identity_name,
    base::OnceCallback<void(StoreErrorOr<std::optional<ClientIdentity>>)>
        callback) {
  auto identity = pref_service_->GetDict(identity_name).Clone();
  if (!identity.size()) {
    // Not finding identity details indicates none existed before.
    std::move(callback).Run(std::nullopt);
    return;
  }

  scoped_refptr<net::X509Certificate> certificate;
  auto* stored_cert = identity.FindString(kCertificate);
  if (stored_cert) {
    certificate = GetX509CertFromPem(*stored_cert);
  }

  auto* key_details = identity.FindDict(kKeyDetails);
  if (!key_details) {
    std::move(callback).Run(ClientIdentity(
        identity_name, /*private_key=*/nullptr, std::move(certificate)));
    return;
  }

  key_factory_->LoadPrivateKeyFromDict(
      key_details->Clone(),
      base::BindOnce(OnKeyLoaded, identity_name, std::move(certificate),
                     std::move(callback)));
}

void PrefsCertificateStore::OnPrivateKeyCreated(
    const std::string& identity_name,
    base::OnceCallback<void(StoreErrorOr<scoped_refptr<PrivateKey>>)> callback,
    scoped_refptr<PrivateKey> private_key) {
  if (!private_key) {
    // Failed to create a private key.
    std::move(callback).Run(base::unexpected(StoreError::kCreateKeyFailed));
    return;
  }

  // Create the stored identity and commit it to the prefs.
  auto serialized_private_key = private_key->ToDict();
  if (!serialized_private_key.size()) {
    std::move(callback).Run(base::unexpected(StoreError::kSaveKeyFailed));
    return;
  }

  base::Value::Dict identity_to_save;
  identity_to_save.Set(kKeyDetails, std::move(serialized_private_key));
  pref_service_->SetDict(identity_name, std::move(identity_to_save));
  std::move(callback).Run(private_key);
}

void PrefsCertificateStore::DeleteIdentities(
    const std::vector<std::string>& identity_names,
    base::OnceCallback<void(std::optional<StoreError>)> callback) {
  // Check that all identity names are non-empty.
  for (const auto& identity_name : identity_names) {
    if (identity_name.empty()) {
      std::move(callback).Run(StoreError::kInvalidIdentityName);
      return;
    }
  }

  // Clear all identities from the prefs.
  for (const auto& identity_name : identity_names) {
    pref_service_->ClearPref(identity_name);
  }

  std::move(callback).Run(std::nullopt);
}

}  // namespace client_certificates
