// Copyright 2020 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/service/display/record_replay_render.h"

#include "base/base64.h"
#include "base/record_replay.h"
#include "base/strings/stringprintf.h"
#include "cc/animation/animation_events.h"
#include "cc/trees/compositor_commit_data.h"
#include "components/viz/common/display/renderer_settings.h"
#include "components/viz/service/display/software_output_device.h"
#include "components/viz/service/display/software_renderer.h"
#include "components/viz/service/display_embedder/software_output_surface.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace blink {
extern bool RecordReplayStateEnsureInitialized();
}

namespace recordreplay {

struct SharedBitmapInfo {
  viz::SharedBitmapId id_;
  uint8_t* memory_;
  size_t size_;
};
typedef std::vector<SharedBitmapInfo> SharedBitmapInfoVector;
static SharedBitmapInfoVector* gSharedBitmaps;

void NotifyRasterBuffer(const viz::SharedBitmapId& shared_bitmap_id,
                        void* memory, size_t size) {
  if (!gSharedBitmaps) {
    gSharedBitmaps = new SharedBitmapInfoVector();
  }
  gSharedBitmaps->push_back({ shared_bitmap_id, (uint8_t*)memory, size });
}

static viz::SoftwareOutputSurface* gOutputSurface;
static viz::SoftwareRenderer* gRenderer;

static void InitializeRenderer() {
  std::unique_ptr<viz::SoftwareOutputDevice> output_device =
      std::make_unique<viz::SoftwareOutputDevice>();
  gOutputSurface = new viz::SoftwareOutputSurface(std::move(output_device));

  gRenderer = new viz::SoftwareRenderer(new viz::RendererSettings(),
                                        new viz::DebugRendererSettings(),
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

void SubmitCompositorFrame(const viz::LocalSurfaceId& local_surface_id,
                           const viz::CompositorFrame& frame) {
  CHECK(IsRecordingOrReplaying());

  if (!gSurfaceId) {
    gSurfaceId = new viz::LocalSurfaceId(local_surface_id);
    InitializeRenderer();
  }

  // Surfaces are allowed to have different parent sequence numbers from the
  // original one.
  if (gSurfaceId->child_sequence_number() != local_surface_id.child_sequence_number() ||
      gSurfaceId->embed_token() != local_surface_id.embed_token()) {
    Print("Ignoring composite to unknown surface %s, expected %s",
          SurfaceIdString(local_surface_id).c_str(),
          SurfaceIdString(*gSurfaceId).c_str());
    return;
  }

  viz::AggregatedRenderPassList render_passes;
  for (const auto& pass : frame.render_pass_list) {
    render_passes.push_back(pass->DeepCopyAggregated());
  }

  gCurrentFrame = &frame;

  gRenderer->DrawFrame(&render_passes,
                       1,
                       frame.size_in_pixels(),
                       gfx::DisplayColorSpaces(),
                       viz::SurfaceDamageRectList());

  gCurrentFrame = nullptr;
}

bool PopulateSkBitmapWithResource(SkBitmap* sk_bitmap, viz::ResourceId resource_id) {
  CHECK(gCurrentFrame);

  const viz::TransferableResource* transferable = nullptr;
  for (const viz::TransferableResource& resource : gCurrentFrame->resource_list) {
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

static char* EncodeBitmapContents(const char* mime_type, int jpeg_quality) {
  CHECK(gCurrentPixmap);

  if (strcmp(mime_type, "image/jpeg")) {
    // NYI
    return nullptr;
  }

  std::vector<unsigned char> data;
  if (!gfx::JPEGCodec::Encode(*gCurrentPixmap, jpeg_quality,
                              SkJpegEncoder::Downsample::k444, &data)) {
    Print("Error: JPEG encoding failed");
    return nullptr;
  }

  std::string encoded = base::Base64Encode(data);
  return strdup(encoded.c_str());
}

static char* PaintWhenDiverged(const char* mime_type, int jpeg_quality);

static char* PaintCallback(const char* mime_type, int jpeg_quality) {
  // The paint callback can be invoked either when we call RecordReplayPaintFinished
  // on the compositor thread, or at any time after diverging from the recording and
  // we are on the main thread. These invocations need different handling.
  if (HasDivergedFromRecording()) {
    return PaintWhenDiverged(mime_type, jpeg_quality);
  }

  return EncodeBitmapContents(mime_type, jpeg_quality);
}

// Paints generally occur in the following way:
//
// 1. Main thread commits a paint in ProxyMain::BeginMainFrame, posts a task
//    to run ProxyImpl::NotifyReadyToCommitOnImpl on the compositor thread,
//    and blocks until the completion event associated with that task is signaled.
//
// 2. Compositor thread runs ProxyImpl::NotifyReadyToCommitOnImpl and at some
//    point signals the completion event so the main thread can resume executing.
//
// 3. Compositor thread finishes painting the frame.
//
// The paint finished in #3 needs to be associated with the bookmark created in #1.
// The state below is used to keep track of the bookmark for the current paint
// in a consistent way between recording and replaying. This isn't necessarily
// correct, though --- #2 and #3 run in separate tasks on the compositor thread,
// so a frame could be marked ready to commit on the compositor thread before
// the previous frame finished painting. For now this is good enough, though.

// Bookmark which was last created on the main thread before blocking until the
// compositor thread is ready to commit.
static std::atomic<size_t> gCurrentPaintBookmark;

// Bookmark for the last point where a paint was committed on the main thread.
static size_t gLastCommitBookmark;

void OnCommitPaint() {
  // Record/replay state has to be initialized before the first paint
  // starts, as a checkpoint must have been taken.
  if (blink::RecordReplayStateEnsureInitialized()) {
    gCurrentPaintBookmark = V8RecordReplayPaintStart();
  } else {
    gCurrentPaintBookmark = 0;
  }
}

void OnReadyToCommit() {
  gLastCommitBookmark = gCurrentPaintBookmark;
}

// Whether the compositor thread is currently repainting.
static bool gCompositorRepainting = false;

void OnCompositorRepainting() {
  gCompositorRepainting = true;
}

// How to encode repainted graphics.
static const char* gRepaintMimeType;
static int gRepaintJPEGQuality;

// Event to signal when repainting has finished.
static base::WaitableEvent* gRepaintEvent;

// Encoded result of repainting.
static char* gRepaintResult;

void OnPaintFinished(const SkPixmap& pixmap) {
  static bool hasPaints = false;
  if (!hasPaints) {
    hasPaints = true;
    V8RecordReplaySetPaintCallback(PaintCallback);
  }

  gCurrentPixmap = &pixmap;

  if (gCompositorRepainting) {
    CHECK(HasDivergedFromRecording());

    char* encoded = EncodeBitmapContents(gRepaintMimeType, gRepaintJPEGQuality);
    CHECK(!gRepaintResult);
    gRepaintResult = encoded;

    gRepaintEvent->Signal();
  } else {
    size_t bookmark = gLastCommitBookmark;
    if (bookmark) {
      V8RecordReplayPaintFinished(bookmark);
    }
  }

  gCurrentPixmap = nullptr;
}

static cc::ProxyMain* gCurrentCompositorProxy;

void SetCompositorProxy(cc::ProxyMain* proxy) {
  gCurrentCompositorProxy = proxy;
}

static char* PaintWhenDiverged(const char* mime_type, int jpeg_quality) {
  gRepaintMimeType = mime_type;
  gRepaintJPEGQuality = jpeg_quality;

  base::WaitableEvent event;
  gRepaintEvent = &event;

  // Update layout state on the main thread and commit a new paint.
  std::unique_ptr<cc::BeginMainFrameAndCommitState> begin_main_frame_state(
      new cc::BeginMainFrameAndCommitState);
  begin_main_frame_state->mutator_events = std::make_unique<cc::AnimationEvents>();
  begin_main_frame_state->commit_data = std::make_unique<cc::CompositorCommitData>();
  gCurrentCompositorProxy->BeginMainFrame(std::move(begin_main_frame_state));

  // Trigger a new frame on the compositor thread.
  gCurrentCompositorProxy->RecordReplayRepaint();

  // Wait for the repainting frame to complete.
  bool signaled = event.TimedWait(base::TimeDelta::FromMilliseconds(200));
  CHECK(signaled);

  gRepaintEvent = nullptr;
  return gRepaintResult;
}

} // namespace recordreplay
