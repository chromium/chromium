// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/send_tab_to_self/features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace send_tab_to_self {

BASE_FEATURE(kSendTabToSelfEnableNotificationTimeOut,
             "SendTabToSelfEnableNotificationTimeOut",
             base::FEATURE_DISABLED_BY_DEFAULT);


#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
BASE_FEATURE(kSendTabToSelfV2,
             "SendTabToSelfV2",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

BASE_FEATURE(kSendTabToSelfIOSPushNotifications,
             "SendTabToSelfIOSPushNotifications",
             base::FEATURE_DISABLED_BY_DEFAULT);

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
#endif  // BUILDFLAG(IS_IOS)

}  // namespace send_tab_to_self
