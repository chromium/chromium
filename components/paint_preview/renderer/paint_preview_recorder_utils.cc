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
#include "cc/paint/paint_op_buffer_iterator.h"
#include "components/paint_preview/common/file_stream.h"
#include "components/paint_preview/common/paint_preview_tracker.h"
#include "mojo/public/cpp/base/shared_memory_utils.h"
#include "third_party/abseil-cpp/absl/types/variant.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"

namespace paint_preview {

namespace {

// Converts a texture backed paint image to one that is not texture backed.
cc::PaintImage MakeUnaccelerated(const cc::PaintImage& paint_image) {
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
      .set_image(std::move(sk_image), cc::PaintImage::GetNextContentId())
      .TakePaintImage();
}

class OpConverterAndTracker {
 public:
  explicit OpConverterAndTracker(PaintPreviewTracker* tracker)
      : tracker_(tracker) {}

  const cc::PaintOp* ConvertAndTrack(const cc::PaintOp& op) {
    converted_op_.emplace<absl::monostate>();
    switch (op.GetType()) {
      case cc::PaintOpType::DrawTextBlob: {
        const auto& text_blob_op = static_cast<const cc::DrawTextBlobOp&>(op);
        tracker_->AddGlyphs(text_blob_op.blob.get());
        break;
      }
      case cc::PaintOpType::Annotate: {
        const auto& annotate_op = static_cast<const cc::AnnotateOp&>(op);
        tracker_->AnnotateLink(GURL(std::string(reinterpret_cast<const char*>(
                                                    annotate_op.data->data()),
                                                annotate_op.data->size())),
                               annotate_op.rect);
        // Delete the op. We no longer need it.
        return nullptr;
      }
      case cc::PaintOpType::CustomData: {
        const auto& custom_op = static_cast<const cc::CustomDataOp&>(op);
        tracker_->TransformClipForFrame(custom_op.id);
        break;
      }
      case cc::PaintOpType::Save: {
        tracker_->Save();
        break;
      }
      case cc::PaintOpType::SaveLayer: {
        tracker_->Save();
        break;
      }
      case cc::PaintOpType::SaveLayerAlpha: {
        tracker_->Save();
        break;
      }
      case cc::PaintOpType::Restore: {
        tracker_->Restore();
        break;
      }
      case cc::PaintOpType::SetMatrix: {
        const auto& matrix_op = static_cast<const cc::SetMatrixOp&>(op);
        tracker_->SetMatrix(matrix_op.matrix.asM33());
        break;
      }
      case cc::PaintOpType::Concat: {
        const auto& concat_op = static_cast<const cc::ConcatOp&>(op);
        tracker_->Concat(concat_op.matrix.asM33());
        break;
      }
      case cc::PaintOpType::Scale: {
        const auto& scale_op = static_cast<const cc::ScaleOp&>(op);
        tracker_->Scale(scale_op.sx, scale_op.sy);
        break;
      }
      case cc::PaintOpType::Rotate: {
        const auto& rotate_op = static_cast<const cc::RotateOp&>(op);
        tracker_->Rotate(rotate_op.degrees);
        break;
      }
      case cc::PaintOpType::Translate: {
        const auto& translate_op = static_cast<const cc::TranslateOp&>(op);
        tracker_->Translate(translate_op.dx, translate_op.dy);
        break;
      }
      case cc::PaintOpType::DrawImage: {
        const auto& image_op = static_cast<const cc::DrawImageOp&>(op);
        if (image_op.image.IsTextureBacked()) {
          converted_op_.emplace<cc::DrawImageOp>(
              MakeUnaccelerated(image_op.image), image_op.left, image_op.top,
              image_op.sampling, &image_op.flags);
          return &absl::get<cc::DrawImageOp>(converted_op_);
        }
        break;
      }
      case cc::PaintOpType::DrawImageRect: {
        const auto& image_op = static_cast<const cc::DrawImageRectOp&>(op);
        if (image_op.image.IsTextureBacked()) {
          converted_op_.emplace<cc::DrawImageRectOp>(
              MakeUnaccelerated(image_op.image), image_op.src, image_op.dst,
              image_op.sampling, &image_op.flags, image_op.constraint);
          return &absl::get<cc::DrawImageRectOp>(converted_op_);
        }
        break;
      }
      default:
        break;
    }
    return &op;
  }

 private:
  PaintPreviewTracker* tracker_;
  absl::variant<absl::monostate, cc::DrawImageOp, cc::DrawImageRectOp>
      converted_op_;
};

}  // namespace

sk_sp<const SkPicture> PaintRecordToSkPicture(
    sk_sp<const cc::PaintRecord> recording,
    PaintPreviewTracker* tracker,
    const gfx::Rect& bounds) {
  // base::Unretained is safe as |tracker| outlives the usage of
  // |custom_callback|.
  cc::PlaybackParams::CustomDataRasterCallback custom_callback =
      base::BindRepeating(&PaintPreviewTracker::CustomDataToSkPictureCallback,
                          base::Unretained(tracker));
  OpConverterAndTracker converter_and_tracker(tracker);
  cc::PlaybackParams::ConvertOpCallback convert_op_callback =
      base::BindRepeating(&OpConverterAndTracker::ConvertAndTrack,
                          base::Unretained(&converter_and_tracker));

  auto skp = ToSkPicture(
      recording, SkRect::MakeWH(bounds.width(), bounds.height()), nullptr,
      std::move(custom_callback), std::move(convert_op_callback));

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
