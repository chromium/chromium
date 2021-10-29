// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_DISPLAY_SYNC_QUERY_COLLECTION_H_
#define COMPONENTS_VIZ_SERVICE_DISPLAY_SYNC_QUERY_COLLECTION_H_

#include <memory>

#include "base/containers/circular_deque.h"
#include "base/memory/ref_counted.h"

namespace gpu {
namespace gles2 {
class GLES2Interface;
}
}  // namespace gpu

namespace viz {
class SyncQuery;
class ResourceFence;

class SyncQueryCollection {
 public:
  explicit SyncQueryCollection(gpu::gles2::GLES2Interface* gl);
  SyncQueryCollection(SyncQueryCollection&&);
  SyncQueryCollection& operator=(SyncQueryCollection&&);
  ~SyncQueryCollection();
  scoped_refptr<ResourceFence> StartNewFrame();
  void EndCurrentFrame();

 private:
  base::circular_deque<std::unique_ptr<SyncQuery>> pending_sync_queries_;
  base::circular_deque<std::unique_ptr<SyncQuery>> available_sync_queries_;
  std::unique_ptr<SyncQuery> current_sync_query_;
  gpu::gles2::GLES2Interface* gl_;
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_DISPLAY_SYNC_QUERY_COLLECTION_H_
