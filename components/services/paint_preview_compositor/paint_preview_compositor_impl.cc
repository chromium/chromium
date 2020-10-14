// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/paint_preview_compositor/paint_preview_compositor_impl.h"

#include <memory>
#include <utility>

#include "base/optional.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/recording_map.h"
#include "components/paint_preview/common/serial_utils.h"
#include "components/paint_preview/common/serialized_recording.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkStream.h"

namespace paint_preview {

namespace {

// Returns |nullopt| if |proto_memory| cannot be mapped or parsed.
base::Optional<PaintPreviewProto> ParsePaintPreviewProto(
    const base::ReadOnlySharedMemoryRegion& proto_memory) {
  auto mapping = proto_memory.Map();
  if (!mapping.IsValid()) {
    DVLOG(1) << "Failed to map proto in shared memory.";
    return base::nullopt;
  }

  PaintPreviewProto paint_preview;
  bool ok = paint_preview.ParseFromArray(mapping.memory(), mapping.size());
  if (!ok) {
    DVLOG(1) << "Failed to parse proto.";
    return base::nullopt;
  }

  return {paint_preview};
}

base::Optional<PaintPreviewFrame> BuildFrame(
    const base::UnguessableToken& token,
    const PaintPreviewFrameProto& frame_proto,
    const base::flat_map<base::UnguessableToken, SkpResult>& results) {
  TRACE_EVENT0("paint_preview", "PaintPreviewCompositorImpl::BuildFrame");
  auto it = results.find(token);
  if (it == results.end())
    return base::nullopt;

  const SkpResult& result = it->second;
  PaintPreviewFrame frame;
  frame.skp = result.skp;

  for (const auto& id_pair : frame_proto.content_id_to_embedding_tokens()) {
    // It is possible that subframes recorded in this map were not captured
    // (e.g. renderer crash, closed, etc.). Missing subframes are allowable
    // since having just the main frame is sufficient to create a preview.
    auto rect_it = result.ctx.find(id_pair.content_id());
    if (rect_it == result.ctx.end())
      continue;

    mojom::SubframeClipRect rect;
    rect.frame_guid = base::UnguessableToken::Deserialize(
        id_pair.embedding_token_high(), id_pair.embedding_token_low());
    rect.clip_rect = rect_it->second;

    if (!results.count(rect.frame_guid))
      continue;

    frame.subframe_clip_rects.push_back(rect);
  }
  return frame;
}

base::Optional<SkBitmap> CreateBitmap(sk_sp<SkPicture> skp,
                                      const gfx::Rect& clip_rect,
                                      float scale_factor) {
  TRACE_EVENT0("paint_preview", "PaintPreviewCompositorImpl::CreateBitmap");
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(
          SkImageInfo::MakeN32Premul(clip_rect.width(), clip_rect.height()))) {
    return base::nullopt;
  }
  SkCanvas canvas(bitmap);
  SkMatrix matrix;
  matrix.setScaleTranslate(scale_factor, scale_factor, -clip_rect.x(),
                           -clip_rect.y());
  canvas.drawPicture(skp, &matrix, nullptr);
  return bitmap;
}

}  // namespace

PaintPreviewCompositorImpl::PaintPreviewCompositorImpl(
    mojo::PendingReceiver<mojom::PaintPreviewCompositor> receiver,
    base::OnceClosure disconnect_handler) {
  if (receiver) {
    receiver_.Bind(std::move(receiver));
    receiver_.set_disconnect_handler(std::move(disconnect_handler));
  }
}

PaintPreviewCompositorImpl::~PaintPreviewCompositorImpl() {
  receiver_.reset();
}

void PaintPreviewCompositorImpl::BeginSeparatedFrameComposite(
    mojom::PaintPreviewBeginCompositeRequestPtr request,
    BeginSeparatedFrameCompositeCallback callback) {
  TRACE_EVENT0("paint_preview",
               "PaintPreviewCompositorImpl::BeginSeparatedFrameComposite");

  // Remove any previously loaded frames, in case |BeginSeparatedFrameComposite|
  // is called multiple times.
  frames_.clear();

  auto response = mojom::PaintPreviewBeginCompositeResponse::New();
  base::Optional<PaintPreviewProto> paint_preview =
      ParsePaintPreviewProto(request->proto);
  if (!paint_preview.has_value()) {
    // Cannot send a null token over mojo. This will be ignored downstream.
    response->root_frame_guid = base::UnguessableToken::Create();
    std::move(callback).Run(mojom::PaintPreviewCompositor::
                                BeginCompositeStatus::kDeserializingFailure,
                            std::move(response));
    return;
  }

  DCHECK(paint_preview.has_value());
  response->root_frame_guid = base::UnguessableToken::Deserialize(
      paint_preview->root_frame().embedding_token_high(),
      paint_preview->root_frame().embedding_token_low());
  if (response->root_frame_guid.is_empty()) {
    DVLOG(1) << "No valid root frame guid";
    // Cannot send a null token over mojo. This will be ignored downstream.
    response->root_frame_guid = base::UnguessableToken::Create();
    std::move(callback).Run(mojom::PaintPreviewCompositor::
                                BeginCompositeStatus::kDeserializingFailure,
                            std::move(response));
    return;
  }
  auto frames = DeserializeAllFrames(std::move(request->recording_map));

  // Adding the root frame must succeed.
  if (!AddFrame(paint_preview->root_frame(), frames, &response)) {
    DVLOG(1) << "Root frame not found.";
    std::move(callback).Run(mojom::PaintPreviewCompositor::
                                BeginCompositeStatus::kCompositingFailure,
                            std::move(response));
    return;
  }

  bool subframe_failed = false;
  // Adding subframes is optional.
  for (const auto& subframe_proto : paint_preview->subframes()) {
    if (!AddFrame(subframe_proto, frames, &response))
      subframe_failed = true;
  }

  std::move(callback).Run(
      subframe_failed
          ? mojom::PaintPreviewCompositor::BeginCompositeStatus::kPartialSuccess
          : mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess,
      std::move(response));
}

void PaintPreviewCompositorImpl::BitmapForSeparatedFrame(
    const base::UnguessableToken& frame_guid,
    const gfx::Rect& clip_rect,
    float scale_factor,
    BitmapForSeparatedFrameCallback callback) {
  TRACE_EVENT0("paint_preview",
               "PaintPreviewCompositorImpl::BitmapForSeparatedFrame");
  auto frame_it = frames_.find(frame_guid);
  if (frame_it == frames_.end()) {
    DVLOG(1) << "Frame not found for " << frame_guid.ToString();
    std::move(callback).Run(
        mojom::PaintPreviewCompositor::BitmapStatus::kMissingFrame, SkBitmap());
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::WithBaseSyncPrimitives()},
      base::BindOnce(&CreateBitmap, frame_it->second.skp, clip_rect,
                     scale_factor),
      base::BindOnce(
          [](BitmapForSeparatedFrameCallback callback,
             const base::Optional<SkBitmap>& maybe_bitmap) {
            if (!maybe_bitmap.has_value()) {
              std::move(callback).Run(
                  mojom::PaintPreviewCompositor::BitmapStatus::kAllocFailed,
                  SkBitmap());
              return;
            }
            std::move(callback).Run(
                mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                maybe_bitmap.value());
          },
          std::move(callback)));
}

void PaintPreviewCompositorImpl::BeginMainFrameComposite(
    mojom::PaintPreviewBeginCompositeRequestPtr request,
    BeginMainFrameCompositeCallback callback) {
  TRACE_EVENT0("paint_preview",
               "PaintPreviewCompositorImpl::BeginMainFrameComposite");
  base::Optional<PaintPreviewProto> paint_preview =
      ParsePaintPreviewProto(request->proto);
  if (!paint_preview.has_value()) {
    std::move(callback).Run(mojom::PaintPreviewCompositor::
                                BeginCompositeStatus::kDeserializingFailure);
    return;
  }

  DCHECK(paint_preview.has_value());
  base::UnguessableToken root_frame_guid = base::UnguessableToken::Deserialize(
      paint_preview->root_frame().embedding_token_high(),
      paint_preview->root_frame().embedding_token_low());
  if (root_frame_guid.is_empty()) {
    DVLOG(1) << "No valid root frame guid";
    std::move(callback).Run(mojom::PaintPreviewCompositor::
                                BeginCompositeStatus::kDeserializingFailure);
    return;
  }

  base::flat_map<base::UnguessableToken, sk_sp<SkPicture>> loaded_frames;
  RecordingMap recording_map = std::move(request->recording_map);
  bool subframes_failed = false;
  root_frame_ = DeserializeFrameRecursive(paint_preview->root_frame(),
                                          *paint_preview, &loaded_frames,
                                          &recording_map, &subframes_failed);

  if (root_frame_ == nullptr) {
    std::move(callback).Run(mojom::PaintPreviewCompositor::
                                BeginCompositeStatus::kCompositingFailure);
    return;
  }

  std::move(callback).Run(
      subframes_failed
          ? mojom::PaintPreviewCompositor::BeginCompositeStatus::kPartialSuccess
          : mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess);
}

void PaintPreviewCompositorImpl::BitmapForMainFrame(
    const gfx::Rect& clip_rect,
    float scale_factor,
    BitmapForMainFrameCallback callback) {
  TRACE_EVENT0("paint_preview",
               "PaintPreviewCompositorImpl::BitmapForSeparatedFrame");
  if (root_frame_ == nullptr) {
    DVLOG(1) << "Root frame not loaded";
    std::move(callback).Run(
        mojom::PaintPreviewCompositor::BitmapStatus::kMissingFrame, SkBitmap());
    return;
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::WithBaseSyncPrimitives()},
      base::BindOnce(&CreateBitmap, root_frame_, clip_rect, scale_factor),
      base::BindOnce(
          [](BitmapForMainFrameCallback callback,
             const base::Optional<SkBitmap>& maybe_bitmap) {
            if (!maybe_bitmap.has_value()) {
              std::move(callback).Run(
                  mojom::PaintPreviewCompositor::BitmapStatus::kAllocFailed,
                  SkBitmap());
              return;
            }
            std::move(callback).Run(
                mojom::PaintPreviewCompositor::BitmapStatus::kSuccess,
                maybe_bitmap.value());
          },
          std::move(callback)));
}

void PaintPreviewCompositorImpl::SetRootFrameUrl(const GURL& url) {
  url_ = url;
}

bool PaintPreviewCompositorImpl::AddFrame(
    const PaintPreviewFrameProto& frame_proto,
    const base::flat_map<base::UnguessableToken, SkpResult>& skp_map,
    mojom::PaintPreviewBeginCompositeResponsePtr* response) {
  base::UnguessableToken guid = base::UnguessableToken::Deserialize(
      frame_proto.embedding_token_high(), frame_proto.embedding_token_low());

  base::Optional<PaintPreviewFrame> maybe_frame =
      BuildFrame(guid, frame_proto, skp_map);
  if (!maybe_frame.has_value())
    return false;
  const PaintPreviewFrame& frame = maybe_frame.value();

  auto frame_data = mojom::FrameData::New();
  SkRect sk_rect = frame.skp->cullRect();
  frame_data->scroll_extents = gfx::Size(sk_rect.width(), sk_rect.height());
  frame_data->scroll_offsets = gfx::Size(
      frame_proto.has_scroll_offset_x() ? frame_proto.scroll_offset_x() : 0,
      frame_proto.has_scroll_offset_y() ? frame_proto.scroll_offset_y() : 0);
  frame_data->subframes.reserve(frame.subframe_clip_rects.size());
  for (const auto& subframe_clip_rect : frame.subframe_clip_rects)
    frame_data->subframes.push_back(subframe_clip_rect.Clone());

  (*response)->frames.insert({guid, std::move(frame_data)});
  frames_.insert({guid, std::move(maybe_frame.value())});
  return true;
}

// static
base::flat_map<base::UnguessableToken, SkpResult>
PaintPreviewCompositorImpl::DeserializeAllFrames(RecordingMap&& recording_map) {
  TRACE_EVENT0("paint_preview",
               "PaintPreviewCompositorImpl::DeserializeAllFrames");
  std::vector<std::pair<base::UnguessableToken, SkpResult>> results;
  results.reserve(recording_map.size());

  for (auto& it : recording_map) {
    base::Optional<SkpResult> maybe_result = std::move(it.second).Deserialize();
    if (!maybe_result.has_value())
      continue;

    SkpResult& result = maybe_result.value();
    if (!result.skp || result.skp->cullRect().width() == 0 ||
        result.skp->cullRect().height() == 0) {
      continue;
    }

    results.emplace_back(it.first, std::move(result));
  }

  return base::flat_map<base::UnguessableToken, SkpResult>(std::move(results));
}

// static
sk_sp<SkPicture> PaintPreviewCompositorImpl::DeserializeFrameRecursive(
    const PaintPreviewFrameProto& frame_proto,
    const PaintPreviewProto& proto,
    base::flat_map<base::UnguessableToken, sk_sp<SkPicture>>* loaded_frames,
    RecordingMap* recording_map,
    bool* subframe_failed) {
  auto frame_guid = base::UnguessableToken::Deserialize(
      frame_proto.embedding_token_high(), frame_proto.embedding_token_low());
  TRACE_EVENT1("paint_preview",
               "PaintPreviewCompositorImpl::DeserializeFrameRecursive",
               "frame_guid", frame_guid.ToString());

  // Sanity check that the frame wasn't already loaded.
  auto it = loaded_frames->find(frame_guid);
  if (it != loaded_frames->end())
    return it->second;

  // Recursively load subframes into to deserialization context. This is done
  // before the current frame is loaded to ensure the order of loading is
  // topologically sorted.
  LoadedFramesDeserialContext deserial_context;

  *subframe_failed = false;
  for (const auto& id_pair : frame_proto.content_id_to_embedding_tokens()) {
    auto subframe_embedding_token = base::UnguessableToken::Deserialize(
        id_pair.embedding_token_high(), id_pair.embedding_token_low());

    // This subframe is already loaded.
    if (loaded_frames->contains(subframe_embedding_token)) {
      DVLOG(1) << "Subframe already loaded: " << subframe_embedding_token;
      continue;
    }

    // Try and find the subframe's proto based on its embedding token.
    auto& subframes = proto.subframes();
    auto subframe_proto_it = std::find_if(
        subframes.begin(), subframes.end(),
        [subframe_embedding_token](const PaintPreviewFrameProto& frame_proto) {
          return subframe_embedding_token ==
                 base::UnguessableToken::Deserialize(
                     frame_proto.embedding_token_high(),
                     frame_proto.embedding_token_low());
        });
    if (subframe_proto_it == subframes.end()) {
      DVLOG(1) << "Frame embeds subframe that does not exist: "
               << subframe_embedding_token;
      *subframe_failed = true;
      continue;
    }

    bool subframes_of_subframe_failed = false;
    sk_sp<SkPicture> subframe =
        DeserializeFrameRecursive(*subframe_proto_it, proto, loaded_frames,
                                  recording_map, &subframes_of_subframe_failed);
    if (subframe == nullptr) {
      DVLOG(1) << "Subframe could not be deserialized: "
               << subframe_embedding_token;
      *subframe_failed = true;
      continue;
    }

    // Subframe failure is transitive and should propogate.
    if (subframes_of_subframe_failed) {
      *subframe_failed = true;
    }

    // Place the subframe dependency in our deserialization context.
    loaded_frames->insert({subframe_embedding_token, subframe});

    FrameAndScrollOffsets frame;
    frame.picture = subframe;
    frame.scroll_offsets = gfx::Size(subframe_proto_it->has_scroll_offset_x()
                                         ? subframe_proto_it->scroll_offset_x()
                                         : 0,
                                     subframe_proto_it->has_scroll_offset_y()
                                         ? subframe_proto_it->scroll_offset_y()
                                         : 0);
    deserial_context.insert({id_pair.content_id(), frame});
  }

  auto recording_it = recording_map->find(frame_guid);
  if (recording_it == recording_map->end()) {
    DVLOG(1) << "Serialized recording doesn't exist for frame: " << frame_guid;
    return nullptr;
  }

  sk_sp<SkPicture> picture =
      std::move(recording_it->second).DeserializeWithContext(&deserial_context);
  recording_map->erase(recording_it);
  return picture;
}

}  // namespace paint_preview
