// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/cast_tracker.h"

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/chrome/stub_devtools_client.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace {

base::Value::Dict CreateSink(const std::string& name, const std::string& id) {
  return base::Value::Dict()
      .Set("name", base::Value(name))
      .Set("id", base::Value(id))
      .Set("session", base::Value("Example session"));
}

class MockDevToolsClient : public StubDevToolsClient {
 public:
  MOCK_METHOD2(SendCommand,
               Status(const std::string& method,
                      const base::Value::Dict& params));
  MOCK_METHOD1(AddListener, void(DevToolsEventListener* listener));
};

}  // namespace

class CastTrackerTest : public testing::Test {
 public:
  void SetUp() override {
    testing::Test::SetUp();
    EXPECT_CALL(devtools_client_, AddListener(_));
    EXPECT_CALL(devtools_client_, SendCommand("Cast.enable", _))
        .WillOnce(Return(Status(kOk)));
    cast_tracker_ = std::make_unique<CastTracker>(&devtools_client_);
  }

 protected:
  std::unique_ptr<CastTracker> cast_tracker_;
  MockDevToolsClient devtools_client_;
};

TEST_F(CastTrackerTest, OnSinksUpdated) {
  const base::Value empty_sinks = base::Value(base::Value::List());
  base::Value::Dict params;
  EXPECT_EQ(0u, cast_tracker_->sinks().GetList().size());

  base::Value::List sinks;
  params.Set("sinks", base::Value::List()
                          .Append(CreateSink("sink1", "1"))
                          .Append(CreateSink("sink2", "2")));
  cast_tracker_->OnEvent(&devtools_client_, "Cast.sinksUpdated", params);
  EXPECT_EQ(2u, cast_tracker_->sinks().GetList().size());

  params.Set("sinks", base::Value(base::Value::Type::LIST));
  cast_tracker_->OnEvent(&devtools_client_, "Cast.sinksUpdated", params);
  EXPECT_EQ(0u, cast_tracker_->sinks().GetList().size());
}

TEST_F(CastTrackerTest, OnIssueUpdated) {
  const std::string issue_message = "There was an issue";
  base::Value::Dict params;
  EXPECT_EQ("", cast_tracker_->issue().GetString());

  params.Set("issueMessage", base::Value(issue_message));
  cast_tracker_->OnEvent(&devtools_client_, "Cast.issueUpdated", params);
  EXPECT_EQ(issue_message, cast_tracker_->issue().GetString());

  params.Set("issueMessage", base::Value(""));
  cast_tracker_->OnEvent(&devtools_client_, "Cast.issueUpdated", params);
  EXPECT_EQ("", cast_tracker_->issue().GetString());
}
