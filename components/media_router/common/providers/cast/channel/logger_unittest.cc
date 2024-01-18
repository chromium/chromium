// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "components/media_router/common/providers/cast/channel/cast_auth_util.h"
#include "components/media_router/common/providers/cast/channel/logger.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace cast_channel {

TEST(CastChannelLoggerTest, LogLastErrorEvents) {
  scoped_refptr<Logger> logger(new Logger());

  // Net return value is set to an error
  logger->LogSocketEventWithRv(1, ChannelEvent::TCP_SOCKET_CONNECT,
                               net::ERR_CONNECTION_FAILED);

  LastError last_error = logger->GetLastError(1);
  EXPECT_EQ(last_error.channel_event, ChannelEvent::TCP_SOCKET_CONNECT);
  EXPECT_EQ(last_error.net_return_value, net::ERR_CONNECTION_FAILED);

  // Challenge reply error set
  AuthResult auth_result = AuthResult::CreateWithParseError(
      "Some error", AuthResult::ErrorType::ERROR_PEER_CERT_EMPTY);

  logger->LogSocketChallengeReplyEvent(2, auth_result);
  last_error = logger->GetLastError(2);
  EXPECT_EQ(last_error.channel_event, ChannelEvent::AUTH_CHALLENGE_REPLY);
  EXPECT_EQ(last_error.challenge_reply_error,
            ChallengeReplyError::PEER_CERT_EMPTY);

  // Logging a non-error event does not set the LastError for the channel.
  logger->LogSocketEventWithRv(3, ChannelEvent::TCP_SOCKET_CONNECT, net::OK);
  last_error = logger->GetLastError(3);
  EXPECT_EQ(last_error.channel_event, ChannelEvent::UNKNOWN);
  EXPECT_EQ(last_error.net_return_value, net::OK);
  EXPECT_EQ(last_error.challenge_reply_error, ChallengeReplyError::NONE);

  // Now log a challenge reply error.  LastError will be set.
  auth_result =
      AuthResult("Some error failed", AuthResult::ERROR_WRONG_PAYLOAD_TYPE);
  logger->LogSocketChallengeReplyEvent(3, auth_result);
  last_error = logger->GetLastError(3);
  EXPECT_EQ(last_error.channel_event, ChannelEvent::AUTH_CHALLENGE_REPLY);
  EXPECT_EQ(last_error.challenge_reply_error,
            ChallengeReplyError::WRONG_PAYLOAD_TYPE);

  // Logging a non-error event does not change the LastError for the channel.
  logger->LogSocketEventWithRv(3, ChannelEvent::TCP_SOCKET_CONNECT, net::OK);
  last_error = logger->GetLastError(3);
  EXPECT_EQ(last_error.channel_event, ChannelEvent::AUTH_CHALLENGE_REPLY);
  EXPECT_EQ(last_error.challenge_reply_error,
            ChallengeReplyError::WRONG_PAYLOAD_TYPE);

  // Logging a CRL related error event should reflect in the reply error type.
  auth_result =
      AuthResult("ERROR_CRL_INVALID failed", AuthResult::ERROR_CRL_INVALID);
  logger->LogSocketChallengeReplyEvent(4, auth_result);
  last_error = logger->GetLastError(4);
  EXPECT_EQ(last_error.channel_event, ChannelEvent::AUTH_CHALLENGE_REPLY);
  EXPECT_EQ(last_error.challenge_reply_error, ChallengeReplyError::CRL_INVALID);

  auth_result =
      AuthResult("ERROR_CERT_REVOKED failed", AuthResult::ERROR_CERT_REVOKED);
  logger->LogSocketChallengeReplyEvent(5, auth_result);
  last_error = logger->GetLastError(5);
  EXPECT_EQ(last_error.channel_event, ChannelEvent::AUTH_CHALLENGE_REPLY);
  EXPECT_EQ(last_error.challenge_reply_error,
            ChallengeReplyError::CERT_REVOKED);

  auth_result = AuthResult("ERROR_CRL_OK_FALLBACK_CRL success",
                           AuthResult::ERROR_CRL_OK_FALLBACK_CRL);
  logger->LogSocketChallengeReplyEvent(5, auth_result);
  last_error = logger->GetLastError(5);
  EXPECT_EQ(last_error.channel_event, ChannelEvent::AUTH_CHALLENGE_REPLY);
  EXPECT_EQ(last_error.challenge_reply_error,
            ChallengeReplyError::CRL_OK_FALLBACK_CRL);

  auth_result = AuthResult("ERROR_CRL_OK_FALLBACK_CRL failed",
                           AuthResult::ERROR_FALLBACK_CRL_INVALID);
  logger->LogSocketChallengeReplyEvent(5, auth_result);
  last_error = logger->GetLastError(5);
  EXPECT_EQ(last_error.channel_event, ChannelEvent::AUTH_CHALLENGE_REPLY);
  EXPECT_EQ(last_error.challenge_reply_error,
            ChallengeReplyError::FALLBACK_CRL_INVALID);

  auth_result = AuthResult("ERROR_CERTS_REVOKED_BY_FALLBACK_CRL failed",
                           AuthResult::ERROR_CERTS_REVOKED_BY_FALLBACK_CRL);
  logger->LogSocketChallengeReplyEvent(5, auth_result);
  last_error = logger->GetLastError(5);
  EXPECT_EQ(last_error.channel_event, ChannelEvent::AUTH_CHALLENGE_REPLY);
  EXPECT_EQ(last_error.challenge_reply_error,
            ChallengeReplyError::CERTS_REVOKED_BY_FALLBACK_CRL);
}

}  // namespace cast_channel
