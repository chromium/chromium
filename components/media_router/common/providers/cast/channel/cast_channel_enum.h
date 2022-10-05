// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_CHANNEL_ENUM_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_CHANNEL_ENUM_H_

#include <string>

namespace cast_channel {

// Helper function to convert scoped enums to their underlying type, for use
// with ostreams.
template <typename Enumeration>
auto AsInteger(Enumeration const value) ->
    typename std::underlying_type<Enumeration>::type {
  return static_cast<typename std::underlying_type<Enumeration>::type>(value);
}

// Maps to enum ReadyState in cast_channel.idl
enum class ReadyState {
  NONE,
  CONNECTING,
  OPEN,
  CLOSING,  // TODO(zhaobin): Remove this value because it is unused.
  CLOSED,
};

// Maps to enum ChannelError in cast_channel.idl
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

// Used in ErrorInfo.eventType in cast_channel.idl
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

// Used in ErrorInfo.challengeReplyErrorType in cast_channel.idl
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

}  // namespace cast_channel

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_CAST_CHANNEL_ENUM_H_
