// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_LOGGER_H_
#define COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_LOGGER_H_

#include <stddef.h>

#include <map>
#include <memory>

#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"

namespace cast_channel {

struct AuthResult;

// Holds the most recent error encountered by a CastSocket.
struct LastError {
 public:
  LastError();
  ~LastError();

  // The most recent event that occurred at the time of the error.
  ChannelEvent channel_event;

  // The most recent ChallengeReplyError logged for the socket.
  // NOTE(mfoltz): AuthResult::ErrorType is zero-indexed and ChallengeReplyError
  // is one-indexed, so we can't use AuthResult::ErrorType here.
  ChallengeReplyError challenge_reply_error;

  // The most recent net_return_value logged for the socket.
  int net_return_value;
};

// Called with events that occur on a Cast Channel and remembers any that
// warrant reporting to the caller in LastError.
class Logger : public base::RefCountedThreadSafe<Logger> {
 public:
  Logger();

  Logger(const Logger&) = delete;
  Logger& operator=(const Logger&) = delete;

  // For events that involves socket / crypto operations that returns a value.
  void LogSocketEventWithRv(int channel_id, ChannelEvent channel_event, int rv);

  // For AUTH_CHALLENGE_REPLY event.
  void LogSocketChallengeReplyEvent(int channel_id,
                                    const AuthResult& auth_result);

  // Returns the last errors logged for |channel_id|.
  LastError GetLastError(int channel_id) const;

  // Removes a LastError entry for |channel_id| if one exists.
  void ClearLastError(int channel_id);

 private:
  friend class base::RefCountedThreadSafe<Logger>;
  ~Logger();

  // Propagate any error values in |rv| or |challenge_reply_error| to the
  // LastError for |channel_id|.
  void MaybeSetLastError(int channel_id,
                         ChannelEvent channel_event,
                         int rv,
                         ChallengeReplyError challenge_reply_error);

  // Maps channel_id to the LastError for the channel.
  std::map<int, LastError> last_errors_;

  THREAD_CHECKER(thread_checker_);
};
}  // namespace cast_channel

#endif  // COMPONENTS_MEDIA_ROUTER_COMMON_PROVIDERS_CAST_CHANNEL_LOGGER_H_
