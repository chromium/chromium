// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/client_certificates/core/key_upload_client.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/enterprise/client_certificates/core/cloud_management_delegate.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/policy/core/common/cloud/dmserver_job_configurations.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/evp.h"
#include "crypto/sign.h"
#include "net/cert/asn1_util.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace client_certificates {

namespace {

BPKUR::KeyTrustLevel SourceToTrustLevel(PrivateKeySource source) {
  switch (source) {
    case PrivateKeySource::kAndroidKey:
    case PrivateKeySource::kUnexportableKey:
    case PrivateKeySource::kChromeOsHwKey:
      return BPKUR::CHROME_BROWSER_HW_KEY;
    case PrivateKeySource::kSoftwareKey:
    case PrivateKeySource::kOsSoftwareKey:
    case PrivateKeySource::kChromeOsSwKey:
      return BPKUR::CHROME_BROWSER_OS_KEY;
  }
}

BPKUR::KeyType AlgorithmToType(crypto::sign::SignatureKind algorithm) {
  switch (algorithm) {
    case crypto::sign::RSA_PKCS1_SHA1:
    case crypto::sign::RSA_PKCS1_SHA256:
    case crypto::sign::RSA_PKCS1_SHA384:
    case crypto::sign::RSA_PKCS1_SHA512:
    case crypto::sign::RSA_PSS_SHA256:
    case crypto::sign::RSA_PSS_SHA384:
    case crypto::sign::RSA_PSS_SHA512:
      return BPKUR::RSA_KEY;
    case crypto::sign::ECDSA_SHA1:
    case crypto::sign::ECDSA_SHA256:
    case crypto::sign::ECDSA_SHA384:
    case crypto::sign::ECDSA_SHA512:
      return BPKUR::EC_KEY;
    case crypto::sign::ED25519:
    case crypto::sign::MLDSA_44:
    case crypto::sign::MLDSA_65:
    case crypto::sign::MLDSA_87:
      NOTREACHED();
  }
}

void OnSignatureCreated(
    scoped_refptr<PrivateKey> private_key,
    bool create_certificate,
    base::OnceCallback<void(
        UploadClientErrorOr<enterprise_management::DeviceManagementRequest>)>
        callback,
    std::optional<std::vector<uint8_t>> signature) {
  if (!signature.has_value()) {
    std::move(callback).Run(
        base::unexpected(UploadClientError::kSignatureCreationFailed));
    return;
  }

  std::vector<uint8_t> pubkey = private_key->GetSubjectPublicKeyInfo();

  enterprise_management::DeviceManagementRequest overall_request;
  auto* mutable_upload_request =
      overall_request.mutable_browser_public_key_upload_request();
  mutable_upload_request->set_public_key(pubkey.data(), pubkey.size());
  mutable_upload_request->set_signature(signature->data(), signature->size());
  mutable_upload_request->set_key_trust_level(
      SourceToTrustLevel(private_key->GetSource()));
  mutable_upload_request->set_key_type(
      AlgorithmToType(private_key->GetAlgorithm()));
  mutable_upload_request->set_provision_certificate(create_certificate);

  std::move(callback).Run(std::move(overall_request));
}

bool VerifySPKI(const net::X509Certificate* cert,
                const PrivateKey* private_key) {
  std::string_view extracted_spki;
  if (!net::asn1::ExtractSPKIFromDERCert(
          net::x509_util::CryptoBufferAsStringPiece(cert->cert_buffer()),
          &extracted_spki)) {
    return false;
  }
  std::vector<uint8_t> expected_spki = private_key->GetSubjectPublicKeyInfo();

  bssl::UniquePtr<EVP_PKEY> cert_key =
      crypto::evp::PublicKeyFromBytes(base::as_byte_span(extracted_spki));
  bssl::UniquePtr<EVP_PKEY> platform_key =
      crypto::evp::PublicKeyFromBytes(expected_spki);

  if (!cert_key || !platform_key) {
    return false;
  }

  return EVP_PKEY_cmp(cert_key.get(), platform_key.get()) == 1;
}

}  // namespace

class KeyUploadClientImpl : public KeyUploadClient {
 public:
  explicit KeyUploadClientImpl(
      std::unique_ptr<enterprise_attestation::CloudManagementDelegate>
          management_delegate);
  ~KeyUploadClientImpl() override;

  // KeyUploadClient:
  void CreateCertificate(scoped_refptr<PrivateKey> private_key,
                         CreateCertificateCallback callback) override;
  void SyncKey(scoped_refptr<PrivateKey> private_key,
               SyncKeyCallback callback) override;

 private:
  // Asynchronously generates the upload request, involving the creation of a
  // proof-of-poseesion of `private_key`.
  void GetRequest(
      scoped_refptr<PrivateKey> private_key,
      bool create_certificate,
      base::OnceCallback<void(
          UploadClientErrorOr<enterprise_management::DeviceManagementRequest>)>
          callback);

  void OnCertificateRequestCreated(
      scoped_refptr<PrivateKey> private_key,
      CreateCertificateCallback callback,
      UploadClientErrorOr<enterprise_management::DeviceManagementRequest>
          request);

  void OnCertificateResponseReceived(scoped_refptr<PrivateKey> private_key,
                                     CreateCertificateCallback callback,
                                     policy::DMServerJobResult result);

  void OnSyncRequestCreated(
      SyncKeyCallback callback,
      UploadClientErrorOr<enterprise_management::DeviceManagementRequest>
          request);

  void OnSyncResponseReceived(SyncKeyCallback callback,
                              policy::DMServerJobResult result);

  std::unique_ptr<enterprise_attestation::CloudManagementDelegate>
      management_delegate_;

  base::WeakPtrFactory<KeyUploadClientImpl> weak_factory_{this};
};

// static
std::unique_ptr<KeyUploadClient> KeyUploadClient::Create(
    std::unique_ptr<enterprise_attestation::CloudManagementDelegate>
        management_delegate) {
  return std::make_unique<KeyUploadClientImpl>(std::move(management_delegate));
}

KeyUploadClientImpl::KeyUploadClientImpl(
    std::unique_ptr<enterprise_attestation::CloudManagementDelegate>
        management_delegate)
    : management_delegate_(std::move(management_delegate)) {
  CHECK(management_delegate_);
}

KeyUploadClientImpl::~KeyUploadClientImpl() = default;

void KeyUploadClientImpl::CreateCertificate(
    scoped_refptr<PrivateKey> private_key,
    CreateCertificateCallback callback) {
  GetRequest(private_key, /*create_certificate=*/true,
             base::BindOnce(&KeyUploadClientImpl::OnCertificateRequestCreated,
                            weak_factory_.GetWeakPtr(), private_key,
                            std::move(callback)));
}

void KeyUploadClientImpl::SyncKey(scoped_refptr<PrivateKey> private_key,
                                  SyncKeyCallback callback) {
  GetRequest(private_key, /*create_certificate=*/false,
             base::BindOnce(&KeyUploadClientImpl::OnSyncRequestCreated,
                            weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KeyUploadClientImpl::GetRequest(
    scoped_refptr<PrivateKey> private_key,
    bool create_certificate,
    base::OnceCallback<void(
        UploadClientErrorOr<enterprise_management::DeviceManagementRequest>)>
        callback) {
  if (!management_delegate_->GetDMToken().has_value()) {
    std::move(callback).Run(
        base::unexpected(UploadClientError::kMissingDMToken));
    return;
  }

  if (!private_key) {
    std::move(callback).Run(
        base::unexpected(UploadClientError::kInvalidKeyParameter));
    return;
  }

  std::vector<uint8_t> pubkey = private_key->GetSubjectPublicKeyInfo();
  auto* key = private_key.get();
  key->Sign(pubkey, base::BindOnce(&OnSignatureCreated, std::move(private_key),
                                   create_certificate, std::move(callback)));
}

void KeyUploadClientImpl::OnCertificateRequestCreated(
    scoped_refptr<PrivateKey> private_key,
    CreateCertificateCallback callback,
    UploadClientErrorOr<enterprise_management::DeviceManagementRequest>
        request) {
  if (!request.has_value()) {
    std::move(callback).Run(base::unexpected(request.error()), nullptr);
    return;
  }

  management_delegate_->UploadBrowserPublicKey(
      std::move(request.value()),
      base::BindOnce(&KeyUploadClientImpl::OnCertificateResponseReceived,
                     weak_factory_.GetWeakPtr(), private_key,
                     std::move(callback)));
}

void KeyUploadClientImpl::OnCertificateResponseReceived(
    scoped_refptr<PrivateKey> private_key,
    CreateCertificateCallback callback,
    policy::DMServerJobResult result) {
  scoped_refptr<net::X509Certificate> certificate = nullptr;
  if (result.dm_status == policy::DM_STATUS_SUCCESS &&
      result.response.has_browser_public_key_upload_response() &&
      result.response.browser_public_key_upload_response()
          .has_pem_encoded_certificate()) {
    std::string_view pem_encoded_certificate =
        result.response.browser_public_key_upload_response()
            .pem_encoded_certificate();
    net::CertificateList certs =
        net::X509Certificate::CreateCertificateListFromBytes(
            base::as_byte_span(pem_encoded_certificate),
            net::X509Certificate::FORMAT_PEM_CERT_SEQUENCE);
    if (!certs.empty() && VerifySPKI(certs[0].get(), private_key.get())) {
      certificate = certs[0];
    }
  }
  // TODO(b/347949238): return a new UploadClientError::kNetworkError when there
  // is a net error.
  std::move(callback).Run(result.response_code, std::move(certificate));
}

void KeyUploadClientImpl::OnSyncRequestCreated(
    SyncKeyCallback callback,
    UploadClientErrorOr<enterprise_management::DeviceManagementRequest>
        request) {
  if (!request.has_value()) {
    std::move(callback).Run(base::unexpected(request.error()));
    return;
  }

  management_delegate_->UploadBrowserPublicKey(
      std::move(request.value()),
      base::BindOnce(&KeyUploadClientImpl::OnSyncResponseReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KeyUploadClientImpl::OnSyncResponseReceived(
    SyncKeyCallback callback,
    policy::DMServerJobResult result) {
  // TODO(b/347949238): return a new UploadClientError::kNetworkError when there
  // is a net error.
  std::move(callback).Run(result.response_code);
}

}  // namespace client_certificates
