// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/focus_mode/certificate_manager.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chrome/browser/ash/attestation/attestation_ca_client.h"
#include "chromeos/ash/components/attestation/attestation_flow_adaptive.h"
#include "chromeos/ash/components/cryptohome/cryptohome_util.h"
#include "chromeos/ash/components/dbus/attestation/attestation_client.h"
#include "chromeos/ash/components/dbus/attestation/interface.pb.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "net/cert/x509_certificate.h"
#include "third_party/boringssl/src/pki/pem.h"

namespace {

const char kKeyName[] = "CrOSFocusMode";
const char kRequestOrigin[] = "youtubemediaconnect.googleapis.com";

bool IsAttestationAllowedByPolicy() {
  // Check the device policy for the feature.
  bool enabled_for_device = false;
  if (!ash::CrosSettings::Get()->GetBoolean(
          ash::kAttestationForContentProtectionEnabled, &enabled_for_device)) {
    LOG(ERROR) << "Failed to get device setting.";
    return false;
  }

  return enabled_for_device;
}

std::optional<base::Time> CertificateExpiration(
    const std::string& certificate) {
  CHECK(!certificate.empty());

  scoped_refptr<net::X509Certificate> x509 =
      net::X509Certificate::CreateFromBytes(base::as_byte_span(certificate));
  if (!x509.get() || x509->valid_expiry().is_null()) {
    // Certificate parsing failed.
    LOG(WARNING) << "Certificate parsing failed";
    return std::nullopt;
  }
  return x509->valid_expiry();
}

class CertificateManagerImpl : public CertificateManager {
 public:
  CertificateManagerImpl(
      const AccountId& account_id,
      base::TimeDelta expiration_buffer,
      std::unique_ptr<ash::attestation::AttestationFlow> attestation_flow,
      ash::AttestationClient* attestation_client)
      : account_id_(account_id),
        expiration_buffer_(expiration_buffer),
        attestation_flow_(std::move(attestation_flow)),
        attestation_client_(attestation_client) {}

  ~CertificateManagerImpl() override = default;

  bool GetCertificate(
      bool force_update,
      CertificateManager::CertificateCallback callback) override {
    if (!IsAttestationAllowedByPolicy()) {
      LOG(ERROR) << "Attestation is not allowed by policy.";
      return false;
    }

    if (certificate_expiration_.has_value() &&
        !CertificateNeedsRefresh(*certificate_expiration_)) {
      LOG(WARNING) << "Return the cached certificate";
      base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
          FROM_HERE, base::BindOnce(std::move(callback),
                                    CertificateManager::Key(
                                        kKeyName, *certificate_expiration_)));
      return true;
    }

    GetCertificateImpl(force_update, std::move(callback));
    return true;
  }

  CertificateManager::CertificateResult Sign(
      const CertificateManager::Key& key,
      std::string_view data,
      CertificateManager::SigningCallback callback) override {
    CHECK_EQ(kKeyName, key.label)
        << "Arbitrary signing requests are not supported";

    if (!IsAttestationAllowedByPolicy()) {
      LOG(ERROR) << "Attestation is not allowed by policy.";
      return CertificateResult::kDisallowedByPolicy;
    }

    if (CertificateNeedsRefresh(key.expiration)) {
      return CertificateResult::kCertificateExpired;
    }

    if (certificate_expiration_ != key.expiration) {
      // Key does not match the currently cached certificate.
      return CertificateResult::kInvalidKey;
    }

    ::attestation::SignRequest request;
    request.set_username(cryptohome::GetCryptohomeId(account_id_));
    request.set_key_label(key.label);
    request.set_data_to_sign(std::string(data));

    attestation_client_->Sign(
        request,
        base::BindOnce(&CertificateManagerImpl::OnRequestSigned,
                       weak_factory_.GetWeakPtr(), std::move(callback)));

    return CertificateResult::kSuccess;
  }

 private:
  // Returns true if `expiration` is within the `expiration_buffer_` and we
  // should fetch a new certificate.
  bool CertificateNeedsRefresh(base::Time expiration) {
    base::TimeDelta time_to_expiration = expiration - base::Time::Now();
    return time_to_expiration < expiration_buffer_;
  }

  void OnCertificateReady(CertificateManager::CertificateCallback callback,
                          ash::attestation::AttestationStatus operation_status,
                          const std::string& certificate_chain) {
    if (operation_status !=
        ash::attestation::AttestationStatus::ATTESTATION_SUCCESS) {
      LOG(ERROR) << "Certificate generation failed " << operation_status;
      std::move(callback).Run({});
      return;
    }

    bssl::PEMTokenizer tokenizer(certificate_chain, {"CERTIFICATE"});
    std::string client_cert;
    std::vector<std::string> cert_chain;
    if (tokenizer.GetNext()) {
      client_cert = tokenizer.data();
      while (tokenizer.GetNext()) {
        const std::string& data = tokenizer.data();
        if (!data.empty()) {
          cert_chain.push_back(tokenizer.data());
        }
      }
    }

    if (client_cert.empty()) {
      LOG(ERROR) << "Client certificate was not found";
      std::move(callback).Run({});
      return;
    }

    std::optional<base::Time> expiration = CertificateExpiration(client_cert);
    if (!expiration.has_value()) {
      LOG(WARNING) << "Certificate has no expiration";
      std::move(callback).Run({});
      return;
    }

    // Cache the certificate.
    certificate_expiration_.swap(expiration);
    client_certificate_ =
        base::StrCat({":", base::Base64Encode(client_cert), ":"});
    intermediate_certificates_.clear();
    intermediate_certificates_.reserve(cert_chain.size());
    for (const std::string& cert : cert_chain) {
      intermediate_certificates_.push_back(
          base::StrCat({":", base::Base64Encode(cert), ":"}));
    }

    CertificateManager::Key key(kKeyName, *certificate_expiration_);

    std::move(callback).Run(std::move(key));
  }

  void OnRequestSigned(CertificateManager::SigningCallback callback,
                       const ::attestation::SignReply& reply) {
    if (reply.status() != ::attestation::AttestationStatus::STATUS_SUCCESS) {
      LOG(ERROR) << "Signing failed " << reply.status();
      std::move(callback).Run(false, "", "", {});
      return;
    }

    std::string signature =
        base::StrCat({":", base::Base64Encode(reply.signature()), ":"});
    std::move(callback).Run(true, signature, client_certificate_,
                            intermediate_certificates_);
  }

  void GetCertificateImpl(bool force_update,
                          CertificateManager::CertificateCallback callback) {
    ash::attestation::AttestationFlow::CertificateCallback
        certificate_callback =
            base::BindOnce(&CertificateManagerImpl::OnCertificateReady,
                           weak_factory_.GetWeakPtr(), std::move(callback));
    const ash::attestation::AttestationCertificateProfile certificate_profile =
        ash::attestation::AttestationCertificateProfile::
            PROFILE_CONTENT_PROTECTION_CERTIFICATE;

    attestation_flow_->GetCertificate(
        /*certificate_profile=*/certificate_profile, /*account_id=*/account_id_,
        /*request_origin=*/kRequestOrigin,
        /*force_new_key=*/force_update,
        /*key_crypto_type=*/::attestation::KEY_TYPE_ECC,
        /*key_name=*/kKeyName, /*profile_specific_data=*/std::nullopt,
        /*callback=*/std::move(certificate_callback));
  }

  const AccountId account_id_;
  const base::TimeDelta expiration_buffer_;
  std::unique_ptr<ash::attestation::AttestationFlow> attestation_flow_;
  raw_ptr<ash::AttestationClient> attestation_client_;

  // Expiration timestamp for the most recently retrieved certificate. nullopt
  // if a certificate has not been retrieved.
  std::optional<base::Time> certificate_expiration_;

  // Cached copy of the most recently retrieved certificate encoded
  // according to RFC 9440 for "Client-Cert". If a certificate is not cached,
  // contains the empty string.
  std::string client_certificate_;

  // Cached copies of the certificates in the root of trust between
  // `client_certificate_` and the root certificate. Certificates are encoded
  // according to RFC 9440 for "Client-Cert-Chain". The vector is empty if there
  // are no certificates or no certificate has been retrieved.
  std::vector<std::string> intermediate_certificates_;

  base::WeakPtrFactory<CertificateManagerImpl> weak_factory_{this};
};

}  // namespace

CertificateManager::Key::Key(const std::string& label, base::Time expiration)
    : label(label), expiration(expiration) {}

CertificateManager::Key::Key(const Key& key) = default;

bool CertificateManager::Key::operator==(const Key& other) {
  return label == other.label && expiration == other.expiration;
}

// static
std::unique_ptr<CertificateManager> CertificateManager::Create(
    const AccountId& account_id,
    base::TimeDelta expiration_buffer) {
  std::unique_ptr<ash::attestation::ServerProxy> attestation_ca_client =
      std::make_unique<ash::attestation::AttestationCAClient>();
  auto attestation_flow =
      std::make_unique<ash::attestation::AttestationFlowAdaptive>(
          std::move(attestation_ca_client));
  return std::make_unique<CertificateManagerImpl>(
      account_id, expiration_buffer, std::move(attestation_flow),
      ash::AttestationClient::Get());
}

// static
std::unique_ptr<CertificateManager> CertificateManager::CreateForTesting(
    const AccountId& account_id,
    base::TimeDelta expiration_buffer,
    std::unique_ptr<ash::attestation::AttestationFlow> attestation_flow,
    ash::AttestationClient* attestation_client) {
  return std::make_unique<CertificateManagerImpl>(account_id, expiration_buffer,
                                                  std::move(attestation_flow),
                                                  attestation_client);
}
