// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/live_caption/features.h"

namespace live_caption {

// By default, keep the existing cloud/default translation behavior until the
// on-device model is ready to be rolled out.
BASE_FEATURE(kLiveCaptionOnDeviceTranslation,
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace live_caption
