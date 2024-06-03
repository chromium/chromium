// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <utility>

#include "base/values.h"
#include "chrome/test/chromedriver/chrome/bidi_tracker.h"
#include "chrome/test/chromedriver/chrome/status.h"
#include "chrome/test/chromedriver/net/timeout.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using testing::ContainsRegex;
using testing::Eq;
using testing::Optional;
using testing::Pointee;

SendBidiPayloadFunc CopyMessageTo(base::Value::Dict& destination) {
  return base::BindRepeating(
      [](base::Value::Dict& dest, base::Value::Dict src) {
        dest = std::move(src);
        return Status{kOk};
      },
      std::ref(destination));
}

Status RejectPayload(base::Value::Dict payload) {
  return Status{kTestError, "rejected"};
}

template <int Code>
testing::AssertionResult StatusCodeIs(const Status& status) {
  if (status.code() == Code) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure() << status.message();
  }
}

testing::AssertionResult StatusOk(const Status& status) {
  return StatusCodeIs<kOk>(status);
}

base::Value::Dict CreateValidParams(std::optional<std::string> channel,
                                    int pong = 1) {
  base::Value::Dict result;
  result.Set("pong", pong);
  base::Value::Dict payload;
  payload.Set("id", 1);
  payload.Set("result", std::move(result));
  if (channel.has_value()) {
    payload.Set("channel", std::move(*channel));
  }

  base::Value::Dict event_params;
  event_params.Set("name", "sendBidiResponse");
  event_params.Set("payload", std::move(payload));
  return event_params;
}

}  // namespace

TEST(BidiTrackerTest, Ctor) {
  BidiTracker tracker;
  EXPECT_FALSE(tracker.ListensToConnections());
  EXPECT_EQ("", tracker.ChannelSuffix());
}

TEST(BidiTrackerTest, SetChannelSuffix) {
  BidiTracker tracker;
  tracker.SetChannelSuffix("/not/used/in/the/tests");
  EXPECT_EQ("/not/used/in/the/tests", tracker.ChannelSuffix());
  tracker.SetChannelSuffix("");
  EXPECT_EQ("", tracker.ChannelSuffix());
}

TEST(BidiTrackerTest, ChannelAndFilter) {
  base::Value::Dict params = CreateValidParams("/some", 222);
  BidiTracker tracker;
  tracker.SetChannelSuffix("/some");
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(CopyMessageTo(actual_payload));
  EXPECT_TRUE(StatusOk(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))));
  EXPECT_THAT(actual_payload.FindString("channel"), Pointee(Eq("/some")));
  EXPECT_THAT(actual_payload.FindIntByDottedPath("result.pong"),
              Optional(Eq(222)));
}

TEST(BidiTrackerTest, ChannelLongerThanFilter) {
  base::Value::Dict params = CreateValidParams("/one/two", 333);
  BidiTracker tracker;
  tracker.SetChannelSuffix("/two");
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(CopyMessageTo(actual_payload));
  EXPECT_TRUE(StatusOk(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))));
  EXPECT_THAT(actual_payload.FindString("channel"), Pointee(Eq("/one/two")));
  EXPECT_THAT(actual_payload.FindIntByDottedPath("result.pong"),
              Optional(Eq(333)));
}

TEST(BidiTrackerTest, ChannelAndFilterAreDifferent) {
  base::Value::Dict params = CreateValidParams("/uno");
  BidiTracker tracker;
  tracker.SetChannelSuffix("/dos");
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(CopyMessageTo(actual_payload));
  EXPECT_TRUE(StatusOk(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))));
  EXPECT_TRUE(actual_payload.empty());
}

TEST(BidiTrackerTest, ChannelAndNoFilter) {
  base::Value::Dict params = CreateValidParams("/some", 321);
  BidiTracker tracker;
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(CopyMessageTo(actual_payload));
  EXPECT_TRUE(StatusOk(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))));
  EXPECT_THAT(actual_payload.FindString("channel"), Pointee(Eq("/some")));
  EXPECT_THAT(actual_payload.FindIntByDottedPath("result.pong"),
              Optional(Eq(321)));
}

TEST(BidiTrackerTest, NoChannelNoFilter) {
  // The infrastructure ensures that that there are no missing or empty channels
  // If such a channel appears in the response we treat it as an error.
  base::Value::Dict params = CreateValidParams("/to-be-removed");
  params.RemoveByDottedPath("payload.channel");
  BidiTracker tracker;
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(CopyMessageTo(actual_payload));
  Status status = tracker.OnEvent(nullptr, "Runtime.bindingCalled", params);
  EXPECT_TRUE(StatusCodeIs<kUnknownError>(status));
  EXPECT_THAT(status.message(), ContainsRegex("channel is missing"));
  EXPECT_TRUE(actual_payload.empty());
}

TEST(BidiTrackerTest, NoChannelAndFilter) {
  // The infrastructure ensures that that there are no missing or empty channels
  // If such a channel appears in the response we treat it as an error.
  base::Value::Dict params = CreateValidParams("/to-be-removed");
  params.RemoveByDottedPath("payload.channel");
  BidiTracker tracker;
  tracker.SetChannelSuffix("/yyy");
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(CopyMessageTo(actual_payload));
  Status status = tracker.OnEvent(nullptr, "Runtime.bindingCalled", params);
  EXPECT_TRUE(StatusCodeIs<kUnknownError>(status));
  EXPECT_THAT(status.message(), ContainsRegex("channel is missing"));
  EXPECT_TRUE(actual_payload.empty());
}

TEST(BidiTrackerTest, EmptyChannelNoFilter) {
  // The infrastructure ensures that that there are no missing or empty channels
  // If such a channel appears in the response we treat it as an error.
  base::Value::Dict params = CreateValidParams("");
  BidiTracker tracker;
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(CopyMessageTo(actual_payload));
  Status status = tracker.OnEvent(nullptr, "Runtime.bindingCalled", params);
  EXPECT_TRUE(StatusCodeIs<kUnknownError>(status));
  EXPECT_THAT(status.message(), ContainsRegex("channel is missing"));
  EXPECT_TRUE(actual_payload.empty());
}

TEST(BidiTrackerTest, EmptyChannelAndFilter) {
  // The infrastructure ensures that that there are no missing or empty channels
  // If such a channel appears in the response we treat it as an error.
  base::Value::Dict params = CreateValidParams("");
  BidiTracker tracker;
  tracker.SetChannelSuffix("/x");
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(CopyMessageTo(actual_payload));
  Status status = tracker.OnEvent(nullptr, "Runtime.bindingCalled", params);
  EXPECT_TRUE(StatusCodeIs<kUnknownError>(status));
  EXPECT_THAT(status.message(), ContainsRegex("channel is missing"));
  EXPECT_TRUE(actual_payload.empty());
}

TEST(BidiTrackerTest, ChannelAndFilterReject) {
  base::Value::Dict params = CreateValidParams("/some");
  BidiTracker tracker;
  tracker.SetChannelSuffix("/some");
  tracker.SetBidiCallback(base::BindRepeating(&RejectPayload));
  EXPECT_TRUE(StatusCodeIs<kTestError>(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))));
}

TEST(BidiTrackerTest, UnexpectedMethod) {
  base::Value::Dict params = CreateValidParams("/one");
  BidiTracker tracker;
  tracker.SetChannelSuffix("/one");
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(CopyMessageTo(actual_payload));
  EXPECT_TRUE(StatusOk(
      tracker.OnEvent(nullptr, "Unexpected.method", std::move(params))));
  EXPECT_TRUE(actual_payload.empty());
}

TEST(BidiTrackerTest, UnexpectedName) {
  base::Value::Dict params = CreateValidParams("/some");
  params.Set("name", "unexpected");
  BidiTracker tracker;
  tracker.SetChannelSuffix("/some");
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(CopyMessageTo(actual_payload));
  EXPECT_TRUE(StatusOk(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))));
  EXPECT_TRUE(actual_payload.empty());
}

TEST(BidiTrackerTest, MissingName) {
  base::Value::Dict params = CreateValidParams("/some");
  params.Remove("name");
  BidiTracker tracker;
  tracker.SetChannelSuffix("/some");
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(CopyMessageTo(actual_payload));
  Status status = tracker.OnEvent(nullptr, "Runtime.bindingCalled", params);
  EXPECT_TRUE(StatusCodeIs<kUnknownError>(status));
  EXPECT_THAT(status.message(), ContainsRegex("missing 'name'"));
  EXPECT_TRUE(actual_payload.empty());
}

TEST(BidiTrackerTest, NoCallback) {
  BidiTracker tracker;
  tracker.SetChannelSuffix("/some");
  base::Value::Dict params = CreateValidParams("/some");
  Status status = tracker.OnEvent(nullptr, "Runtime.bindingCalled", params);
  EXPECT_TRUE(StatusCodeIs<kUnknownError>(status));
  EXPECT_THAT(status.message(), ContainsRegex("no callback"));
}

TEST(BidiTrackerTest, MissingPayload) {
  BidiTracker tracker;
  tracker.SetChannelSuffix("/some");
  base::Value::Dict params = CreateValidParams("/some");
  params.Remove("payload");
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(CopyMessageTo(actual_payload));
  Status status = tracker.OnEvent(nullptr, "Runtime.bindingCalled", params);
  EXPECT_TRUE(StatusCodeIs<kUnknownError>(status));
  EXPECT_THAT(status.message(), ContainsRegex("missing 'payload'"));
  EXPECT_TRUE(actual_payload.empty());
}
