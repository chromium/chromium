// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/policy_error_map.h"

#include "base/memory/raw_ptr.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/resource/resource_bundle.h"

namespace policy {
namespace {
constexpr char kPolicyWithError[] = "policy-error";
constexpr char kPolicyWithoutError[] = "policy";
}  // namespace

class PolicyErrorMapTestResourceBundle : public ::testing::TestWithParam<bool> {
 public:
  PolicyErrorMapTestResourceBundle() : has_resource_bundle_(GetParam()) {
    if (!has_resource_bundle_) {
      original_resource_bundle_ =
          ui::ResourceBundle::SwapSharedInstanceForTesting(nullptr);
    }
  }

  void TearDown() override {
    if (!has_resource_bundle_) {
      ui::ResourceBundle::SwapSharedInstanceForTesting(
          original_resource_bundle_);
    }
  }

  bool has_resource_bundle() { return has_resource_bundle_; }

 private:
  bool has_resource_bundle_;
  raw_ptr<ui::ResourceBundle> original_resource_bundle_;
};

TEST_P(PolicyErrorMapTestResourceBundle, CheckForErrorsWithoutFatalErrors) {
  PolicyErrorMap errors;
  ASSERT_EQ(errors.IsReady(), has_resource_bundle());
  errors.AddError(kPolicyWithError, IDS_POLICY_BLOCKED, /*error_path=*/{},
                  PolicyMap::MessageType::kWarning);
  errors.AddError(kPolicyWithError, IDS_POLICY_BLOCKED, /*error_path=*/{},
                  PolicyMap::MessageType::kInfo);

  EXPECT_TRUE(errors.HasError(kPolicyWithError));
  EXPECT_FALSE(errors.HasFatalError(kPolicyWithError));

  EXPECT_FALSE(errors.HasError(kPolicyWithoutError));
  EXPECT_FALSE(errors.HasFatalError(kPolicyWithoutError));
}

TEST_P(PolicyErrorMapTestResourceBundle, CheckForErrorsWithFatalErrors) {
  PolicyErrorMap errors;
  ASSERT_EQ(errors.IsReady(), has_resource_bundle());
  errors.AddError(kPolicyWithError, IDS_POLICY_BLOCKED, /*error_path=*/{},
                  PolicyMap::MessageType::kError);
  errors.AddError(kPolicyWithError, IDS_POLICY_BLOCKED, /*error_path=*/{},
                  PolicyMap::MessageType::kWarning);
  errors.AddError(kPolicyWithError, IDS_POLICY_BLOCKED, /*error_path=*/{},
                  PolicyMap::MessageType::kInfo);

  EXPECT_TRUE(errors.HasError(kPolicyWithError));
  EXPECT_TRUE(errors.HasFatalError(kPolicyWithError));

  EXPECT_FALSE(errors.HasError(kPolicyWithoutError));
  EXPECT_FALSE(errors.HasFatalError(kPolicyWithoutError));
}

INSTANTIATE_TEST_SUITE_P(/* no label */,
                         PolicyErrorMapTestResourceBundle,
                         testing::Bool());

TEST(PolicyErrorMapTest, GetErrorMessages) {
  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_BLOCKED);

  EXPECT_EQ(errors.GetErrorMessages(kPolicyWithError),
            u"This policy is blocked, its value will be ignored.");
}

TEST(PolicyErrorMapTest, GetErrorMessagesWithReplacement) {
  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_TYPE_ERROR, "string");

  EXPECT_EQ(errors.GetErrorMessages(kPolicyWithError),
            u"Expected string value.");
}

TEST(PolicyErrorMapTest, GetErrorMessagesWithTwoReplacements) {
  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_DEPENDENCY_ERROR, "foo",
                  "Enabled");

  EXPECT_EQ(errors.GetErrorMessages(kPolicyWithError),
            u"Ignored because foo is not set to Enabled.");
}

TEST(PolicyErrorMapTest, GetErrorMessagesWithThreeReplacements) {
  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(
      kPolicyWithError, IDS_POLICY_IDLE_TIMEOUT_ACTIONS_DEPENDENCY_ERROR,
      std::vector<std::string>{"SyncDisabled", "Enabled",
                               "clear_browsing_history, clear_bookmarks"});

  EXPECT_EQ(errors.GetErrorMessages(kPolicyWithError),
            u"These actions require the SyncDisabled policy to be set to "
            u"Enabled: clear_browsing_history, clear_bookmarks.");
}

TEST(PolicyErrorMapTest, GetErrorMessagesWithNonAsciiReplacement) {
  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_PROTO_PARSING_ERROR,
                  "\U0001D11E");

  EXPECT_EQ(errors.GetErrorMessages(kPolicyWithError),
            u"Policy parsing error: \U0001D11E");
}

TEST(PolicyErrorMapTest, GetErrorMessagesWithBadUnicodeReplacement) {
  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_PROTO_PARSING_ERROR, "\xff");

  EXPECT_EQ(errors.GetErrorMessages(kPolicyWithError),
            u"Policy parsing error: <invalid Unicode string>");
}

TEST(PolicyErrorMapTest, GetErrorsWithNonfatalError) {
  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_BLOCKED, {},
                  PolicyMap::MessageType::kWarning);

  std::vector<PolicyErrorMap::Data> expected = {PolicyErrorMap::Data{
      .message = u"This policy is blocked, its value will be ignored.",
      .level = PolicyMap::MessageType::kWarning}};
  EXPECT_EQ(errors.GetErrors(kPolicyWithError), expected);
}

TEST(PolicyErrorMapTest, GetErrorsWithFatalError) {
  PolicyErrorMap errors;
  ASSERT_TRUE(errors.IsReady());
  errors.AddError(kPolicyWithError, IDS_POLICY_BLOCKED, {},
                  PolicyMap::MessageType::kError);

  std::vector<PolicyErrorMap::Data> expected = {PolicyErrorMap::Data{
      .message = u"This policy is blocked, its value will be ignored.",
      .level = PolicyMap::MessageType::kError}};
  EXPECT_EQ(errors.GetErrors(kPolicyWithError), expected);
}

}  // namespace policy
