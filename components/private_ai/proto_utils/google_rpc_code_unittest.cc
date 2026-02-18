// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/private_ai/proto_utils/google_rpc_code.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace private_ai {
namespace {

TEST(ParseGoogleRpcCodeTest, ValidErrorCode) {
  const std::string reason =
      "[ORIGINAL ERROR] generic::unavailable: Fail to do something";
  EXPECT_EQ(ParseGoogleRpcCode(reason), rpc::GoogleRpcCode::UNAVAILABLE);
}

TEST(ParseGoogleRpcCodeTest, NoGenericPrefix) {
  const std::string reason = "[UNKNOWN] unauthenticated: request unauthorized";
  EXPECT_EQ(ParseGoogleRpcCode(reason), rpc::GoogleRpcCode::UNKNOWN);
}

TEST(ParseGoogleRpcCodeTest, NoColon) {
  const std::string reason =
      "[UNKNOWN] generic::unauthenticated request unauthorized";
  EXPECT_EQ(ParseGoogleRpcCode(reason), rpc::GoogleRpcCode::UNKNOWN);
}

TEST(ParseGoogleRpcCodeTest, InvalidCode) {
  const std::string reason =
      "[UNKNOWN] generic::invalid_code: request unauthorized";
  EXPECT_EQ(ParseGoogleRpcCode(reason), rpc::GoogleRpcCode::UNKNOWN);
}

TEST(ParseGoogleRpcCodeTest, EmptyReason) {
  const std::string reason = "";
  EXPECT_EQ(ParseGoogleRpcCode(reason), rpc::GoogleRpcCode::UNKNOWN);
}

TEST(ParseGoogleRpcCodeTest, OnlyPrefix) {
  const std::string reason = "generic::";
  EXPECT_EQ(ParseGoogleRpcCode(reason), rpc::GoogleRpcCode::UNKNOWN);
}

TEST(ParseGoogleRpcCodeTest, CodeWithoutPrefix) {
  const std::string reason = "generic::OK: success";
  EXPECT_EQ(ParseGoogleRpcCode(reason), rpc::GoogleRpcCode::OK);
}

TEST(ParseGoogleRpcCodeTest, Utf8String) {
  // The parser should correctly extract the ASCII error code from a UTF-8
  // encoded string.
  const std::string reason =
      "エラー generic::unavailable: ネットワークに接続できません";
  EXPECT_EQ(ParseGoogleRpcCode(reason), rpc::GoogleRpcCode::UNAVAILABLE);
}

}  // namespace
}  // namespace private_ai
