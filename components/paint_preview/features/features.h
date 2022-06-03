// Copyright 2020 The Chromium Authors. All rights reserved.
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
extern const base::Feature kPaintPreviewDemo;

// Used to enable the paint preview capture and show on startup for Android. If
// enabled, paint previews for each tab are captured when a tab is hidden and
// are deleted when a tab is closed. When a tab with a captured paint perview
// is shown at startup and there is no cached page we will show the paint
// preview.
extern const base::Feature kPaintPreviewShowOnStartup;

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_FEATURES_FEATURES_H_
