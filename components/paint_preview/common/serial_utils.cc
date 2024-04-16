// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/serial_utils.h"

#include "base/trace_event/common/trace_event_common.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/paint_preview/common/subset_font.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/codec/SkBmpDecoder.h"
#include "third_party/skia/include/codec/SkCodec.h"
#include "third_party/skia/include/codec/SkGifDecoder.h"
#include "third_party/skia/include/codec/SkJpegDecoder.h"
#include "third_party/skia/include/codec/SkPngDecoder.h"
#include "third_party/skia/include/codec/SkWebpDecoder.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkMatrix.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"
#include "third_party/skia/include/core/SkString.h"
#include "third_party/skia/include/core/SkTypeface.h"
#include "third_party/skia/include/encode/SkPngEncoder.h"
#include "third_party/skia/include/private/chromium/Slug.h"

namespace paint_preview {

namespace {

// Supported by MSVC, g++, and clang++. Ensures no gaps in packing.
#pragma pack(push, 1)
struct SerializedRectData {
  uint32_t content_id;

  // The size of the subframe in the local coordinates when it was drawn.
  float subframe_width;
  float subframe_height;

  // The rect of the subframe in its parent frame's root coordinate system.
  float transformed_x;
  float transformed_y;
  float transformed_width;
  float transformed_height;
};
#pragma pack(pop)

// Serializes a SkPicture representing a subframe as a custom data placeholder.
sk_sp<SkData> SerializePictureAsRectData(SkPicture* picture, void* ctx) {
  const PictureSerializationContext* context =
      reinterpret_cast<PictureSerializationContext*>(ctx);

  auto it = context->content_id_to_transformed_clip.find(picture->uniqueID());
  // Defers picture serialization behavior to Skia.
  if (it == context->content_id_to_transformed_clip.end()) {
    return nullptr;
  }

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
// If subsetting fails (or on Android) this returns data only for non-system
// fonts. This means the resulting SkPicture is not portable across devices.
sk_sp<SkData> SerializeTypeface(SkTypeface* typeface, void* ctx) {
  TRACE_EVENT0("paint_preview", "SerializeTypeface");
  TypefaceSerializationContext* context =
      reinterpret_cast<TypefaceSerializationContext*>(ctx);

  if (context->finished.count(typeface->uniqueID())) {
    return typeface->serialize(SkTypeface::SerializeBehavior::kDontIncludeData);
  }
  context->finished.insert(typeface->uniqueID());

  auto usage_it = context->usage->find(typeface->uniqueID());
  if (usage_it == context->usage->end()) {
    return typeface->serialize(SkTypeface::SerializeBehavior::kDontIncludeData);
  }

#if BUILDFLAG(IS_ANDROID)
  {
    SkString familyName;
    typeface->getFamilyName(&familyName);
    // On Android MakeFromName will return nullptr rather than falling back to
    // an alternative font if a system font doesn't match. As such, we can use
    // this to check if the SkTypeface is for a system font. If it is a system
    // font we don't need to subset/serialize it.
    if (skia::MakeTypefaceFromName(familyName.c_str(), typeface->fontStyle())) {
      return typeface->serialize(
          SkTypeface::SerializeBehavior::kIncludeDataIfLocal);
    }
  }
#endif

  auto subset_data = SubsetFont(typeface, *usage_it->second);
  if (!subset_data) {
    return typeface->serialize(
        SkTypeface::SerializeBehavior::kIncludeDataIfLocal);
  }
  return subset_data;
}

static sk_sp<SkTypeface> DeserializeTypeface(const void* data,
                                             size_t length,
                                             void* ctx) {
  // TODO(bungeman,kjlubick) This should not be how the Skia deserial proc
  // works.
  SkStream* stream = *(reinterpret_cast<SkStream**>(const_cast<void*>(data)));
  if (length < sizeof(stream)) {
    return nullptr;
  }
  // The default implementation of SkPicture deserialization of SkTypeface
  // does not use a fallback (system) font manager, but this is necessary
  // on Android due to the above behavior w/r to system fonts. Thus, we
  // call the underlying SkTypeface::MakeDeserialize and pass in the
  // system font manager ourselves.
  return SkTypeface::MakeDeserialize(stream, skia::DefaultFontMgr());
}

static bool is_supported_codec(sk_sp<SkData> data) {
  CHECK(data);
  return SkBmpDecoder::IsBmp(data->data(), data->size()) ||
         SkGifDecoder::IsGif(data->data(), data->size()) ||
         SkPngDecoder::IsPng(data->data(), data->size()) ||
         SkJpegDecoder::IsJpeg(data->data(), data->size()) ||
         SkWebpDecoder::IsWebp(data->data(), data->size());
}

sk_sp<SkData> SerializeImage(SkImage* image, void* ctx) {
  ImageSerializationContext* context =
      reinterpret_cast<ImageSerializationContext*>(ctx);
  // Ignore texture backed content if any slipped through. This shouldn't occur
  // now that ToSkPicture has a dedicated ImageProvider that forces software
  // SkImage inputs, but this is a safeguard.
  if (context->skip_texture_backed && image->isTextureBacked()) {
    return SkData::MakeEmpty();
  }

  // If the decoded form of the image would result in it exceeding the allowable
  // size then effectively delete it by providing no data.
  const SkImageInfo& image_info = image->imageInfo();
  if (context->max_decoded_image_size_bytes !=
          std::numeric_limits<uint64_t>::max() &&
      image_info.computeMinByteSize() > context->max_decoded_image_size_bytes) {
    return SkData::MakeEmpty();
  }

  // If there already exists encoded data use it directly.
  sk_sp<SkData> encoded_data = image->refEncodedData();
  if (!encoded_data || !is_supported_codec(encoded_data)) {
    // Use the default PNG at quality 100 as it is safe.
    // TODO(crbug.com/40177283): Investigate supporting JPEG at quality 100 for
    // opaque images.
    encoded_data = SkPngEncoder::Encode(nullptr, image, {});
  }

  if (!encoded_data) {
    return SkData::MakeEmpty();
  }

  // Ensure the encoded data fits in the size restriction if present.
  // OOM Prevention: This avoids creating/keeping large serialized images
  // in-memory during serialization if the size budget is already exceeded due
  // to images.
  if (context->remaining_image_size != std::numeric_limits<uint64_t>::max()) {
    if (context->remaining_image_size < encoded_data->size()) {
      context->memory_budget_exceeded = true;
      return SkData::MakeEmpty();
    }
    context->remaining_image_size -= encoded_data->size();
  }

  return encoded_data;
}

sk_sp<SkImage> DeserializeImage(const void* bytes, size_t length, void*) {
  // Although we usually serialize images to the PNG format, if an image was
  // already encoded as a JPEG or WEBP, those bytes are written to the
  // SKP as-is, so we should try to decode those as well.
  sk_sp<SkData> data = SkData::MakeWithoutCopy(bytes, length);
  const auto get_image = [](std::unique_ptr<SkCodec> codec) -> sk_sp<SkImage> {
    if (!codec) {
      return nullptr;
    }
    // prefer premul over unpremul (this produces better filtering in general)
    SkImageInfo targetInfo =
        codec->getInfo().makeAlphaType(kPremul_SkAlphaType);
    return std::get<0>(codec->getImage(targetInfo));
  };
  if (SkPngDecoder::IsPng(bytes, length)) {
    return get_image(SkPngDecoder::Decode(data, nullptr));
  }
  if (SkBmpDecoder::IsBmp(bytes, length)) {
    return get_image(SkBmpDecoder::Decode(data, nullptr));
  }
  if (SkGifDecoder::IsGif(bytes, length)) {
    return get_image(SkGifDecoder::Decode(data, nullptr));
  }
  if (SkJpegDecoder::IsJpeg(bytes, length)) {
    return get_image(SkJpegDecoder::Decode(data, nullptr));
  }
  return get_image(SkWebpDecoder::Decode(data, nullptr));
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
  if (length < sizeof(rect_data)) {
    return MakeEmptyPicture();
  }
  memcpy(&rect_data, data, sizeof(rect_data));
  auto* context = reinterpret_cast<DeserializationContext*>(ctx);
  context->insert(
      {rect_data.content_id,
       gfx::RectF(rect_data.transformed_x, rect_data.transformed_y,
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
  if (length < sizeof(rect_data)) {
    return MakeEmptyPicture();
  }
  memcpy(&rect_data, data, sizeof(rect_data));
  auto* context = reinterpret_cast<LoadedFramesDeserialContext*>(ctx);

  auto it = context->find(rect_data.content_id);
  if (it == context->end()) {
    return MakeEmptyPicture();
  }

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
    PictureSerializationContext&&) noexcept = default;
PictureSerializationContext& PictureSerializationContext::operator=(
    PictureSerializationContext&&) noexcept = default;

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

  image_ctx->memory_budget_exceeded = false;
  procs.fImageProc = SerializeImage;
  procs.fImageCtx = image_ctx;
  return procs;
}

SkDeserialProcs MakeDeserialProcs(DeserializationContext* ctx) {
  SkDeserialProcs procs;
  procs.fPictureProc = DeserializePictureAsRectData;
  procs.fPictureCtx = ctx;
  procs.fImageProc = DeserializeImage;
  procs.fTypefaceProc = DeserializeTypeface;
  sktext::gpu::Slug::AddDeserialProcs(&procs, nullptr);
  return procs;
}

SkDeserialProcs MakeDeserialProcs(LoadedFramesDeserialContext* ctx) {
  SkDeserialProcs procs;
  procs.fPictureProc = GetPictureFromDeserialContext;
  procs.fPictureCtx = ctx;
  procs.fImageProc = DeserializeImage;
  procs.fTypefaceProc = DeserializeTypeface;
  return procs;
}

}  // namespace paint_preview
