// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_auth_util.h"

#include <vector>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/memory/singleton.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "components/media_router/common/providers/cast/certificate/cast_cert_validator.h"
#include "components/media_router/common/providers/cast/certificate/cast_crl.h"
#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"
#include "components/media_router/common/providers/cast/channel/cast_message_util.h"
#include "crypto/random.h"
#include "net/cert/pki/signature_algorithm.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util.h"
#include "net/der/parse_values.h"

namespace cast_channel {

// Enforce nonce checking when enabled.
// If disabled, the nonce value returned from the device is not checked against
// the one sent to the device. As a result, the nonce can be empty and omitted
// from the signature. This allows backwards compatibility with legacy Cast
// receivers.
BASE_FEATURE(kEnforceNonceChecking,
             "CastNonceEnforced",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enforce the use of SHA256 digest for signatures.
// If disabled, the device may respond with a signature with SHA1 digest even
// though a signature with SHA256 digest was requested in the challenge. This
// allows for backwards compatibility with legacy Cast receivers.
BASE_FEATURE(kEnforceSHA256Checking,
             "CastSHA256Enforced",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace {

const char kParseErrorPrefix[] = "Failed to parse auth message: ";

// The maximum number of days a cert can live for.
const int kMaxSelfSignedCertLifetimeInDays = 4;

// The size of the nonce challenge in bytes.
const int kNonceSizeInBytes = 16;

// The number of hours after which a nonce is regenerated.
long kNonceExpirationTimeInHours = 24;

// Enforce certificate revocation when enabled.
// If disabled, any revocation failures are ignored.
//
// This flags only controls the enforcement. Revocation is checked regardless.
//
// This flag tracks the changes necessary to fully enforce revocation.
BASE_FEATURE(kEnforceRevocationChecking,
             "CastCertificateRevocation",
             base::FEATURE_DISABLED_BY_DEFAULT);

namespace cast_crypto = ::cast_certificate;

// Extracts an embedded DeviceAuthMessage payload from an auth challenge reply
// message.
AuthResult ParseAuthMessage(const CastMessage& challenge_reply,
                            DeviceAuthMessage* auth_message) {
  if (challenge_reply.payload_type() !=
      cast::channel::CastMessage_PayloadType_BINARY) {
    return AuthResult::CreateWithParseError(
        "Wrong payload type in challenge reply",
        AuthResult::ERROR_WRONG_PAYLOAD_TYPE);
  }
  if (!challenge_reply.has_payload_binary()) {
    return AuthResult::CreateWithParseError(
        "Payload type is binary but payload_binary field not set",
        AuthResult::ERROR_NO_PAYLOAD);
  }
  if (!auth_message->ParseFromString(challenge_reply.payload_binary())) {
    return AuthResult::CreateWithParseError(
        "Cannot parse binary payload into DeviceAuthMessage",
        AuthResult::ERROR_PAYLOAD_PARSING_FAILED);
  }

  VLOG(1) << "Auth message: " << AuthMessageToString(*auth_message);

  if (auth_message->has_error()) {
    return AuthResult::CreateWithParseError(
        "Auth message error: " +
            base::NumberToString(auth_message->error().error_type()),
        AuthResult::ERROR_MESSAGE_ERROR);
  }
  if (!auth_message->has_response()) {
    return AuthResult::CreateWithParseError(
        "Auth message has no response field", AuthResult::ERROR_NO_RESPONSE);
  }
  return AuthResult();
}

class CastNonce {
 public:
  static CastNonce* GetInstance() {
    return base::Singleton<CastNonce,
                           base::LeakySingletonTraits<CastNonce>>::get();
  }

  static const std::string& Get() {
    GetInstance()->EnsureNonceTimely();
    return GetInstance()->nonce_;
  }

 private:
  friend struct base::DefaultSingletonTraits<CastNonce>;

  CastNonce() { GenerateNonce(); }
  void GenerateNonce() {
    // Create a cryptographically secure nonce.
    crypto::RandBytes(base::WriteInto(&nonce_, kNonceSizeInBytes + 1),
                      kNonceSizeInBytes);
    nonce_generation_time_ = base::Time::Now();
  }

  void EnsureNonceTimely() {
    if (base::Time::Now() >
        (nonce_generation_time_ + base::Hours(kNonceExpirationTimeInHours))) {
      GenerateNonce();
    }
  }

  // The nonce challenge to send to the Cast receiver.
  // The nonce is updated daily.
  std::string nonce_;
  base::Time nonce_generation_time_;
};

// Must match with histogram enum CastCertificateStatus.
// This should never be reordered.
enum CertVerificationStatus {
  CERT_STATUS_OK,
  CERT_STATUS_INVALID_CRL,
  CERT_STATUS_VERIFICATION_FAILED,
  CERT_STATUS_REVOKED,
  CERT_STATUS_MISSING_CRL,
  CERT_STATUS_PARSE_FAILED,
  CERT_STATUS_DATE_INVALID,
  CERT_STATUS_RESTRICTIONS_FAILED,
  CERT_STATUS_MISSING_CERTS,
  CERT_STATUS_UNEXPECTED_FAILED,
  CERT_STATUS_COUNT,
};

// Must match with histogram enum CastNonce.
// This should never be reordered.
enum NonceVerificationStatus {
  NONCE_MATCH,
  NONCE_MISMATCH,
  NONCE_MISSING,
  NONCE_COUNT,
};

// Must match with the histogram enum CastSignature.
// This should never be reordered.
enum SignatureStatus {
  SIGNATURE_OK,
  SIGNATURE_EMPTY,
  SIGNATURE_VERIFY_FAILED,
  SIGNATURE_ALGORITHM_UNSUPPORTED,
  SIGNATURE_COUNT,
};

// TODO(crbug.com/1413760): Move Record* functions and related enums to
// cast_channel_metrics.h to simplify this file.

// Record certificate verification histogram events.
void RecordCertificateEvent(CertVerificationStatus event) {
  UMA_HISTOGRAM_ENUMERATION("Cast.Channel.Certificate", event,
                            CERT_STATUS_COUNT);
}

// Record nonce verification histogram events.
void RecordNonceEvent(NonceVerificationStatus event) {
  UMA_HISTOGRAM_ENUMERATION("Cast.Channel.Nonce", event, NONCE_COUNT);
}

// Record signature verification histogram events.
void RecordSignatureEvent(SignatureStatus event) {
  UMA_HISTOGRAM_ENUMERATION("Cast.Channel.Signature", event, SIGNATURE_COUNT);
}

// Maps CastCertError to AuthResult.
// If crl_required is set to false, all revocation related errors are ignored.
AuthResult MapToAuthResult(cast_certificate::CastCertError error,
                           bool crl_required) {
  switch (error) {
    case cast_certificate::CastCertError::ERR_CERTS_MISSING:
      RecordCertificateEvent(CERT_STATUS_MISSING_CERTS);
      return AuthResult("Failed to locate certificates.",
                        AuthResult::ERROR_PEER_CERT_EMPTY);
    case cast_certificate::CastCertError::ERR_CERTS_PARSE:
      RecordCertificateEvent(CERT_STATUS_PARSE_FAILED);
      return AuthResult("Failed to parse certificates.",
                        AuthResult::ERROR_CERT_PARSING_FAILED);
    case cast_certificate::CastCertError::ERR_CERTS_DATE_INVALID:
      RecordCertificateEvent(CERT_STATUS_DATE_INVALID);
      return AuthResult("Failed date validity check.",
                        AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA);
    case cast_certificate::CastCertError::ERR_CERTS_VERIFY_GENERIC:
      RecordCertificateEvent(CERT_STATUS_VERIFICATION_FAILED);
      return AuthResult("Failed with a generic certificate verification error.",
                        AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA);
    case cast_certificate::CastCertError::ERR_CERTS_RESTRICTIONS:
      RecordCertificateEvent(CERT_STATUS_RESTRICTIONS_FAILED);
      return AuthResult("Failed certificate restrictions.",
                        AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA);
    case cast_certificate::CastCertError::ERR_CRL_INVALID:
      // Histogram events are recorded during CRL verification.
      // This error is only encountered if |crl_required| is true.
      DCHECK(crl_required);
      return AuthResult("Failed to provide a valid CRL.",
                        AuthResult::ERROR_CRL_INVALID,
                        CastChannelFlag::kCRLInvalid);
    case cast_certificate::CastCertError::ERR_CERTS_REVOKED:
      RecordCertificateEvent(CERT_STATUS_REVOKED);
      // Revocation check is the last step of Cast certificate verification.
      // If this error is encountered, the rest of certificate verification has
      // succeeded.
      if (!crl_required) {
        AuthResult success;
        success.set_flag(CastChannelFlag::kCertificateRevoked);
        return success;
      }
      return AuthResult("Failed certificate revocation check.",
                        AuthResult::ERROR_CERT_REVOKED,
                        CastChannelFlag::kCertificateRevoked);
    case cast_certificate::CastCertError::ERR_UNEXPECTED:
      RecordCertificateEvent(CERT_STATUS_UNEXPECTED_FAILED);
      return AuthResult("Failed verifying cast device certificate.",
                        AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA);
    case cast_certificate::CastCertError::OK:
      return AuthResult();
  }
  return AuthResult();
}

}  // namespace

AuthResult::AuthResult() = default;

AuthResult::AuthResult(const std::string& error_message,
                       ErrorType error_type,
                       CastChannelFlag flag)
    : error_message(error_message),
      error_type(error_type),
      flags(static_cast<CastChannelFlags>(flag)) {}

AuthResult::~AuthResult() = default;

// static
AuthResult AuthResult::CreateWithParseError(const std::string& error_message,
                                            ErrorType error_type) {
  return AuthResult(kParseErrorPrefix + error_message, error_type);
}

// static
AuthContext AuthContext::Create() {
  return AuthContext(CastNonce::Get());
}

// static
AuthContext AuthContext::CreateForTest(const std::string& nonce_data) {
  // Given some garbage data, try to turn it into a string that at least has the
  // right length.
  std::string nonce;
  if (nonce_data.empty()) {
    nonce = std::string(kNonceSizeInBytes, '0');
  } else {
    while (nonce.size() < kNonceSizeInBytes) {
      nonce += nonce_data;
    }
    nonce.erase(kNonceSizeInBytes);
  }
  DCHECK(nonce.size() == kNonceSizeInBytes);
  return AuthContext(nonce);
}

AuthContext::AuthContext(const std::string& nonce) : nonce_(nonce) {}

AuthContext::~AuthContext() = default;

AuthResult AuthContext::VerifySenderNonce(
    const std::string& nonce_response) const {
  AuthResult success;
  if (nonce_ != nonce_response) {
    if (nonce_response.empty()) {
      RecordNonceEvent(NONCE_MISSING);
      success.set_flag(CastChannelFlag::kSenderNonceMissing);
    } else {
      RecordNonceEvent(NONCE_MISMATCH);
      success.set_flag(CastChannelFlag::kSenderNonceMismatch);
    }
    if (base::FeatureList::IsEnabled(kEnforceNonceChecking)) {
      return AuthResult("Sender nonce mismatched.",
                        AuthResult::ERROR_SENDER_NONCE_MISMATCH,
                        CastChannelFlag::kSenderNonceMismatch);
    }
  } else {
    RecordNonceEvent(NONCE_MATCH);
  }
  return success;
}

AuthResult VerifyAndMapDigestAlgorithm(
    cast::channel::HashAlgorithm response_digest_algorithm,
    cast_certificate::CastDigestAlgorithm* digest_algorithm) {
  AuthResult success;
  switch (response_digest_algorithm) {
    case cast::channel::SHA1:
      RecordSignatureEvent(SIGNATURE_ALGORITHM_UNSUPPORTED);
      *digest_algorithm = cast_certificate::CastDigestAlgorithm::SHA1;
      if (base::FeatureList::IsEnabled(kEnforceSHA256Checking)) {
        return AuthResult("Unsupported digest algorithm.",
                          AuthResult::ERROR_DIGEST_UNSUPPORTED,
                          CastChannelFlag::kSha1DigestAlgorithm);
      } else {
        success.set_flag(CastChannelFlag::kSha1DigestAlgorithm);
      }
      break;
    case cast::channel::SHA256:
      *digest_algorithm = cast_certificate::CastDigestAlgorithm::SHA256;
      break;
  }
  return success;
}

// Verifies the peer certificate and populates |peer_cert_der| with the DER
// encoded certificate.
AuthResult VerifyTLSCertificate(const net::X509Certificate& peer_cert,
                                std::string* peer_cert_der,
                                const base::Time& verification_time) {
  // Get the DER-encoded form of the certificate.
  *peer_cert_der = std::string(
      net::x509_util::CryptoBufferAsStringPiece(peer_cert.cert_buffer()));

  // Ensure the peer cert is valid and doesn't have an excessive remaining
  // lifetime. Although it is not verified as an X.509 certificate, the entire
  // structure is signed by the AuthResponse, so the validity field from X.509
  // is repurposed as this signature's expiration.
  base::Time expiry = peer_cert.valid_expiry();
  base::Time lifetime_limit =
      verification_time + base::Days(kMaxSelfSignedCertLifetimeInDays);
  if (peer_cert.valid_start().is_null() ||
      peer_cert.valid_start() > verification_time) {
    return AuthResult::CreateWithParseError(
        "Certificate's valid start date is in the future.",
        AuthResult::ERROR_TLS_CERT_VALID_START_DATE_IN_FUTURE);
  }
  if (expiry.is_null() || peer_cert.valid_expiry() < verification_time) {
    return AuthResult::CreateWithParseError("Certificate has expired.",
                                            AuthResult::ERROR_TLS_CERT_EXPIRED);
  }
  if (expiry > lifetime_limit) {
    return AuthResult::CreateWithParseError(
        "Peer cert lifetime is too long.",
        AuthResult::ERROR_TLS_CERT_VALIDITY_PERIOD_TOO_LONG);
  }
  return AuthResult();
}

AuthResult AuthenticateChallengeReply(const CastMessage& challenge_reply,
                                      const net::X509Certificate& peer_cert,
                                      const AuthContext& auth_context) {
  DeviceAuthMessage auth_message;
  AuthResult result = ParseAuthMessage(challenge_reply, &auth_message);
  if (!result.success()) {
    return result;
  }

  std::string peer_cert_der;
  result = VerifyTLSCertificate(peer_cert, &peer_cert_der, base::Time::Now());
  if (!result.success()) {
    return result;
  }

  const AuthResponse& response = auth_message.response();
  const std::string& nonce_response = response.sender_nonce();

  result = auth_context.VerifySenderNonce(nonce_response);
  if (!result.success()) {
    return result;
  }

  return VerifyCredentials(response, nonce_response + peer_cert_der);
}

// This function does the following
//
// * Verifies that the certificate chain |response.client_auth_certificate| +
//   |response.intermediate_certificate| is valid and chains to a trusted
//   Cast root. The list of trusted Cast roots can be overrided by providing a
//   non-nullptr |cast_trust_store|. The certificate is verified at
//   |verification_time|.
//
// * Verifies that none of the certificates in the chain are revoked based on
//   the CRL provided in the response |response.crl|. The CRL is verified to be
//   valid and its issuer certificate chains to a trusted Cast CRL root. The
//   list of trusted Cast CRL roots can be overrided by providing a non-nullptr
//   |crl_trust_store|. If |crl_policy| is CRL_OPTIONAL then the result of
//   revocation checking is ignored. The CRL is verified at
//   |verification_time|.
//
// * Verifies that |response.signature| matches the signature
//   of |signature_input| by |response.client_auth_certificate|'s public
//   key.
AuthResult VerifyCredentialsImpl(const AuthResponse& response,
                                 const std::string& signature_input,
                                 const cast_crypto::CRLPolicy& crl_policy,
                                 net::TrustStore* cast_trust_store,
                                 net::TrustStore* crl_trust_store,
                                 const base::Time& verification_time) {
  // Verify the certificate
  std::unique_ptr<cast_crypto::CertVerificationContext> verification_context;

  // Build a single vector containing the certificate chain.
  std::vector<std::string> cert_chain;
  cert_chain.push_back(response.client_auth_certificate());
  cert_chain.insert(cert_chain.end(),
                    response.intermediate_certificate().begin(),
                    response.intermediate_certificate().end());

  // Parse the CRL.
  std::unique_ptr<cast_crypto::CastCRL> crl;
  if (response.crl().empty()) {
    RecordCertificateEvent(CERT_STATUS_MISSING_CRL);
  } else {
    crl = cast_crypto::ParseAndVerifyCRLUsingCustomTrustStore(
        response.crl(), verification_time, crl_trust_store);
    if (!crl) {
      RecordCertificateEvent(CERT_STATUS_INVALID_CRL);
    }
  }

  // Perform certificate verification.
  cast_crypto::CastDeviceCertPolicy device_policy;
  cast_crypto::CastCertError verify_result =
      cast_crypto::VerifyDeviceCertUsingCustomTrustStore(
          cert_chain, verification_time, &verification_context, &device_policy,
          crl.get(), crl_policy, cast_trust_store);

  // Handle and report errors.
  AuthResult result = MapToAuthResult(
      verify_result, crl_policy == cast_crypto::CRLPolicy::CRL_REQUIRED);
  if (!result.success())
    return result;

  // The certificate is verified at this point.
  RecordCertificateEvent(CERT_STATUS_OK);

  if (response.signature().empty() && !signature_input.empty()) {
    RecordSignatureEvent(SIGNATURE_EMPTY);
    return AuthResult("Signature is empty.", AuthResult::ERROR_SIGNATURE_EMPTY);
  }
  cast_certificate::CastDigestAlgorithm digest_algorithm;
  AuthResult digest_result =
      VerifyAndMapDigestAlgorithm(response.hash_algorithm(), &digest_algorithm);
  if (!digest_result.success())
    return digest_result;

  if (!verification_context->VerifySignatureOverData(
          response.signature(), signature_input, digest_algorithm)) {
    // For fuzz testing we just pretend the signature was OK.  The signature is
    // normally verified using boringssl, which has its own fuzz tests.
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    RecordSignatureEvent(SIGNATURE_VERIFY_FAILED);
    return AuthResult("Failed verifying signature over data.",
                      AuthResult::ERROR_SIGNED_BLOBS_MISMATCH);
#endif
  }
  RecordSignatureEvent(SIGNATURE_OK);

  AuthResult success;

  // Set the policy into the result.
  switch (device_policy) {
    case cast_crypto::CastDeviceCertPolicy::AUDIO_ONLY:
      success.channel_policies = AuthResult::POLICY_AUDIO_ONLY;
      break;
    case cast_crypto::CastDeviceCertPolicy::NONE:
      success.channel_policies = AuthResult::POLICY_NONE;
      break;
  }

  return success;
}

AuthResult VerifyCredentials(const AuthResponse& response,
                             const std::string& signature_input) {
  base::Time now = base::Time::Now();
  cast_crypto::CRLPolicy policy = cast_crypto::CRLPolicy::CRL_REQUIRED;
  if (!base::FeatureList::IsEnabled(kEnforceRevocationChecking)) {
    policy = cast_crypto::CRLPolicy::CRL_OPTIONAL;
  }
  return VerifyCredentialsImpl(response, signature_input, policy, nullptr,
                               nullptr, now);
}

AuthResult VerifyCredentialsForTest(const AuthResponse& response,
                                    const std::string& signature_input,
                                    const cast_crypto::CRLPolicy& crl_policy,
                                    net::TrustStore* cast_trust_store,
                                    net::TrustStore* crl_trust_store,
                                    const base::Time& verification_time) {
  return VerifyCredentialsImpl(response, signature_input, crl_policy,
                               cast_trust_store, crl_trust_store,
                               verification_time);
}

}  // namespace cast_channel
