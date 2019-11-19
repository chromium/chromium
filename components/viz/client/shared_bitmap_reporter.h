// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_CLIENT_SHARED_BITMAP_REPORTER_H_
#define COMPONENTS_VIZ_CLIENT_SHARED_BITMAP_REPORTER_H_

#include "base/memory/read_only_shared_memory_region.h"
#include "components/viz/client/viz_client_export.h"
#include "components/viz/common/resources/shared_bitmap.h"

namespace viz {

// An interface that can be used for code without a direct connection to the viz
// display compositor, as an intermediary in order to notify the display
// compositor about SharedMemory allocated for shared bitmaps. The
// implementation of this interface would be responsible to passing the
// notifications on to the display compositor via the CompositorFrameSink.
class VIZ_CLIENT_EXPORT SharedBitmapReporter {
 public:
  // Associates a SharedBitmapId with a shared buffer handle.
  virtual void DidAllocateSharedBitmap(base::ReadOnlySharedMemoryRegion region,
                                       const SharedBitmapId& id) = 0;

  // Disassociates a SharedBitmapId previously passed to
  // DidAllocateSharedBitmap.
  virtual void DidDeleteSharedBitmap(const SharedBitmapId& id) = 0;

 protected:
  virtual ~SharedBitmapReporter();
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_CLIENT_SHARED_BITMAP_REPORTER_H_
