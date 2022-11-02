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

using testing::Eq;
using testing::Pointee;

Status AcceptPayload(base::Value::Dict payload) {
  return Status{kOk};
}

Status AcceptAndSavePayload(base::Value::Dict* dest,
                            base::Value::Dict payload) {
  *dest = std::move(payload);
  return Status{kOk};
}

Status RejectPayload(base::Value::Dict payload) {
  return Status{kUnknownError, "rejected"};
}

testing::AssertionResult StatusOk(const Status& status) {
  if (status.IsOk()) {
    return testing::AssertionSuccess();
  } else {
    return testing::AssertionFailure() << status.message();
  }
}

base::Value::Dict createValidParams(std::string channel) {
  base::Value::Dict payload;
  payload.Set("channel", std::move(channel));
  base::Value::Dict params;
  params.Set("name", "sendBidiResponse");
  params.Set("payload", std::move(payload));
  return params;
}

}  // namespace

TEST(BidiTracker, Ctor) {
  BidiTracker tracker;
  EXPECT_FALSE(tracker.ListensToConnections());
  EXPECT_EQ("", tracker.ChannelSuffix());
}

TEST(BidiTracker, SetChannelSuffix) {
  BidiTracker tracker;
  tracker.SetChannelSuffix("/not/used/in/the/tests");
  EXPECT_EQ("/not/used/in/the/tests", tracker.ChannelSuffix());
}

TEST(BidiTracker, ChannelAndFilter) {
  base::Value::Dict params = createValidParams("/some");
  BidiTracker tracker;
  tracker.SetChannelSuffix("/some");
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(
      base::BindRepeating(&AcceptAndSavePayload, &actual_payload));
  EXPECT_TRUE(StatusOk(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))));
  EXPECT_THAT(actual_payload.FindString("channel"), Pointee(Eq("/some")));
}

TEST(BidiTracker, ChannelLongerThanFilter) {
  base::Value::Dict params = createValidParams("/one/two");
  BidiTracker tracker;
  tracker.SetChannelSuffix("/two");
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(
      base::BindRepeating(&AcceptAndSavePayload, &actual_payload));
  EXPECT_TRUE(StatusOk(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))));
  EXPECT_THAT(actual_payload.FindString("channel"), Pointee(Eq("/one/two")));
}

TEST(BidiTracker, ChannelAndFilterAreDifferent) {
  base::Value::Dict params = createValidParams("/uno");
  BidiTracker tracker;
  tracker.SetChannelSuffix("/dos");
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(
      base::BindRepeating(&AcceptAndSavePayload, &actual_payload));
  EXPECT_TRUE(StatusOk(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))));
  EXPECT_TRUE(actual_payload.empty());
}

TEST(BidiTracker, ChannelAndNoFilter) {
  base::Value::Dict params = createValidParams("/some");
  BidiTracker tracker;
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(
      base::BindRepeating(&AcceptAndSavePayload, &actual_payload));
  EXPECT_TRUE(StatusOk(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))));
  EXPECT_THAT(actual_payload.FindString("channel"), Pointee(Eq("/some")));
}

TEST(BidiTracker, NoChannelNoFilter) {
  // The infrastructure ensures that that there are no missing or empty channels
  // If such a channel appears in the response we treat it as an error.
  base::Value::Dict params;
  params.Set("name", "sendBidiResponse");
  params.Set("payload", base::Value::Dict());
  BidiTracker tracker;
  tracker.SetBidiCallback(base::BindRepeating(&AcceptPayload));
  EXPECT_TRUE(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))
          .IsError());
}

TEST(BidiTracker, NoChannelAndFilter) {
  // The infrastructure ensures that that there are no missing or empty channels
  // If such a channel appears in the response we treat it as an error.
  base::Value::Dict params;
  params.Set("name", "sendBidiResponse");
  params.Set("payload", base::Value::Dict());
  BidiTracker tracker;
  tracker.SetChannelSuffix("/yyy");
  tracker.SetBidiCallback(base::BindRepeating(&AcceptPayload));
  EXPECT_TRUE(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))
          .IsError());
}

TEST(BidiTracker, EmptyChannelNoFilter) {
  // The infrastructure ensures that that there are no missing or empty channels
  // If such a channel appears in the response we treat it as an error.
  base::Value::Dict payload;
  payload.Set("channel", "");
  base::Value::Dict params;
  params.Set("name", "sendBidiResponse");
  params.Set("payload", std::move(payload));
  BidiTracker tracker;
  tracker.SetBidiCallback(base::BindRepeating(&AcceptPayload));
  EXPECT_TRUE(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))
          .IsError());
}

TEST(BidiTracker, EmptyChannelAndFilter) {
  // The infrastructure ensures that that there are no missing or empty channels
  // If such a channel appears in the response we treat it as an error.
  base::Value::Dict payload;
  payload.Set("channel", "");
  base::Value::Dict params;
  params.Set("name", "sendBidiResponse");
  params.Set("payload", std::move(payload));
  BidiTracker tracker;
  tracker.SetChannelSuffix("/x");
  tracker.SetBidiCallback(base::BindRepeating(&AcceptPayload));
  EXPECT_TRUE(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))
          .IsError());
}

TEST(BidiTracker, ChannelAndFilterReject) {
  base::Value::Dict params = createValidParams("/some");
  BidiTracker tracker;
  tracker.SetChannelSuffix("/some");
  tracker.SetBidiCallback(base::BindRepeating(&RejectPayload));
  EXPECT_TRUE(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))
          .IsError());
}

TEST(BidiTracker, UnexpectedMethod) {
  base::Value::Dict params = createValidParams("/one");
  BidiTracker tracker;
  tracker.SetChannelSuffix("/one");
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(
      base::BindRepeating(&AcceptAndSavePayload, &actual_payload));
  EXPECT_TRUE(StatusOk(
      tracker.OnEvent(nullptr, "Unexpected.method", std::move(params))));
  EXPECT_TRUE(actual_payload.empty());
}

TEST(BidiTracker, UnexpectedName) {
  base::Value::Dict params = createValidParams("/some");
  params.Set("name", "unexpected");
  BidiTracker tracker;
  tracker.SetChannelSuffix("/some");
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(
      base::BindRepeating(&AcceptAndSavePayload, &actual_payload));
  EXPECT_TRUE(StatusOk(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))));
  EXPECT_TRUE(actual_payload.empty());
}

TEST(BidiTracker, MissingName) {
  base::Value::Dict params = createValidParams("/some");
  params.Remove("name");
  BidiTracker tracker;
  tracker.SetChannelSuffix("/some");
  base::Value::Dict actual_payload;
  tracker.SetBidiCallback(
      base::BindRepeating(&AcceptAndSavePayload, &actual_payload));
  EXPECT_TRUE(
      tracker.OnEvent(nullptr, "Runtime.bindingCalled", std::move(params))
          .IsError());
  EXPECT_TRUE(actual_payload.empty());
}
