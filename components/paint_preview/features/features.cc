// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/features/features.h"

#include "base/feature_list.h"

namespace paint_preview {

BASE_FEATURE(kPaintPreviewDemo,
             "PaintPreviewDemo",
             base::FEATURE_DISABLED_BY_DEFAULT);

BASE_FEATURE(kPaintPreviewShowOnStartup,
             "PaintPreviewShowOnStartup",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace paint_preview
