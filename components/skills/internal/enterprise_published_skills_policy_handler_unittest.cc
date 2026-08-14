// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/skills/internal/enterprise_published_skills_policy_handler.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/values.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_value_map.h"
#include "components/skills/public/skills_metrics.h"
#include "components/skills/public/skills_prefs.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace skills {

using policy::POLICY_LEVEL_MANDATORY;
using policy::POLICY_SCOPE_MACHINE;
using policy::POLICY_SOURCE_CLOUD;
using policy::PolicyErrorMap;
using policy::PolicyMap;
using policy::Schema;

namespace {

constexpr char kPolicyName[] = "EnterprisePublishedSkills";

constexpr char kSchemaJson[] = R"(
  {
    "type": "object",
    "properties": {
      "EnterprisePublishedSkills": {
        "type": "array",
        "items": {
          "type": "object",
          "properties": {
            "url": { "type": "string" },
            "hash": { "type": "string" }
          },
          "required": ["url", "hash"]
        }
      }
    }
  }
)";

}  // namespace

class EnterprisePublishedSkillsPolicyHandlerTest : public testing::Test {
 protected:
  void SetUp() override {
    auto schema_result = Schema::Parse(kSchemaJson);
    ASSERT_TRUE(schema_result.has_value()) << schema_result.error();
    schema_ = schema_result.value();
    ASSERT_TRUE(schema_.valid());

    handler_ =
        std::make_unique<EnterprisePublishedSkillsPolicyHandler>(schema_);
  }

  void SetPolicy(base::ListValue list) {
    policies_.Set(kPolicyName, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_CLOUD, base::Value(std::move(list)), nullptr);
  }

  Schema schema_;
  std::unique_ptr<EnterprisePublishedSkillsPolicyHandler> handler_;
  PolicyMap policies_;
  PrefValueMap prefs_;
  PolicyErrorMap errors_;
};

TEST_F(EnterprisePublishedSkillsPolicyHandlerTest, UnderLimit) {
  base::ListValue list;
  for (int i = 0; i < 5; ++i) {
    base::DictValue dict;
    dict.Set("url", "https://example.com/" + base::NumberToString(i));
    dict.Set("hash", "hash");
    list.Append(std::move(dict));
  }
  SetPolicy(std::move(list));

  EXPECT_TRUE(handler_->CheckPolicySettings(policies_, &errors_));
  EXPECT_FALSE(errors_.HasError(kPolicyName));

  handler_->ApplyPolicySettings(policies_, &prefs_);
  const base::Value* value = nullptr;
  EXPECT_TRUE(
      prefs_.GetValue(skills::prefs::kEnterprisePublishedSkills, &value));
  ASSERT_TRUE(value->is_list());
  EXPECT_EQ(5u, value->GetList().size());
}

TEST_F(EnterprisePublishedSkillsPolicyHandlerTest, OverLimit) {
  base::HistogramTester histogram_tester;
  base::ListValue list;
  for (size_t i = 0;
       i < EnterprisePublishedSkillsPolicyHandler::kMaxSkillsLimit + 5; ++i) {
    base::DictValue dict;
    dict.Set("url", "https://example.com/" + base::NumberToString(i));
    dict.Set("hash", "hash");
    list.Append(std::move(dict));
  }
  SetPolicy(std::move(list));

  EXPECT_TRUE(handler_->CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.HasError(kPolicyName));

  auto errors_list = errors_.GetErrors(kPolicyName);
  ASSERT_EQ(1u, errors_list.size());
  EXPECT_EQ(PolicyMap::MessageType::kWarning, errors_list[0].level);

  auto expected_str = l10n_util::GetStringFUTF16(
      IDS_POLICY_URL_ALLOW_BLOCK_LIST_MAX_FILTERS_LIMIT_WARNING,
      base::NumberToString16(
          EnterprisePublishedSkillsPolicyHandler::kMaxSkillsLimit));
  EXPECT_NE(errors_list[0].message.find(expected_str), std::u16string::npos);

  handler_->ApplyPolicySettings(policies_, &prefs_);
  const base::Value* value = nullptr;
  EXPECT_TRUE(
      prefs_.GetValue(skills::prefs::kEnterprisePublishedSkills, &value));
  ASSERT_TRUE(value->is_list());
  EXPECT_EQ(EnterprisePublishedSkillsPolicyHandler::kMaxSkillsLimit,
            value->GetList().size());
  histogram_tester.ExpectUniqueSample(
      "Enterprise.Skills.PolicyError",
      EnterprisePublishedSkillsError::kExceedsLimit, 1);
}

TEST_F(EnterprisePublishedSkillsPolicyHandlerTest, DuplicateUrls) {
  base::HistogramTester histogram_tester;
  base::ListValue list;
  for (int i = 0; i < 5; ++i) {
    base::DictValue dict;
    // Same URL for all entries
    dict.Set("url", "https://example.com");
    dict.Set("hash", "hash");
    list.Append(std::move(dict));
  }
  SetPolicy(std::move(list));

  EXPECT_TRUE(handler_->CheckPolicySettings(policies_, &errors_));
  EXPECT_FALSE(errors_.HasError(kPolicyName));

  handler_->ApplyPolicySettings(policies_, &prefs_);
  const base::Value* value = nullptr;
  EXPECT_TRUE(
      prefs_.GetValue(skills::prefs::kEnterprisePublishedSkills, &value));
  ASSERT_TRUE(value->is_list());
  // The policy handler now filters duplicates before applying to preferences.
  EXPECT_EQ(1u, value->GetList().size());
  histogram_tester.ExpectUniqueSample(
      "Enterprise.Skills.PolicyError",
      EnterprisePublishedSkillsError::kDuplicateUrl, 4);
}

TEST_F(EnterprisePublishedSkillsPolicyHandlerTest, InvalidUrl) {
  base::HistogramTester histogram_tester;
  base::ListValue list;
  base::DictValue dict1;
  dict1.Set("url", "not a url");
  dict1.Set("hash", "hash1");
  list.Append(std::move(dict1));

  base::DictValue dict2;
  dict2.Set("url", "https://example.com");
  dict2.Set("hash", "hash2");
  list.Append(std::move(dict2));

  SetPolicy(std::move(list));

  EXPECT_TRUE(handler_->CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.HasError(kPolicyName));

  auto errors_list = errors_.GetErrors(kPolicyName);
  ASSERT_EQ(1u, errors_list.size());
  EXPECT_EQ(PolicyMap::MessageType::kWarning, errors_list[0].level);

  auto expected_str = l10n_util::GetStringUTF16(IDS_POLICY_INVALID_URL_ERROR);
  EXPECT_NE(errors_list[0].message.find(expected_str), std::u16string::npos);

  handler_->ApplyPolicySettings(policies_, &prefs_);
  const base::Value* value = nullptr;
  EXPECT_TRUE(
      prefs_.GetValue(skills::prefs::kEnterprisePublishedSkills, &value));
  ASSERT_TRUE(value->is_list());
  // The policy handler now filters invalid URLs before applying to preferences.
  EXPECT_EQ(1u, value->GetList().size());
  histogram_tester.ExpectUniqueSample(
      "Enterprise.Skills.PolicyError",
      EnterprisePublishedSkillsError::kInvalidUrl, 1);
}

TEST_F(EnterprisePublishedSkillsPolicyHandlerTest, MissingUrlIgnored) {
  base::ListValue list;
  base::DictValue dict1;
  // No URL
  dict1.Set("hash", "hash1");
  list.Append(std::move(dict1));

  SetPolicy(std::move(list));

  EXPECT_TRUE(handler_->CheckPolicySettings(policies_, &errors_));
  EXPECT_TRUE(errors_.HasError(kPolicyName));

  auto errors_list = errors_.GetErrors(kPolicyName);
  ASSERT_EQ(1u, errors_list.size());
  EXPECT_EQ(PolicyMap::MessageType::kWarning, errors_list[0].level);
  EXPECT_NE(
      errors_list[0].message.find(u"Missing or invalid required property: url"),
      std::u16string::npos);

  handler_->ApplyPolicySettings(policies_, &prefs_);
  const base::Value* value = nullptr;
  EXPECT_FALSE(
      prefs_.GetValue(skills::prefs::kEnterprisePublishedSkills, &value));
}

}  // namespace skills
#endif
