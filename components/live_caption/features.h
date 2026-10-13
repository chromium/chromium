// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_LIVE_CAPTION_FEATURES_H_
#define COMPONENTS_LIVE_CAPTION_FEATURES_H_

#include "base/feature_list.h"

namespace live_caption {

// Controls whether on-device translation is used for the Live Caption feature.
BASE_DECLARE_FEATURE(kLiveCaptionOnDeviceTranslation);

// Controls whether on-device translation is restricted to/from English and
// not enabled for zh-TW.
BASE_DECLARE_FEATURE(kLiveCaptionOnDeviceTranslationEnglishOnly);

}  // namespace live_caption

#endif  // COMPONENTS_LIVE_CAPTION_FEATURES_H_
