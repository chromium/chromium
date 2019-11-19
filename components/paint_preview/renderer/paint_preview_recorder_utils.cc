// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/renderer/paint_preview_recorder_utils.h"

#include <utility>

#include "base/bind.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"

namespace paint_preview {

void ParseGlyphs(const cc::PaintOpBuffer* buffer,
                 PaintPreviewTracker* tracker) {
  for (cc::PaintOpBuffer::Iterator it(buffer); it; ++it) {
    if (it->GetType() == cc::PaintOpType::DrawTextBlob) {
      auto* text_blob_op = static_cast<cc::DrawTextBlobOp*>(*it);
      tracker->AddGlyphs(text_blob_op->blob.get());
    } else if (it->GetType() == cc::PaintOpType::DrawRecord) {
      // Recurse into nested records if they contain text blobs (equivalent to
      // nested SkPictures).
      auto* record_op = static_cast<cc::DrawRecordOp*>(*it);
      if (record_op->HasText())
        ParseGlyphs(record_op->record.get(), tracker);
    }
  }
}

bool SerializeAsSkPicture(sk_sp<const cc::PaintRecord> record,
                          PaintPreviewTracker* tracker,
                          const gfx::Rect& dimensions,
                          base::File file) {
  if (!file.IsValid())
    return false;

  // base::Unretained is safe as |tracker| outlives the usage of
  // |custom_callback|.
  cc::PlaybackParams::CustomDataRasterCallback custom_callback =
      base::BindRepeating(&PaintPreviewTracker::CustomDataToSkPictureCallback,
                          base::Unretained(tracker));
  auto skp = ToSkPicture(
      record, SkRect::MakeWH(dimensions.width(), dimensions.height()), nullptr,
      custom_callback);
  if (!skp)
    return false;

  TypefaceSerializationContext typeface_context(tracker->GetTypefaceUsageMap());
  auto serial_procs = MakeSerialProcs(tracker->GetPictureSerializationContext(),
                                      &typeface_context);
  FileWStream stream(std::move(file));
  skp->serialize(&stream, &serial_procs);
  stream.flush();
  stream.Close();
  return true;
}

void BuildResponse(PaintPreviewTracker* tracker,
                   mojom::PaintPreviewCaptureResponse* response) {
  response->id = tracker->RoutingId();
  for (const auto& id_pair : *(tracker->GetPictureSerializationContext())) {
    response->content_id_proxy_id_map.insert({id_pair.first, id_pair.second});
  }
  for (const auto& link : tracker->GetLinks())
    response->links.push_back(link.Clone());
}

}  // namespace paint_preview
