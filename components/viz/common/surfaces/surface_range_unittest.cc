// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/surface_range.h"
#include "components/viz/common/surfaces/surface_id.h"

#include "testing/gtest/include/gtest/gtest.h"

// Verifies that SurfaceId::IsInRangeExclusive and SurfaceId::IsInRangeInclusive
// works properly.
TEST(SurfaceRangeTest, InRangeTest) {
  viz::FrameSinkId FrameSink1(65564, 0);
  viz::FrameSinkId FrameSink2(65565, 0);
  const base::UnguessableToken token1 = base::UnguessableToken::Create();
  const base::UnguessableToken token2 = base::UnguessableToken::Create();

  const viz::SurfaceId start(FrameSink1, viz::LocalSurfaceId(1, 1, token1));
  const viz::SurfaceId end(FrameSink1, viz::LocalSurfaceId(2, 2, token1));
  const viz::SurfaceId end_token2(FrameSink2,
                                  viz::LocalSurfaceId(2, 2, token2));

  const viz::SurfaceRange surface_range1(start, end);
  const viz::SurfaceRange surface_range2(std::nullopt, end);
  const viz::SurfaceRange surface_range1_token2(start, end_token2);

  const viz::SurfaceId surface_id1(FrameSink1,
                                   viz::LocalSurfaceId(1, 2, token1));
  const viz::SurfaceId surface_id2(FrameSink2,
                                   viz::LocalSurfaceId(1, 2, token2));
  const viz::SurfaceId surface_id3(FrameSink1,
                                   viz::LocalSurfaceId(2, 2, token1));
  const viz::SurfaceId surface_id4(FrameSink1,
                                   viz::LocalSurfaceId(3, 3, token1));

  // |surface_id1| has the right embed token and is inside the range
  // (start,end).
  EXPECT_TRUE(surface_range1.IsInRangeExclusive(surface_id1));

  // |surface_id1| has the right embed token and inside the range
  // (std::nullopt,end).
  EXPECT_TRUE(surface_range2.IsInRangeExclusive(surface_id1));

  // |surface_id2| has an unmatching token.
  EXPECT_FALSE(surface_range1.IsInRangeExclusive(surface_id2));
  EXPECT_FALSE(surface_range2.IsInRangeExclusive(surface_id2));

  // |surface_id2| doesn't match any end point either.
  EXPECT_FALSE(surface_range1.IsInRangeInclusive(surface_id2));
  EXPECT_FALSE(surface_range2.IsInRangeInclusive(surface_id2));

  // |surface_id2| has a matching token to |end_token2|.
  EXPECT_TRUE(surface_range1_token2.IsInRangeExclusive(surface_id2));

  // |surface_id3| is not included when end points are not considered.
  EXPECT_FALSE(surface_range1.IsInRangeExclusive(surface_id3));
  EXPECT_FALSE(surface_range2.IsInRangeExclusive(surface_id3));

  // But |surface_id3| is included when end points are considered.
  EXPECT_TRUE(surface_range1.IsInRangeInclusive(surface_id3));
  EXPECT_TRUE(surface_range2.IsInRangeInclusive(surface_id3));

  // |surface_id4| is not included in the range in all cases.
  EXPECT_FALSE(surface_range1.IsInRangeExclusive(surface_id4));
  EXPECT_FALSE(surface_range2.IsInRangeExclusive(surface_id4));
  EXPECT_FALSE(surface_range1.IsInRangeInclusive(surface_id4));
  EXPECT_FALSE(surface_range2.IsInRangeInclusive(surface_id4));
}

TEST(SurfaceRangeTest, SameFrameSinkDifferentEmbedToken) {
  viz::FrameSinkId frame_sink_id(1, 0);
  const base::UnguessableToken token1 = base::UnguessableToken::Create();
  const base::UnguessableToken token2 = base::UnguessableToken::Create();

  const viz::SurfaceId start(frame_sink_id, viz::LocalSurfaceId(2, 2, token1));
  const viz::SurfaceId end(frame_sink_id, viz::LocalSurfaceId(1, 1, token2));

  const viz::SurfaceRange surface_range(start, end);
  EXPECT_TRUE(surface_range.IsValid());
}
