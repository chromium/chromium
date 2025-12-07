// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_
#define COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"
#include "build/buildflag.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace send_tab_to_self {

// If this feature is enabled, the notification shown to users will disapear
// after a fixed timeout. When disabled, instead it will remain until the
// user interacts with it either by dismissing or openning it.
BASE_DECLARE_FEATURE(kSendTabToSelfEnableNotificationTimeOut);

// If this feature is enabled, allow the user to receive Send Tab to Self
// notifications as a push notification to their target iOS device.
BASE_DECLARE_FEATURE(kSendTabToSelfIOSPushNotifications);

// Param for whether or not to include the URL image in the push notification
// for the kSendTabToSelfIOSPushNotifications feature.
extern const char kSendTabIOSPushNotificationsURLImageParam[];

// Convenience method for determining when SendTabIOSPushNotifications is
// enabled with a URL image in the notification.
bool IsSendTabIOSPushNotificationsEnabledWithURLImage();

#if BUILDFLAG(IS_IOS)
// Param for the iOS Magic Stack Card variant for the
// kSendTabToSelfIOSPushNotifications feature.
extern const char kSendTabIOSPushNotificationsWithMagicStackCardParam[];

// Convenience method for determining when SendTabIOSPushNotifications is
// enabled with Magic Stack Card.
bool IsSendTabIOSPushNotificationsEnabledWithMagicStackCard();

// Param for the iOS Tab Reminders variant for the
// `kSendTabToSelfIOSPushNotifications` feature.
extern const char kSendTabIOSPushNotificationsWithTabRemindersParam[];

// Convenience method for determining when `kSendTabToSelfIOSPushNotifications`
// is enabled with Tab Reminders.
bool IsSendTabIOSPushNotificationsEnabledWithTabReminders();

// Parameter representing the default time offset initially presented in the
// 'Set a Reminder' UI half-sheet. Users can select a different offset manually.
extern const char kReminderNotificationsDefaultTimeOffset[];

// Returns the default time offset used to pre-populate the date/time picker
// when the 'Set a Reminder' UI half-sheet is first shown. This value is
// controlled by the `kReminderNotificationsDefaultTimeOffset` Finch parameter.
const base::TimeDelta GetReminderNotificationsDefaultTimeOffset();
#endif  // BUILDFLAG(IS_IOS)

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_
