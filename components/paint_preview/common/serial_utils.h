// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAINT_PREVIEW_COMMON_SERIAL_UTILS_H_
#define COMPONENTS_PAINT_PREVIEW_COMMON_SERIAL_UTILS_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/files/file.h"
#include "components/paint_preview/common/glyph_usage.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkSerialProcs.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "ui/gfx/geometry/rect.h"

namespace paint_preview {

// Maps a content ID to a frame ID (Process ID || Routing ID).
using PictureSerializationContext = base::flat_map<uint32_t, uint64_t>;

// Maps a typeface ID to a glyph usage tracker.
using TypefaceUsageMap = base::flat_map<SkFontID, std::unique_ptr<GlyphUsage>>;

// Tracks typeface deduplication and handles subsetting.
struct TypefaceSerializationContext {
  TypefaceSerializationContext(TypefaceUsageMap* usage);
  ~TypefaceSerializationContext();

  TypefaceUsageMap* usage;
  base::flat_set<SkFontID> finished;  // Should be empty on first use.
};

// Maps a content ID to a clip rect.
using DeserializationContext = base::flat_map<uint32_t, gfx::Rect>;

// Creates a no-op SkPicture.
sk_sp<SkPicture> MakeEmptyPicture();

// Creates a SkSerialProcs object. The object *does not* copy |picture_ctx| or
// |typeface_ctx| so they must outlive the use of the returned object.
SkSerialProcs MakeSerialProcs(PictureSerializationContext* picture_ctx,
                              TypefaceSerializationContext* typeface_ctx);

// Creates a SkDeserialProcs object. The object *does not* copy |ctx| so |ctx|
// must outlive the use of the returned object.
SkDeserialProcs MakeDeserialProcs(DeserializationContext* ctx);

}  // namespace paint_preview

#endif  // COMPONENTS_PAINT_PREVIEW_COMMON_SERIAL_UTILS_H_
