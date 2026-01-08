// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sharing_message/ios_push/ios_push_notification_util.h"

#include <string>

#include "base/notreached.h"

// Type Ids for the send tab push notification feature.
constexpr char kSendTabStableTypeId[] = "send_tab_notify_ios";
constexpr char kSendTabUnstableTypeId[] = "send_tab_notify_ios_unstable";

// Type Ids for the desktop-to-mobile promo push notification feature.
constexpr char kDesktopMobilePromoStableTypeId[] =
    "cross_platform_growth_promo_ios";
constexpr char kDesktopMobilePromoUnstableTypeId[] =
    "cross_platform_growth_promo_ios_unstable";

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

std::string GetDesktopMobilePromoTypeIdForChannel(
    version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::UNKNOWN:
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::BETA:
      return kDesktopMobilePromoUnstableTypeId;
    case version_info::Channel::STABLE:
      return kDesktopMobilePromoStableTypeId;
  }
}
}  // namespace

namespace sharing_message {
std::string GetIosPushMessageTypeIdForChannel(MessageType message_type,
                                              version_info::Channel channel) {
  switch (message_type) {
    case sharing_message::SEND_TAB_TO_SELF_PUSH_NOTIFICATION:
      return GetSendTabTypeIdForChannel(channel);
    case sharing_message::DESKTOP_TO_MOBILE_PROMO_PUSH_NOTIFICATION:
      return GetDesktopMobilePromoTypeIdForChannel(channel);
    default:
      // Not all MessageType are supported by iOS Push.
      NOTREACHED();
  }
}
}  // namespace sharing_message
