// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/policy_error_map.h"

#include "components/strings/grit/components_strings.h"
#include "ui/base/resource/resource_bundle.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {
constexpr char kPolicyWithError[] = "policy-error";
constexpr char kPolicyWithoutError[] = "policy";
}  // namespace

using PolicyErrorMapTest = ::testing::Test;

TEST_F(PolicyErrorMapTest, HasErrorWithoutResource) {
  ui::ResourceBundle* original_resource_bundle =
      ui::ResourceBundle::SwapSharedInstanceForTesting(nullptr);
  PolicyErrorMap errors;
  ASSERT_FALSE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_BLOCKED);

  EXPECT_TRUE(errors.HasError(kPolicyWithError));
  EXPECT_FALSE(errors.HasError(kPolicyWithoutError));
  ui::ResourceBundle::SwapSharedInstanceForTesting(original_resource_bundle);
}
TEST_F(PolicyErrorMapTest, HasErrorWithResource) {
  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_BLOCKED);

  EXPECT_TRUE(errors.HasError(kPolicyWithError));
  EXPECT_FALSE(errors.HasError(kPolicyWithoutError));
}

}  // namespace policy
