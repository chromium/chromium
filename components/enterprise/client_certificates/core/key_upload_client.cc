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
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected.h"
#include "components/enterprise/client_certificates/core/cloud_management_delegate.h"
#include "components/enterprise/client_certificates/core/dm_server_client.h"
#include "components/enterprise/client_certificates/core/private_key.h"
#include "components/enterprise/client_certificates/core/private_key_types.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "crypto/signature_verifier.h"
#include "net/cert/x509_certificate.h"
#include "url/gurl.h"

using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace client_certificates {

namespace {

BPKUR::KeyTrustLevel SourceToTrustLevel(PrivateKeySource source) {
  switch (source) {
    case PrivateKeySource::kUnexportableKey:
      return BPKUR::CHROME_BROWSER_HW_KEY;
    case PrivateKeySource::kSoftwareKey:
      return BPKUR::CHROME_BROWSER_OS_KEY;
  }
}

BPKUR::KeyType AlgorithmToType(
    crypto::SignatureVerifier::SignatureAlgorithm algorithm) {
  switch (algorithm) {
    case crypto::SignatureVerifier::RSA_PKCS1_SHA1:
    case crypto::SignatureVerifier::RSA_PKCS1_SHA256:
    case crypto::SignatureVerifier::RSA_PSS_SHA256:
      return BPKUR::RSA_KEY;
    case crypto::SignatureVerifier::ECDSA_SHA256:
      return BPKUR::EC_KEY;
  }
}

struct RequestParameters {
  RequestParameters(const GURL& url,
                    std::string_view dm_token,
                    enterprise_management::DeviceManagementRequest request)
      : url(url), dm_token(dm_token), request(std::move(request)) {}

  ~RequestParameters() = default;

  GURL url;
  std::string dm_token;
  enterprise_management::DeviceManagementRequest request;
};

UploadClientErrorOr<RequestParameters> CreateRequest(
    GURL dm_server_url,
    std::string_view dm_token,
    scoped_refptr<PrivateKey> private_key,
    bool create_certificate) {
  if (!private_key) {
    return base::unexpected(UploadClientError::kInvalidKeyParameter);
  }

  // Generate the proof-of-possesion.
  std::vector<uint8_t> pubkey = private_key->GetSubjectPublicKeyInfo();
  std::optional<std::vector<uint8_t>> signature =
      private_key->SignSlowly(pubkey);
  if (!signature.has_value()) {
    return base::unexpected(UploadClientError::kSignatureCreationFailed);
  }

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

  return RequestParameters(dm_server_url, dm_token, std::move(overall_request));
}

}  // namespace

class KeyUploadClientImpl : public KeyUploadClient {
 public:
  KeyUploadClientImpl(
      std::unique_ptr<CloudManagementDelegate> management_delegate,
      std::unique_ptr<DMServerClient> dm_server_client);
  ~KeyUploadClientImpl() override;

  // KeyUploadClient:
  void CreateCertificate(scoped_refptr<PrivateKey> private_key,
                         CreateCertificateCallback callback) override;
  void SyncKey(scoped_refptr<PrivateKey> private_key,
               SyncKeyCallback callback) override;

 private:
  // Asynchronously generates the request parameters for the upload request,
  // involving the creation of a proof-of-poseesion of `private_key`.
  void GetRequestParameters(
      scoped_refptr<PrivateKey> private_key,
      bool create_certificate,
      base::OnceCallback<void(UploadClientErrorOr<RequestParameters>)>
          callback);

  void SendRequest(
      const RequestParameters& parameters,
      base::OnceCallback<
          void(int,
               std::optional<enterprise_management::DeviceManagementResponse>)>
          callback);

  void OnCertificateRequestCreated(
      CreateCertificateCallback callback,
      UploadClientErrorOr<RequestParameters> parameters);

  void OnCertificateResponseReceived(
      CreateCertificateCallback callback,
      int response_code,
      std::optional<enterprise_management::DeviceManagementResponse>
          response_body);

  void OnSyncRequestCreated(SyncKeyCallback callback,
                            UploadClientErrorOr<RequestParameters> parameters);

  void OnSyncResponseReceived(
      SyncKeyCallback callback,
      int response_code,
      std::optional<enterprise_management::DeviceManagementResponse>
          response_body);

  std::unique_ptr<CloudManagementDelegate> management_delegate_;
  std::unique_ptr<DMServerClient> dm_server_client_;

  base::WeakPtrFactory<KeyUploadClientImpl> weak_factory_{this};
};

// static
std::unique_ptr<KeyUploadClient> KeyUploadClient::Create(
    std::unique_ptr<CloudManagementDelegate> management_delegate,
    std::unique_ptr<DMServerClient> dm_server_client) {
  return std::make_unique<KeyUploadClientImpl>(std::move(management_delegate),
                                               std::move(dm_server_client));
}

KeyUploadClientImpl::KeyUploadClientImpl(
    std::unique_ptr<CloudManagementDelegate> management_delegate,
    std::unique_ptr<DMServerClient> dm_server_client)
    : management_delegate_(std::move(management_delegate)),
      dm_server_client_(std::move(dm_server_client)) {
  CHECK(management_delegate_);
  CHECK(dm_server_client_);
}

KeyUploadClientImpl::~KeyUploadClientImpl() = default;

void KeyUploadClientImpl::CreateCertificate(
    scoped_refptr<PrivateKey> private_key,
    CreateCertificateCallback callback) {
  GetRequestParameters(
      private_key, /*create_certificate=*/true,
      base::BindOnce(&KeyUploadClientImpl::OnCertificateRequestCreated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KeyUploadClientImpl::SyncKey(scoped_refptr<PrivateKey> private_key,
                                  SyncKeyCallback callback) {
  GetRequestParameters(
      private_key, /*create_certificate=*/false,
      base::BindOnce(&KeyUploadClientImpl::OnSyncRequestCreated,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}
void KeyUploadClientImpl::GetRequestParameters(
    scoped_refptr<PrivateKey> private_key,
    bool create_certificate,
    base::OnceCallback<void(UploadClientErrorOr<RequestParameters>)> callback) {
  auto dm_token = management_delegate_->GetDMToken();
  if (!dm_token.has_value()) {
    std::move(callback).Run(
        base::unexpected(UploadClientError::kMissingDMToken));
    return;
  }

  auto upload_url_string = management_delegate_->GetUploadBrowserPublicKeyUrl();
  if (!upload_url_string.has_value()) {
    std::move(callback).Run(
        base::unexpected(UploadClientError::kMissingUploadURL));
    return;
  }

  GURL dm_server_url(upload_url_string.value());
  if (!dm_server_url.is_valid()) {
    std::move(callback).Run(
        base::unexpected(UploadClientError::kInvalidUploadURL));
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      base::BindOnce(CreateRequest, dm_server_url, dm_token.value(),
                     std::move(private_key), create_certificate),
      std::move(callback));
}

void KeyUploadClientImpl::SendRequest(
    const RequestParameters& parameters,
    base::OnceCallback<
        void(int,
             std::optional<enterprise_management::DeviceManagementResponse>)>
        callback) {
  dm_server_client_->SendRequest(parameters.url, parameters.dm_token,
                                 parameters.request, std::move(callback));
}

void KeyUploadClientImpl::OnCertificateRequestCreated(
    CreateCertificateCallback callback,
    UploadClientErrorOr<RequestParameters> parameters) {
  if (!parameters.has_value()) {
    std::move(callback).Run(base::unexpected(parameters.error()), nullptr);
    return;
  }

  SendRequest(
      parameters.value(),
      base::BindOnce(&KeyUploadClientImpl::OnCertificateResponseReceived,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KeyUploadClientImpl::OnCertificateResponseReceived(
    CreateCertificateCallback callback,
    int response_code,
    std::optional<enterprise_management::DeviceManagementResponse>
        response_body) {
  scoped_refptr<net::X509Certificate> certificate = nullptr;
  if (response_body.has_value() &&
      response_body->has_browser_public_key_upload_response() &&
      response_body->browser_public_key_upload_response()
          .has_pem_encoded_certificate()) {
    // Try to parse the client certificate.
    std::string_view pem_encoded_certificate =
        response_body->browser_public_key_upload_response()
            .pem_encoded_certificate();
    net::CertificateList certs =
        net::X509Certificate::CreateCertificateListFromBytes(
            base::as_bytes(base::make_span(pem_encoded_certificate)),
            net::X509Certificate::FORMAT_AUTO);
    if (!certs.empty()) {
      certificate = certs[0];
    }
  }

  std::move(callback).Run(response_code, std::move(certificate));
}

void KeyUploadClientImpl::OnSyncRequestCreated(
    SyncKeyCallback callback,
    UploadClientErrorOr<RequestParameters> parameters) {
  if (!parameters.has_value()) {
    std::move(callback).Run(base::unexpected(parameters.error()));
    return;
  }

  SendRequest(parameters.value(),
              base::BindOnce(&KeyUploadClientImpl::OnSyncResponseReceived,
                             weak_factory_.GetWeakPtr(), std::move(callback)));
}

void KeyUploadClientImpl::OnSyncResponseReceived(
    SyncKeyCallback callback,
    int response_code,
    std::optional<enterprise_management::DeviceManagementResponse>
        response_body) {
  std::move(callback).Run(response_code);
}

}  // namespace client_certificates
