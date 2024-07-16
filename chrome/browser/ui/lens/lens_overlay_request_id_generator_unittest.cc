// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_request_id_generator.h"

#include <memory>
#include <string>
#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace lens {

class LensOverlayRequestIdGeneratorTest : public testing::Test {};

TEST_F(LensOverlayRequestIdGeneratorTest, ResetRequestId_hasSequenceOne) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  ASSERT_EQ(request_id_generator.GetNextRequestId()->sequence_id(), 1);
  request_id_generator.ResetRequestId();
  ASSERT_EQ(request_id_generator.GetNextRequestId()->sequence_id(), 1);
}

TEST_F(LensOverlayRequestIdGeneratorTest, GetNextRequestId_IncrementsSequence) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  request_id_generator.GetNextRequestId();
  ASSERT_EQ(request_id_generator.GetNextRequestId()->sequence_id(), 2);
}

TEST_F(LensOverlayRequestIdGeneratorTest, ResetRequestId_ChangesAnalyticsId) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId();
  request_id_generator.ResetRequestId();
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId();
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(second_id->sequence_id(), 1);
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextRequestId_DoesNotChangeAnalyticsId) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId();
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId();
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(first_id->analytics_id(), second_id->analytics_id());
}

}  // namespace lens
