// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_deep_link_payload.h"

#include <optional>

#include "testing/gtest/include/gtest/gtest.h"

namespace signin {
class SigninDeepLinkPayloadTest : public testing::Test {};

TEST_F(SigninDeepLinkPayloadTest, HasAllRequiredFields) {
  const SigninDeepLinkPayload payload = {
      .entry_point_id = ExternalEntryPoint::kDesktopDefault,
      .entry_point_id_raw_value_for_metrics = 1,
      .email = "test@gmail.com"};
  EXPECT_TRUE(payload.HasAllRequiredFields());
}

TEST_F(SigninDeepLinkPayloadTest, MissingEntryPointId) {
  const SigninDeepLinkPayload payload = {
      .entry_point_id = std::nullopt,
      .entry_point_id_raw_value_for_metrics = std::nullopt,
      .email = "test@gmail.com"};
  EXPECT_FALSE(payload.HasAllRequiredFields());
}

TEST_F(SigninDeepLinkPayloadTest, UnknownEntryPointId) {
  const SigninDeepLinkPayload payload = {
      .entry_point_id = ExternalEntryPoint::kUnknown,
      .entry_point_id_raw_value_for_metrics = 1000,
      .email = "test@gmail.com"};
  EXPECT_TRUE(payload.HasAllRequiredFields());
}

TEST_F(SigninDeepLinkPayloadTest, MissingEmail) {
  const SigninDeepLinkPayload payload = {
      .entry_point_id = ExternalEntryPoint::kDesktopDefault,
      .entry_point_id_raw_value_for_metrics = 1,
      .email = std::nullopt};
  EXPECT_FALSE(payload.HasAllRequiredFields());
}

TEST_F(SigninDeepLinkPayloadTest, MissingAllFields) {
  const SigninDeepLinkPayload payload = {};
  EXPECT_FALSE(payload.HasAllRequiredFields());
}

}  // namespace signin
