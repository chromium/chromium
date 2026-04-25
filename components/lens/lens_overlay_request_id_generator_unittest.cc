// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/lens/lens_overlay_request_id_generator.h"

#include <memory>
#include <string>
#include <vector>

#include "base/base64url.h"
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
                .GetNextRequestId(
                    RequestIdUpdateMode::kFullImageRequest,
                    lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE)
                ->sequence_id(),
            1);
  request_id_generator.ResetRequestId();
  ASSERT_EQ(request_id_generator
                .GetNextRequestId(
                    RequestIdUpdateMode::kFullImageRequest,
                    lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE)
                ->sequence_id(),
            1);
}

TEST_F(LensOverlayRequestIdGeneratorTest, GetNextIdHasTimeUsec) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInitialRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 1);
  ASSERT_EQ(first_id->long_context_id(), 1);
  ASSERT_GT(first_id->time_usec(), 0ul);
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdForInitialRequest_FailsOnSecondCall) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInitialRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 1);
  ASSERT_EQ(first_id->long_context_id(), 1);

#if !BUILDFLAG(IS_IOS)
  // Verify that the initial request id is only generated once.
  EXPECT_DEATH(request_id_generator.GetNextRequestId(
                   RequestIdUpdateMode::kInitialRequest,
                   lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE),
               "");
#endif
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdForFullImageRequest_IncrementsSequenceAndImageSequence) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kFullImageRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 1);
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kFullImageRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->image_sequence_id(), 2);
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdForInteractionRequest_IncrementsSequence) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInteractionRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 0);
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInteractionRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->image_sequence_id(), 0);
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdForSearchUrl_IncrementsSequenceAndKeepsAnalyticsId) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kSearchUrl,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 0);
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kSearchUrl,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->image_sequence_id(), 0);
  ASSERT_EQ(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdForOpenInNewTab_OutputsNewAnalyticsIdButDoesNotStore) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kFullImageRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 1);
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kOpenInNewTab,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(second_id->sequence_id(), 1);
  ASSERT_EQ(second_id->image_sequence_id(), 1);
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
  std::unique_ptr<lens::LensOverlayRequestId> third_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kSearchUrl,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(third_id->sequence_id(), 2);
  ASSERT_EQ(third_id->image_sequence_id(), 1);
  ASSERT_EQ(first_id->analytics_id(), third_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest, ResetRequestId_ChangesAnalyticsId) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  // Use kSearchUrl to ensure the analytics id is not changed by the
  // GetNextRequestId call.
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kSearchUrl,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  request_id_generator.ResetRequestId();
  // Use kSearchUrl to ensure the analytics id is not changed by the
  // GetNextRequestId call.
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kSearchUrl,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(second_id->sequence_id(), 1);
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetBase32EncodedAnalyticsId_GeneratesCorrectString) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInteractionRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);

  // Decode the encoded analytics ID and ensure it's the correct value.
  std::vector<uint8_t> decoded_analytics_id =
      base32::Base32Decode(request_id_generator.GetBase32EncodedAnalyticsId());
  ASSERT_EQ(base::as_byte_span(request_id->analytics_id()),
            decoded_analytics_id);
}

TEST_F(
    LensOverlayRequestIdGeneratorTest,
    GetNextIdForPartialPageContentUpdate_IncrementsSequenceAndSameAnalyticsId) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kPartialPageContentRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 0);
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kPartialPageContentRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->image_sequence_id(), 0);
  ASSERT_EQ(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdForPageContentUpdate_IncremenentsSequenceAndLongContextId) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      lens::features::kLensOverlayContextualSearchbox, {});

  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kPageContentRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(first_id->image_sequence_id(), 0);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->long_context_id(), 1);

  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kPageContentRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(second_id->image_sequence_id(), 0);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->long_context_id(), 2);
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(
    LensOverlayRequestIdGeneratorTest,
    GetNextIdForPageContentWithViewportUpdate_IncremenentsSequenceAndLongContextId) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeatureWithParameters(
      lens::features::kLensOverlayContextualSearchbox, {});

  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kPageContentWithViewportRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(first_id->image_sequence_id(), 1);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->long_context_id(), 1);

  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kPageContentWithViewportRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(second_id->image_sequence_id(), 2);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->long_context_id(), 2);
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
}

TEST_F(
    LensOverlayRequestIdGeneratorTest,
    GetNextIdForMultiContextUploadRequest_SetsSequenceAndImageSequenceAndChangesUuid) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kMultiContextUploadRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(first_id->image_sequence_id(), 1);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->long_context_id(), 0);

  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kMultiContextUploadRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_PDF);
  ASSERT_EQ(second_id->image_sequence_id(), 0);
  ASSERT_EQ(second_id->sequence_id(), 1);
  ASSERT_EQ(second_id->long_context_id(), 1);
  ASSERT_NE(first_id->analytics_id(), second_id->analytics_id());
  ASSERT_NE(first_id->uuid(), second_id->uuid());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextIdWithMediaType_SetsMediaType) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kMultiContextUploadRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(first_id->media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);

  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kMultiContextUploadRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_PDF);
  ASSERT_EQ(second_id->media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_PDF);

  std::unique_ptr<lens::LensOverlayRequestId> third_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kMultiContextUploadRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE);
  ASSERT_EQ(third_id->media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_WEBPAGE_AND_IMAGE);
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       CreateNextRequestIdForUpdate_IncrementsFields) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInitialRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(first_id->sequence_id(), 1);
  ASSERT_EQ(first_id->image_sequence_id(), 1);
  ASSERT_EQ(first_id->long_context_id(), 1);

  std::unique_ptr<lens::LensOverlayRequestId> input_id =
      std::make_unique<lens::LensOverlayRequestId>(*first_id);
  input_id->set_media_type(
      lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.CreateNextRequestIdForUpdate(
          std::move(input_id),
          RequestIdUpdateMode::kPageContentWithViewportRequest);
  ASSERT_EQ(second_id->sequence_id(), 2);
  ASSERT_EQ(second_id->image_sequence_id(), 2);
  ASSERT_EQ(second_id->long_context_id(), 2);
  ASSERT_EQ(second_id->uuid(), first_id->uuid());
  ASSERT_EQ(second_id->context_id(), first_id->context_id());
  ASSERT_NE(second_id->analytics_id(), first_id->analytics_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest, GetNextRequestId_SetsContextId) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInitialRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_NE(first_id->context_id(), 0);

  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kPageContentRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(first_id->context_id(), second_id->context_id());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       SetContextId_SetsContextIdOnNextRequest) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  request_id_generator.SetContextId(12345);
  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInitialRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(request_id->context_id(), 12345);
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       SetHasChromeTabData_SetsHasChromeTabDataOnNextRequest) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  request_id_generator.SetHasChromeTabData(true);
  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInitialRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_TRUE(request_id->has_chrome_tab_data());

  request_id_generator.SetHasChromeTabData(false);
  std::unique_ptr<lens::LensOverlayRequestId> request_id2 =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kPageContentRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_FALSE(request_id2->has_chrome_tab_data());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       SetIsImplicitUpload_SetsIsImplicitUploadOnNextRequest) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  request_id_generator.SetIsImplicitUpload(true);
  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kInitialRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_TRUE(request_id->is_implicit_upload());

  request_id_generator.SetIsImplicitUpload(false);
  std::unique_ptr<lens::LensOverlayRequestId> request_id2 =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kPageContentRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_FALSE(request_id2->is_implicit_upload());
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextRequestIdWithMimeType_SetsMediaTypeAndMimeType) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> request_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kFullImageRequest, "application/pdf",
          lens::LensOverlayRequestId::MEDIA_TYPE_RAW_FILE);
  ASSERT_EQ(request_id->media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_RAW_FILE);
  ASSERT_EQ(request_id->mime_type(), "application/pdf");
}

TEST_F(LensOverlayRequestIdGeneratorTest,
       GetNextRequestIdWithMediaType_ResetsMimeType) {
  lens::LensOverlayRequestIdGenerator request_id_generator;
  std::unique_ptr<lens::LensOverlayRequestId> first_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kFullImageRequest, "application/pdf",
          lens::LensOverlayRequestId::MEDIA_TYPE_RAW_FILE);
  ASSERT_EQ(first_id->media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_RAW_FILE);
  ASSERT_EQ(first_id->mime_type(), "application/pdf");

  std::unique_ptr<lens::LensOverlayRequestId> second_id =
      request_id_generator.GetNextRequestId(
          RequestIdUpdateMode::kFullImageRequest,
          lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_EQ(second_id->media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
  ASSERT_FALSE(second_id->has_mime_type());
}

TEST_F(LensOverlayRequestIdGeneratorTest, ParseRequestId_ValidRequestId) {
  lens::LensOverlayRequestId original_request_id;
  original_request_id.set_uuid(12345);
  original_request_id.set_sequence_id(1);

  std::string serialized_request_id;
  ASSERT_TRUE(original_request_id.SerializeToString(&serialized_request_id));

  std::string encoded_request_id;
  base::Base64UrlEncode(serialized_request_id,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_request_id);

  std::unique_ptr<lens::LensOverlayRequestId> parsed_request_id =
      lens::LensOverlayRequestIdGenerator::ParseRequestId(encoded_request_id);

  ASSERT_NE(parsed_request_id, nullptr);
  EXPECT_EQ(parsed_request_id->uuid(), 12345ULL);
  EXPECT_EQ(parsed_request_id->sequence_id(), 1);
}

TEST_F(LensOverlayRequestIdGeneratorTest, ParseRequestId_InvalidBase64) {
  std::unique_ptr<lens::LensOverlayRequestId> parsed_request_id =
      lens::LensOverlayRequestIdGenerator::ParseRequestId("invalid base64 %$#");

  EXPECT_EQ(parsed_request_id, nullptr);
}

TEST_F(LensOverlayRequestIdGeneratorTest, ParseRequestId_InvalidProto) {
  std::string encoded_request_id;
  base::Base64UrlEncode("not a proto",
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_request_id);

  std::unique_ptr<lens::LensOverlayRequestId> parsed_request_id =
      lens::LensOverlayRequestIdGenerator::ParseRequestId(encoded_request_id);

  EXPECT_EQ(parsed_request_id, nullptr);
}

}  // namespace lens
