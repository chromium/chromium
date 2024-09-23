// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SHARING_MESSAGE_IOS_PUSH_IOS_PUSH_NOTIFICATION_UTIL_H_
#define COMPONENTS_SHARING_MESSAGE_IOS_PUSH_IOS_PUSH_NOTIFICATION_UTIL_H_

#include "base/version_info/channel.h"
#include "components/sharing_message/proto/sharing_message_type.pb.h"

namespace sharing_message {
// Returns the Chime type Id for the notification type.
std::string GetIosPushMessageTypeIdForChannel(MessageType message_type,
                                              version_info::Channel channel);
}  // namespace sharing_message

#endif  // COMPONENTS_SHARING_MESSAGE_IOS_PUSH_IOS_PUSH_NOTIFICATION_UTIL_H_
