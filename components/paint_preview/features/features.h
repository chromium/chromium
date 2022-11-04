// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_FEATURES_FEATURES_H_
#define COMPONENTS_PAINT_PREVIEW_FEATURES_FEATURES_H_

#include "base/feature_list.h"

namespace paint_preview {

// IMPORTANT: Please keep this file in alphabetical order.

// Used to enable a main menu item on Android that captures and displays a paint
// preview for the current page. The paint preview UI will be dismissed on back
// press and all associated stored files deleted. This intended to test whether
// capturing and playing paint preview works on a specific site.
BASE_DECLARE_FEATURE(kPaintPreviewDemo);

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_FEATURES_FEATURES_H_
