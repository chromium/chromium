// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_request_id_generator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "components/base32/base32.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lens {

class LensOverlayRequestIdGeneratorTest : public testing::Test {};

TEST_F(LensOverlayRequestIdGeneratorTest, ResetRequestId_hasSequenceZero) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  ASSERT_EQ(request_id_generator.GetNextRequestId(RequestIdUpdateMode::kNone)
                ->sequence_id(),
            0);
  request_id_generator.ResetRequestId();
  ASSERT_EQ(request_id_generator.GetNextRequestId(RequestIdUpdateMode::kNone)
                ->sequence_id(),
            0);
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdForFullImageRequest_IncrementsSequenceAndImageSequence) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kFullImageRequest);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 1);
  ASSERT_EQ(first_id->sequence_id(),
            request_id_generator.GetNextRequestId(RequestIdUpdateMode::kNone)
                ->sequence_id());
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kFullImageRequest);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->image_sequence_id(), 2);
  ASSERT_EQ(second_id->sequence_id(),
            request_id_generator.GetNextRequestId(RequestIdUpdateMode::kNone)
                ->sequence_id());
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdForInteractionRequest_IncrementsSequence) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInteractionRequest);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 0);
  ASSERT_EQ(first_id->sequence_id(),
            request_id_generator.GetNextRequestId(RequestIdUpdateMode::kNone)
                ->sequence_id());
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInteractionRequest);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->image_sequence_id(), 0);
  ASSERT_EQ(second_id->sequence_id(),
            request_id_generator.GetNextRequestId(RequestIdUpdateMode::kNone)
                ->sequence_id());
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdForSearchUrl_IncrementsSequenceAndKeepsAnalyticsId) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(RequestIdUpdateMode::kSearchUrl);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 0);
  ASSERT_EQ(first_id->sequence_id(),
            request_id_generator.GetNextRequestId(RequestIdUpdateMode::kNone)
                ->sequence_id());
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(RequestIdUpdateMode::kSearchUrl);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->image_sequence_id(), 0);
  ASSERT_EQ(second_id->sequence_id(),
            request_id_generator.GetNextRequestId(RequestIdUpdateMode::kNone)
                ->sequence_id());
  ASSERT_EQ(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest, ResetRequestId_ChangesAnalyticsId) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(RequestIdUpdateMode::kNone);
  request_id_generator.ResetRequestId();
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(RequestIdUpdateMode::kNone);
  ASSERT_EQ(first_id->sequence_id(), 0);
  ASSERT_EQ(second_id->sequence_id(), 0);
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetBase32EncodedAnalyticsId_GeneratesCorrectString) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      request_id_generator.GetNextRequestId(RequestIdUpdateMode::kNone);

  // Decode the encoded analytics ID and ensure it's the correct value.
  std::vector<uint8_t> decoded_analytics_id =
      base32::Base32Decode(request_id_generator.GetBase32EncodedAnalyticsId());
  ASSERT_EQ(base::as_byte_span(request_id->analytics_id()),
            decoded_analytics_id);
}

}  // namespace lens
