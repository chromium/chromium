// Copyright 2020 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/record_replay_render.h"

#include "base/base64.h"
#include "base/record_replay.h"
#include "base/strings/stringprintf.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/display/software_renderer.h"
#include "components/viz/service/display_embedder/software_output_surface.h"
#include "ui/gfx/codec/jpeg_codec.h"

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
  if (!gSharedBitmaps) {
    gSharedBitmaps = new SharedBitmapInfoVector();
  }
  gSharedBitmaps->push_back({ shared_bitmap_id, (uint8_t*)memory, size });
}

static SoftwareOutputSurface* gOutputSurface;
static SoftwareRenderer* gRenderer;

static void InitializeRenderer() {
  std::unique_ptr<SoftwareOutputDevice> output_device = std::make_unique<SoftwareOutputDevice>();
  gOutputSurface = new SoftwareOutputSurface(std::move(output_device));

  gRenderer = new SoftwareRenderer(new RendererSettings(),
                                   new DebugRendererSettings(),
                                   gOutputSurface,
                                   nullptr,
                                   nullptr);
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

// Current compositor frame.
static const viz::CompositorFrame* gCurrentFrame;

void RecordReplaySubmitCompositorFrame(const viz::LocalSurfaceId& local_surface_id,
                                       const viz::CompositorFrame& frame) {
  CHECK(recordreplay::IsRecordingOrReplaying());

  if (!gSurfaceId) {
    gSurfaceId = new viz::LocalSurfaceId(local_surface_id);
    InitializeRenderer();
  }

  // Surfaces are allowed to have different parent sequence numbers from the
  // original one.
  if (gSurfaceId->child_sequence_number() != local_surface_id.child_sequence_number() ||
      gSurfaceId->embed_token() != local_surface_id.embed_token()) {
    recordreplay::Print("Ignoring composite to unknown surface %s, expected %s",
                        SurfaceIdString(local_surface_id).c_str(),
                        SurfaceIdString(*gSurfaceId).c_str());
    return;
  }

  AggregatedRenderPassList render_passes;
  for (const auto& pass : frame.render_pass_list) {
    render_passes.push_back(pass->DeepCopyAggregated());
  }

  gCurrentFrame = &frame;

  gRenderer->DrawFrame(&render_passes,
                       1,
                       frame.size_in_pixels(),
                       gfx::DisplayColorSpaces(),
                       SurfaceDamageRectList());

  gCurrentFrame = nullptr;
}

bool RecordReplayPopulateSkBitmapWithResource(SkBitmap* sk_bitmap, ResourceId resource_id) {
  CHECK(gCurrentFrame);

  const TransferableResource* transferable = nullptr;
  for (const TransferableResource& resource : gCurrentFrame->resource_list) {
    if (resource.id == resource_id) {
      transferable = &resource;
      break;
    }
  }
  if (!transferable) {
    return false;
  }

  void* pixels = nullptr;
  if (gSharedBitmaps) {
    for (const SharedBitmapInfo& info : *gSharedBitmaps) {
      if (info.id_ == transferable->mailbox_holder.mailbox) {
        pixels = info.memory_;
      }
    }
  }

  if (!pixels) {
    return false;
  }

  SkImageInfo info =
      SkImageInfo::MakeN32Premul(transferable->size.width(),
                                 transferable->size.height());
  return sk_bitmap->installPixels(info, pixels, info.minRowBytes());
}

extern "C" size_t V8RecordReplayPaintStart();
extern "C" void V8RecordReplayPaintFinished(size_t bookmark);
extern "C" void V8RecordReplaySetPaintCallback(char* (*callback)(const char*, int));

const SkPixmap* gCurrentPixmap;

static char* PaintCallback(const char* mime_type, int jpeg_quality) {
  if (recordreplay::HasDivergedFromRecording()) {
    // NYI
    return nullptr;
  }

  CHECK(gCurrentPixmap);

  if (strcmp(mime_type, "image/jpeg")) {
    // NYI
    return nullptr;
  }

  std::vector<unsigned char> data;
  if (!gfx::JPEGCodec::Encode(*gCurrentPixmap, jpeg_quality,
                              SkJpegEncoder::Downsample::k444, &data)) {
    recordreplay::Print("Error: JPEG encoding failed");
    return nullptr;
  }

  std::string encoded = base::Base64Encode(data);
  return strdup(encoded.c_str());
}

// Bookmark for the last point where a paint was committed on the main thread.
static size_t gLastPaintBookmark;

void RecordReplayOnCommitPaint() {
  gLastPaintBookmark = V8RecordReplayPaintStart();
}

void RecordReplayPaintFinished(const SkPixmap& pixmap) {
  static bool hasPaints = false;
  if (!hasPaints) {
    hasPaints = true;
    V8RecordReplaySetPaintCallback(PaintCallback);
  }

  CHECK(gLastPaintBookmark);
  gCurrentPixmap = &pixmap;
  V8RecordReplayPaintFinished(gLastPaintBookmark);
  gCurrentPixmap = nullptr;
}

} // namespace viz
