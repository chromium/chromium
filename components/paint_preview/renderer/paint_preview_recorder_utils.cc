// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/renderer/paint_preview_recorder_utils.h"

#include <utility>

#include "base/bind.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_image.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace paint_preview {

void ParseGlyphsAndLinks(const cc::PaintOpBuffer* buffer,
                         PaintPreviewTracker* tracker) {
  for (cc::PaintOpBuffer::Iterator it(buffer); it; ++it) {
    switch (it->GetType()) {
      case cc::PaintOpType::DrawTextBlob: {
        auto* text_blob_op = static_cast<cc::DrawTextBlobOp*>(*it);
        tracker->AddGlyphs(text_blob_op->blob.get());
        break;
      }
      case cc::PaintOpType::DrawRecord: {
        // Recurse into nested records if they contain text blobs (equivalent to
        // nested SkPictures).
        auto* record_op = static_cast<cc::DrawRecordOp*>(*it);
        ParseGlyphsAndLinks(record_op->record.get(), tracker);
        break;
      }
      case cc::PaintOpType::Annotate: {
        auto* annotate_op = static_cast<cc::AnnotateOp*>(*it);
        tracker->AnnotateLink(GURL(std::string(reinterpret_cast<const char*>(
                                                   annotate_op->data->data()),
                                               annotate_op->data->size())),
                              annotate_op->rect);
        // Delete the data. We no longer need it.
        annotate_op->data.reset();
        break;
      }
      case cc::PaintOpType::CustomData: {
        auto* custom_op = static_cast<cc::CustomDataOp*>(*it);
        tracker->TransformClipForFrame(custom_op->id);
        break;
      }
      case cc::PaintOpType::Save: {
        tracker->Save();
        break;
      }
      case cc::PaintOpType::SaveLayer: {
        tracker->Save();
        break;
      }
      case cc::PaintOpType::SaveLayerAlpha: {
        tracker->Save();
        break;
      }
      case cc::PaintOpType::Restore: {
        tracker->Restore();
        break;
      }
      case cc::PaintOpType::SetMatrix: {
        auto* matrix_op = static_cast<cc::SetMatrixOp*>(*it);
        tracker->SetMatrix(matrix_op->matrix);
        break;
      }
      case cc::PaintOpType::Concat: {
        auto* concat_op = static_cast<cc::ConcatOp*>(*it);
        tracker->Concat(concat_op->matrix);
        break;
      }
      case cc::PaintOpType::Scale: {
        auto* scale_op = static_cast<cc::ScaleOp*>(*it);
        tracker->Scale(scale_op->sx, scale_op->sy);
        break;
      }
      case cc::PaintOpType::Rotate: {
        auto* rotate_op = static_cast<cc::RotateOp*>(*it);
        tracker->Rotate(rotate_op->degrees);
        break;
      }
      case cc::PaintOpType::Translate: {
        auto* translate_op = static_cast<cc::TranslateOp*>(*it);
        tracker->Translate(translate_op->dx, translate_op->dy);
        break;
      }
      default:
        continue;
    }
  }
}

sk_sp<const SkPicture> PaintRecordToSkPicture(
    sk_sp<const cc::PaintRecord> recording,
    PaintPreviewTracker* tracker,
    const gfx::Rect& bounds) {
  // base::Unretained is safe as |tracker| outlives the usage of
  // |custom_callback|.
  cc::PlaybackParams::CustomDataRasterCallback custom_callback =
      base::BindRepeating(&PaintPreviewTracker::CustomDataToSkPictureCallback,
                          base::Unretained(tracker));

  DCHECK_EQ(bounds.x(), 0);
  DCHECK_EQ(bounds.y(), 0);
  auto skp =
      ToSkPicture(recording, SkRect::MakeWH(bounds.width(), bounds.height()),
                  nullptr, custom_callback);

  if (!skp || skp->cullRect().width() == 0 || skp->cullRect().height() == 0)
    return nullptr;

  return skp;
}

void BuildResponse(PaintPreviewTracker* tracker,
                   mojom::PaintPreviewCaptureResponse* response,
                   bool log) {
  // Ensure these always exist.
  DCHECK(tracker);
  DCHECK(response);

  // paint_preview::BuildResponse has been showing in a large number of crashes
  // under stack scans. In order to determine if these entries are "real" we
  // should log the calls and check the log output.
  if (log)
    LOG(WARNING) << "paint_preview::BuildResponse() called";

  response->embedding_token = tracker->EmbeddingToken();
  tracker->MoveLinks(&response->links);

  PictureSerializationContext* picture_context =
      tracker->GetPictureSerializationContext();
  if (!picture_context)
    return;

  for (const auto& id_pair : picture_context->content_id_to_embedding_token) {
    response->content_id_to_embedding_token.insert(id_pair);
  }
}

}  // namespace paint_preview
