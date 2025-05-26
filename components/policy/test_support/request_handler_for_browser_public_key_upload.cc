// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_browser_public_key_upload.h"

#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace em = enterprise_management;
using BPKUR = enterprise_management::BrowserPublicKeyUploadRequest;

namespace policy {

namespace {

// Generates a self-signed issuer_cert_builder certificate with
// `issuer_common_name`. Then issues a certificate for the public key `spki_der`
// (DER-encoded X.509 SubjectPublicKeyInfo) with SubjectCommonName
// `subject_common_name`. Returns a PEM-encoded representation of that
// certificate.
std::string GeneratePEMEncodedCertificate(
    const std::string& public_key,
    const std::string& issuer_common_name,
    const std::string& subject_common_name) {
  std::vector<uint8_t> pk(public_key.begin(), public_key.end());
  base::span<const uint8_t> spki_der(pk);
  net::CertBuilder issuer_cert_builder(/*orig_cert=*/nullptr,
                                       /*issuer=*/nullptr);
  issuer_cert_builder.SetSubjectCommonName(issuer_common_name);
  issuer_cert_builder.GenerateRSAKey();
  std::unique_ptr<net::CertBuilder> cert_builder =
      net::CertBuilder::FromSubjectPublicKeyInfo(spki_der,
                                                 &issuer_cert_builder);
  cert_builder->SetSignatureAlgorithm(
      bssl::SignatureAlgorithm::kRsaPkcs1Sha256);
  cert_builder->SetValidity(base::Time::Now(),
                            base::Time::Now() + base::Days(30));
  cert_builder->SetSubjectCommonName(subject_common_name);

  return cert_builder->GetPEM();
}

}  // namespace

RequestHandlerForBrowserPublicKeyUpload::
    RequestHandlerForBrowserPublicKeyUpload(EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForBrowserPublicKeyUpload::
    ~RequestHandlerForBrowserPublicKeyUpload() = default;

std::string RequestHandlerForBrowserPublicKeyUpload::RequestType() {
  return dm_protocol::kValueBrowserUploadPublicKey;
}

std::unique_ptr<HttpResponse>
RequestHandlerForBrowserPublicKeyUpload::HandleRequest(
    const HttpRequest& request) {
  // Make sure dm token is present.
  std::string dm_token;
  if (!GetDeviceTokenFromRequest(request, &dm_token)) {
    return CreateHttpResponse(net::HTTP_UNAUTHORIZED, "Missing dm token");
  }

  // Verify if Profile or Browser.
  std::string profile_id;
  bool is_profile = GetProfileIdFromRequest(request, &profile_id);

  em::DeviceManagementRequest device_management_request;
  device_management_request.ParseFromString(request.content);
  const em::BrowserPublicKeyUploadRequest& key_upload_request =
      device_management_request.browser_public_key_upload_request();

  if (key_upload_request.public_key().empty()) {
    return CreateHttpResponse(net::HTTP_BAD_REQUEST, "Missing public key");
  }

  if (key_upload_request.key_trust_level() ==
      BPKUR::KEY_TRUST_LEVEL_UNSPECIFIED) {
    return CreateHttpResponse(net::HTTP_BAD_REQUEST,
                              "Missing public key trust level");
  }

  if (key_upload_request.key_type() == BPKUR::KEY_TYPE_UNSPECIFIED) {
    return CreateHttpResponse(net::HTTP_BAD_REQUEST, "Missing public key type");
  }

  em::DeviceManagementResponse device_management_response;
  if (key_upload_request.provision_certificate()) {
    device_management_response.mutable_browser_public_key_upload_response()
        ->set_pem_encoded_certificate(GeneratePEMEncodedCertificate(
            key_upload_request.public_key(),
            /*issuer_common_name=*/
            is_profile ? "Profile Root CA" : "Browser Root CA",
            /*subject_common_name=*/"Test Client Cert"));
  }

  device_management_response.mutable_browser_public_key_upload_response()
      ->set_response_code(em::BrowserPublicKeyUploadResponse::SUCCESS);

  return CreateHttpResponse(net::HTTP_OK, device_management_response);
}

}  // namespace policy
