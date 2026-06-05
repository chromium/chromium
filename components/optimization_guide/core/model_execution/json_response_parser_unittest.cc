// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/optimization_guide/core/model_execution/json_response_parser.h"

#include <optional>

#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/types/expected.h"
#include "base/values.h"
#include "components/optimization_guide/proto/features/compose.pb.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class JsonResponseParserTest : public testing::Test {
 public:
   base::test::TaskEnvironment task_environment_;
};

TEST_F(JsonResponseParserTest, Parse) {
  base::test::TestFuture<ResponseParser::Result> response_future;
  constexpr char proto_type[] =
      "optimization_guide.proto.ComposeResponse";
  JsonResponseParser(proto_type)
      .ParseAsync(
          R"({
        "output": "my output text"
      })",
          response_future.GetCallback());
  auto response = response_future.Get();
  ASSERT_TRUE(response.has_value());
  EXPECT_EQ(
      response->type_url(),
      "type.googleapis.com/optimization_guide.proto.ComposeResponse");
  proto::ComposeResponse resp;
  ASSERT_TRUE(resp.ParseFromString(response->value()));
  EXPECT_EQ(resp.output(), "my output text");
}

}  // namespace optimization_guide
