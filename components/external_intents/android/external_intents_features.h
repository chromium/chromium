// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXTERNAL_INTENTS_ANDROID_EXTERNAL_INTENTS_FEATURES_H_
#define COMPONENTS_EXTERNAL_INTENTS_ANDROID_EXTERNAL_INTENTS_FEATURES_H_

#include "base/feature_list.h"

namespace external_intents {

BASE_DECLARE_FEATURE(kExternalNavigationDebugLogs);
BASE_DECLARE_FEATURE(kBlockFrameRenavigations);
BASE_DECLARE_FEATURE(kBlockIntentsToSelf);
BASE_DECLARE_FEATURE(kTrustedClientGestureBypass);

}  // namespace external_intents

#endif  // COMPONENTS_EXTERNAL_INTENTS_ANDROID_EXTERNAL_INTENTS_FEATURES_H_
