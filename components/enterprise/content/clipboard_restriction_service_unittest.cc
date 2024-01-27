// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/content/clipboard_restriction_service.h"

#include "components/enterprise/content/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {
const char kTestUrl[] = "https://www.example.com";
const char kTestPatternAllSubdomains[] = "example.com";
const char kTestPatternWildcard[] = "*";
const int kLargeDataSize = 1024;
}  // namespace

class ClipboardRestrictionServiceTest : public testing::Test {
 public:
  ClipboardRestrictionService* service() { return service_.get(); }

 protected:
  void SetPolicy(std::optional<std::vector<std::string>> enable_patterns,
                 std::optional<std::vector<std::string>> disable_patterns,
                 int min_data_size = 100) {
    base::Value::Dict pref_dict;

    if (enable_patterns) {
      base::Value::List enable_list;
      for (auto& p : *enable_patterns) {
        enable_list.Append(std::move(p));
      }
      pref_dict.Set("enable", std::move(enable_list));
    }

    if (disable_patterns) {
      base::Value::List disable_list;
      for (auto& p : *disable_patterns) {
        disable_list.Append(std::move(p));
      }
      pref_dict.Set("disable", std::move(disable_list));
    }

    pref_dict.Set("minimum_data_size", min_data_size);

    pref_service_.SetManagedPref(enterprise::content::kCopyPreventionSettings,
                                 base::Value(std::move(pref_dict)));
  }

  void CreateService() {
    service_ = std::unique_ptr<ClipboardRestrictionService>(
        new ClipboardRestrictionService(&pref_service_));
  }

 private:
  void SetUp() override {
    pref_service_.registry()->RegisterDictionaryPref(
        enterprise::content::kCopyPreventionSettings);
  }

  TestingPrefServiceSimple pref_service_;
  std::unique_ptr<ClipboardRestrictionService> service_;
};

TEST_F(ClipboardRestrictionServiceTest, TestNoPolicySet) {
  CreateService();
  // All copies are allowed if the policy is unset.
  EXPECT_TRUE(service()->IsUrlAllowedToCopy(GURL(kTestUrl), kLargeDataSize));
}

TEST_F(ClipboardRestrictionServiceTest, BlockPatternIfMatchingEnable) {
  CreateService();
  std::vector<std::string> patterns{kTestPatternAllSubdomains};
  std::vector<std::string> empty_patterns{};
  SetPolicy(patterns, empty_patterns);
  EXPECT_FALSE(service()->IsUrlAllowedToCopy(GURL(kTestUrl), kLargeDataSize));
}

TEST_F(ClipboardRestrictionServiceTest, BlockPatternIfMatchingEnableWildcard) {
  CreateService();
  std::vector<std::string> patterns{kTestPatternWildcard};
  std::vector<std::string> empty_patterns{};
  SetPolicy(patterns, empty_patterns);
  EXPECT_FALSE(service()->IsUrlAllowedToCopy(GURL(kTestUrl), kLargeDataSize));
}

TEST_F(ClipboardRestrictionServiceTest, DontBlockPatternIfMatchingDisable) {
  CreateService();
  std::vector<std::string> patterns{kTestPatternWildcard};
  std::vector<std::string> empty_patterns{};
  SetPolicy(empty_patterns, patterns);
  EXPECT_TRUE(service()->IsUrlAllowedToCopy(GURL(kTestUrl), kLargeDataSize));
}

TEST_F(ClipboardRestrictionServiceTest, DontBlockPatternIfMatchingBoth) {
  CreateService();
  // The service's contract is that it blocks any copy for which the URL matches
  // a pattern in the enable list AND doesn't match a pattern in the disable
  // list. On the admin console side, admins only specify "ON by default" or
  // "OFF by default" and a single list of exceptions, and the enable and
  // disable lists are generated from that.
  std::vector<std::string> wildcard_patterns{kTestPatternWildcard};
  std::vector<std::string> subdomain_wildcard_pattern{
      kTestPatternAllSubdomains};
  SetPolicy(wildcard_patterns, subdomain_wildcard_pattern);
  EXPECT_TRUE(service()->IsUrlAllowedToCopy(GURL(kTestUrl), kLargeDataSize));
}

TEST_F(ClipboardRestrictionServiceTest,
       BlockPatternIfMatchingEnableAndDataSizeLargerThanMin) {
  CreateService();
  std::vector<std::string> patterns{kTestPatternAllSubdomains};
  std::vector<std::string> empty_patterns{};
  SetPolicy(patterns, empty_patterns, 10);
  EXPECT_FALSE(service()->IsUrlAllowedToCopy(GURL(kTestUrl), 50));
}

TEST_F(ClipboardRestrictionServiceTest,
       DontBlockPatternIfMatchingEnableAndDataSizeSmallerThanMin) {
  CreateService();
  std::vector<std::string> patterns{kTestPatternAllSubdomains};
  std::vector<std::string> empty_patterns{};
  SetPolicy(patterns, empty_patterns, 50);
  EXPECT_TRUE(service()->IsUrlAllowedToCopy(GURL(kTestUrl), 10));
}

TEST_F(ClipboardRestrictionServiceTest,
       DontBlockPatternIfMatchingEnableAndDataSizeSmallerThanDefaultMin) {
  CreateService();
  std::vector<std::string> patterns{kTestPatternAllSubdomains};
  std::vector<std::string> empty_patterns{};
  SetPolicy(patterns, empty_patterns);
  EXPECT_TRUE(service()->IsUrlAllowedToCopy(GURL(kTestUrl), 10));
}

TEST_F(ClipboardRestrictionServiceTest, ServiceCreatedAfterPrefValueSet) {
  std::vector<std::string> patterns{kTestPatternAllSubdomains};
  std::vector<std::string> empty_patterns{};
  SetPolicy(patterns, empty_patterns, 10);

  CreateService();

  EXPECT_FALSE(service()->IsUrlAllowedToCopy(GURL(kTestUrl), 50));
}

TEST_F(ClipboardRestrictionServiceTest, BlockPatternPopulatesMessage) {
  CreateService();
  std::vector<std::string> patterns{kTestPatternAllSubdomains};
  std::vector<std::string> empty_patterns{};
  SetPolicy(patterns, empty_patterns);
  std::u16string message;
  EXPECT_FALSE(
      service()->IsUrlAllowedToCopy(GURL(kTestUrl), kLargeDataSize, &message));

  EXPECT_FALSE(message.empty());
}