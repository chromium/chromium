// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/browser/test_paint_preview_policy.h"

#include "base/functional/callback.h"
#include "components/paint_preview/browser/paint_preview_policy.h"
#include "content/public/browser/web_contents.h"

namespace paint_preview {

TestPaintPreviewPolicy::TestPaintPreviewPolicy() = default;

TestPaintPreviewPolicy::~TestPaintPreviewPolicy() = default;

bool TestPaintPreviewPolicy::SupportedForContents(
    content::WebContents* web_contents) {
  return supported_for_contents_;
}

}  // namespace paint_preview
