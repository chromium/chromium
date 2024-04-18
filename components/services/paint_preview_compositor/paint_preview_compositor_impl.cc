// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/paint_preview_compositor/paint_preview_compositor_impl.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <utility>

#include "base/memory/memory_pressure_listener.h"
#include "base/ranges/algorithm.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/recording_map.h"
#include "components/paint_preview/common/serial_utils.h"
#include "components/paint_preview/common/serialized_recording.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "skia/ext/legacy_display_globals.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPicture.h"
#include "third_party/skia/include/core/SkScalar.h"
#include "third_party/skia/include/core/SkStream.h"

namespace paint_preview {

namespace {

std::optional<PaintPreviewFrame> BuildFrame(
    const base::UnguessableToken& token,
    const PaintPreviewFrameProto& frame_proto,
    const base::flat_map<base::UnguessableToken, SkpResult>& results) {
  TRACE_EVENT0("paint_preview", "PaintPreviewCompositorImpl::BuildFrame");
  auto it = results.find(token);
  if (it == results.end())
    return std::nullopt;

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
    std::optional<base::UnguessableToken> maybe_deserialized_token =
        base::UnguessableToken::Deserialize(id_pair.embedding_token_high(),
                                            id_pair.embedding_token_low());
    if (!maybe_deserialized_token.has_value()) {
      continue;
    }
    rect.frame_guid = maybe_deserialized_token.value();

    rect.clip_rect = rect_it->second;

    if (!results.count(rect.frame_guid))
      continue;

    frame.subframe_clip_rects.push_back(rect);
  }
  return frame;
}

// Adjusts `clip_rect` to be bounded by `picture_rectf` when scaled by
// `scale_factor`. This also replaces a 0 value for the width or height of
// `clip_rect` to fill the extent of that dimension remaining.
gfx::Rect AdjustClipRect(const gfx::Rect& clip_rect,
                         const SkRect& picture_rectf,
                         float scale_factor) {
  // Scale the picture dimensions and clamp ceil to an int as `picture_size`
  // needs to be pixel aligned.
  gfx::Size picture_size(
      base::ClampCeil(picture_rectf.width() * scale_factor),
      base::ClampCeil(picture_rectf.height() * scale_factor));

  // Clamp the x/y to be within the bounds of the picture.
  gfx::Rect out_rect;
  out_rect.set_x(std::clamp(clip_rect.x(), 0, picture_size.width()));
  out_rect.set_y(std::clamp(clip_rect.y(), 0, picture_size.height()));

  // Default the width/height to be that of the picture if no value was
  // provided.
  const int width =
      (clip_rect.width() <= 0) ? picture_size.width() : clip_rect.width();
  const int height =
      (clip_rect.height() <= 0) ? picture_size.height() : clip_rect.height();

  // Clamp the width/height to be within the picture's bounds. Using the
  // `width` and `height` calculated above could result in an `out_rect` that
  // overflows the picture if x/y are non-zero.
  //
  // Example:
  // - `picture_size` is 100x200
  // - `clip_rect` is (10, 20) 0x230
  // - Above the `width` and `height` will be set to 100 and 230 respectively.
  // However, 10+100 and 20+230 will overflow the `picture_size` so these values
  // need to be clamped. The maximum value to clamp to (in both dimensions) is:
  // `picture_size` - `out_rect.origin()`.
  out_rect.set_width(std::min(width, picture_size.width() - out_rect.x()));
  out_rect.set_height(std::min(height, picture_size.height() - out_rect.y()));

  return out_rect;
}

// Holds a ref to the discardable_shared_memory_manager so it sticks around
// until at least after skia is finished with it.
std::optional<SkBitmap> CreateBitmap(
    scoped_refptr<discardable_memory::ClientDiscardableSharedMemoryManager>
        discardable_shared_memory_manager,
    sk_sp<SkPicture> skp,
    const gfx::Rect& raw_clip_rect,
    float scale_factor) {
  TRACE_EVENT0("paint_preview", "PaintPreviewCompositorImpl::CreateBitmap");
  const gfx::Rect clip_rect =
      AdjustClipRect(raw_clip_rect, skp->cullRect(), scale_factor);
  SkBitmap bitmap;
  if (!bitmap.tryAllocPixels(
          SkImageInfo::MakeN32Premul(clip_rect.width(), clip_rect.height()))) {
    return std::nullopt;
  }

  SkCanvas canvas(bitmap, skia::LegacyDisplayGlobals::GetSkSurfaceProps());
  SkMatrix matrix;
  matrix.setScaleTranslate(scale_factor, scale_factor, -clip_rect.x(),
                           -clip_rect.y());
  canvas.drawPicture(skp, &matrix, nullptr);
  return bitmap;
}

}  // namespace

PaintPreviewCompositorImpl::PaintPreviewCompositorImpl(
    mojo::PendingReceiver<mojom::PaintPreviewCompositor> receiver,
    scoped_refptr<discardable_memory::ClientDiscardableSharedMemoryManager>
        discardable_shared_memory_manager,
    base::OnceClosure disconnect_handler)
    : discardable_shared_memory_manager_(discardable_shared_memory_manager) {
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
  root_frame_ = nullptr;
  frames_.clear();

  auto response = mojom::PaintPreviewBeginCompositeResponse::New();
  auto paint_preview = request->preview.As<PaintPreviewProto>();
  if (!paint_preview.has_value()) {
    // Cannot send a null token over mojo. This will be ignored downstream.
    response->root_frame_guid = base::UnguessableToken::Create();
    std::move(callback).Run(mojom::PaintPreviewCompositor::
                                BeginCompositeStatus::kDeserializingFailure,
                            std::move(response));
    return;
  }

  DCHECK(paint_preview.has_value());
  std::optional<base::UnguessableToken> embedding_token =
      base::UnguessableToken::Deserialize(
          paint_preview->root_frame().embedding_token_high(),
          paint_preview->root_frame().embedding_token_low());
  if (!embedding_token.has_value()) {
    DVLOG(1) << "No valid root frame guid";
    // Cannot send a null token over mojo. This will be ignored downstream.
    response->root_frame_guid = base::UnguessableToken::Create();
    std::move(callback).Run(mojom::PaintPreviewCompositor::
                                BeginCompositeStatus::kDeserializingFailure,
                            std::move(response));
    return;
  }
  response->root_frame_guid = embedding_token.value();
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
      {base::TaskPriority::USER_VISIBLE, base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CreateBitmap, discardable_shared_memory_manager_,
                     frame_it->second.skp, clip_rect, scale_factor),
      base::BindOnce(
          [](BitmapForSeparatedFrameCallback callback,
             const std::optional<SkBitmap>& maybe_bitmap) {
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
  frames_.clear();
  auto response = mojom::PaintPreviewBeginCompositeResponse::New();
  auto paint_preview = request->preview.As<PaintPreviewProto>();
  if (!paint_preview.has_value()) {
    response->root_frame_guid = base::UnguessableToken::Create();
    std::move(callback).Run(mojom::PaintPreviewCompositor::
                                BeginCompositeStatus::kDeserializingFailure,
                            std::move(response));
    return;
  }

  DCHECK(paint_preview.has_value());
  std::optional<base::UnguessableToken> maybe_root_frame_guid =
      base::UnguessableToken::Deserialize(
          paint_preview->root_frame().embedding_token_high(),
          paint_preview->root_frame().embedding_token_low());
  if (!maybe_root_frame_guid.has_value()) {
    DVLOG(1) << "No valid root frame guid";
    response->root_frame_guid = base::UnguessableToken::Create();
    std::move(callback).Run(mojom::PaintPreviewCompositor::
                                BeginCompositeStatus::kDeserializingFailure,
                            std::move(response));
    return;
  }
  base::UnguessableToken root_frame_guid = maybe_root_frame_guid.value();

  base::flat_map<base::UnguessableToken, sk_sp<SkPicture>> loaded_frames;
  RecordingMap recording_map = std::move(request->recording_map);
  bool subframes_failed = false;
  root_frame_ = DeserializeFrameRecursive(paint_preview->root_frame(),
                                          *paint_preview, &loaded_frames,
                                          &recording_map, &subframes_failed);

  if (root_frame_ == nullptr) {
    response->root_frame_guid = base::UnguessableToken::Create();
    std::move(callback).Run(mojom::PaintPreviewCompositor::
                                BeginCompositeStatus::kCompositingFailure,
                            std::move(response));
    return;
  }

  response->root_frame_guid = root_frame_guid;
  auto frame_data = mojom::FrameData::New();
  SkRect sk_rect = root_frame_->cullRect();
  frame_data->scroll_extents = gfx::Size(sk_rect.width(), sk_rect.height());
  frame_data->scroll_offsets =
      gfx::Size(paint_preview->root_frame().has_scroll_offset_x()
                    ? paint_preview->root_frame().scroll_offset_x()
                    : 0,
                paint_preview->root_frame().has_scroll_offset_y()
                    ? paint_preview->root_frame().scroll_offset_y()
                    : 0);
  response->frames.insert({root_frame_guid, std::move(frame_data)});

  std::move(callback).Run(
      subframes_failed
          ? mojom::PaintPreviewCompositor::BeginCompositeStatus::kPartialSuccess
          : mojom::PaintPreviewCompositor::BeginCompositeStatus::kSuccess,
      std::move(response));
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
      {base::TaskPriority::USER_VISIBLE, base::WithBaseSyncPrimitives(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&CreateBitmap, discardable_shared_memory_manager_,
                     root_frame_, clip_rect, scale_factor),
      base::BindOnce(
          [](BitmapForMainFrameCallback callback,
             const std::optional<SkBitmap>& maybe_bitmap) {
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
  std::optional<base::UnguessableToken> maybe_guid =
      base::UnguessableToken::Deserialize(frame_proto.embedding_token_high(),
                                          frame_proto.embedding_token_low());
  if (!maybe_guid.has_value()) {
    return false;
  }
  base::UnguessableToken guid = maybe_guid.value();

  std::optional<PaintPreviewFrame> maybe_frame =
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
    std::optional<SkpResult> maybe_result = std::move(it.second).Deserialize();
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
  std::optional<base::UnguessableToken> maybe_frame_guid =
      base::UnguessableToken::Deserialize(frame_proto.embedding_token_high(),
                                          frame_proto.embedding_token_low());
  if (!maybe_frame_guid.has_value()) {
    return nullptr;
  }
  base::UnguessableToken frame_guid = maybe_frame_guid.value();
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
    std::optional<base::UnguessableToken> maybe_subframe_embedding_token =
        base::UnguessableToken::Deserialize(id_pair.embedding_token_high(),
                                            id_pair.embedding_token_low());

    if (!maybe_subframe_embedding_token.has_value()) {
      DVLOG(1) << "Subframe has invalid embedding token";
      continue;
    }
    base::UnguessableToken subframe_embedding_token =
        maybe_subframe_embedding_token.value();

    // This subframe is already loaded.
    if (loaded_frames->contains(subframe_embedding_token)) {
      DVLOG(1) << "Subframe already loaded: " << subframe_embedding_token;
      continue;
    }

    // Try and find the subframe's proto based on its embedding token.
    auto& subframes = proto.subframes();
    auto subframe_proto_it =
        base::ranges::find(subframes, subframe_embedding_token,
                           [](const PaintPreviewFrameProto& frame_proto) {
                             std::optional<base::UnguessableToken> token =
                                 base::UnguessableToken::Deserialize(
                                     frame_proto.embedding_token_high(),
                                     frame_proto.embedding_token_low());
                             if (!token.has_value()) {
                               return base::UnguessableToken::Create();
                             }
                             return token.value();
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
