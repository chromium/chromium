// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_CHANNEL_ENUM_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_CHANNEL_ENUM_H_

#include <cstdint>
#include <string>

#include "base/types/cxx23_to_underlying.h"

namespace cast_channel {

// Helper function to convert scoped enums to their underlying type, for use
// with ostreams.
template <typename Enumeration>
auto AsInteger(Enumeration const value) {
  return base::to_underlying(value);
}

enum class ReadyState {
  NONE,
  CONNECTING,
  OPEN,
  CLOSING,  // TODO(zhaobin): Remove this value because it is unused.
  CLOSED,
};

enum class ChannelError {
  NONE,
  CHANNEL_NOT_OPEN,
  AUTHENTICATION_ERROR,
  CONNECT_ERROR,
  CAST_SOCKET_ERROR,
  TRANSPORT_ERROR,
  INVALID_MESSAGE,
  INVALID_CHANNEL_ID,
  CONNECT_TIMEOUT,
  PING_TIMEOUT,
  UNKNOWN,
};

enum class ChannelEvent {
  UNKNOWN = 0,
  CAST_SOCKET_CREATED,
  READY_STATE_CHANGED,
  CONNECTION_STATE_CHANGED,
  READ_STATE_CHANGED,
  WRITE_STATE_CHANGED,
  ERROR_STATE_CHANGED,
  CONNECT_FAILED,
  TCP_SOCKET_CONNECT,  // Logged with RV.
  TCP_SOCKET_SET_KEEP_ALIVE,
  SSL_CERT_WHITELISTED,
  SSL_SOCKET_CONNECT,  // Logged with RV.
  SSL_INFO_OBTAINED,
  DER_ENCODED_CERT_OBTAIN,  // Logged with RV.
  RECEIVED_CHALLENGE_REPLY,
  AUTH_CHALLENGE_REPLY,
  CONNECT_TIMED_OUT,
  SEND_MESSAGE_FAILED,
  MESSAGE_ENQUEUED,  // Message
  SOCKET_WRITE,      // Logged with RV.
  MESSAGE_WRITTEN,   // Message
  SOCKET_READ,       // Logged with RV.
  MESSAGE_READ,      // Message
  SOCKET_CLOSED,
  SSL_CERT_EXCESSIVE_LIFETIME,
  CHANNEL_POLICY_ENFORCED,
  TCP_SOCKET_CONNECT_COMPLETE,  // Logged with RV.
  SSL_SOCKET_CONNECT_COMPLETE,  // Logged with RV.
  SSL_SOCKET_CONNECT_FAILED,    // Logged with RV.
  SEND_AUTH_CHALLENGE_FAILED,   // Logged with RV.
  AUTH_CHALLENGE_REPLY_INVALID,
  PING_WRITE_ERROR,  // Logged with RV.
};

enum class ChallengeReplyError {
  NONE = 1,
  PEER_CERT_EMPTY,
  WRONG_PAYLOAD_TYPE,
  NO_PAYLOAD,
  PAYLOAD_PARSING_FAILED,
  MESSAGE_ERROR,
  NO_RESPONSE,
  FINGERPRINT_NOT_FOUND,
  CERT_PARSING_FAILED,
  CERT_NOT_SIGNED_BY_TRUSTED_CA,
  CANNOT_EXTRACT_PUBLIC_KEY,
  SIGNED_BLOBS_MISMATCH,
  TLS_CERT_VALIDITY_PERIOD_TOO_LONG,
  TLS_CERT_VALID_START_DATE_IN_FUTURE,
  TLS_CERT_EXPIRED,
  CRL_INVALID,
  CERT_REVOKED,
  CRL_OK_FALLBACK_CRL,
  FALLBACK_CRL_INVALID,
  CERTS_REVOKED_BY_FALLBACK_CRL,
  SENDER_NONCE_MISMATCH,
  SIGNATURE_EMPTY,
  DIGEST_UNSUPPORTED,
};

// Used by CastSocket/CastTransport to track connection state.
enum class ConnectionState {
  UNKNOWN,
  TCP_CONNECT,
  TCP_CONNECT_COMPLETE,
  SSL_CONNECT,
  SSL_CONNECT_COMPLETE,
  AUTH_CHALLENGE_SEND,
  AUTH_CHALLENGE_SEND_COMPLETE,
  AUTH_CHALLENGE_REPLY_COMPLETE,
  START_CONNECT,
  FINISHED,  // Terminal states here and below.
  CONNECT_ERROR,
  TIMEOUT,
};

// Used by CastSocket/CastTransport to track read state.
enum class ReadState {
  UNKNOWN,
  READ,
  READ_COMPLETE,
  DO_CALLBACK,
  HANDLE_ERROR,
  READ_ERROR,  // Terminal state.
};

// Used by CastSocket/CastTransport to track write state.
enum class WriteState {
  UNKNOWN,
  WRITE,
  WRITE_COMPLETE,
  DO_CALLBACK,
  HANDLE_ERROR,
  WRITE_ERROR,  // Terminal states here and below.
  IDLE,
};

std::string ReadyStateToString(ReadyState ready_state);
std::string ChannelErrorToString(ChannelError channel_error);

constexpr int kNumCastChannelFlags = 9;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Keep in sync with CastChannelFlag enum in tools/metrics/histograms/enums.xml.
enum class CastChannelFlag : uint16_t {
  kFlagsNone = 0,
  kSha1DigestAlgorithm = 1,
  kSenderNonceMissing = 1 << 1,
  kSenderNonceMismatch = 1 << 2,
  kCRLMissing = 1 << 3,
  kCRLInvalid = 1 << 4,
  kCertificateRevoked = 1 << 5,
  kInvalidFallbackCRL = 1 << 6,
  kCertificateRevokedByFallbackCRL = 1 << 7,
  kCertificateAcceptedByFallbackCRL = 1 << 8,
  kMaxValue = kCertificateAcceptedByFallbackCRL,
};

using CastChannelFlags = uint16_t;

constexpr CastChannelFlags kCastChannelFlagsNone =
    static_cast<CastChannelFlags>(CastChannelFlag::kFlagsNone);

}  // namespace cast_channel

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_CHANNEL_ENUM_H_
