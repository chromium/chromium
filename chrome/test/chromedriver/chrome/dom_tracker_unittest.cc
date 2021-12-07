// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <list>
#include <string>

#include "base/json/json_reader.h"
#include "base/values.h"
#include "chrome/test/chromedriver/chrome/dom_tracker.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class FakeDevToolsClient : public StubDevToolsClient {
 public:
  FakeDevToolsClient() {}
  ~FakeDevToolsClient() override {}

  std::string PopSentCommand() {
    std::string command;
    if (!sent_command_queue_.empty()) {
      command = sent_command_queue_.front();
      sent_command_queue_.pop_front();
    }
    return command;
  }

  // Overridden from DevToolsClient:
  Status SendCommand(const std::string& method,
                     const base::DictionaryValue& params) override {
    sent_command_queue_.push_back(method);
    return Status(kOk);
  }
  Status SendCommandAndGetResult(
      const std::string& method,
      const base::DictionaryValue& params,
      std::unique_ptr<base::DictionaryValue>* result) override {
    return SendCommand(method, params);
  }

 private:
  std::list<std::string> sent_command_queue_;
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
  params.Set("nodes", base::JSONReader::ReadDeprecated(nodes));
  ASSERT_EQ(kOk, tracker.OnEvent(&client, "DOM.setChildNodes", params).code());
  ASSERT_TRUE(tracker.GetFrameIdForNode(101, &frame_id).IsError());
  ASSERT_TRUE(frame_id.empty());
  ASSERT_TRUE(tracker.GetFrameIdForNode(102, &frame_id).IsOk());
  ASSERT_STREQ("f", frame_id.c_str());

  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "DOM.documentUpdated", params).code());
  ASSERT_TRUE(tracker.GetFrameIdForNode(102, &frame_id).IsError());
  ASSERT_STREQ("DOM.getDocument", client.PopSentCommand().c_str());
}

TEST(DomTracker, ChildNodeInserted) {
  FakeDevToolsClient client;
  DomTracker tracker(&client);
  std::string frame_id;

  base::DictionaryValue params;
  params.Set("node", base::JSONReader::ReadDeprecated("{\"nodeId\":1}"));
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "DOM.childNodeInserted", params).code());
  ASSERT_TRUE(tracker.GetFrameIdForNode(1, &frame_id).IsError());
  ASSERT_TRUE(frame_id.empty());

  params.DictClear();
  params.Set("node", base::JSONReader::ReadDeprecated(
                         "{\"nodeId\":2,\"frameId\":\"f\"}"));
  ASSERT_EQ(kOk,
            tracker.OnEvent(&client, "DOM.childNodeInserted", params).code());
  ASSERT_TRUE(tracker.GetFrameIdForNode(2, &frame_id).IsOk());
  ASSERT_STREQ("f", frame_id.c_str());
}
