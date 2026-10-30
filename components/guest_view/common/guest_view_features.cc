// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/guest_view/common/guest_view_features.h"

namespace guest_view {

BASE_FEATURE(kGuestViewAutoSizeInLogicalPixels,
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kGuestViewMatchEmbedderZoomAfterNavigation,
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace guest_view
