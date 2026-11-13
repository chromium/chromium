// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/media_router/common/providers/cast/channel/cast_channel_enum.h"

#include "base/notreached.h"
#include "components/media_router/common/providers/cast/channel/enum_table.h"

namespace cast_util {

using cast_channel::ChannelError;
using cast_channel::ReadyState;

template <>
const EnumTable<ReadyState>& EnumTable<ReadyState>::GetInstance() {
  static constexpr EnumTable<ReadyState>::Entry kEntries[] = {
      {ReadyState::NONE, "ReadyState::NONE"},
      {ReadyState::CONNECTING, "ReadyState::CONNECTING"},
      {ReadyState::OPEN, "ReadyState::OPEN"},
      {ReadyState::CLOSED, "ReadyState::CLOSED"},
  };
  static const EnumTable<ReadyState> kInstance(kEntries, ReadyState::kMaxValue);
  return kInstance;
}

template <>
const EnumTable<ChannelError>& EnumTable<ChannelError>::GetInstance() {
  static constexpr EnumTable<ChannelError>::Entry kEntries[] = {
      {ChannelError::NONE, "ChannelError::NONE"},
      {ChannelError::CHANNEL_NOT_OPEN, "ChannelError::CHANNEL_NOT_OPEN"},
      {ChannelError::AUTHENTICATION_ERROR,
       "ChannelError::AUTHENTICATION_ERROR"},
      {ChannelError::CONNECT_ERROR, "ChannelError::CONNECT_ERROR"},
      {ChannelError::CAST_SOCKET_ERROR, "ChannelError::CAST_SOCKET_ERROR"},
      {ChannelError::TRANSPORT_ERROR, "ChannelError::TRANSPORT_ERROR"},
      {ChannelError::INVALID_MESSAGE, "ChannelError::INVALID_MESSAGE"},
      {ChannelError::INVALID_CHANNEL_ID, "ChannelError::INVALID_CHANNEL_ID"},
      {ChannelError::CONNECT_TIMEOUT, "ChannelError::CONNECT_TIMEOUT"},
      {ChannelError::PING_TIMEOUT, "ChannelError::PING_TIMEOUT"},
      {ChannelError::UNKNOWN, "ChannelError::UNKNOWN"},
  };
  static const EnumTable<ChannelError> kInstance(kEntries,
                                                 ChannelError::kMaxValue);
  return kInstance;
}

}  // namespace cast_util

namespace cast_channel {

std::string_view ReadyStateToString(ReadyState ready_state) {
  if (auto name = cast_util::EnumToString(ready_state)) {
    return *name;
  }
  NOTREACHED() << "Unknown ready_state " << AsInteger(ready_state);
}

std::string_view ChannelErrorToString(ChannelError channel_error) {
  if (auto name = cast_util::EnumToString(channel_error)) {
    return *name;
  }
  NOTREACHED() << "Unknown channel_error " << AsInteger(channel_error);
}

}  // namespace cast_channel
