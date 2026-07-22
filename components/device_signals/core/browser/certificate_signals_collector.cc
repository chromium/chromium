// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/device_signals/core/browser/certificate_signals_collector.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <utility>

#include "base/barrier_callback.h"
#include "base/check.h"
#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/time/time.h"
#include "components/device_signals/core/browser/metrics_utils.h"
#include "components/device_signals/core/browser/user_permission_service.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/ssl/client_cert_identity.h"
#include "net/ssl/client_cert_store.h"
#include "net/ssl/ssl_cert_request_info.h"
#include "net/ssl/ssl_private_key.h"
#include "third_party/boringssl/src/include/openssl/ssl.h"

namespace em = enterprise_management;

namespace device_signals {

namespace {

// The hardcoded prefix required before signing the challenge to prevent
// cross-protocol attacks via domain separation.
constexpr std::string_view kCertificatePrefix =
    "SecuritySignalsReportCertificate";

em::CertificateDetails_SignatureAlgorithm ToProtoSignatureAlgorithm(
    uint16_t algorithm) {
  switch (algorithm) {
    case SSL_SIGN_RSA_PKCS1_SHA1:
      return em::CertificateDetails_SignatureAlgorithm_RSA_PKCS1_SHA1;
    case SSL_SIGN_RSA_PKCS1_SHA256:
      return em::CertificateDetails_SignatureAlgorithm_RSA_PKCS1_SHA256;
    case SSL_SIGN_ECDSA_SECP256R1_SHA256:
      return em::CertificateDetails_SignatureAlgorithm_ECDSA_SHA256;
    case SSL_SIGN_RSA_PSS_RSAE_SHA256:
      return em::CertificateDetails_SignatureAlgorithm_RSA_PSS_SHA256;
    default:
      return em::
          CertificateDetails_SignatureAlgorithm_SIGNATURE_ALGORITHM_UNKNOWN;
  }
}

struct MatchedCert {
  std::unique_ptr<net::ClientCertIdentity> identity;
  std::string challenge;
};

std::vector<MatchedCert> MatchCertsWithChallenges(
    net::ClientCertIdentityList cert_identities,
    const std::vector<GetCertificateOptions>& options) {
  std::vector<MatchedCert> matched_certs;
  for (auto& identity : cert_identities) {
    if (!identity || !identity->certificate()) {
      continue;
    }
    const auto& issuer = identity->certificate()->issuer();
    const auto& subject = identity->certificate()->subject();
    for (const auto& option : options) {
      if (option.challenge.empty()) {
        continue;
      }
      bool issuer_match = option.issuer_pattern.Empty() ||
                          option.issuer_pattern.Matches(issuer);
      bool subject_match = option.subject_pattern.Empty() ||
                           option.subject_pattern.Matches(subject);
      if (issuer_match && subject_match) {
        matched_certs.push_back({std::move(identity), option.challenge});
        break;
      }
    }
  }
  return matched_certs;
}

}  // namespace

CertificateSignalsCollector::CertificateSignalsCollector(
    std::unique_ptr<net::ClientCertStore> client_cert_store)
    : BaseSignalsCollector({
          {SignalName::kCertificates,
           base::BindRepeating(
               &CertificateSignalsCollector::GetCertificateSignal,
               base::Unretained(this))},
      }),
      client_cert_store_(std::move(client_cert_store)) {}

CertificateSignalsCollector::~CertificateSignalsCollector() = default;

void CertificateSignalsCollector::GetCertificateSignal(
    UserPermission permission,
    const SignalsAggregationRequest& request,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (permission != UserPermission::kGranted) {
    std::move(done_closure).Run();
    return;
  }
  bool missing_challenge =
      std::any_of(request.certificate_signal_parameters.begin(),
                  request.certificate_signal_parameters.end(),
                  [](const GetCertificateOptions& option) {
                    return option.challenge.empty();
                  });
  if (request.certificate_signal_parameters.empty() || missing_challenge) {
    CertificateSignalsResponse cert_response;
    cert_response.collection_error = SignalCollectionError::kMissingParameters;
    OnSignalsCollected(base::TimeTicks::Now(), response,
                       std::move(done_closure), std::move(cert_response));
    return;
  }
  if (!client_cert_store_) {
    OnClientCertsRetrieved(
        base::TimeTicks::Now(), request.certificate_signal_parameters, response,
        std::move(done_closure), net::ClientCertIdentityList());
    return;
  }
  auto cert_request_info = base::MakeRefCounted<net::SSLCertRequestInfo>();
  client_cert_store_->GetClientCerts(
      cert_request_info,
      base::BindOnce(&CertificateSignalsCollector::OnClientCertsRetrieved,
                     weak_ptr_factory_.GetWeakPtr(), base::TimeTicks::Now(),
                     request.certificate_signal_parameters, std::ref(response),
                     std::move(done_closure)));
}

void CertificateSignalsCollector::OnClientCertsRetrieved(
    base::TimeTicks start_time,
    std::vector<GetCertificateOptions> options,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    std::vector<std::unique_ptr<net::ClientCertIdentity>> cert_identities) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::vector<MatchedCert> matched_certs =
      MatchCertsWithChallenges(std::move(cert_identities), options);
  // Enforcing the 50 cert limit with issuance date priority.
  bool truncated = matched_certs.size() > 50;
  if (truncated) {
    std::sort(matched_certs.begin(), matched_certs.end(),
              [](const MatchedCert& a, const MatchedCert& b) {
                return a.identity->certificate()->valid_start() >
                       b.identity->certificate()->valid_start();
              });
    matched_certs.resize(50);
  }
  if (matched_certs.empty()) {
    CertificateSignalsResponse cert_response;
    OnSignalsCollected(start_time, response, std::move(done_closure),
                       std::move(cert_response));
    return;
  }
  auto barrier_callback = base::BarrierCallback<
      std::optional<enterprise_management::SignedCertificateDetails>>(
      matched_certs.size(),
      base::BindOnce(&CertificateSignalsCollector::OnAllCertificatesProcessed,
                     weak_ptr_factory_.GetWeakPtr(), start_time,
                     std::ref(response), std::move(done_closure), truncated));
  for (auto& matched : matched_certs) {
    ProcessSingleCertificateAsync(std::move(matched.identity),
                                  matched.challenge, barrier_callback);
  }
}

void CertificateSignalsCollector::ProcessSingleCertificateAsync(
    std::unique_ptr<net::ClientCertIdentity> cert_identity,
    const std::string& challenge,
    base::RepeatingCallback<
        void(std::optional<enterprise_management::SignedCertificateDetails>)>
        barrier_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!cert_identity || !cert_identity->certificate()) {
    barrier_callback.Run(std::nullopt);
    return;
  }
  base::span<const uint8_t> cert_span = net::x509_util::CryptoBufferAsSpan(
      cert_identity->certificate()->cert_buffer());
  std::vector<uint8_t> cert_details(cert_span.begin(), cert_span.end());
  auto* cert_identity_ptr = cert_identity.get();
  cert_identity_ptr->AcquirePrivateKey(
      base::BindOnce(&CertificateSignalsCollector::OnPrivateKeyAcquired,
                     weak_ptr_factory_.GetWeakPtr(), std::move(cert_identity),
                     std::move(cert_details), challenge, barrier_callback));
}

void CertificateSignalsCollector::OnPrivateKeyAcquired(
    std::unique_ptr<net::ClientCertIdentity> cert_identity,
    std::vector<uint8_t> details,
    std::string challenge,
    base::RepeatingCallback<
        void(std::optional<enterprise_management::SignedCertificateDetails>)>
        barrier_callback,
    scoped_refptr<net::SSLPrivateKey> private_key) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!private_key) {
    LogCertificateCollectionError(
        CertificateCollectionError::kPrivateKeyAcquisitionFailed);
    barrier_callback.Run(std::nullopt);
    return;
  }
  std::optional<uint16_t> algorithm;
  auto preferences = private_key->GetAlgorithmPreferences();
  for (auto pref : preferences) {
    if (ToProtoSignatureAlgorithm(pref) !=
        enterprise_management::
            CertificateDetails_SignatureAlgorithm_SIGNATURE_ALGORITHM_UNKNOWN) {
      algorithm = pref;
      break;
    }
  }
  if (!algorithm.has_value()) {
    LogCertificateCollectionError(
        CertificateCollectionError::kNoSupportedAlgorithm);
    barrier_callback.Run(std::nullopt);
    return;
  }
  uint16_t chosen_algorithm = *algorithm;
  enterprise_management::CertificateDetails proto_details;
  proto_details.set_certificate(std::string(details.begin(), details.end()));
  proto_details.set_challenge(challenge);
  proto_details.set_algorithm(ToProtoSignatureAlgorithm(chosen_algorithm));
  std::string serialized_details;
  if (!proto_details.SerializeToString(&serialized_details)) {
    LogCertificateCollectionError(
        CertificateCollectionError::kSerializationFailed);
    barrier_callback.Run(std::nullopt);
    return;
  }

  auto data_to_sign = std::make_unique<std::vector<uint8_t>>();
  auto prefix_span = base::as_byte_span(kCertificatePrefix);
  data_to_sign->insert(data_to_sign->end(), prefix_span.begin(),
                       prefix_span.end());
  data_to_sign->insert(data_to_sign->end(), serialized_details.begin(),
                       serialized_details.end());
  base::span<const uint8_t> data_span(*data_to_sign);

  std::vector<uint8_t> serialized_details_vec(serialized_details.begin(),
                                              serialized_details.end());
  private_key->Sign(
      chosen_algorithm, data_span,
      base::BindOnce(&CertificateSignalsCollector::OnCertificateDetailsSigned,
                     weak_ptr_factory_.GetWeakPtr(),
                     std::move(serialized_details_vec), barrier_callback,
                     std::move(data_to_sign)));
}

void CertificateSignalsCollector::OnCertificateDetailsSigned(
    std::vector<uint8_t> certificate_details,
    base::RepeatingCallback<
        void(std::optional<enterprise_management::SignedCertificateDetails>)>
        barrier_callback,
    std::unique_ptr<std::vector<uint8_t>> bound_data_to_sign,
    net::Error error,
    const std::vector<uint8_t>& signature) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (error != net::OK || signature.empty()) {
    LogCertificateCollectionError(CertificateCollectionError::kSigningFailed);
    barrier_callback.Run(std::nullopt);
    return;
  }
  enterprise_management::SignedCertificateDetails signed_details;
  signed_details.set_data(
      std::string(certificate_details.begin(), certificate_details.end()));
  signed_details.set_signature(std::string(signature.begin(), signature.end()));
  barrier_callback.Run(std::move(signed_details));
}

void CertificateSignalsCollector::OnAllCertificatesProcessed(
    base::TimeTicks start_time,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    bool truncated,
    std::vector<std::optional<enterprise_management::SignedCertificateDetails>>
        results) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  CertificateSignalsResponse cert_response;
  cert_response.truncated_certificates = truncated;
  for (auto& result : results) {
    if (result.has_value()) {
      std::string serialized;
      if (!result->SerializeToString(&serialized)) {
        LogCertificateCollectionError(
            CertificateCollectionError::kSerializationFailed);
        continue;
      }
      cert_response.serialized_caa_responses.push_back(std::move(serialized));
    }
  }
  OnSignalsCollected(start_time, response, std::move(done_closure),
                     std::move(cert_response));
}

void CertificateSignalsCollector::OnSignalsCollected(
    base::TimeTicks start_time,
    SignalsAggregationResponse& response,
    base::OnceClosure done_closure,
    CertificateSignalsResponse certificate_signals_response) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (certificate_signals_response.collection_error) {
    LogSignalCollectionFailed(
        SignalName::kCertificates, start_time,
        certificate_signals_response.collection_error.value(),
        /*is_top_level_error=*/false);
  } else {
    LogSignalCollectionSucceeded(
        SignalName::kCertificates, start_time,
        certificate_signals_response.serialized_caa_responses.size());
  }
  response.certificate_signals_response =
      std::move(certificate_signals_response);
  std::move(done_closure).Run();
}

}  // namespace device_signals
