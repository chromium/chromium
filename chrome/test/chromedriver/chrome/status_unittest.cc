// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/chromedriver/chrome/status.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(StatusTest, Ok) {
  Status ok(kOk);
  ASSERT_TRUE(ok.IsOk());
  ASSERT_FALSE(ok.IsError());
  ASSERT_EQ(kOk, ok.code());
  ASSERT_STREQ("ok", ok.message().c_str());
}

TEST(StatusTest, Error) {
  Status error(kUnknownCommand);
  ASSERT_FALSE(error.IsOk());
  ASSERT_TRUE(error.IsError());
  ASSERT_EQ(kUnknownCommand, error.code());
  ASSERT_STREQ("unknown command", error.message().c_str());
}

TEST(StatusTest, ErrorWithDetails) {
  Status error(kUnknownError, "something happened");
  ASSERT_FALSE(error.IsOk());
  ASSERT_TRUE(error.IsError());
  ASSERT_EQ(kUnknownError, error.code());
  ASSERT_STREQ("unknown error: something happened", error.message().c_str());
}

TEST(StatusTest, ErrorWithCause) {
  Status error(
      kUnknownCommand, "quit",
      Status(
          kUnknownError, "something happened",
          Status(kSessionNotCreated)));
  ASSERT_FALSE(error.IsOk());
  ASSERT_TRUE(error.IsError());
  ASSERT_EQ(kUnknownCommand, error.code());
  ASSERT_STREQ(
      "unknown command: quit\n"
      "from unknown error: something happened\n"
      "from session not created",
      error.message().c_str());
}

TEST(StatusTest, AddDetails) {
  Status error(kUnknownError);
  error.AddDetails("details");
  ASSERT_STREQ("unknown error\n  (details)", error.message().c_str());
}
