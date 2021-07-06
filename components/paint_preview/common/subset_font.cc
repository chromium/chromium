// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/subset_font.h"

// clang-format off
#include <hb.h>
#include <hb-subset.h>
// clang-format on

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "third_party/harfbuzz-ng/utils/hb_scoped.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace paint_preview {

namespace {

// Converts and SkStream to an SkData object without copy if possible or
// falls back to a copy.
sk_sp<SkData> StreamToData(std::unique_ptr<SkStreamAsset> stream) {
  DCHECK(stream);
  bool rewind = stream->rewind();
  DCHECK(rewind);
  DCHECK(stream->hasLength());
  size_t size = stream->getLength();
  // TODO: Replace with const SkData* = SkStreamAsset::getData() when Skia
  // adds such a method.
  if (const void* base = stream->getMemoryBase()) {
    SkData::ReleaseProc proc = [](const void*, void* ctx) {
      delete static_cast<SkStreamAsset*>(ctx);
    };
    return SkData::MakeWithProc(base, size, proc, stream.release());
  }
  return SkData::MakeFromStream(stream.get(), size);
}

// Converts SkData to a hb_blob_t.
HbScoped<hb_blob_t> MakeBlob(sk_sp<SkData> data) {
  if (!data ||
      !base::IsValueInRangeForNumericType<unsigned int, size_t>(data->size()))
    return nullptr;
  return HbScoped<hb_blob_t>(
      hb_blob_create(static_cast<const char*>(data->data()),
                     static_cast<unsigned int>(data->size()),
                     HB_MEMORY_MODE_READONLY, nullptr, nullptr));
}

// Adds |glyph_id| to the set of glyphs to be retained.
void AddGlyphs(hb_set_t* glyph_id_set, uint16_t glyph_id) {
  hb_set_add(glyph_id_set, glyph_id);
}

}  // namespace

// Implementation based on SkPDFSubsetFont() using harfbuzz.
sk_sp<SkData> SubsetFont(SkTypeface* typeface, const GlyphUsage& usage) {
  int ttc_index = 0;
  sk_sp<SkData> data = StreamToData(typeface->openStream(&ttc_index));
  HbScoped<hb_face_t> face(hb_face_create(MakeBlob(data).get(), ttc_index));
  HbScoped<hb_subset_input_t> input(hb_subset_input_create_or_fail());
  if (!face || !input)
    return nullptr;

  hb_set_t* glyphs =
      hb_subset_input_glyph_set(input.get());  // Owned by |input|.
  usage.ForEach(base::BindRepeating(&AddGlyphs, base::Unretained(glyphs)));
  hb_subset_input_set_retain_gids(input.get(), true);

  HbScoped<hb_face_t> subset_face(hb_subset(face.get(), input.get()));
  HbScoped<hb_blob_t> subset_blob(hb_face_reference_blob(subset_face.get()));
  if (!subset_blob)
    return nullptr;

  unsigned int length = 0;
  const char* subset_data = hb_blob_get_data(subset_blob.get(), &length);
  if (!subset_data || !length)
    return nullptr;

  auto sk_data = SkData::MakeWithProc(
      subset_data, static_cast<size_t>(length),
      [](const void*, void* ctx) { hb_blob_destroy((hb_blob_t*)ctx); },
      subset_blob.release());
  if (!sk_data)
    return nullptr;

  // Ensure the data is in SkTypeface format so it will deserialize when
  // embedded in an SkPicture. This is *not* a validation/sanitation and the
  // inner workings may vary by platform.
  auto sk_subset_typeface = SkTypeface::MakeFromData(sk_data);
  if (!sk_subset_typeface)
    return nullptr;
  return sk_subset_typeface->serialize(
      SkTypeface::SerializeBehavior::kDoIncludeData);
}

}  // namespace paint_preview
