// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_IOS)
namespace {

// The default time offset used to pre-populate the date/time picker when the
// 'Set a Reminder' UI half-sheet is first shown.
const base::TimeDelta kReminderNotificationsDefaultOffset = base::Hours(24);

}  // namespace
#endif  // BUILDFLAG(IS_IOS)

namespace send_tab_to_self {

BASE_FEATURE(kSendTabToSelfEnableNotificationTimeOut,
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kSendTabToSelfIOSPushNotifications,
#if BUILDFLAG(IS_IOS)
             base::FEATURE_DISABLED_BY_DEFAULT);
#else
             base::FEATURE_ENABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_IOS)

const char kSendTabIOSPushNotificationsURLImageParam[] =
    "variant_with_URL_image";

bool IsSendTabIOSPushNotificationsEnabledWithURLImage() {
  if (base::FeatureList::IsEnabled(kSendTabToSelfIOSPushNotifications)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kSendTabToSelfIOSPushNotifications,
        kSendTabIOSPushNotificationsURLImageParam, false);
  }
  return false;
}

#if BUILDFLAG(IS_IOS)
const char kSendTabIOSPushNotificationsWithMagicStackCardParam[] =
    "variant_with_magic_stack_card";

bool IsSendTabIOSPushNotificationsEnabledWithMagicStackCard() {
  if (base::FeatureList::IsEnabled(kSendTabToSelfIOSPushNotifications)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kSendTabToSelfIOSPushNotifications,
        kSendTabIOSPushNotificationsWithMagicStackCardParam, false);
  }
  return false;
}

const char kSendTabIOSPushNotificationsWithTabRemindersParam[] =
    "variant_with_tab_reminders";

bool IsSendTabIOSPushNotificationsEnabledWithTabReminders() {
  if (base::FeatureList::IsEnabled(kSendTabToSelfIOSPushNotifications)) {
    return base::GetFieldTrialParamByFeatureAsBool(
        kSendTabToSelfIOSPushNotifications,
        kSendTabIOSPushNotificationsWithTabRemindersParam, false);
  }
  return false;
}

const char kReminderNotificationsDefaultTimeOffset[] =
    "ReminderNotificationsDefaultTimeOffset";

const base::TimeDelta GetReminderNotificationsDefaultTimeOffset() {
  // Default to 24 hours.
  return base::GetFieldTrialParamByFeatureAsTimeDelta(
      kSendTabToSelfIOSPushNotifications,
      kReminderNotificationsDefaultTimeOffset,
      kReminderNotificationsDefaultOffset);
}
#endif  // BUILDFLAG(IS_IOS)

}  // namespace send_tab_to_self
