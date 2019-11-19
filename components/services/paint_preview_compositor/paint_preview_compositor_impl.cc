// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/paint_preview_compositor/paint_preview_compositor_impl.h"

#include <utility>

#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/proto/paint_preview.pb.h"
#include "components/paint_preview/common/serial_utils.h"
#include "components/services/paint_preview_compositor/public/mojom/paint_preview_compositor.mojom.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkMatrix.h"

namespace paint_preview {

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

void PaintPreviewCompositorImpl::BeginComposite(
    mojom::PaintPreviewBeginCompositeRequestPtr request,
    BeginCompositeCallback callback) {
  auto response = mojom::PaintPreviewBeginCompositeResponse::New();
  auto mapping = request->proto.Map();
  if (!mapping.IsValid()) {
    std::move(callback).Run(
        mojom::PaintPreviewCompositor::Status::kDeserializingFailure,
        std::move(response));
    return;
  }

  PaintPreviewProto paint_preview;
  bool ok = paint_preview.ParseFromArray(mapping.memory(), mapping.size());
  if (!ok) {
    std::move(callback).Run(
        mojom::PaintPreviewCompositor::Status::kDeserializingFailure,
        std::move(response));
    return;
  }
  if (!AddFrame(paint_preview.root_frame(), &request->file_map, &response)) {
    std::move(callback).Run(
        mojom::PaintPreviewCompositor::Status::kCompositingFailure,
        std::move(response));
    return;
  }
  response->root_frame_guid = paint_preview.root_frame().id();
  for (const auto& subframe_proto : paint_preview.subframes())
    AddFrame(subframe_proto, &request->file_map, &response);

  std::move(callback).Run(mojom::PaintPreviewCompositor::Status::kSuccess,
                          std::move(response));
}

void PaintPreviewCompositorImpl::BitmapForFrame(
    uint64_t frame_guid,
    const gfx::Rect& clip_rect,
    float scale_factor,
    BitmapForFrameCallback callback) {
  SkBitmap bitmap;
  auto frame_it = frames_.find(frame_guid);
  if (frame_it == frames_.end()) {
    std::move(callback).Run(
        mojom::PaintPreviewCompositor::Status::kCompositingFailure, bitmap);
    return;
  }

  auto skp = frame_it->second.skp;
  bitmap.allocPixels(
      SkImageInfo::MakeN32Premul(clip_rect.width(), clip_rect.height()));
  SkCanvas canvas(bitmap);
  SkMatrix matrix;
  matrix.setScaleTranslate(scale_factor, scale_factor, -clip_rect.x(),
                           -clip_rect.y());
  canvas.drawPicture(skp, &matrix, nullptr);

  std::move(callback).Run(mojom::PaintPreviewCompositor::Status::kSuccess,
                          bitmap);
}

void PaintPreviewCompositorImpl::SetRootFrameUrl(const GURL& url) {
  url_ = url;
}

PaintPreviewFrame PaintPreviewCompositorImpl::DeserializeFrame(
    const PaintPreviewFrameProto& frame_proto,
    base::File file_handle) {
  PaintPreviewFrame frame;
  FileRStream rstream(std::move(file_handle));
  DeserializationContext ctx;
  SkDeserialProcs procs = MakeDeserialProcs(&ctx);

  frame.skp = SkPicture::MakeFromStream(&rstream, &procs);

  for (const auto& id_pair : frame_proto.content_id_proxy_id_map()) {
    // It is possible that subframes recorded in this map were not captured
    // (e.g. renderer crash, closed, etc.). Missing subframes are allowable
    // since having just the main frame is sufficient to create a preview.
    auto rect_it = ctx.find(id_pair.first);
    if (rect_it == ctx.end())
      continue;

    mojom::SubframeClipRect rect;
    rect.frame_guid = id_pair.second;
    rect.clip_rect = rect_it->second;
    frame.subframe_clip_rects.push_back(rect);
  }
  return frame;
}

bool PaintPreviewCompositorImpl::AddFrame(
    const PaintPreviewFrameProto& frame_proto,
    FileMap* file_map,
    mojom::PaintPreviewBeginCompositeResponsePtr* response) {
  uint64_t id = frame_proto.id();
  auto file_it = file_map->find(id);
  if (file_it == file_map->end() || !file_it->second.IsValid())
    return false;
  PaintPreviewFrame frame =
      DeserializeFrame(frame_proto, std::move(file_it->second));
  file_map->erase(file_it);

  auto frame_data = mojom::FrameData::New();
  SkRect sk_rect = frame.skp->cullRect();
  frame_data->scroll_extents = gfx::Size(sk_rect.width(), sk_rect.height());
  frame_data->subframes.reserve(frame.subframe_clip_rects.size());
  for (const auto& subframe_clip_rect : frame.subframe_clip_rects)
    frame_data->subframes.push_back(subframe_clip_rect.Clone());

  (*response)->frames.insert({id, std::move(frame_data)});
  frames_.insert({id, std::move(frame)});
  return true;
}

}  // namespace paint_preview
