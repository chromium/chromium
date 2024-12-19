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
#include "components/optimization_guide/proto/features/tab_organization.pb.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/data_decoder/public/cpp/test_support/in_process_data_decoder.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace optimization_guide {

class JsonResponseParserTest : public testing::Test {
 public:
  JsonResponseParserTest() = default;
  ~JsonResponseParserTest() override = default;

  base::test::TaskEnvironment task_environment_;
  data_decoder::test::InProcessDataDecoder in_process_data_decoder_;
};

TEST_F(JsonResponseParserTest, Parse) {
  base::test::TestFuture<ResponseParser::Result> response_future;
  proto::OnDeviceModelExecutionOutputConfig config;
  config.set_proto_type("optimization_guide.proto.TabOrganizationResponse");
  JsonResponseParserFactory().CreateParser(config)->ParseAsync(
      R"({
        "tabGroups": [
          {
            "label": "mylabel",
            "groupId": "someID",
            "tabs": [
              {
                "tabId": 3,
                "url": "someURL"
              },
              {
                "title": "mytitle"
              }
            ]
          }
        ]
      })",
      response_future.GetCallback());
  auto response = response_future.Get();
  EXPECT_TRUE(response.has_value());
  EXPECT_EQ(
      response->type_url(),
      "type.googleapis.com/optimization_guide.proto.TabOrganizationResponse");
  proto::TabOrganizationResponse resp;
  ASSERT_TRUE(resp.ParseFromString(response->value()));
  EXPECT_EQ(resp.tab_groups(0).tabs(1).title(), "mytitle");
  EXPECT_EQ(resp.tab_groups(0).tabs(0).tab_id(), 3);
}

}  // namespace optimization_guide
