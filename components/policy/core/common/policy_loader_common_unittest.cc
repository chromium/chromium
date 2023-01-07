// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_common.h"
#include <algorithm>

#include "base/debug/activity_tracker.h"
#include "base/json/json_reader.h"
#include "base/logging.h"
#include "base/values.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {
namespace {
base::Value ToListValue(const std::vector<std::string>& values) {
  base::Value::List storage;
  storage.reserve(values.size());
  for (const auto& value : values)
    storage.Append(value);

  return base::Value(std::move(storage));
}

base::Value ToDictValue(const std::string& json) {
  absl::optional<base::Value> value = base::JSONReader::Read(json);
  return std::move(value.value());
}

}  // namespace

class SensitivePolicyFilterTest : public ::testing::Test {
 public:
  void AddNewPolicy(const std::string& name, const base::Value& value) {
    policies_.Set(name, PolicyLevel::POLICY_LEVEL_MANDATORY,
                  PolicyScope::POLICY_SCOPE_MACHINE,
                  PolicySource::POLICY_SOURCE_PLATFORM, value.Clone(), nullptr);
  }

  PolicyMap* policies() { return &policies_; }

 private:
  PolicyMap policies_;
};

TEST_F(SensitivePolicyFilterTest, TestSimplePolicyFilter) {
  AddNewPolicy(key::kHomepageLocation, base::Value("https://example.com"));
  AddNewPolicy(key::kBrowserSignin, base::Value(2));

  EXPECT_TRUE(policies()->Get(key::kHomepageLocation));
  EXPECT_TRUE(policies()->Get(key::kBrowserSignin));

  FilterSensitivePolicies(policies());

  EXPECT_FALSE(policies()->Get(key::kHomepageLocation));
  EXPECT_TRUE(policies()->Get(key::kBrowserSignin));
}

TEST_F(SensitivePolicyFilterTest, TestExtensionInstallForceListFilter) {
  base::Value policy = ToListValue(
      {"extension0", "extension1;example.com", "extension2;",
       "extension3;https://clients2.google.com/service/update2/crx"});
  AddNewPolicy(key::kExtensionInstallForcelist, policy);

  EXPECT_TRUE(policies()->Get(key::kExtensionInstallForcelist));

  FilterSensitivePolicies(policies());

  const auto* actual_filtered_policy = policies()->GetValue(
      key::kExtensionInstallForcelist, base::Value::Type::LIST);
  ASSERT_TRUE(actual_filtered_policy);
  base::Value expected_filtered_policy = ToListValue(
      {"extension0", "[BLOCKED]extension1;example.com", "[BLOCKED]extension2;",
       "extension3;https://clients2.google.com/service/update2/crx"});
  EXPECT_EQ(expected_filtered_policy, *actual_filtered_policy);
}

TEST_F(SensitivePolicyFilterTest, TestExtensionSettingsFilter) {
  base::Value policy = ToDictValue(R"({
    "*": {
      "installation_mode": "force_installed",
      "update_url": "https://example.com"
    },
    "extension0": {
      "installation_mode": "blocked",
      "update_url": "https://example.com"
    },
    "extension1": {
      "update_url": "https://example.com"
    },
    "extension2": {
      "installation_mode": "force_installed"
    },
    "extension3": {
      "installation_mode": "force_installed",
      "update_url": "https://clients2.google.com/service/update2/crx"
    },
    "extension4": {
      "installation_mode": "force_installed",
      "update_url": "https://example.com"
    },
    "extension5": {
      "installation_mode": "normal_installed",
      "update_url": "https://example.com"
    },
    "invalid": "settings"
  })");
  AddNewPolicy(key::kExtensionSettings, policy);

  EXPECT_TRUE(policies()->Get(key::kExtensionSettings));

  FilterSensitivePolicies(policies());

  const auto* filtered_policy =
      policies()->GetValue(key::kExtensionSettings, base::Value::Type::DICT);
  ASSERT_TRUE(filtered_policy);
  EXPECT_EQ(policy.DictSize(), filtered_policy->DictSize());
  for (const auto entry : policy.DictItems()) {
    std::string extension = entry.first;
    if (extension == "extension4" || extension == "extension5")
      extension = "[BLOCKED]" + extension;

    const base::Value* filtered_setting = filtered_policy->FindKey(extension);
    ASSERT_TRUE(filtered_setting);
    EXPECT_EQ(entry.second, *filtered_setting)
        << "Mismatch for extension: " + extension;
  }
}

}  // namespace policy
