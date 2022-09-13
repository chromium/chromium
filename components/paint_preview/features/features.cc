// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/features/features.h"

#include "base/feature_list.h"

namespace paint_preview {

const base::Feature kPaintPreviewDemo{"PaintPreviewDemo",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kPaintPreviewShowOnStartup{
    "PaintPreviewShowOnStartup", base::FEATURE_ENABLED_BY_DEFAULT};

}  // namespace paint_preview
