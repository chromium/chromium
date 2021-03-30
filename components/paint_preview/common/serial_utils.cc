// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/serial_utils.h"

#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "components/paint_preview/common/subset_font.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace paint_preview {

namespace {

// Supported by MSVC, g++, and clang++. Ensures no gaps in packing.
#pragma pack(push, 1)
struct SerializedRectData {
  uint32_t content_id;

  // The size of the subframe in the local coordinates when it was drawn.
  int64_t subframe_width;
  int64_t subframe_height;

  // The rect of the subframe in its parent frame's root coordinate system.
  int64_t transformed_x;
  int64_t transformed_y;
  int64_t transformed_width;
  int64_t transformed_height;
};
#pragma pack(pop)

// Serializes a SkPicture representing a subframe as a custom data placeholder.
sk_sp<SkData> SerializePictureAsRectData(SkPicture* picture, void* ctx) {
  const PictureSerializationContext* context =
      reinterpret_cast<PictureSerializationContext*>(ctx);

  auto it = context->content_id_to_transformed_clip.find(picture->uniqueID());
  // Defers picture serialization behavior to Skia.
  if (it == context->content_id_to_transformed_clip.end())
    return nullptr;

  // This data originates from |PaintPreviewTracker|.
  const SkRect& transformed_cull_rect = it->second;
  SerializedRectData rect_data = {
      picture->uniqueID(),           picture->cullRect().width(),
      picture->cullRect().height(),  transformed_cull_rect.x(),
      transformed_cull_rect.y(),     transformed_cull_rect.width(),
      transformed_cull_rect.height()};
  return SkData::MakeWithCopy(&rect_data, sizeof(rect_data));
}

// De-duplicates and subsets used typefaces and discards any unused typefaces.
sk_sp<SkData> SerializeTypeface(SkTypeface* typeface, void* ctx) {
  TRACE_EVENT0("paint_preview", "SerializeTypeface");
  TypefaceSerializationContext* context =
      reinterpret_cast<TypefaceSerializationContext*>(ctx);

  if (context->finished.count(typeface->uniqueID()))
    return typeface->serialize(SkTypeface::SerializeBehavior::kDontIncludeData);
  context->finished.insert(typeface->uniqueID());

  auto usage_it = context->usage->find(typeface->uniqueID());
  if (usage_it == context->usage->end())
    return typeface->serialize(SkTypeface::SerializeBehavior::kDontIncludeData);

  auto subset_data = SubsetFont(typeface, *usage_it->second);
  // This will fail if the font cannot be subsetted properly. In such cases
  // all typeface data should be added for portability.
  if (!subset_data)
    return typeface->serialize(SkTypeface::SerializeBehavior::kDoIncludeData);
  return subset_data;
}

sk_sp<SkData> SerializeImage(SkImage* image, void* ctx) {
  ImageSerializationContext* context =
      reinterpret_cast<ImageSerializationContext*>(ctx);
  if (context->skip_texture_backed && image->isTextureBacked()) {
    return SkData::MakeEmpty();
  }

  const SkImageInfo& image_info = image->imageInfo();
  // If decoding/encoding the image would result in it exceeding the allowable
  // size, effectively delete it by providing no data.
  if (context->max_representation_size != 0 &&
      image_info.computeMinByteSize() > context->max_representation_size) {
    return SkData::MakeEmpty();
  }

  // If there already exists encoded data use it directly.
  sk_sp<SkData> encoded_data = image->refEncodedData();
  if (!encoded_data) {
    encoded_data = image->encodeToData();
  }

  // If encoding failed then no-op.
  if (!encoded_data)
    return SkData::MakeEmpty();

  // Ensure the encoded data fits in the restrictions if they are present.
  if ((context->remaining_image_size == std::numeric_limits<uint64_t>::max() ||
       context->remaining_image_size >= encoded_data->size()) &&
      (context->max_representation_size == 0 ||
       encoded_data->size() < context->max_representation_size)) {
    if (context->remaining_image_size != std::numeric_limits<uint64_t>::max())
      context->remaining_image_size -= encoded_data->size();

    return encoded_data;
  }

  return SkData::MakeEmpty();
}

// Deserializes a clip rect for a subframe within the main SkPicture. These
// represent subframes and require special decoding as they are custom data
// rather than a valid SkPicture.
// Precondition: the version of the SkPicture should be checked prior to
// invocation to ensure deserialization will succeed.
sk_sp<SkPicture> DeserializePictureAsRectData(const void* data,
                                              size_t length,
                                              void* ctx) {
  SerializedRectData rect_data;
  if (length < sizeof(rect_data))
    return MakeEmptyPicture();
  memcpy(&rect_data, data, sizeof(rect_data));
  auto* context = reinterpret_cast<DeserializationContext*>(ctx);
  context->insert(
      {rect_data.content_id,
       gfx::Rect(rect_data.transformed_x, rect_data.transformed_y,
                 rect_data.transformed_width, rect_data.transformed_height)});
  return MakeEmptyPicture();
}

// Similar to |DeserializePictureAsRectData|, but instead of writing out the
// serialized rect data to |ctx|, |ctx| is instead a
// |LoadedFramesDeserialContext*| that is looked up to return the picture
// itself. This assumes that the picture was already previously deserialized
// and recorded into |ctx|. Returns an empty picture if |ctx| does not contain
// the content ID embedded in |data|.
sk_sp<SkPicture> GetPictureFromDeserialContext(const void* data,
                                               size_t length,
                                               void* ctx) {
  SerializedRectData rect_data;
  if (length < sizeof(rect_data))
    return MakeEmptyPicture();
  memcpy(&rect_data, data, sizeof(rect_data));
  auto* context = reinterpret_cast<LoadedFramesDeserialContext*>(ctx);

  auto it = context->find(rect_data.content_id);
  if (it == context->end())
    return MakeEmptyPicture();

  // Scroll and clip the subframe manually since the picture in |ctx| does not
  // encode this information.
  SkRect subframe_bounds =
      SkRect::MakeWH(rect_data.subframe_width, rect_data.subframe_height);
  SkPictureRecorder recorder;
  SkCanvas* canvas = recorder.beginRecording(subframe_bounds);
  canvas->clipRect(subframe_bounds);
  SkMatrix apply_scroll_offsets = SkMatrix::Translate(
      -it->second.scroll_offsets.width(), -it->second.scroll_offsets.height());
  canvas->drawPicture(it->second.picture, &apply_scroll_offsets, nullptr);
  return recorder.finishRecordingAsPicture();
}

}  // namespace

PictureSerializationContext::PictureSerializationContext() = default;
PictureSerializationContext::~PictureSerializationContext() = default;

PictureSerializationContext::PictureSerializationContext(
    PictureSerializationContext&&) = default;
PictureSerializationContext& PictureSerializationContext::operator=(
    PictureSerializationContext&&) = default;

TypefaceSerializationContext::TypefaceSerializationContext(
    TypefaceUsageMap* usage)
    : usage(usage) {}
TypefaceSerializationContext::~TypefaceSerializationContext() = default;

FrameAndScrollOffsets::FrameAndScrollOffsets() = default;
FrameAndScrollOffsets::~FrameAndScrollOffsets() = default;
FrameAndScrollOffsets::FrameAndScrollOffsets(const FrameAndScrollOffsets&) =
    default;
FrameAndScrollOffsets& FrameAndScrollOffsets::operator=(
    const FrameAndScrollOffsets&) = default;

sk_sp<SkPicture> MakeEmptyPicture() {
  // Effectively a no-op.
  SkPictureRecorder rec;
  rec.beginRecording(1, 1);
  return rec.finishRecordingAsPicture();
}

SkSerialProcs MakeSerialProcs(PictureSerializationContext* picture_ctx,
                              TypefaceSerializationContext* typeface_ctx,
                              ImageSerializationContext* image_ctx) {
  SkSerialProcs procs;
  procs.fPictureProc = SerializePictureAsRectData;
  procs.fPictureCtx = picture_ctx;
  procs.fTypefaceProc = SerializeTypeface;
  procs.fTypefaceCtx = typeface_ctx;

  // TODO(crbug/1008875): find a consistently smaller and low-memory overhead
  // image downsampling method to use as fImageProc.
  //
  // At present this uses the native representation, but skips serializing if
  // loading to a bitmap for encoding might cause an OOM.
  if (image_ctx->max_representation_size > 0 ||
      image_ctx->remaining_image_size != std::numeric_limits<uint64_t>::max()) {
    procs.fImageProc = SerializeImage;
    procs.fImageCtx = image_ctx;
  }
  return procs;
}

SkDeserialProcs MakeDeserialProcs(DeserializationContext* ctx) {
  SkDeserialProcs procs;
  procs.fPictureProc = DeserializePictureAsRectData;
  procs.fPictureCtx = ctx;
  return procs;
}

SkDeserialProcs MakeDeserialProcs(LoadedFramesDeserialContext* ctx) {
  SkDeserialProcs procs;
  procs.fPictureProc = GetPictureFromDeserialContext;
  procs.fPictureCtx = ctx;
  return procs;
}

}  // namespace paint_preview
