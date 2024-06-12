// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_AUTH_UTIL_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_AUTH_UTIL_H_

#include <string>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"
#include "third_party/openscreen/src/cast/common/channel/proto/cast_channel.pb.h"

namespace cast_certificate {
enum class CRLPolicy;
}

namespace net {
class X509Certificate;
}  // namespace net

namespace bssl {
class TrustStore;
}  // namespace bssl

namespace cast_channel {

using ::openscreen::cast::proto::AuthResponse;
using ::openscreen::cast::proto::CastMessage;

BASE_DECLARE_FEATURE(kEnforceNonceChecking);
BASE_DECLARE_FEATURE(kEnforceSHA256Checking);
BASE_DECLARE_FEATURE(kEnforceFallbackCRLRevocationChecking);
BASE_DECLARE_FEATURE(kEnforceRevocationChecking);

struct AuthResult {
 public:
  enum ErrorType {
    ERROR_NONE,
    ERROR_PEER_CERT_EMPTY,
    ERROR_WRONG_PAYLOAD_TYPE,
    ERROR_NO_PAYLOAD,
    ERROR_PAYLOAD_PARSING_FAILED,
    ERROR_MESSAGE_ERROR,
    ERROR_NO_RESPONSE,
    ERROR_FINGERPRINT_NOT_FOUND,
    ERROR_CERT_PARSING_FAILED,
    ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA,
    ERROR_CANNOT_EXTRACT_PUBLIC_KEY,
    ERROR_SIGNED_BLOBS_MISMATCH,
    ERROR_TLS_CERT_VALIDITY_PERIOD_TOO_LONG,
    ERROR_TLS_CERT_VALID_START_DATE_IN_FUTURE,
    ERROR_TLS_CERT_EXPIRED,
    ERROR_CRL_INVALID,
    ERROR_CERT_REVOKED,
    ERROR_CRL_OK_FALLBACK_CRL,
    ERROR_FALLBACK_CRL_INVALID,
    ERROR_CERTS_REVOKED_BY_FALLBACK_CRL,
    ERROR_SENDER_NONCE_MISMATCH,
    ERROR_DIGEST_UNSUPPORTED,
    ERROR_SIGNATURE_EMPTY,
  };

  enum PolicyType { POLICY_NONE = 0, POLICY_AUDIO_ONLY = 1 << 0 };

  // Constructs a AuthResult that corresponds to success.
  AuthResult();

  AuthResult(const std::string& error_message,
             ErrorType error_type,
             CastChannelFlag flag = CastChannelFlag::kFlagsNone);

  ~AuthResult();

  static AuthResult CreateWithParseError(const std::string& error_message,
                                         ErrorType error_type);

  void set_flag(CastChannelFlag flag) { flags |= static_cast<uint16_t>(flag); }

  bool success() const {
    return error_type == ERROR_NONE || error_type == ERROR_CRL_OK_FALLBACK_CRL;
  }

  // Copies any flags set in `source` to this object's flags.
  void CopyFlagsFrom(const AuthResult& source);

  std::string error_message;
  ErrorType error_type{ERROR_NONE};
  unsigned int channel_policies{POLICY_NONE};
  CastChannelFlags flags{kCastChannelFlagsNone};
};

class AuthContext {
 public:
  ~AuthContext();

  // Get an auth challenge context.
  // The same context must be used in the challenge and reply.
  static AuthContext Create();

  static AuthContext CreateForTest(const std::string& nonce);

  // Verifies the nonce received in the response is equivalent to the one sent.
  // Returns success if |nonce_response| matches nonce_
  AuthResult VerifySenderNonce(const std::string& nonce_response) const;

  // The nonce challenge.
  const std::string& nonce() const { return nonce_; }

 private:
  explicit AuthContext(const std::string& nonce);

  const std::string nonce_;
};

// Authenticates the given |challenge_reply|:
// 1. Signature contained in the reply is valid.
// 2. Certficate used to sign is rooted to a trusted CA.
AuthResult AuthenticateChallengeReply(const CastMessage& challenge_reply,
                                      const net::X509Certificate& peer_cert,
                                      const AuthContext& auth_context);

// Performs a quick check of the TLS certificate for time validity requirements.
AuthResult VerifyTLSCertificate(const net::X509Certificate& peer_cert,
                                std::string* peer_cert_der,
                                const base::Time& verification_time);

// Auth-library specific implementation of cryptographic signature
// verification routines. Verifies that |response| contains a
// valid signature of |signature_input|.
AuthResult VerifyCredentials(const AuthResponse& response,
                             const std::string& signature_input);

// Exposed for testing only.
//
// Overloaded version of VerifyCredentials that allows modifying
// the crl policy, trust stores, and verification times.
AuthResult VerifyCredentialsForTest(
    const AuthResponse& response,
    const std::string& signature_input,
    const cast_certificate::CRLPolicy& crl_policy,
    bssl::TrustStore* crl_trust_store,
    const base::Time& verification_time);

}  // namespace cast_channel

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_AUTH_UTIL_H_
