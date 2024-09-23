// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"

#include <ostream>

#include "base/notreached.h"

namespace cast_channel {

#define CAST_CHANNEL_TYPE_TO_STRING(enum) \
  case enum:                              \
    return #enum

// TODO(crbug.com/1291730): Replace with EnumTable.
std::string ReadyStateToString(ReadyState ready_state) {
  switch (ready_state) {
    CAST_CHANNEL_TYPE_TO_STRING(ReadyState::NONE);
    CAST_CHANNEL_TYPE_TO_STRING(ReadyState::CONNECTING);
    CAST_CHANNEL_TYPE_TO_STRING(ReadyState::OPEN);
    CAST_CHANNEL_TYPE_TO_STRING(ReadyState::CLOSING);
    CAST_CHANNEL_TYPE_TO_STRING(ReadyState::CLOSED);
  }
  NOTREACHED_IN_MIGRATION()
      << "Unknown ready_state " << ReadyStateToString(ready_state);
  return "Unknown ready_state";
}

// TODO(crbug.com/1291730): Replace with EnumTable.
std::string ChannelErrorToString(ChannelError channel_error) {
  switch (channel_error) {
    CAST_CHANNEL_TYPE_TO_STRING(ChannelError::NONE);
    CAST_CHANNEL_TYPE_TO_STRING(ChannelError::CHANNEL_NOT_OPEN);
    CAST_CHANNEL_TYPE_TO_STRING(ChannelError::AUTHENTICATION_ERROR);
    CAST_CHANNEL_TYPE_TO_STRING(ChannelError::CONNECT_ERROR);
    CAST_CHANNEL_TYPE_TO_STRING(ChannelError::CAST_SOCKET_ERROR);
    CAST_CHANNEL_TYPE_TO_STRING(ChannelError::TRANSPORT_ERROR);
    CAST_CHANNEL_TYPE_TO_STRING(ChannelError::INVALID_MESSAGE);
    CAST_CHANNEL_TYPE_TO_STRING(ChannelError::INVALID_CHANNEL_ID);
    CAST_CHANNEL_TYPE_TO_STRING(ChannelError::CONNECT_TIMEOUT);
    CAST_CHANNEL_TYPE_TO_STRING(ChannelError::PING_TIMEOUT);
    CAST_CHANNEL_TYPE_TO_STRING(ChannelError::UNKNOWN);
  }
  NOTREACHED_IN_MIGRATION()
      << "Unknown channel_error " << ChannelErrorToString(channel_error);
  return "Unknown channel_error";
}

}  // namespace cast_channel
