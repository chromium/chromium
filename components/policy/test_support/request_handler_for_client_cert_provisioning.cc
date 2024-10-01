// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/test_support/request_handler_for_client_cert_provisioning.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/base64.h"
#include "base/containers/span.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "components/policy/core/common/cloud/cloud_policy_constants.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/test_server_helpers.h"
#include "net/base/url_util.h"
#include "net/http/http_status_code.h"
#include "net/test/cert_builder.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"

using net::test_server::HttpRequest;
using net::test_server::HttpResponse;

namespace em = enterprise_management;

namespace policy {

namespace {
// Real but outdated base64-encoded verified access challenge received from
// the Enterprise Verified Access Test extension.
const char kValueChallengeB64[] =
    "CkEKFkVudGVycHJpc2VLZXlDaGFsbGVuZ2USIO6YSl1AvTjbEvRukIFMF2pA4AwCc1w4fZ"
    "X3n3sGcLInGOPh+IWKLhKAAm/WHGk7ahCjPk4IXLfDlUUmmZdfW1scNcwkKk/"
    "x24ZnvbT7tyrmxLzO5nG69ycW7f+"
    "2bacbtfGlf0UOGeljcqBIIoHjJPlm0d2gCTa2msghS9ovaSg/"
    "wbY5DPeNkcG5drDq5Es5hzlZ49Bhvv5cAbDDsGNobPJQ3ojbu/"
    "mrdlb3mlB1oNTmbfoPTBrr6n9JXvywsJmHyInTySiFPOR8TT1cQoDA0pZ0ccHMJfLia1/"
    "FCW/"
    "pGpI6GpSzCQLq2hH0cFVuef3lGn09EeUHTPejbm6gcrHe9VDAFXMI8SzUlgMBBjHtTpo9G"
    "XJbwkTrGFXdkEU5BY1KukrsIVqdmAGFTDM=";

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

bool ProcessClientCertProvisioningRequest(
    const em::ClientCertificateProvisioningRequest& provisioning_request,
    em::ClientCertificateProvisioningResponse* provisioning_response) {
  if (provisioning_request.has_start_csr_request()) {
    std::string decoded_va_challenge_b64;
    DCHECK(base::Base64Decode(kValueChallengeB64, &decoded_va_challenge_b64));
    em::StartCsrResponse* start_csr_response =
        provisioning_response->mutable_start_csr_response();
    start_csr_response->set_invalidation_topic("invalidation_topic_123");
    start_csr_response->set_va_challenge(decoded_va_challenge_b64);
    start_csr_response->set_hashing_algorithm(em::HashingAlgorithm::SHA256);
    start_csr_response->set_signing_algorithm(
        em::SigningAlgorithm::RSA_PKCS1_V1_5);
    start_csr_response->set_data_to_sign("data_to_sign_123");
    return true;
  }

  if (provisioning_request.has_finish_csr_request()) {
    provisioning_response->mutable_finish_csr_response();
    return true;
  }

  if (provisioning_request.has_download_cert_request()) {
    // Issue a certificate for the client's `public_key`.
    // The issuer_cert_builder certificate is also generated on the fly.
    // Both issuer_cert_builder certificate and issued certificate have a
    // SubjectCommonName "TastTest" because the tests currently expects that.
    std::string pem_cert = GeneratePEMEncodedCertificate(
        provisioning_request.public_key(), "TastTest", "TastTest");
    em::DownloadCertResponse* download_cert_response =
        provisioning_response->mutable_download_cert_response();
    download_cert_response->set_pem_encoded_certificate(pem_cert);
    return true;
  }

  return false;
}

}  // namespace

RequestHandlerForClientCertProvisioning::
    RequestHandlerForClientCertProvisioning(EmbeddedPolicyTestServer* parent)
    : EmbeddedPolicyTestServer::RequestHandler(parent) {}

RequestHandlerForClientCertProvisioning::
    ~RequestHandlerForClientCertProvisioning() = default;

std::string RequestHandlerForClientCertProvisioning::RequestType() {
  return dm_protocol::kValueRequestCertProvisioningRequest;
}

std::unique_ptr<HttpResponse>
RequestHandlerForClientCertProvisioning::HandleRequest(
    const HttpRequest& request) {
  em::DeviceManagementRequest device_management_request;
  device_management_request.ParseFromString(request.content);
  const em::ClientCertificateProvisioningRequest&
      certificate_provisioning_request =
          device_management_request.client_certificate_provisioning_request();
  em::DeviceManagementResponse device_management_response;

  if (!ProcessClientCertProvisioningRequest(
          certificate_provisioning_request,
          device_management_response
              .mutable_client_certificate_provisioning_response())) {
    return CreateHttpResponse(net::HTTP_BAD_REQUEST,
                              "Invalid request parameter");
  }

  return CreateHttpResponse(net::HTTP_OK, device_management_response);
}

}  // namespace policy
