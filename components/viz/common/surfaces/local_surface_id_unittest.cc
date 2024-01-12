// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/viz/common/surfaces/local_surface_id.h"

#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

// Verifying that Local_Surface_Id::ToString() prints its corresponding
// UnguessableToken as ABCD... if logging is not verbose and prints full
// 16-character token otherwise.
TEST(LocalSurfaceIdTest, VerifyToString) {
  const base::UnguessableToken token =
      base::UnguessableToken::CreateForTesting(0x111111, 0);
  const base::UnguessableToken big_token =
      base::UnguessableToken::CreateForTesting(0x123456789, 0xABCABCABC);
  const base::UnguessableToken small_token =
      base::UnguessableToken::CreateForTesting(0, 0x1);
  const viz::LocalSurfaceId local_surface_id(11, 22, token);
  const viz::LocalSurfaceId big_local_surface_id(11, 22, big_token);
  const viz::LocalSurfaceId small_local_surface_id(11, 22, small_token);

  const std::string verbose_expected =
      "LocalSurfaceId(11, 22, " + token.ToString() + ")";
  const std::string brief_expected =
      "LocalSurfaceId(11, 22, " + token.ToString().substr(0, 4) + "...)";

  const std::string big_verbose_expected =
      "LocalSurfaceId(11, 22, " + big_token.ToString() + ")";
  const std::string big_brief_expected =
      "LocalSurfaceId(11, 22, " + big_token.ToString().substr(0, 4) + "...)";

  const std::string small_verbose_expected =
      "LocalSurfaceId(11, 22, " + small_token.ToString() + ")";
  const std::string small_brief_expected =
      "LocalSurfaceId(11, 22, " + small_token.ToString().substr(0, 4) + "...)";

  int previous_log_lvl = logging::GetMinLogLevel();

  // When |g_min_log_level| is set to LOGGING_VERBOSE we expect verbose versions
  // of local_surface_id::ToString().
  logging::SetMinLogLevel(logging::LOGGING_VERBOSE);
  EXPECT_TRUE(VLOG_IS_ON(1));
  EXPECT_EQ(verbose_expected, local_surface_id.ToString());
  EXPECT_EQ(big_verbose_expected, big_local_surface_id.ToString());
  EXPECT_EQ(small_verbose_expected, small_local_surface_id.ToString());

  // When |g_min_log_level| is set to LOGGING_INFO we expect less verbose
  // versions of local_surface_id::ToString().
  logging::SetMinLogLevel(logging::LOGGING_INFO);
  EXPECT_FALSE(VLOG_IS_ON(1));
  EXPECT_EQ(brief_expected, local_surface_id.ToString());
  EXPECT_EQ(big_brief_expected, big_local_surface_id.ToString());
  EXPECT_EQ(small_brief_expected, small_local_surface_id.ToString());

  logging::SetMinLogLevel(previous_log_lvl);
}

// Test that when two allocators have advanced different sequence numbers, that
// IsNewerThan gives prcedence to neither sequence. Instead reporting that
// neither is newer than the other.
TEST(LocalSurfaceIdTest, NewerThan) {
  const base::UnguessableToken token = base::UnguessableToken::Create();
  // Consider a base case of (6, 2). Here `id1` has only the child advancing.
  // While `id2` only the parent advanced.
  const viz::LocalSurfaceId id1(6, 3, token);
  const viz::LocalSurfaceId id2(8, 2, token);
  const viz::LocalSurfaceId id3(8, 3, token);
  // This represents both the parent and child sequence having been advanced in
  // parallel. We do not give precedence to either sequence, and such treat
  // neither as newer than the other.
  EXPECT_FALSE(id1.IsNewerThan(id2));
  EXPECT_FALSE(id2.IsNewerThan(id1));
  // This represents the merged LocalSurfaceId that would be submitted to Viz.
  // It would be newer than both of the separately advanced sequences.
  EXPECT_TRUE(id3.IsNewerThan(id1));
  EXPECT_TRUE(id3.IsNewerThan(id2));
}
