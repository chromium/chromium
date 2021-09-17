// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/subset_font.h"

#include "components/paint_preview/common/glyph_usage.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace paint_preview {

TEST(PaintPreviewSubsetFontTest, TestBasicSubset) {
  auto typeface = SkTypeface::MakeDefault();
  ASSERT_NE(typeface, nullptr);
  SparseGlyphUsage sparse(typeface->countGlyphs());
  sparse.Set(0);
  uint16_t glyph_a = typeface->unicharToGlyph('a');
  sparse.Set(glyph_a);
  uint16_t glyph_t = typeface->unicharToGlyph('t');
  sparse.Set(glyph_t);
  auto subset_data = SubsetFont(typeface.get(), sparse);
  ASSERT_NE(subset_data, nullptr);
  SkMemoryStream stream(subset_data);
  auto subset_typeface = SkTypeface::MakeDeserialize(&stream);
  ASSERT_NE(subset_typeface, nullptr);

  // Subsetting doesn't guarantee all glyphs are removed, so just check that the
  // size is smaller and that the requested glyphs still exist and have the same
  // glyphs ids.
  auto origin_data =
      typeface->serialize(SkTypeface::SerializeBehavior::kDoIncludeData);
  EXPECT_LE(subset_data->size(), origin_data->size());
  EXPECT_LE(subset_typeface->countTables(), typeface->countTables());
  EXPECT_LE(subset_typeface->countGlyphs(), typeface->countGlyphs());

  // TODO(ckitagawa): Find a reliable way to check that |glyph_{a, t}| are in
  // the subset_typeface. This is non-trivial.
}

}  // namespace paint_preview
