// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/renderer/paint_preview_recorder_utils.h"

#include <utility>

#include "base/bind.h"
#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "cc/paint/paint_image.h"
#include "cc/paint/paint_image_builder.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace paint_preview {

namespace {

// Converts a texture backed paint image in the PaintOpBuffer to one that is not
// texture backed.
cc::PaintImage MakeUnaccelerated(cc::PaintImage& paint_image) {
  DCHECK(paint_image.IsTextureBacked());
  auto sk_image = paint_image.GetSwSkImage();
  if (sk_image->isLazyGenerated()) {
    // Texture backed images should always be returned as SkImage_Raster type
    // (bitmap). This is just a catchall in the event a lazy image is somehow
    // returned in which case we should just raster it.
    SkBitmap bitmap;
    bitmap.allocPixels(sk_image->imageInfo(),
                       sk_image->imageInfo().minRowBytes());
    if (!sk_image->readPixels(bitmap.pixmap(), 0, 0)) {
      return paint_image;
    }
    // Make immutable to skip an extra copy.
    bitmap.setImmutable();
    sk_image = SkImage::MakeFromBitmap(bitmap);
  }
  return cc::PaintImageBuilder::WithDefault()
      .set_id(cc::PaintImage::GetNextId())
      .set_image(sk_image, cc::PaintImage::GetNextContentId())
      .TakePaintImage();
}

}  // namespace

void PreProcessPaintOpBuffer(const cc::PaintOpBuffer* buffer,
                             PaintPreviewTracker* tracker) {
  for (cc::PaintOpBuffer::Iterator it(buffer); it; ++it) {
    switch (it->GetType()) {
      case cc::PaintOpType::DrawTextBlob: {
        const auto& text_blob_op = static_cast<const cc::DrawTextBlobOp&>(*it);
        tracker->AddGlyphs(text_blob_op.blob.get());
        break;
      }
      case cc::PaintOpType::DrawRecord: {
        // Recurse into nested records if they contain text blobs (equivalent to
        // nested SkPictures).
        const auto& record_op = static_cast<const cc::DrawRecordOp&>(*it);
        PreProcessPaintOpBuffer(record_op.record.get(), tracker);
        break;
      }
      case cc::PaintOpType::Annotate: {
        auto& annotate_op = static_cast<cc::AnnotateOp&>(*it);
        tracker->AnnotateLink(GURL(std::string(reinterpret_cast<const char*>(
                                                   annotate_op.data->data()),
                                               annotate_op.data->size())),
                              annotate_op.rect);
        // Delete the data. We no longer need it.
        annotate_op.data.reset();
        break;
      }
      case cc::PaintOpType::CustomData: {
        const auto& custom_op = static_cast<const cc::CustomDataOp&>(*it);
        tracker->TransformClipForFrame(custom_op.id);
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
        const auto& matrix_op = static_cast<const cc::SetMatrixOp&>(*it);
        tracker->SetMatrix(matrix_op.matrix.asM33());
        break;
      }
      case cc::PaintOpType::Concat: {
        const auto& concat_op = static_cast<const cc::ConcatOp&>(*it);
        tracker->Concat(concat_op.matrix.asM33());
        break;
      }
      case cc::PaintOpType::Scale: {
        const auto& scale_op = static_cast<const cc::ScaleOp&>(*it);
        tracker->Scale(scale_op.sx, scale_op.sy);
        break;
      }
      case cc::PaintOpType::Rotate: {
        const auto& rotate_op = static_cast<const cc::RotateOp&>(*it);
        tracker->Rotate(rotate_op.degrees);
        break;
      }
      case cc::PaintOpType::Translate: {
        const auto& translate_op = static_cast<const cc::TranslateOp&>(*it);
        tracker->Translate(translate_op.dx, translate_op.dy);
        break;
      }
      case cc::PaintOpType::DrawImage: {
        auto& image_op = static_cast<cc::DrawImageOp&>(*it);
        if (image_op.image.IsTextureBacked()) {
          image_op.image = MakeUnaccelerated(image_op.image);
        }
        break;
      }
      case cc::PaintOpType::DrawImageRect: {
        auto& image_op = static_cast<cc::DrawImageRectOp&>(*it);
        if (image_op.image.IsTextureBacked()) {
          image_op.image = MakeUnaccelerated(image_op.image);
        }
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

  auto skp =
      ToSkPicture(recording, SkRect::MakeWH(bounds.width(), bounds.height()),
                  nullptr, custom_callback);

  if (!skp || skp->cullRect().width() == 0 || skp->cullRect().height() == 0)
    return nullptr;

  return skp;
}

void BuildResponse(PaintPreviewTracker* tracker,
                   mojom::PaintPreviewCaptureResponse* response) {
  // Ensure these always exist.
  DCHECK(tracker);
  DCHECK(response);

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
