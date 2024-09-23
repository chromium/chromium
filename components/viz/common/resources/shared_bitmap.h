// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_BITMAP_H_
#define COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_BITMAP_H_

#include <stddef.h>
#include <stdint.h>

#include "base/hash/hash.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "components/viz/common/resources/shared_bitmap_id.h"
#include "components/viz/common/viz_common_export.h"
#include "ui/gfx/geometry/size.h"

namespace viz {
struct SharedBitmapIdHash {
  size_t operator()(const SharedBitmapId& id) const {
    return base::FastHash(base::as_bytes(base::make_span(id.name)));
  }
};

// An object returned by the SharedBitmapGenerator that exposes the
// pixels for a SharedBitmapId. They are exposed via a class so that
// this object (or its subclass) can ensure the lifetime of the pixels
// is not cut short. While this object is kept alive, the pixels must
// remain valid.
class VIZ_COMMON_EXPORT SharedBitmap {
 public:
  static SharedBitmapId GenerateId();

  explicit SharedBitmap(uint8_t* pixels);

  SharedBitmap(const SharedBitmap&) = delete;
  SharedBitmap& operator=(const SharedBitmap&) = delete;

  virtual ~SharedBitmap();

  uint8_t* pixels() { return pixels_; }

 protected:
  raw_ptr<uint8_t> pixels_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_RESOURCES_SHARED_BITMAP_H_
