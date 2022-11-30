// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_VERSION_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_VERSION_H_

#include <stdint.h>

namespace paint_preview {

// Version of the paint preview. Should be incremented on breaking changes to
// the storage format or the SkPicture deserialization process.
extern const uint32_t kPaintPreviewVersion;

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_VERSION_H_
