// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/ios_push/ios_push_notification_util.h"

#include <string>

#include "base/notreached.h"

// Type Ids for the send tab push notification feature.
constexpr char kSendTabStableTypeId[] = "send_tab_notify_ios";
constexpr char kSendTabUnstableTypeId[] = "send_tab_notify_ios_unstable";

namespace {
std::string GetSendTabTypeIdForChannel(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      return kSendTabUnstableTypeId;
    case version_info::Channel::STABLE:
      return kSendTabStableTypeId;
  }
}
}  // namespace

namespace sharing_message {
std::string GetIosPushMessageTypeIdForChannel(MessageType message_type,
                                              version_info::Channel channel) {
  switch (message_type) {
    case sharing_message::SEND_TAB_TO_SELF_PUSH_NOTIFICATION:
      return GetSendTabTypeIdForChannel(channel);
    default:
      // Only SEND_TAB_TO_SELF_PUSH_NOTIFICATION is supported by iOS Push.
      NOTREACHED();
  }
}
}  // namespace sharing_message
