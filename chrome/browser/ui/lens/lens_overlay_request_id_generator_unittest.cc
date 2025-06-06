// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_request_id_generator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/test/scoped_feature_list.h"
#include "components/base32/base32.h"
#include "components/lens/lens_features.h"
#include "lens_overlay_request_id_generator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace lens {

class LensOverlayRequestIdGeneratorTest : public testing::Test {};

TEST_F(LensOverlayRequestIdGeneratorTest, ResetRequestId_resetsSequence) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  ASSERT_EQ(request_id_generator
                .GetNextRequestId(RequestIdUpdateMode::kFullImageRequest)
                ->sequence_id(),
            1);
  request_id_generator.ResetRequestId();
  ASSERT_EQ(request_id_generator
                .GetNextRequestId(RequestIdUpdateMode::kFullImageRequest)
                ->sequence_id(),
            1);
}

TEST_F(
    LensOverlayRequestIdGeneratorTest,
    GetNextIdForInitialRequest_IncrementsSequenceAndImageSequenceAndLongContext) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      lens::features::kLensOverlayContextualSearchbox,
      {{"page-content-request-id-fix", "false"}});

  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInitialRequest);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 1);
  ASSERT_EQ(first_id->long_context_id(), 0);

  // Verify that the initial request id is only generated once.
  EXPECT_DEATH(request_id_generator.GetNextRequestId(
                   RequestIdUpdateMode::kInitialRequest),
               "");
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdForInitialRequest_WithRequestIdFixEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      lens::features::kLensOverlayContextualSearchbox,
      {{"page-content-request-id-fix", "true"}});

  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInitialRequest);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 1);
  ASSERT_EQ(first_id->long_context_id(), 1);

  // Verify that the initial request id is only generated once.
  EXPECT_DEATH(request_id_generator.GetNextRequestId(
                   RequestIdUpdateMode::kInitialRequest),
               "");
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdForFullImageRequest_IncrementsSequenceAndImageSequence) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kFullImageRequest);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 1);
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kFullImageRequest);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->image_sequence_id(), 2);
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
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInteractionRequest);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->image_sequence_id(), 0);
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdForSearchUrl_IncrementsSequenceAndKeepsAnalyticsId) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(RequestIdUpdateMode::kSearchUrl);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 0);
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(RequestIdUpdateMode::kSearchUrl);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->image_sequence_id(), 0);
  ASSERT_EQ(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdForOpenInNewTab_OutputsNewAnalyticsIdButDoesNotStore) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kFullImageRequest);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 1);
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(RequestIdUpdateMode::kOpenInNewTab);
  ASSERT_EQ(second_id->sequence_id(), 1);
  ASSERT_EQ(second_id->image_sequence_id(), 1);
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
  std::unique_ptr<lens::LensOverlayRequestId> third_id =
      request_id_generator.GetNextRequestId(RequestIdUpdateMode::kSearchUrl);
  ASSERT_EQ(third_id->sequence_id(), 2);
  ASSERT_EQ(third_id->image_sequence_id(), 1);
  ASSERT_EQ(first_id->analytics_id(), third_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest, ResetRequestId_ChangesAnalyticsId) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  // Use kSearchUrl to ensure the analytics id is not changed by the
  // GetNextRequestId call.
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(RequestIdUpdateMode::kSearchUrl);
  request_id_generator.ResetRequestId();
  // Use kSearchUrl to ensure the analytics id is not changed by the
  // GetNextRequestId call.
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(RequestIdUpdateMode::kSearchUrl);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(second_id->sequence_id(), 1);
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetBase32EncodedAnalyticsId_GeneratesCorrectString) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInteractionRequest);

  // Decode the encoded analytics ID and ensure it's the correct value.
  std::vector<uint8_t> decoded_analytics_id =
      base32::Base32Decode(request_id_generator.GetBase32EncodedAnalyticsId());
  ASSERT_EQ(base::as_byte_span(request_id->analytics_id()),
            decoded_analytics_id);
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdForPageContentUpdate_IncrementsSequenceAndNewAnalyticsId) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      lens::features::kLensOverlayContextualSearchbox,
      {{"page-content-request-id-fix", "false"}});

  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kPageContentRequest);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 1);
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kPageContentRequest);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->image_sequence_id(), 2);
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(
    LensOverlayRequestIdGeneratorTest,
    GetNextIdForPartialPageContentUpdate_IncrementsSequenceAndSameAnalyticsId) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kPartialPageContentRequest);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 0);
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kPartialPageContentRequest);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->image_sequence_id(), 0);
  ASSERT_EQ(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdForPageContentUpdate_WithRequestIdFixEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      lens::features::kLensOverlayContextualSearchbox,
      {{"page-content-request-id-fix", "true"}});

  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kPageContentRequest);
  ASSERT_EQ(first_id->image_sequence_id(), 0);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->long_context_id(), 1);

  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kPageContentRequest);
  ASSERT_EQ(second_id->image_sequence_id(), 0);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->long_context_id(), 2);
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
}

}  // namespace lens
