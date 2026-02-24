// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/proto_utils/generate_content_response_utils.h"

#include <optional>
#include <string>

#include "components/private_ai/proto/private_ai.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {

namespace {

TEST(ConvertGenerateContentResponseToTextTest, Success) {
  const std::string kExpectedResponseText = "response text";

  proto::GenerateContentResponse response;
  {
    auto* candidate = response.add_candidates();
    auto* content = candidate->mutable_content();
    auto* part = content->add_parts();
    part->set_text(kExpectedResponseText);
  }

  auto result = ConvertGenerateContentResponseToText(response);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), kExpectedResponseText);
}

TEST(ConvertGenerateContentResponseToTextTest, EmptyResponse) {
  proto::GenerateContentResponse response;
  auto result = ConvertGenerateContentResponseToText(response);
  EXPECT_FALSE(result.has_value());
}

TEST(ConvertGenerateContentResponseToTextTest, NoContent) {
  proto::GenerateContentResponse response;
  response.add_candidates();

  auto result = ConvertGenerateContentResponseToText(response);
  EXPECT_FALSE(result.has_value());
}

TEST(ConvertGenerateContentResponseToTextTest, NoParts) {
  proto::GenerateContentResponse response;
  response.add_candidates()->mutable_content();

  auto result = ConvertGenerateContentResponseToText(response);
  EXPECT_FALSE(result.has_value());
}

TEST(ConvertGenerateContentResponseToTextTest, NoText) {
  proto::GenerateContentResponse response;
  response.add_candidates()->mutable_content()->add_parts();

  auto result = ConvertGenerateContentResponseToText(response);
  EXPECT_FALSE(result.has_value());
}

}  // namespace

}  // namespace private_ai
