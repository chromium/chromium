// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/policy_error_map.h"

#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace policy {
namespace {
constexpr char kPolicyWithError[] = "policy-error";
constexpr char kPolicyWithoutError[] = "policy";
}  // namespace

TEST(PolicyErrorMapTest, HasErrorWithoutResource) {
  ui::ResourceBundle* original_resource_bundle =
      ui::ResourceBundle::SwapSharedInstanceForTesting(nullptr);
  PolicyErrorMap errors;
  ASSERT_FALSE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_BLOCKED);

  EXPECT_TRUE(errors.HasError(kPolicyWithError));
  EXPECT_FALSE(errors.HasError(kPolicyWithoutError));
  ui::ResourceBundle::SwapSharedInstanceForTesting(original_resource_bundle);
}

TEST(PolicyErrorMapTest, HasErrorWithResource) {
  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_BLOCKED);

  EXPECT_TRUE(errors.HasError(kPolicyWithError));
  EXPECT_FALSE(errors.HasError(kPolicyWithoutError));
}

TEST(PolicyErrorMapTest, GetErrors) {
  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_BLOCKED);

  EXPECT_EQ(errors.GetErrors(kPolicyWithError),
            u"This policy is blocked, its value will be ignored.");
}

TEST(PolicyErrorMapTest, GetErrorsWithReplacement) {
  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_TYPE_ERROR, "string");

  EXPECT_EQ(errors.GetErrors(kPolicyWithError), u"Expected string value.");
}

TEST(PolicyErrorMapTest, GetErrorsWithTwoReplacements) {
  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_DEPENDENCY_ERROR, "foo",
                  "Enabled");

  EXPECT_EQ(errors.GetErrors(kPolicyWithError),
            u"Ignored because foo is not set to Enabled.");
}

TEST(PolicyErrorMapTest, GetErrorsWithNonAsciiReplacement) {
  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_PROTO_PARSING_ERROR,
                  "\U0001D11E");

  EXPECT_EQ(errors.GetErrors(kPolicyWithError),
            u"Policy parsing error: \U0001D11E");
}

TEST(PolicyErrorMapTest, GetErrorsWithBadUnicodeReplacement) {
  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_PROTO_PARSING_ERROR, "\xff");

  EXPECT_EQ(errors.GetErrors(kPolicyWithError),
            u"Policy parsing error: <invalid Unicode string>");
}

}  // namespace policy
