// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/features.h"

namespace live_caption {

// Enables the on-device translation model by default.
BASE_FEATURE(kLiveCaptionOnDeviceTranslation, base::FEATURE_ENABLED_BY_DEFAULT);

// Restricts on-device translation by default to only enable translations for
// language pairs that contain English.
BASE_FEATURE(kLiveCaptionOnDeviceTranslationEnglishOnly,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace live_caption
