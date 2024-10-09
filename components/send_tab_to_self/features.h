// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_
#define COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"

namespace send_tab_to_self {

// If this feature is enabled, the notification shown to users will disapear
// after a fixed timeout. When disabled, instead it will remain until the
// user interacts with it either by dismissing or openning it.
BASE_DECLARE_FEATURE(kSendTabToSelfEnableNotificationTimeOut);

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
// If this feature is enabled, show received tabs in a new UI next to the
// profile icon rather than in a system notification.
//
// V2 is the default on desktop and the V1 code path has been deleted there, so
// this base::Feature no longer exists on desktop platforms.
BASE_DECLARE_FEATURE(kSendTabToSelfV2);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

// If this feature is enabled, allow the user to receive Send Tab to Self
// notifications as a push notification to their target iOS device.
BASE_DECLARE_FEATURE(kSendTabToSelfIOSPushNotifications);

#if BUILDFLAG(IS_IOS)
// Param for the iOS Magic Stack Card variant for the
// kSendTabToSelfIOSPushNotifications feature.
extern const char kSendTabIOSPushNotificationsWithMagicStackCardParam[];

// Convenience method for determining when SendTabIOSPushNotifications is
// enabled with Magic Stack Card.
bool IsSendTabIOSPushNotificationsEnabledWithMagicStackCard();
#endif  // BUILDFLAG(IS_IOS)

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_
