// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/subset_font.h"

// clang-format off
#include <hb.h>
#include <hb-subset.h>
#include <hb-cplusplus.hh>
// clang-format on

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/numerics/safe_conversions.h"
#include "components/crash/core/common/crash_key.h"
#include "skia/ext/font_utils.h"
#include "third_party/skia/include/core/SkFontMgr.h"
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
hb::unique_ptr<hb_blob_t> MakeBlob(sk_sp<SkData> data) {
  if (!data ||
      !base::IsValueInRangeForNumericType<unsigned int, size_t>(data->size()))
    return hb::unique_ptr<hb_blob_t>(nullptr);
  return hb::unique_ptr<hb_blob_t>(
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
  static crash_reporter::CrashKeyString<128> crash_key(
      "PaintPreview-SubsetFont");
  SkString family_name;
  typeface->getFamilyName(&family_name);
  crash_reporter::ScopedCrashKeyString auto_clear(&crash_key,
                                                  family_name.c_str());
  int ttc_index = 0;
  sk_sp<SkData> data = StreamToData(typeface->openStream(&ttc_index));
  hb::unique_ptr<hb_face_t> face(
      hb_face_create(MakeBlob(data).get(), ttc_index));
  hb::unique_ptr<hb_subset_input_t> input(hb_subset_input_create_or_fail());
  if (!face || !input) {
    return nullptr;
  }

  hb_set_t* glyphs =
      hb_subset_input_glyph_set(input.get());  // Owned by |input|.
  usage.ForEach(base::BindRepeating(&AddGlyphs, base::Unretained(glyphs)));
  hb_subset_input_set_flags(input.get(), HB_SUBSET_FLAGS_RETAIN_GIDS);

  // Retain all variation information for OpenType variation fonts. See:
  // https://docs.microsoft.com/en-us/typography/opentype/spec/otvaroverview
  hb_set_t* skip_subset =
      hb_subset_input_set(input.get(), HB_SUBSET_SETS_NO_SUBSET_TABLE_TAG);
  hb_set_add(skip_subset, HB_TAG('a', 'v', 'a', 'r'));
  hb_set_add(skip_subset, HB_TAG('c', 'v', 'a', 'r'));
  hb_set_add(skip_subset, HB_TAG('f', 'v', 'a', 'r'));
  hb_set_add(skip_subset, HB_TAG('M', 'V', 'A', 'R'));
  // Normally harfbuzz would subset these variation tables, but they are needed
  // for variation fonts.
  hb_set_add(skip_subset, HB_TAG('g', 'v', 'a', 'r'));
  hb_set_add(skip_subset, HB_TAG('H', 'V', 'A', 'R'));
  hb_set_add(skip_subset, HB_TAG('V', 'V', 'A', 'R'));

  // Also retain layout information which is important for non-latin characters.
  hb_set_add(skip_subset, HB_TAG('G', 'D', 'E', 'F'));
  hb_set_add(skip_subset, HB_TAG('G', 'S', 'U', 'B'));
  hb_set_add(skip_subset, HB_TAG('G', 'P', 'O', 'S'));

  hb::unique_ptr<hb_face_t> subset_face(
      hb_subset_or_fail(face.get(), input.get()));
  if (!subset_face) {
    return nullptr;
  }
  // Store the correct collection index for the subsetted font.
  const int final_ttc_index = hb_face_get_index(subset_face.get());

  hb::unique_ptr<hb_blob_t> subset_blob(
      hb_face_reference_blob(subset_face.get()));
  if (!subset_blob) {
    return nullptr;
  }

  unsigned int length = 0;
  const char* subset_data = hb_blob_get_data(subset_blob.get(), &length);
  if (!subset_data || !length) {
    return nullptr;
  }

  auto sk_data = SkData::MakeWithProc(
      subset_data, static_cast<size_t>(length),
      [](const void*, void* ctx) { hb_blob_destroy((hb_blob_t*)ctx); },
      subset_blob.release());
  if (!sk_data) {
    return nullptr;
  }

  // Ensure the data is in SkTypeface format so it will deserialize when
  // embedded in an SkPicture. This is *not* a validation/sanitation and the
  // inner workings may vary by platform.
  sk_sp<SkFontMgr> mgr = skia::DefaultFontMgr();
  sk_sp<SkTypeface> sk_subset_typeface =
      mgr->makeFromData(sk_data, final_ttc_index);
  if (!sk_subset_typeface) {
    return nullptr;
  }

  // For fonts with variations we need to force the right variant of SkTypeface
  // post subset.
  const int axis_count = typeface->getVariationDesignPosition(nullptr, 0);
  if (axis_count > 0) {
    std::vector<SkFontArguments::VariationPosition::Coordinate> typeface_axis;
    typeface_axis.resize(axis_count);
    if (typeface->getVariationDesignPosition(typeface_axis.data(),
                                             typeface_axis.size()) > 0) {
      SkFontArguments::VariationPosition variation;
      variation.coordinates = typeface_axis.data();
      variation.coordinateCount = typeface_axis.size();

      SkFontArguments args;
      args.setVariationDesignPosition(variation);
      args.setCollectionIndex(final_ttc_index);

      sk_subset_typeface = sk_subset_typeface->makeClone(args);
    }
  }

  // TODO(crbug.com/40197502): Even after forcing the right variation,
  // `sk_subset_typeface` may have the wrong SkFontStyle as there is no way to
  // manipulate the style while loading the font from data.
  return sk_subset_typeface->serialize(
      SkTypeface::SerializeBehavior::kDoIncludeData);
}

}  // namespace paint_preview
