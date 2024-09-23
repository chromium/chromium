// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_MESSAGES_FEATURE_H_
#define COMPONENTS_MESSAGES_ANDROID_MESSAGES_FEATURE_H_

#include "base/feature_list.h"

namespace messages {

// Feature that controls whether "ads blocked" messages use Messages or
// Infobars infrastructure.
BASE_DECLARE_FEATURE(kMessagesForAndroidAdsBlocked);

// Feature that controls whether "save card" prompts use Messages or
// Infobars infrastructure.
BASE_DECLARE_FEATURE(kMessagesForAndroidSaveCard);

// Feature that controls whether Messages for Android should use
// new Stacking Animation.
BASE_DECLARE_FEATURE(kMessagesForAndroidStackingAnimation);

// Feature that exposes a listener to notify whether the current message
// is fully visible.
BASE_DECLARE_FEATURE(kMessagesForAndroidFullyVisibleCallback);

// Feature that enables extra histogram recordings.
BASE_DECLARE_FEATURE(kMessagesAndroidExtraHistograms);

bool IsAdsBlockedMessagesUiEnabled();

bool IsPermissionUpdateMessagesUiEnabled();

bool IsSafetyTipMessagesUiEnabled();

bool IsSaveCardMessagesUiEnabled();

bool UseFollowupButtonTextForSaveCardMessage();

bool UseGPayIconForSaveCardMessage();

bool UseDialogV2ForSaveCardMessage();

}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_MESSAGES_FEATURE_H_
