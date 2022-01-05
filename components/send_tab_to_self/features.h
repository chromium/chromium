// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_
#define COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_

#include "base/feature_list.h"
#include "build/build_config.h"

class PrefService;

namespace send_tab_to_self {

// If this feature is enabled, we will use signed-in, ephemeral data rather than
// persistent sync data. Users who are signed in can use the feature regardless
// of whether they have the sync feature enabled.
extern const base::Feature kSendTabToSelfWhenSignedIn;

// If this feature is enabled, a link to manage the user's devices will be shown
// below the device list when sharing.
extern const base::Feature kSendTabToSelfManageDevicesLink;

#if defined(OS_ANDROID) || defined(OS_IOS)
// If this feature is enabled, show received tabs in a new UI next to the
// profile icon rather than in a system notification.
//
// V2 is the default on desktop and the V1 code path has been deleted there, so
// this base::Feature no longer exists on desktop platforms.
extern const base::Feature kSendTabToSelfV2;
#endif  // OS_ANDROID || OS_IOS

// Returns whether the receiving components of the feature is enabled on this
// device. This doesn't rely on the SendTabToSelfSyncService to be actively up
// and ready.
bool IsReceivingEnabledByUserOnThisDevice(PrefService* prefs);

}  // namespace send_tab_to_self

#endif  // COMPONENTS_SEND_TAB_TO_SELF_FEATURES_H_
