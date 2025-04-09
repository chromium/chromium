// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/fullscreen_control/fullscreen_features.h"

namespace features {

BASE_FEATURE(kFullscreenBubbleShowOpaque,
             "FullscreenBubbleShowOpaque",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kFullscreenBubbleShowOrigin,
             "FullscreenBubbleShowOrigin",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace features
