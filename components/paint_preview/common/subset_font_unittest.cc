// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/subset_font.h"

#include "base/base_paths.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "components/paint_preview/common/glyph_usage.h"
#include "skia/ext/font_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkData.h"
#include "third_party/skia/include/core/SkFontMgr.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "third_party/skia/include/core/SkStream.h"
#include "third_party/skia/include/core/SkTypeface.h"

namespace paint_preview {

TEST(PaintPreviewSubsetFontTest, TestBasicSubset) {
  auto typeface = skia::DefaultTypeface();
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
  auto subset_typeface =
      SkTypeface::MakeDeserialize(&stream, skia::DefaultFontMgr());
  ASSERT_NE(subset_typeface, nullptr);

  // Subsetting doesn't guarantee all glyphs are removed, so just check that the
  // size is smaller.
  auto origin_data =
      typeface->serialize(SkTypeface::SerializeBehavior::kDoIncludeData);
  EXPECT_LT(subset_data->size(), origin_data->size());
  EXPECT_LE(subset_typeface->countTables(), typeface->countTables());
  EXPECT_LT(subset_typeface->countGlyphs(), typeface->countGlyphs());

  // TODO(ckitagawa): Find a reliable way to check that |glyph_{a, t}| are in
  // the subset_typeface. This is non-trivial.
}

// TODO(crbug.com/40198064): Investigate removing the early exits for
// unsupported variation fonts on at least Linux/Android.
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_ANDROID)

namespace {

constexpr SkFourByteTag kItal = SkSetFourByteTag('i', 't', 'a', 'l');
constexpr SkFourByteTag kWdth = SkSetFourByteTag('w', 'd', 't', 'h');
constexpr SkFourByteTag kWght = SkSetFourByteTag('w', 'g', 'h', 't');

}  // namespace

TEST(PaintPreviewSubsetFontTest, TestVariantSubset) {
  std::vector<SkFontArguments::VariationPosition::Coordinate> axes = {
      {kItal, 1}, {kWdth, 100}, {kWght, 700}};

  // This is a variant font. Loading it from a file isn't entirely
  // straightforward in a platform generic way.
  base::FilePath base_path;
  CHECK(base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &base_path));
  auto final_path = base_path.AppendASCII(
      "components/test/data/paint_preview/Roboto-Regular.ttf");
  std::string data_str;
  ASSERT_TRUE(base::ReadFileToString(final_path, &data_str));
  ASSERT_GT(data_str.size(), 0U);
  auto data = SkData::MakeWithCopy(data_str.data(), data_str.size());
  ASSERT_NE(data, nullptr);
  sk_sp<SkFontMgr> mgr = skia::DefaultFontMgr();
  sk_sp<SkTypeface> base_typeface = mgr->makeFromData(data);
  // Some older OS versions/platforms may not support variation font data.
  if (!base_typeface) {
    return;
  }

  // Select the variant.
  SkFontArguments args;
  SkFontArguments::VariationPosition variations;
  variations.coordinates = axes.data();
  variations.coordinateCount = axes.size();
  args.setVariationDesignPosition(variations);
  auto typeface = base_typeface->makeClone(args);
  // Some older OS versions/platforms may not support variation font data and
  // they fallback. In these cases trying to get variation information may fail.
  if (!typeface || typeface->getVariationDesignPosition(nullptr, 0) != 3) {
    return;
  }

  // Subset.
  SparseGlyphUsage sparse(typeface->countGlyphs());
  sparse.Set(0);
  uint16_t glyph_a = typeface->unicharToGlyph('a');
  sparse.Set(glyph_a);
  uint16_t glyph_t = typeface->unicharToGlyph('t');
  sparse.Set(glyph_t);
  auto subset_data = SubsetFont(typeface.get(), sparse);
  ASSERT_NE(subset_data, nullptr);
  SkMemoryStream stream(subset_data);
  auto subset_typeface =
      SkTypeface::MakeDeserialize(&stream, skia::DefaultFontMgr());
  ASSERT_NE(subset_typeface, nullptr);

  // Ensure the variants are the same before and after.
  auto subset_axes_count =
      subset_typeface->getVariationDesignPosition(nullptr, 0);
  ASSERT_GT(subset_axes_count, 0);
  EXPECT_EQ(static_cast<size_t>(subset_axes_count), axes.size());
  std::vector<SkFontArguments::VariationPosition::Coordinate> subset_axes;
  subset_axes.resize(subset_axes_count);
  ASSERT_GT(subset_typeface->getVariationDesignPosition(subset_axes.data(),
                                                        subset_axes.size()),
            0);
  struct {
    bool operator()(SkFontArguments::VariationPosition::Coordinate a,
                    SkFontArguments::VariationPosition::Coordinate b) {
      return a.axis < b.axis;
    }
  } sort_axes;
  std::sort(axes.begin(), axes.end(), sort_axes);
  std::sort(subset_axes.begin(), subset_axes.end(), sort_axes);
  EXPECT_EQ(axes[0].axis, subset_axes[0].axis);
  EXPECT_EQ(axes[0].value, subset_axes[0].value);
  EXPECT_EQ(axes[1].axis, subset_axes[1].axis);
  EXPECT_EQ(axes[1].value, subset_axes[1].value);
  EXPECT_EQ(axes[2].axis, subset_axes[2].axis);
  EXPECT_EQ(axes[2].value, subset_axes[2].value);

  // Check that something was subsetted.
  auto origin_data =
      typeface->serialize(SkTypeface::SerializeBehavior::kDoIncludeData);
  EXPECT_LT(subset_data->size(), origin_data->size());
  EXPECT_LE(subset_typeface->countTables(), typeface->countTables());
  EXPECT_LT(subset_typeface->countGlyphs(), typeface->countGlyphs());
}
#endif

}  // namespace paint_preview
