// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/commander/fuzzy_finder.h"

#include "base/i18n/case_conversion.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace commander {

namespace {
// Convenience function to avoid visual noise from constructing FuzzyFinder
// objects in-test.
double FuzzyFind(const std::u16string& needle,
                 const std::u16string& haystack,
                 std::vector<gfx::Range>* matched_ranges) {
  return FuzzyFinder(needle).Find(haystack, matched_ranges);
}
}  // namespace

TEST(CommanderFuzzyFinder, NonmatchIsZero) {
  std::vector<gfx::Range> ranges;
  EXPECT_EQ(0, FuzzyFind(u"orange", u"orangutan", &ranges));
  EXPECT_TRUE(ranges.empty());
  EXPECT_EQ(0, FuzzyFind(u"elephant", u"orangutan", &ranges));
  EXPECT_TRUE(ranges.empty());
}

TEST(CommanderFuzzyFinder, ExactMatchIsOne) {
  std::vector<gfx::Range> ranges;
  EXPECT_EQ(1, FuzzyFind(u"orange", u"orange", &ranges));
  EXPECT_EQ(ranges, std::vector<gfx::Range>({{0, 6}}));
}

// This ensures coverage for a fast path. Successful match is
// tested in ExactMatchIsOne() above.
TEST(CommanderFuzzyFinder, NeedleHaystackSameLength) {
  std::vector<gfx::Range> ranges;
  EXPECT_EQ(0, FuzzyFind(u"ranges", u"orange", &ranges));
  EXPECT_TRUE(ranges.empty());
}

// This ensures coverage for a fast path (just making sure the path has
// coverage rather than ensuring the path is taken).
TEST(CommanderFuzzyFinder, SingleCharNeedle) {
  std::vector<gfx::Range> ranges;
  FuzzyFinder finder(u"o");

  double prefix_score = finder.Find(u"orange", &ranges);
  EXPECT_EQ(ranges, std::vector<gfx::Range>({{0, 1}}));
  double internal_score = finder.Find(u"phone", &ranges);
  EXPECT_EQ(ranges, std::vector<gfx::Range>({{2, 3}}));
  double boundary_score = finder.Find(u"phone operator", &ranges);
  EXPECT_EQ(ranges, std::vector<gfx::Range>({{6, 7}}));

  // Expected ordering:
  // - Prefix should rank highest.
  // - Word boundary matches that are not the prefix should rank next
  // highest, even if there's an internal match earlier in the haystack.
  // - Internal matches should rank lowest.
  EXPECT_GT(prefix_score, boundary_score);
  EXPECT_GT(boundary_score, internal_score);

  // ...and non-matches should have score = 0.
  EXPECT_EQ(0, finder.Find(u"aquarium", &ranges));
  EXPECT_TRUE(ranges.empty());
}

TEST(CommanderFuzzyFinder, CaseInsensitive) {
  std::vector<gfx::Range> ranges;
  EXPECT_EQ(1, FuzzyFind(u"orange", u"Orange", &ranges));
  EXPECT_EQ(ranges, std::vector<gfx::Range>({{0, 6}}));
}

TEST(CommanderFuzzyFinder, PrefixRanksHigherThanInternal) {
  std::vector<gfx::Range> ranges;
  FuzzyFinder finder(u"orange");
  double prefix_rank = finder.Find(u"Orange juice", &ranges);
  double non_prefix_rank = finder.Find(u"William of Orange", &ranges);

  EXPECT_GT(prefix_rank, 0);
  EXPECT_GT(non_prefix_rank, 0);
  EXPECT_LT(prefix_rank, 1);
  EXPECT_LT(non_prefix_rank, 1);
  EXPECT_GT(prefix_rank, non_prefix_rank);
}

TEST(CommanderFuzzyFinder, NeedleLongerThanHaystack) {
  std::vector<gfx::Range> ranges;
  EXPECT_EQ(0, FuzzyFind(u"orange juice", u"orange", &ranges));
  EXPECT_TRUE(ranges.empty());
}

TEST(CommanderFuzzyFinder, Noncontiguous) {
  std::vector<gfx::Range> ranges;
  EXPECT_GT(FuzzyFind(u"tuot", u"Tlön, Uqbar, Orbis Tertius", &ranges), 0);
  EXPECT_EQ(ranges,
            std::vector<gfx::Range>({{0, 1}, {6, 7}, {13, 14}, {19, 20}}));
}

TEST(CommanderFuzzyFinder, EmptyStringDoesNotMatch) {
  std::vector<gfx::Range> ranges;
  EXPECT_EQ(0, FuzzyFind(u"", u"orange", &ranges));
  EXPECT_TRUE(ranges.empty());
}

}  // namespace commander
