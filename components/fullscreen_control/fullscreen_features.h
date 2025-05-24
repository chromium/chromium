// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FULLSCREEN_CONTROL_FULLSCREEN_FEATURES_H_
#define COMPONENTS_FULLSCREEN_CONTROL_FULLSCREEN_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// Enable to make the fullscreen bubble use an opaque background.
BASE_DECLARE_FEATURE(kFullscreenBubbleShowOpaque);

// Enable to show the origin domain when entering HTML fullscreen.
BASE_DECLARE_FEATURE(kFullscreenBubbleShowOrigin);

}  // namespace features

#endif  // COMPONENTS_FULLSCREEN_CONTROL_FULLSCREEN_FEATURES_H_
