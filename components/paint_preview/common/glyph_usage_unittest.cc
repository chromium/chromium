// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/paint_preview/common/glyph_usage.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace paint_preview {

TEST(PaintPreviewGlyphUsageTest, TestEmpty) {
  DenseGlyphUsage dense;
  EXPECT_EQ(dense.First(), 0U);
  EXPECT_EQ(dense.Last(), 0U);
  EXPECT_FALSE(dense.ShouldSubset());
  dense.Set(0);
  EXPECT_FALSE(dense.IsSet(0));
  dense.Set(5);
  EXPECT_FALSE(dense.IsSet(5));
  dense.Set(60);
  EXPECT_FALSE(dense.IsSet(60));
  size_t counter = 0;
  auto cb = base::BindRepeating(
      [](size_t* counter, uint16_t glyph_id) { ++(*counter); },
      base::Unretained(&counter));
  dense.ForEach(cb);
  EXPECT_EQ(counter, 0U);

  SparseGlyphUsage sparse;
  EXPECT_EQ(sparse.First(), 0U);
  EXPECT_EQ(sparse.Last(), 0U);
  EXPECT_FALSE(sparse.ShouldSubset());
  sparse.Set(0);
  EXPECT_FALSE(sparse.IsSet(0));
  sparse.Set(5);
  EXPECT_FALSE(sparse.IsSet(5));
  sparse.Set(60);
  EXPECT_FALSE(sparse.IsSet(60));
  counter = 0;
  sparse.ForEach(cb);
  EXPECT_EQ(counter, 0U);
}

TEST(PaintPreviewGlyphUsageTest, TestDense) {
  const uint16_t kFirst = 1;
  const uint16_t kNumGlyphs = 70;
  DenseGlyphUsage dense(kNumGlyphs);
  EXPECT_EQ(dense.First(), kFirst);
  EXPECT_EQ(dense.Last(), kNumGlyphs);
  EXPECT_TRUE(dense.ShouldSubset());
  // 0 should be valid even though it is before "first".
  std::vector<uint16_t> test_glyph_ids = {0, 1, 24, 70, 68, 1, 9, 8};
  for (const auto& gid : test_glyph_ids) {
    dense.Set(gid);
    EXPECT_TRUE(dense.IsSet(gid));
  }
  dense.Set(kNumGlyphs + 1);
  EXPECT_FALSE(dense.IsSet(kNumGlyphs + 1));
  dense.Set(kNumGlyphs * 2);
  EXPECT_FALSE(dense.IsSet(kNumGlyphs * 2));
  std::vector<uint16_t> set_gids;
  auto cb = base::BindRepeating(
      [](std::vector<uint16_t>* set_gids, uint16_t glyph_id) {
        set_gids->push_back(glyph_id);
      },
      base::Unretained(&set_gids));
  dense.ForEach(cb);
  EXPECT_THAT(set_gids,
              testing::UnorderedElementsAre(0U, 1U, 24U, 70U, 68U, 9U, 8U));
}

TEST(PaintPreviewGlyphUsageTest, TestSparse) {
  const uint16_t kFirst = 1;
  const uint16_t kNumGlyphs = 70;
  SparseGlyphUsage sparse(kNumGlyphs);
  EXPECT_EQ(sparse.First(), kFirst);
  EXPECT_EQ(sparse.Last(), kNumGlyphs);
  EXPECT_TRUE(sparse.ShouldSubset());
  // 0 should be valid even though it is before "first".
  std::vector<uint16_t> test_glyph_ids = {0, 1, 24, 70, 68, 1, 9, 8};
  for (const auto& gid : test_glyph_ids) {
    sparse.Set(gid);
    EXPECT_TRUE(sparse.IsSet(gid));
  }
  sparse.Set(kNumGlyphs + 1);
  EXPECT_FALSE(sparse.IsSet(kNumGlyphs + 1));
  sparse.Set(kNumGlyphs * 2);
  EXPECT_FALSE(sparse.IsSet(kNumGlyphs * 2));
  std::vector<uint16_t> set_gids;
  auto cb = base::BindRepeating(
      [](std::vector<uint16_t>* set_gids, uint16_t glyph_id) {
        set_gids->push_back(glyph_id);
      },
      base::Unretained(&set_gids));
  sparse.ForEach(cb);
  EXPECT_THAT(set_gids,
              testing::UnorderedElementsAre(0U, 1U, 24U, 70U, 68U, 9U, 8U));
}

}  // namespace paint_preview
