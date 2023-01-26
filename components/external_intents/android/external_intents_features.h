// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXTERNAL_INTENTS_ANDROID_EXTERNAL_INTENTS_FEATURES_H_
#define COMPONENTS_EXTERNAL_INTENTS_ANDROID_EXTERNAL_INTENTS_FEATURES_H_

#include "base/feature_list.h"

namespace external_intents {

// Alphabetical:
BASE_DECLARE_FEATURE(kBlockExternalFormSubmitWithoutGesture);
BASE_DECLARE_FEATURE(kExternalNavigationDebugLogs);
BASE_DECLARE_FEATURE(kExternalNavigationSubframeRedirects);
BASE_DECLARE_FEATURE(kBlockSubframeIntentToSelf);
}  // namespace external_intents

#endif  // COMPONENTS_EXTERNAL_INTENTS_ANDROID_EXTERNAL_INTENTS_FEATURES_H_
