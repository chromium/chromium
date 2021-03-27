// Copyright 2020 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/record_replay_render.h"

#include "base/record_replay.h"
#include "base/strings/stringprintf.h"

namespace viz {

struct SharedBitmapInfo {
  SharedBitmapId id_;
  uint8_t* memory_;
  size_t size_;
};
typedef std::vector<SharedBitmapInfo> SharedBitmapInfoVector;
static SharedBitmapInfoVector* gSharedBitmaps;

void RecordReplayNotifyRasterBuffer(const SharedBitmapId& shared_bitmap_id,
                                    void* memory, size_t size) {
  recordreplay::Print("RecordReplayNotifyRasterBuffer %s %p %lu",
                      shared_bitmap_id.ToDebugString().c_str(), memory, size);
  if (!gSharedBitmaps) {
    gSharedBitmaps = new SharedBitmapInfoVector();
  }
  gSharedBitmaps->push_back({ shared_bitmap_id, (uint8_t*)memory, size });
}

static std::string SurfaceIdString(const viz::LocalSurfaceId& local_surface_id) {
  // LocalSurfaceId has ToString(), but without verbose logging the token is truncated.
  return base::StringPrintf("%u:%u:%s",
                            local_surface_id.parent_sequence_number(),
                            local_surface_id.child_sequence_number(),
                            local_surface_id.embed_token().ToString().c_str());
}

// For now we only support drawing a single surface, which is the first one
// which the process tried to draw.
static viz::LocalSurfaceId* gSurfaceId;

void RecordReplaySubmitCompositorFrame(const viz::LocalSurfaceId& local_surface_id,
                                       const viz::CompositorFrame& frame) {
  CHECK(recordreplay::IsRecordingOrReplaying());

  if (!gSurfaceId) {
    gSurfaceId = new viz::LocalSurfaceId(local_surface_id);
  }

  if (*gSurfaceId != local_surface_id) {
    recordreplay::Print("Ignoring composite to unknown surface %s, expected %s",
                        SurfaceIdString(local_surface_id).c_str(),
                        SurfaceIdString(*gSurfaceId).c_str());
  }

  recordreplay::Print("RecordReplaySubmitCompositorFrame %d %d NumResources %lu NumRenderPasses %lu",
                      frame.size_in_pixels().width(), frame.size_in_pixels().height(),
                      frame.resource_list.size(),
                      frame.render_pass_list.size());

  for (const TransferableResource& resource : frame.resource_list) {
    recordreplay::Print("CompositorFrameResource Software %d TextureTarget %u Mailbox %s",
                        resource.is_software,
                        resource.mailbox_holder.texture_target,
                        resource.mailbox_holder.mailbox.ToDebugString().c_str());
    if (gSharedBitmaps) {
      for (const SharedBitmapInfo& info : *gSharedBitmaps) {
        if (info.id_ == resource.mailbox_holder.mailbox) {
          size_t numNonZeroBytes = 0;
          for (size_t i = 0; i < info.size_; i++) {
            if (info.memory_[i]) {
              numNonZeroBytes++;
            }
          }
          recordreplay::Print("FoundBitmapInfo NumBytes %lu", numNonZeroBytes);
        }
      }
    }
  }
}

} // namespace viz
