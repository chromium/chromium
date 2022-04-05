// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <memory>
#include <string>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/dom_tracker.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeDevToolsClient : public StubDevToolsClient {
 public:
  FakeDevToolsClient() = default;
  ~FakeDevToolsClient() override = default;

  MOCK_METHOD(Status,
              SendCommand,
              (const std::string&, const base::DictionaryValue&),
              (override));

  MOCK_METHOD(Status,
              SendCommandAndGetResult,
              (const std::string&, const base::DictionaryValue&, base::Value*),
              (override));
};

}  // namespace

TEST(DomTracker, GetFrameIdForNode) {
  FakeDevToolsClient client;
  DomTracker tracker(&client);
  std::string frame_id;
  ASSERT_TRUE(tracker.GetFrameIdForNode(101, &frame_id).IsError());
  ASSERT_TRUE(frame_id.empty());

  const char nodes[] =
      "[{\"nodeId\":100,\"children\":"
      "    [{\"nodeId\":101},"
      "     {\"nodeId\":102,\"frameId\":\"f\"}]"
      "}]";
  base::DictionaryValue params;
  params.GetDict().Set("nodes",
                       std::move(*base::JSONReader::ReadDeprecated(nodes)));
  ASSERT_EQ(kOk, tracker.OnEvent(&client, "DOM.setChildNodes", params).code());
  ASSERT_TRUE(tracker.GetFrameIdForNode(101, &frame_id).IsError());
  ASSERT_TRUE(frame_id.empty());
  ASSERT_TRUE(tracker.GetFrameIdForNode(102, &frame_id).IsOk());
  ASSERT_STREQ("f", frame_id.c_str());

  using ::testing::_;
  EXPECT_CALL(client, SendCommandAndGetResult("DOM.getDocument", _, _))
      .WillOnce([](const std::string& method,
                   const base::DictionaryValue& params, base::Value* result) {
        *result = base::Value(base::Value::Type::DICTIONARY);
        result->SetKey("root", base::Value(base::Value::Type::DICTIONARY));
        return Status(kOk);
      });

  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "DOM.documentUpdated", params).code());
  ASSERT_TRUE(tracker.GetFrameIdForNode(102, &frame_id).IsError());
}

TEST(DomTracker, ChildNodeInserted) {
  FakeDevToolsClient client;
  DomTracker tracker(&client);
  std::string frame_id;

  base::DictionaryValue params;
  params.GetDict().Set(
      "node", std::move(*base::JSONReader::ReadDeprecated("{\"nodeId\":1}")));
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "DOM.childNodeInserted", params).code());
  ASSERT_TRUE(tracker.GetFrameIdForNode(1, &frame_id).IsError());
  ASSERT_TRUE(frame_id.empty());

  params.DictClear();
  params.GetDict().Set("node", std::move(*base::JSONReader::ReadDeprecated(
                                   "{\"nodeId\":2,\"frameId\":\"f\"}")));
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "DOM.childNodeInserted", params).code());
  ASSERT_TRUE(tracker.GetFrameIdForNode(2, &frame_id).IsOk());
  ASSERT_STREQ("f", frame_id.c_str());
}
