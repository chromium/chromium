// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/serial_utils.h"

#include "components/paint_preview/common/subset_font.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkPictureRecorder.h"

namespace paint_preview {

namespace {

// Supported by MSVC, g++, and clang++. Ensures no gaps in packing.
#pragma pack(push, 1)
struct SerializedRectData {
  uint32_t content_id;
  int64_t x;
  int64_t y;
  int64_t width;
  int64_t height;
};
#pragma pack(pop)

// Serializes a SkPicture representing a subframe as a custom data placeholder.
sk_sp<SkData> SerializeSubframe(SkPicture* picture, void* ctx) {
  const PictureSerializationContext* context =
      reinterpret_cast<PictureSerializationContext*>(ctx);
  SerializedRectData rect_data = {
      picture->uniqueID(), picture->cullRect().x(), picture->cullRect().y(),
      picture->cullRect().width(), picture->cullRect().height()};

  if (context->count(picture->uniqueID()))
    return SkData::MakeWithCopy(&rect_data, sizeof(rect_data));
  // Defers picture serialization behavior to Skia.
  return nullptr;
}

// De-duplicates and subsets used typefaces and discards any unused typefaces.
sk_sp<SkData> SerializeTypeface(SkTypeface* typeface, void* ctx) {
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

// Deserializies a clip rect for a subframe within the main SkPicture. These
// represent subframes and require special decoding as they are custom data
// rather than a valid SkPicture.
// Precondition: the version of the SkPicture should be checked prior to
// invocation to ensure deserialization will succeed.
sk_sp<SkPicture> DeserializeSubframe(const void* data,
                                     size_t length,
                                     void* ctx) {
  SerializedRectData rect_data;
  if (length < sizeof(rect_data))
    return MakeEmptyPicture();
  memcpy(&rect_data, data, sizeof(rect_data));
  auto* context = reinterpret_cast<DeserializationContext*>(ctx);
  context->insert(
      {rect_data.content_id,
       gfx::Rect(rect_data.x, rect_data.y, rect_data.width, rect_data.height)});
  return MakeEmptyPicture();
}

}  // namespace

TypefaceSerializationContext::TypefaceSerializationContext(
    TypefaceUsageMap* usage)
    : usage(usage) {}
TypefaceSerializationContext::~TypefaceSerializationContext() = default;

sk_sp<SkPicture> MakeEmptyPicture() {
  // Effectively a no-op.
  SkPictureRecorder rec;
  rec.beginRecording(1, 1);
  return rec.finishRecordingAsPicture();
}

SkSerialProcs MakeSerialProcs(PictureSerializationContext* picture_ctx,
                              TypefaceSerializationContext* typeface_ctx) {
  SkSerialProcs procs;
  procs.fPictureProc = SerializeSubframe;
  procs.fPictureCtx = picture_ctx;
  procs.fTypefaceProc = SerializeTypeface;
  procs.fTypefaceCtx = typeface_ctx;
  // TODO(crbug/1008875): find a consistently smaller and low-memory overhead
  // image downsampling method to use as fImageProc.
  return procs;
}

SkDeserialProcs MakeDeserialProcs(DeserializationContext* ctx) {
  SkDeserialProcs procs;
  procs.fPictureProc = DeserializeSubframe;
  procs.fPictureCtx = ctx;
  return procs;
}

}  // namespace paint_preview
