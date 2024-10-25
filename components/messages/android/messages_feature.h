// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MESSAGES_ANDROID_MESSAGES_FEATURE_H_
#define COMPONENTS_MESSAGES_ANDROID_MESSAGES_FEATURE_H_

#include "base/feature_list.h"

namespace messages {

// Feature that exposes a listener to notify whether the current message
// is fully visible.
BASE_DECLARE_FEATURE(kMessagesForAndroidFullyVisibleCallback);

// Feature that enables extra histogram recordings.
BASE_DECLARE_FEATURE(kMessagesAndroidExtraHistograms);

}  // namespace messages

#endif  // COMPONENTS_MESSAGES_ANDROID_MESSAGES_FEATURE_H_
