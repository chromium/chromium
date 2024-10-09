// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/logger.h"

#include <stdint.h>

#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "components/media_router/common/providers/cast/channel/cast_auth_util.h"
#include "components/media_router/common/providers/cast/channel/cast_socket.h"
#include "net/base/net_errors.h"

namespace cast_channel {

using net::IPEndPoint;

namespace {

ChallengeReplyError AuthErrorToChallengeReplyError(
    AuthResult::ErrorType error_type) {
  switch (error_type) {
    case AuthResult::ERROR_NONE:
      return ChallengeReplyError::NONE;
    case AuthResult::ERROR_PEER_CERT_EMPTY:
      return ChallengeReplyError::PEER_CERT_EMPTY;
    case AuthResult::ERROR_WRONG_PAYLOAD_TYPE:
      return ChallengeReplyError::WRONG_PAYLOAD_TYPE;
    case AuthResult::ERROR_NO_PAYLOAD:
      return ChallengeReplyError::NO_PAYLOAD;
    case AuthResult::ERROR_PAYLOAD_PARSING_FAILED:
      return ChallengeReplyError::PAYLOAD_PARSING_FAILED;
    case AuthResult::ERROR_MESSAGE_ERROR:
      return ChallengeReplyError::MESSAGE_ERROR;
    case AuthResult::ERROR_NO_RESPONSE:
      return ChallengeReplyError::NO_RESPONSE;
    case AuthResult::ERROR_FINGERPRINT_NOT_FOUND:
      return ChallengeReplyError::FINGERPRINT_NOT_FOUND;
    case AuthResult::ERROR_CERT_PARSING_FAILED:
      return ChallengeReplyError::CERT_PARSING_FAILED;
    case AuthResult::ERROR_CERT_NOT_SIGNED_BY_TRUSTED_CA:
      return ChallengeReplyError::CERT_NOT_SIGNED_BY_TRUSTED_CA;
    case AuthResult::ERROR_CANNOT_EXTRACT_PUBLIC_KEY:
      return ChallengeReplyError::CANNOT_EXTRACT_PUBLIC_KEY;
    case AuthResult::ERROR_SIGNED_BLOBS_MISMATCH:
      return ChallengeReplyError::SIGNED_BLOBS_MISMATCH;
    case AuthResult::ERROR_TLS_CERT_VALIDITY_PERIOD_TOO_LONG:
      return ChallengeReplyError::TLS_CERT_VALIDITY_PERIOD_TOO_LONG;
    case AuthResult::ERROR_TLS_CERT_VALID_START_DATE_IN_FUTURE:
      return ChallengeReplyError::TLS_CERT_VALID_START_DATE_IN_FUTURE;
    case AuthResult::ERROR_TLS_CERT_EXPIRED:
      return ChallengeReplyError::TLS_CERT_EXPIRED;
    case AuthResult::ERROR_CRL_INVALID:
      return ChallengeReplyError::CRL_INVALID;
    case AuthResult::ERROR_CERT_REVOKED:
      return ChallengeReplyError::CERT_REVOKED;
    case AuthResult::ERROR_CRL_OK_FALLBACK_CRL:
      return ChallengeReplyError::CRL_OK_FALLBACK_CRL;
    case AuthResult::ERROR_FALLBACK_CRL_INVALID:
      return ChallengeReplyError::FALLBACK_CRL_INVALID;
    case AuthResult::ERROR_CERTS_REVOKED_BY_FALLBACK_CRL:
      return ChallengeReplyError::CERTS_REVOKED_BY_FALLBACK_CRL;
    case AuthResult::ERROR_SENDER_NONCE_MISMATCH:
      return ChallengeReplyError::SENDER_NONCE_MISMATCH;
    case AuthResult::ERROR_SIGNATURE_EMPTY:
      return ChallengeReplyError::SIGNATURE_EMPTY;
    case AuthResult::ERROR_DIGEST_UNSUPPORTED:
      return ChallengeReplyError::DIGEST_UNSUPPORTED;
    default:
      NOTREACHED_IN_MIGRATION();
      return ChallengeReplyError::NONE;
  }
}

}  // namespace

LastError::LastError()
    : channel_event(ChannelEvent::UNKNOWN),
      challenge_reply_error(ChallengeReplyError::NONE),
      net_return_value(net::OK) {}

LastError::~LastError() = default;

Logger::Logger() {
  // Logger may not be necessarily be created on the IO thread, but logging
  // happens exclusively there.
  DETACH_FROM_THREAD(thread_checker_);
}

Logger::~Logger() = default;

void Logger::LogSocketEventWithRv(int channel_id,
                                  ChannelEvent channel_event,
                                  int rv) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  MaybeSetLastError(channel_id, channel_event, rv, ChallengeReplyError::NONE);
}

void Logger::LogSocketChallengeReplyEvent(int channel_id,
                                          const AuthResult& auth_result) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  MaybeSetLastError(channel_id, ChannelEvent::AUTH_CHALLENGE_REPLY, net::OK,
                    AuthErrorToChallengeReplyError(auth_result.error_type));
}

LastError Logger::GetLastError(int channel_id) const {
  const auto it = last_errors_.find(channel_id);
  return it != last_errors_.end() ? it->second : LastError();
}

void Logger::ClearLastError(int channel_id) {
  last_errors_.erase(channel_id);
}

void Logger::MaybeSetLastError(int channel_id,
                               ChannelEvent channel_event,
                               int rv,
                               ChallengeReplyError challenge_reply_error) {
  auto it = last_errors_.find(channel_id);
  if (it == last_errors_.end())
    last_errors_[channel_id] = LastError();

  LastError* last_error = &last_errors_[channel_id];
  if (rv < net::ERR_IO_PENDING) {
    last_error->net_return_value = rv;
    last_error->channel_event = channel_event;
  }

  if (challenge_reply_error != ChallengeReplyError::NONE) {
    last_error->challenge_reply_error = challenge_reply_error;
    last_error->channel_event = channel_event;
  }
}

}  // namespace cast_channel
