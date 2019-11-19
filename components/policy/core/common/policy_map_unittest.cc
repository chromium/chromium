// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_map.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/policy/core/common/external_data_manager.h"
#include "components/policy/core/common/policy_merger.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/policy_constants.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Dummy policy names.
const char kTestPolicyName1[] = "policy.test.1";
const char kTestPolicyName2[] = "policy.test.2";
const char kTestPolicyName3[] = "policy.test.3";
const char kTestPolicyName4[] = "policy.test.4";
const char kTestPolicyName5[] = "policy.test.5";
const char kTestPolicyName6[] = "policy.test.6";
const char kTestPolicyName7[] = "policy.test.7";
const char kTestPolicyName8[] = "policy.test.8";

// Dummy error message.
const char kTestError[] = "Test error message";

// Utility functions for the tests.
void SetPolicy(PolicyMap* map,
               const char* name,
               std::unique_ptr<base::Value> value) {
  map->Set(name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
           std::move(value), nullptr);
}

void SetPolicy(PolicyMap* map,
               const char* name,
               std::unique_ptr<ExternalDataFetcher> external_data_fetcher) {
  map->Set(name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
           nullptr, std::move(external_data_fetcher));
}

template <class T>
std::vector<base::Value> GetListStorage(const std::vector<T> entry) {
  std::vector<base::Value> result;
  for (const auto& it : entry)
    result.emplace_back(base::Value(it));
  return result;
}

}  // namespace

class PolicyMapTest : public testing::Test {
 protected:
  std::unique_ptr<ExternalDataFetcher> CreateExternalDataFetcher(
      const std::string& policy) const;
};

std::unique_ptr<ExternalDataFetcher> PolicyMapTest::CreateExternalDataFetcher(
    const std::string& policy) const {
  return std::make_unique<ExternalDataFetcher>(
      base::WeakPtr<ExternalDataManager>(), policy);
}

TEST_F(PolicyMapTest, SetAndGet) {
  PolicyMap map;
  SetPolicy(&map, kTestPolicyName1, std::make_unique<base::Value>("aaa"));
  base::Value expected("aaa");
  EXPECT_TRUE(expected.Equals(map.GetValue(kTestPolicyName1)));
  SetPolicy(&map, kTestPolicyName1, std::make_unique<base::Value>("bbb"));
  base::Value expected_b("bbb");
  EXPECT_TRUE(expected_b.Equals(map.GetValue(kTestPolicyName1)));
  SetPolicy(&map, kTestPolicyName1, CreateExternalDataFetcher("dummy"));
  map.AddError(kTestPolicyName1, kTestError);
  EXPECT_FALSE(map.GetValue(kTestPolicyName1));
  const PolicyMap::Entry* entry = map.Get(kTestPolicyName1);
  ASSERT_TRUE(entry != nullptr);
  EXPECT_EQ(POLICY_LEVEL_MANDATORY, entry->level);
  EXPECT_EQ(POLICY_SCOPE_USER, entry->scope);
  EXPECT_EQ(POLICY_SOURCE_CLOUD, entry->source);
  PolicyMap::Entry::L10nLookupFunction lookup = base::BindRepeating(
      static_cast<base::string16 (*)(int)>(&base::NumberToString16));
  EXPECT_EQ(base::UTF8ToUTF16(kTestError), entry->GetLocalizedErrors(lookup));
  EXPECT_TRUE(
      ExternalDataFetcher::Equals(entry->external_data_fetcher.get(),
                                  CreateExternalDataFetcher("dummy").get()));
  map.Set(kTestPolicyName1, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_ENTERPRISE_DEFAULT, nullptr, nullptr);
  EXPECT_FALSE(map.GetValue(kTestPolicyName1));
  entry = map.Get(kTestPolicyName1);
  ASSERT_TRUE(entry != nullptr);
  EXPECT_EQ(POLICY_LEVEL_RECOMMENDED, entry->level);
  EXPECT_EQ(POLICY_SCOPE_MACHINE, entry->scope);
  EXPECT_EQ(POLICY_SOURCE_ENTERPRISE_DEFAULT, entry->source);
  EXPECT_EQ(base::string16(), entry->GetLocalizedErrors(lookup));
  EXPECT_FALSE(entry->external_data_fetcher);
}

TEST_F(PolicyMapTest, AddError) {
  PolicyMap map;
  SetPolicy(&map, kTestPolicyName1, std::make_unique<base::Value>(0));
  PolicyMap::Entry* entry = map.GetMutable(kTestPolicyName1);
  PolicyMap::Entry::L10nLookupFunction lookup = base::BindRepeating(
      static_cast<base::string16 (*)(int)>(&base::NumberToString16));
  EXPECT_EQ(base::string16(), entry->GetLocalizedErrors(lookup));
  map.AddError(kTestPolicyName1, 1234);
  EXPECT_EQ(base::UTF8ToUTF16("1234"), entry->GetLocalizedErrors(lookup));
  map.AddError(kTestPolicyName1, 5678);
  EXPECT_EQ(base::UTF8ToUTF16("1234\n5678"), entry->GetLocalizedErrors(lookup));
  map.AddError(kTestPolicyName1, "abcd");
  EXPECT_EQ(base::UTF8ToUTF16("abcd\n1234\n5678"),
            entry->GetLocalizedErrors(lookup));
}

TEST_F(PolicyMapTest, Equals) {
  PolicyMap a;
  SetPolicy(&a, kTestPolicyName1, std::make_unique<base::Value>("aaa"));
  PolicyMap a2;
  SetPolicy(&a2, kTestPolicyName1, std::make_unique<base::Value>("aaa"));
  PolicyMap b;
  SetPolicy(&b, kTestPolicyName1, std::make_unique<base::Value>("bbb"));
  PolicyMap c;
  SetPolicy(&c, kTestPolicyName1, std::make_unique<base::Value>("aaa"));
  SetPolicy(&c, kTestPolicyName2, std::make_unique<base::Value>(true));
  PolicyMap d;
  SetPolicy(&d, kTestPolicyName1, CreateExternalDataFetcher("ddd"));
  PolicyMap d2;
  SetPolicy(&d2, kTestPolicyName1, CreateExternalDataFetcher("ddd"));
  PolicyMap e;
  SetPolicy(&e, kTestPolicyName1, CreateExternalDataFetcher("eee"));
  EXPECT_FALSE(a.Equals(b));
  EXPECT_FALSE(a.Equals(c));
  EXPECT_FALSE(a.Equals(d));
  EXPECT_FALSE(a.Equals(e));
  EXPECT_FALSE(b.Equals(a));
  EXPECT_FALSE(b.Equals(c));
  EXPECT_FALSE(b.Equals(d));
  EXPECT_FALSE(b.Equals(e));
  EXPECT_FALSE(c.Equals(a));
  EXPECT_FALSE(c.Equals(b));
  EXPECT_FALSE(c.Equals(d));
  EXPECT_FALSE(c.Equals(e));
  EXPECT_FALSE(d.Equals(a));
  EXPECT_FALSE(d.Equals(b));
  EXPECT_FALSE(d.Equals(c));
  EXPECT_FALSE(d.Equals(e));
  EXPECT_FALSE(e.Equals(a));
  EXPECT_FALSE(e.Equals(b));
  EXPECT_FALSE(e.Equals(c));
  EXPECT_FALSE(e.Equals(d));
  EXPECT_TRUE(a.Equals(a2));
  EXPECT_TRUE(a2.Equals(a));
  EXPECT_TRUE(d.Equals(d2));
  EXPECT_TRUE(d2.Equals(d));
  PolicyMap empty1;
  PolicyMap empty2;
  EXPECT_TRUE(empty1.Equals(empty2));
  EXPECT_TRUE(empty2.Equals(empty1));
  EXPECT_FALSE(empty1.Equals(a));
  EXPECT_FALSE(a.Equals(empty1));
}

TEST_F(PolicyMapTest, Swap) {
  PolicyMap a;
  SetPolicy(&a, kTestPolicyName1, std::make_unique<base::Value>("aaa"));
  SetPolicy(&a, kTestPolicyName2, CreateExternalDataFetcher("dummy"));
  PolicyMap b;
  SetPolicy(&b, kTestPolicyName1, std::make_unique<base::Value>("bbb"));
  SetPolicy(&b, kTestPolicyName3, std::make_unique<base::Value>(true));

  a.Swap(&b);
  base::Value expected("bbb");
  EXPECT_TRUE(expected.Equals(a.GetValue(kTestPolicyName1)));
  base::Value expected_bool(true);
  EXPECT_TRUE(expected_bool.Equals(a.GetValue(kTestPolicyName3)));
  EXPECT_FALSE(a.GetValue(kTestPolicyName2));
  EXPECT_FALSE(a.Get(kTestPolicyName2));
  base::Value expected_a("aaa");
  EXPECT_TRUE(expected_a.Equals(b.GetValue(kTestPolicyName1)));
  EXPECT_FALSE(b.GetValue(kTestPolicyName3));
  EXPECT_FALSE(a.GetValue(kTestPolicyName2));
  const PolicyMap::Entry* entry = b.Get(kTestPolicyName2);
  ASSERT_TRUE(entry);
  EXPECT_TRUE(
      ExternalDataFetcher::Equals(CreateExternalDataFetcher("dummy").get(),
                                  entry->external_data_fetcher.get()));

  b.Clear();
  a.Swap(&b);
  PolicyMap empty;
  EXPECT_TRUE(a.Equals(empty));
  EXPECT_FALSE(b.Equals(empty));
}

TEST_F(PolicyMapTest, MergeFrom) {
  PolicyMap a;
  a.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("google.com"),
        nullptr);
  a.Set(kTestPolicyName2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true), nullptr);
  a.Set(kTestPolicyName3, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_ENTERPRISE_DEFAULT, nullptr,
        CreateExternalDataFetcher("a"));
  a.Set(kTestPolicyName4, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(false), nullptr);
  a.Set(kTestPolicyName5, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("google.com/q={x}"),
        nullptr);
  a.Set(kTestPolicyName7, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_ENTERPRISE_DEFAULT, std::make_unique<base::Value>(false),
        nullptr);
  a.Set(kTestPolicyName8, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_ACTIVE_DIRECTORY,
        std::make_unique<base::Value>("blocked AD policy"), nullptr);
  a.GetMutable(kTestPolicyName8)->SetBlocked();

  PolicyMap b;
  b.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("chromium.org"),
        nullptr);
  b.Set(kTestPolicyName2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(false), nullptr);
  b.Set(kTestPolicyName3, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_ENTERPRISE_DEFAULT, nullptr,
        CreateExternalDataFetcher("b"));
  b.Set(kTestPolicyName4, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE,
        std::make_unique<base::Value>(true), nullptr);
  b.Set(kTestPolicyName5, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_PLATFORM, std::make_unique<base::Value>(std::string()),
        nullptr);
  b.Set(kTestPolicyName6, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true), nullptr);
  b.Set(kTestPolicyName7, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_ACTIVE_DIRECTORY, std::make_unique<base::Value>(true),
        nullptr);
  b.Set(kTestPolicyName8, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD,
        std::make_unique<base::Value>("non blocked cloud policy"), nullptr);

  auto conflicted_policy_1 = a.Get(kTestPolicyName1)->DeepCopy();
  auto conflicted_policy_4 = a.Get(kTestPolicyName4)->DeepCopy();
  auto conflicted_policy_5 = a.Get(kTestPolicyName5)->DeepCopy();
  auto conflicted_policy_8 = b.Get(kTestPolicyName8)->DeepCopy();

  a.GetMutable(kTestPolicyName7)->SetBlocked();
  b.GetMutable(kTestPolicyName7)->SetBlocked();
  a.MergeFrom(b);

  PolicyMap c;
  // POLICY_SCOPE_MACHINE over POLICY_SCOPE_USER.
  c.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("chromium.org"),
        nullptr);
  c.GetMutable(kTestPolicyName1)->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);
  c.GetMutable(kTestPolicyName1)
      ->AddConflictingPolicy(std::move(conflicted_policy_1));
  // |a| has precedence over |b|.
  c.Set(kTestPolicyName2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true), nullptr);
  c.GetMutable(kTestPolicyName2)->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);
  c.GetMutable(kTestPolicyName2)
      ->AddConflictingPolicy(b.Get(kTestPolicyName2)->DeepCopy());
  c.Set(kTestPolicyName3, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_ENTERPRISE_DEFAULT, nullptr,
        CreateExternalDataFetcher("a"));
  c.GetMutable(kTestPolicyName3)->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);
  c.GetMutable(kTestPolicyName3)
      ->AddConflictingPolicy(b.Get(kTestPolicyName3)->DeepCopy());
  // POLICY_SCOPE_MACHINE over POLICY_SCOPE_USER for POLICY_LEVEL_RECOMMENDED.
  c.Set(kTestPolicyName4, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE,
        std::make_unique<base::Value>(true), nullptr);
  c.GetMutable(kTestPolicyName4)->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);
  c.GetMutable(kTestPolicyName4)
      ->AddConflictingPolicy(std::move(conflicted_policy_4));
  // POLICY_LEVEL_MANDATORY over POLICY_LEVEL_RECOMMENDED.
  c.Set(kTestPolicyName5, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_PLATFORM, std::make_unique<base::Value>(std::string()),
        nullptr);
  c.GetMutable(kTestPolicyName5)->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);
  c.GetMutable(kTestPolicyName5)
      ->AddConflictingPolicy(std::move(conflicted_policy_5));
  // Merge new ones.
  c.Set(kTestPolicyName6, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true), nullptr);
  // POLICY_SOURCE_ACTIVE_DIRECTORY over POLICY_SOURCE_ENTERPRISE_DEFAULT.
  c.Set(kTestPolicyName7, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_ACTIVE_DIRECTORY, std::make_unique<base::Value>(true),
        nullptr);
  c.GetMutable(kTestPolicyName7)->SetBlocked();

  c.Set(kTestPolicyName8, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_ACTIVE_DIRECTORY,
        std::make_unique<base::Value>("blocked AD policy"), nullptr);
  c.GetMutable(kTestPolicyName8)->AddWarning(IDS_POLICY_CONFLICT_DIFF_VALUE);
  c.GetMutable(kTestPolicyName8)
      ->AddConflictingPolicy(std::move(conflicted_policy_8));
  c.GetMutable(kTestPolicyName8)->SetBlocked();

  EXPECT_TRUE(a.Equals(c));
}

TEST_F(PolicyMapTest, MergeValuesList) {
  std::vector<base::Value> abcd =
      GetListStorage<std::string>({"a", "b", "c", "d"});
  std::vector<base::Value> abc = GetListStorage<std::string>({"a", "b", "c"});
  std::vector<base::Value> ab = GetListStorage<std::string>({"a", "b"});
  std::vector<base::Value> cd = GetListStorage<std::string>({"c", "d"});
  std::vector<base::Value> ef = GetListStorage<std::string>({"e", "f"});

  std::vector<base::Value> int12 = GetListStorage<int>({1, 2});
  std::vector<base::Value> int34 = GetListStorage<int>({3, 4});
  std::vector<base::Value> int56 = GetListStorage<int>({5, 6});
  std::vector<base::Value> int1234 = GetListStorage<int>({1, 2, 3, 4});

  base::Value dict_ab(base::Value::Type::DICTIONARY);
  dict_ab.SetBoolKey("a", true);
  dict_ab.SetBoolKey("b", false);
  base::Value dict_c(base::Value::Type::DICTIONARY);
  dict_c.SetBoolKey("c", false);
  base::Value dict_d(base::Value::Type::DICTIONARY);
  dict_d.SetBoolKey("d", false);

  std::vector<base::Value> list_dict_abd;
  list_dict_abd.emplace_back(dict_ab.Clone());
  list_dict_abd.emplace_back(dict_d.Clone());
  std::vector<base::Value> list_dict_c;
  list_dict_c.emplace_back(dict_c.Clone());

  std::vector<base::Value> list_dict_abcd;
  list_dict_abcd.emplace_back(dict_ab.Clone());
  list_dict_abcd.emplace_back(dict_d.Clone());
  list_dict_abcd.emplace_back(dict_c.Clone());

  // Case 1 - kTestPolicyName1
  // Enterprise default policies should not be merged with other sources.
  PolicyMap::Entry case1(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         POLICY_SOURCE_PLATFORM,
                         std::make_unique<base::Value>(abc), nullptr);

  case1.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_ACTIVE_DIRECTORY,
      std::make_unique<base::Value>(cd), nullptr));

  case1.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       POLICY_SOURCE_ENTERPRISE_DEFAULT,
                       std::make_unique<base::Value>(ef), nullptr));

  case1.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                       POLICY_SOURCE_ENTERPRISE_DEFAULT,
                       std::make_unique<base::Value>(ef), nullptr));

  PolicyMap::Entry expected_case1(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                                  POLICY_SOURCE_MERGED,
                                  std::make_unique<base::Value>(abcd), nullptr);
  expected_case1.AddConflictingPolicy(case1.DeepCopy());

  // Case 2 - kTestPolicyName2
  // Policies should only be merged with other policies with the same target,
  // level and scope.
  PolicyMap::Entry case2(POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_PRIORITY_CLOUD,
                         std::make_unique<base::Value>(int12), nullptr);

  case2.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      std::make_unique<base::Value>(int34), nullptr));

  case2.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
      std::make_unique<base::Value>(int56), nullptr));

  PolicyMap::Entry expected_case2(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE, POLICY_SOURCE_MERGED,
      std::make_unique<base::Value>(int1234), nullptr);
  expected_case2.AddConflictingPolicy(case2.DeepCopy());

  // Case 3 - kTestPolicyName3
  // Trivial case with 2 sources.
  PolicyMap::Entry case3(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_PRIORITY_CLOUD,
                         std::make_unique<base::Value>(ab), nullptr);

  case3.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      std::make_unique<base::Value>(cd), nullptr));

  PolicyMap::Entry expected_case3(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                  POLICY_SOURCE_MERGED,
                                  std::make_unique<base::Value>(abcd), nullptr);
  auto case3_blocked_by_group = expected_case3.DeepCopy();
  case3_blocked_by_group.SetIgnoredByPolicyAtomicGroup();
  expected_case3.AddConflictingPolicy(case3.DeepCopy());

  // Case 4 - kTestPolicyName4
  // Policies with a single source should stay the same.
  PolicyMap::Entry case4(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_ACTIVE_DIRECTORY,
                         std::make_unique<base::Value>(ef), nullptr);
  PolicyMap::Entry expected_case4(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                  POLICY_SOURCE_MERGED,
                                  std::make_unique<base::Value>(ef), nullptr);
  expected_case4.AddConflictingPolicy(case4.DeepCopy());

  // Case 5 - kTestPolicyName5
  // Policies that are not lists should not be merged.
  // If such a policy is explicitly in the list of policies to merge, an error
  // is added to the entry and the policy stays intact.
  PolicyMap::Entry case5(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_ACTIVE_DIRECTORY,
                         std::make_unique<base::Value>("bad stuff"), nullptr);

  PolicyMap::Entry expected_case5(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                  POLICY_SOURCE_ACTIVE_DIRECTORY,
                                  std::make_unique<base::Value>("bad stuff"),
                                  nullptr);
  expected_case5.AddError(IDS_POLICY_LIST_MERGING_WRONG_POLICY_TYPE_SPECIFIED);

  // Case 6 - kTestPolicyName6
  // User cloud policies should not be merged with other sources.
  PolicyMap::Entry case6(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         POLICY_SOURCE_PLATFORM,
                         std::make_unique<base::Value>(ab), nullptr);
  case6.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>(cd), nullptr));
  case6.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_PRIORITY_CLOUD,
      std::make_unique<base::Value>(ef), nullptr));
  PolicyMap::Entry expected_case6(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                                  POLICY_SOURCE_MERGED,
                                  std::make_unique<base::Value>(ab), nullptr);
  expected_case6.AddConflictingPolicy(case6.DeepCopy());

  // Case 7 - kTestPolicyName7
  // Lists of dictionaries should not have duplicates.
  PolicyMap::Entry case7(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         POLICY_SOURCE_PLATFORM,
                         std::make_unique<base::Value>(list_dict_abd), nullptr);

  case7.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_ACTIVE_DIRECTORY,
      std::make_unique<base::Value>(list_dict_abd), nullptr));

  case7.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       POLICY_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE,
                       std::make_unique<base::Value>(list_dict_c), nullptr));

  PolicyMap::Entry expected_case7(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_MERGED,
      std::make_unique<base::Value>(list_dict_abcd), nullptr);
  expected_case7.AddConflictingPolicy(case7.DeepCopy());

  PolicyMap policy_not_merged;
  policy_not_merged.Set(kTestPolicyName1, case1.DeepCopy());
  policy_not_merged.Set(kTestPolicyName2, case2.DeepCopy());
  policy_not_merged.Set(kTestPolicyName3, case3.DeepCopy());
  policy_not_merged.Set(kTestPolicyName4, case4.DeepCopy());
  policy_not_merged.Set(kTestPolicyName5, case5.DeepCopy());
  policy_not_merged.Set(kTestPolicyName6, case6.DeepCopy());
  policy_not_merged.Set(kTestPolicyName7, case7.DeepCopy());

  PolicyMap expected_list_merged;
  expected_list_merged.Set(kTestPolicyName1, expected_case1.DeepCopy());
  expected_list_merged.Set(kTestPolicyName2, expected_case2.DeepCopy());
  expected_list_merged.Set(kTestPolicyName3, expected_case3.DeepCopy());
  expected_list_merged.Set(kTestPolicyName4, expected_case4.DeepCopy());
  expected_list_merged.Set(kTestPolicyName5, expected_case5.DeepCopy());
  expected_list_merged.Set(kTestPolicyName6, expected_case6.DeepCopy());
  expected_list_merged.Set(kTestPolicyName7, expected_case7.DeepCopy());

  PolicyMap list_merged;
  list_merged.CopyFrom(policy_not_merged);

  PolicyMap list_merged_wildcard;
  list_merged_wildcard.CopyFrom(policy_not_merged);

  // Merging with no restrictions specified
  PolicyListMerger empty_policy_list({});
  list_merged.MergeValues({&empty_policy_list});
  EXPECT_TRUE(list_merged.Equals(policy_not_merged));

  PolicyListMerger bad_policy_list({"unknown"});
  // Merging with wrong restrictions specified
  list_merged.MergeValues({&bad_policy_list});
  EXPECT_TRUE(list_merged.Equals(policy_not_merged));

  // Merging lists restrictions specified
  PolicyListMerger good_policy_list(
      {kTestPolicyName1, kTestPolicyName2, kTestPolicyName3, kTestPolicyName4,
       kTestPolicyName5, kTestPolicyName6, kTestPolicyName7});
  PolicyListMerger wildcard_policy_list({"*"});
  list_merged.MergeValues({&good_policy_list});
  EXPECT_TRUE(list_merged.Equals(expected_list_merged));

  PolicyMap expected_list_merged_wildcard;
  expected_list_merged_wildcard.CopyFrom(expected_list_merged);
  expected_list_merged_wildcard.Set(kTestPolicyName5, case5.DeepCopy());
  list_merged_wildcard.MergeValues({&wildcard_policy_list});
  EXPECT_TRUE(list_merged_wildcard.Equals(expected_list_merged_wildcard));
}

TEST_F(PolicyMapTest, MergeDictionaryValues) {
  base::Value dict_a(base::Value::Type::DICTIONARY);
  dict_a.SetBoolKey("keyA", true);

  base::Value dict_b(base::Value::Type::DICTIONARY);
  dict_b.SetStringKey("keyB", "ValueB2");
  dict_b.SetStringKey("keyC", "ValueC2");
  dict_b.SetStringKey("keyD", "ValueD2");

  base::Value dict_c(base::Value::Type::DICTIONARY);
  dict_c.SetStringKey("keyA", "ValueA");
  dict_c.SetStringKey("keyB", "ValueB");
  dict_c.SetStringKey("keyC", "ValueC");
  dict_c.SetStringKey("keyD", "ValueD");
  dict_c.SetStringKey("keyZ", "ValueZ");

  base::Value dict_d(base::Value::Type::DICTIONARY);
  dict_d.SetStringKey("keyC", "ValueC3");

  base::Value dict_e(base::Value::Type::DICTIONARY);
  dict_e.SetStringKey("keyD", "ValueD4");
  dict_e.SetIntKey("keyE", 123);

  base::Value dict_f(base::Value::Type::DICTIONARY);
  dict_f.SetStringKey("keyX", "ValueX");
  dict_f.SetStringKey("keyE", "ValueE5");

  // Case 1: kTestPolicyName1 - Merging should only keep keys with the highest
  // priority
  PolicyMap::Entry case1(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_PRIORITY_CLOUD,
                         base::Value::ToUniquePtrValue(dict_a.Clone()),
                         nullptr);
  case1.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      base::Value::ToUniquePtrValue(dict_b.Clone()), nullptr));
  case1.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                       POLICY_SOURCE_DEVICE_LOCAL_ACCOUNT_OVERRIDE,
                       base::Value::ToUniquePtrValue(dict_c.Clone()), nullptr));
  case1.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                       POLICY_SOURCE_ACTIVE_DIRECTORY,
                       base::Value::ToUniquePtrValue(dict_d.Clone()), nullptr));
  case1.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                       POLICY_SOURCE_ACTIVE_DIRECTORY,
                       base::Value::ToUniquePtrValue(dict_f.Clone()), nullptr));

  base::Value merged_dict_case1(base::Value::Type::DICTIONARY);
  merged_dict_case1.MergeDictionary(&dict_f);
  merged_dict_case1.MergeDictionary(&dict_d);
  merged_dict_case1.MergeDictionary(&dict_c);
  merged_dict_case1.MergeDictionary(&dict_b);
  merged_dict_case1.MergeDictionary(&dict_a);

  PolicyMap::Entry expected_case1(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_MERGED,
      base::Value::ToUniquePtrValue(merged_dict_case1.Clone()), nullptr);
  expected_case1.AddConflictingPolicy(case1.DeepCopy());

  // Case  - kTestPolicyName2
  // Policies should only be merged with other policies with the same target,
  // level and scope.
  PolicyMap::Entry case2(POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_PRIORITY_CLOUD,
                         base::Value::ToUniquePtrValue(dict_e.Clone()),
                         nullptr);

  case2.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      base::Value::ToUniquePtrValue(dict_f.Clone()), nullptr));

  case2.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
      base::Value::ToUniquePtrValue(dict_a.Clone()), nullptr));

  base::Value merged_dict_case2(base::Value::Type::DICTIONARY);
  merged_dict_case2.MergeDictionary(&dict_f);
  merged_dict_case2.MergeDictionary(&dict_e);

  PolicyMap::Entry expected_case2(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE, POLICY_SOURCE_MERGED,
      base::Value::ToUniquePtrValue(merged_dict_case2.Clone()), nullptr);
  expected_case2.AddConflictingPolicy(case2.DeepCopy());

  // Case 3 - kTestPolicyName3
  // Enterprise default policies should not be merged with other sources.
  PolicyMap::Entry case3(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
      base::Value::ToUniquePtrValue(dict_a.Clone()), nullptr);

  case3.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_ACTIVE_DIRECTORY,
      base::Value::ToUniquePtrValue(dict_b.Clone()), nullptr));

  case3.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       POLICY_SOURCE_ENTERPRISE_DEFAULT,
                       base::Value::ToUniquePtrValue(dict_e.Clone()), nullptr));

  case3.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                       POLICY_SOURCE_ENTERPRISE_DEFAULT,
                       base::Value::ToUniquePtrValue(dict_f.Clone()), nullptr));

  base::Value merged_dict_case3(base::Value::Type::DICTIONARY);
  merged_dict_case3.MergeDictionary(&dict_b);
  merged_dict_case3.MergeDictionary(&dict_a);

  PolicyMap::Entry expected_case3(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_MERGED,
      base::Value::ToUniquePtrValue(merged_dict_case3.Clone()), nullptr);
  expected_case3.AddConflictingPolicy(case3.DeepCopy());

  // Case 4 - kTestPolicyName4
  // Policies with a single source should be merged.
  PolicyMap::Entry case4(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_ACTIVE_DIRECTORY,
                         base::Value::ToUniquePtrValue(dict_a.Clone()),
                         nullptr);
  PolicyMap::Entry expected_case4(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_MERGED,
      base::Value::ToUniquePtrValue(dict_a.Clone()), nullptr);
  expected_case4.AddConflictingPolicy(case4.DeepCopy());

  // Case 5 - kTestPolicyName5
  // Policies that are not dictionaries should not be merged.
  // If such a policy is explicitly in the list of policies to merge, an error
  // is added to the entry and the policy stays intact.
  PolicyMap::Entry case5(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_ACTIVE_DIRECTORY,
                         std::make_unique<base::Value>("bad stuff"), nullptr);

  PolicyMap::Entry expected_case5(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                  POLICY_SOURCE_ACTIVE_DIRECTORY,
                                  std::make_unique<base::Value>("bad stuff"),
                                  nullptr);
  expected_case5.AddError(
      IDS_POLICY_DICTIONARY_MERGING_WRONG_POLICY_TYPE_SPECIFIED);

  // Case 6 - kTestPolicyName6
  // User cloud policies should not be merged with other sources.
  PolicyMap::Entry case6(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
      base::Value::ToUniquePtrValue(dict_a.Clone()), nullptr);
  case6.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      base::Value::ToUniquePtrValue(dict_e.Clone()), nullptr));
  case6.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_PRIORITY_CLOUD,
      base::Value::ToUniquePtrValue(dict_f.Clone()), nullptr));
  PolicyMap::Entry expected_case6(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_MERGED,
      base::Value::ToUniquePtrValue(dict_a.Clone()), nullptr);
  expected_case6.AddConflictingPolicy(case6.DeepCopy());

  // Case 7 - kTestPolicyName7
  // Policies that are not dictionaries should not be merged.
  // If such a policy is explicitly in the list of policies to merge, an error
  // is added to the entry and the policy stays intact.
  PolicyMap::Entry case7(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_ACTIVE_DIRECTORY,
                         base::Value::ToUniquePtrValue(dict_a.Clone()),
                         nullptr);

  PolicyMap::Entry expected_case7 = case7.DeepCopy();

  expected_case7.AddError(IDS_POLICY_DICTIONARY_MERGING_POLICY_NOT_ALLOWED);

  PolicyMap policy_not_merged;
  policy_not_merged.Set(kTestPolicyName1, case1.DeepCopy());
  policy_not_merged.Set(kTestPolicyName2, case2.DeepCopy());
  policy_not_merged.Set(kTestPolicyName3, case3.DeepCopy());
  policy_not_merged.Set(kTestPolicyName4, case4.DeepCopy());
  policy_not_merged.Set(kTestPolicyName5, case5.DeepCopy());
  policy_not_merged.Set(kTestPolicyName6, case6.DeepCopy());
  policy_not_merged.Set(kTestPolicyName7, case7.DeepCopy());

  PolicyMap expected_list_merged;
  expected_list_merged.Set(kTestPolicyName1, expected_case1.DeepCopy());
  expected_list_merged.Set(kTestPolicyName2, expected_case2.DeepCopy());
  expected_list_merged.Set(kTestPolicyName3, expected_case3.DeepCopy());
  expected_list_merged.Set(kTestPolicyName4, expected_case4.DeepCopy());
  expected_list_merged.Set(kTestPolicyName5, expected_case5.DeepCopy());
  expected_list_merged.Set(kTestPolicyName6, expected_case6.DeepCopy());
  expected_list_merged.Set(kTestPolicyName7, expected_case7.DeepCopy());

  PolicyMap list_merged;
  list_merged.CopyFrom(policy_not_merged);

  PolicyMap list_merged_wildcard;
  list_merged_wildcard.CopyFrom(policy_not_merged);

  // Merging with no restrictions specified
  PolicyDictionaryMerger empty_policy_list({});
  list_merged.MergeValues({&empty_policy_list});
  EXPECT_TRUE(list_merged.Equals(policy_not_merged));

  PolicyDictionaryMerger bad_policy_list({"unknown"});
  // Merging with wrong restrictions specified
  list_merged.MergeValues({&bad_policy_list});
  EXPECT_TRUE(list_merged.Equals(policy_not_merged));

  // Merging lists restrictions specified
  PolicyDictionaryMerger good_policy_list(
      {kTestPolicyName1, kTestPolicyName2, kTestPolicyName3, kTestPolicyName4,
       kTestPolicyName5, kTestPolicyName6, kTestPolicyName7});
  good_policy_list.SetAllowedPoliciesForTesting(
      {kTestPolicyName1, kTestPolicyName2, kTestPolicyName3, kTestPolicyName4,
       kTestPolicyName5, kTestPolicyName6});
  PolicyDictionaryMerger wildcard_policy_list({"*"});
  wildcard_policy_list.SetAllowedPoliciesForTesting(
      {kTestPolicyName1, kTestPolicyName2, kTestPolicyName3, kTestPolicyName4,
       kTestPolicyName5, kTestPolicyName6});
  list_merged.MergeValues({&good_policy_list});
  EXPECT_TRUE(list_merged.Equals(expected_list_merged));

  PolicyMap expected_list_merged_wildcard;
  expected_list_merged_wildcard.CopyFrom(expected_list_merged);
  expected_list_merged_wildcard.Set(kTestPolicyName5, case5.DeepCopy());
  expected_list_merged_wildcard.Set(kTestPolicyName7, case7.DeepCopy());
  list_merged_wildcard.MergeValues({&wildcard_policy_list});
  EXPECT_TRUE(list_merged_wildcard.Equals(expected_list_merged_wildcard));
}

TEST_F(PolicyMapTest, MergeValuesGroup) {
  std::vector<base::Value> abc = GetListStorage<std::string>({"a", "b", "c"});
  std::vector<base::Value> ab = GetListStorage<std::string>({"a", "b"});
  std::vector<base::Value> cd = GetListStorage<std::string>({"c", "d"});
  std::vector<base::Value> ef = GetListStorage<std::string>({"e", "f"});

  // Case 1 - kTestPolicyName1
  // Should not be affected by the atomic groups
  PolicyMap::Entry platform_user_mandatory(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
      std::make_unique<base::Value>(abc), nullptr);

  platform_user_mandatory.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      std::make_unique<base::Value>(cd), nullptr));

  platform_user_mandatory.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       POLICY_SOURCE_ENTERPRISE_DEFAULT,
                       std::make_unique<base::Value>(ef), nullptr));

  platform_user_mandatory.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
                       POLICY_SOURCE_ENTERPRISE_DEFAULT,
                       std::make_unique<base::Value>(ef), nullptr));

  // Case 2 - policy::key::kExtensionInstallBlacklist
  // This policy is part of the atomic group "Extensions" and has the highest
  // source in its group, its value should remain the same.
  PolicyMap::Entry cloud_machine_mandatory(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
      POLICY_SOURCE_PRIORITY_CLOUD, std::make_unique<base::Value>(ab), nullptr);

  cloud_machine_mandatory.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      std::make_unique<base::Value>(cd), nullptr));

  // Case 3 - policy::key::kExtensionInstallWhitelist
  // This policy is part of the atomic group "Extensions" and has a lower
  // source than policy::key::kExtensionInstallBlacklist from the same group,
  // its value should be ignored.
  PolicyMap::Entry ad_machine_mandatory(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
      POLICY_SOURCE_ACTIVE_DIRECTORY, std::make_unique<base::Value>(ef),
      nullptr);
  auto ad_machine_mandatory_ignored = ad_machine_mandatory.DeepCopy();
  ad_machine_mandatory_ignored.SetIgnoredByPolicyAtomicGroup();

  // Case 4 - policy::key::kExtensionInstallBlacklist
  // This policy is part of the atomic group "Extensions" and has the highest
  // source in its group, its value should remain the same.
  PolicyMap::Entry cloud_machine_recommended(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
      POLICY_SOURCE_PRIORITY_CLOUD, std::make_unique<base::Value>(ab), nullptr);

  PolicyMap policy_not_merged;
  policy_not_merged.Set(kTestPolicyName1, platform_user_mandatory.DeepCopy());
  policy_not_merged.Set(policy::key::kExtensionInstallBlacklist,
                        cloud_machine_mandatory.DeepCopy());
  policy_not_merged.Set(policy::key::kExtensionInstallWhitelist,
                        ad_machine_mandatory.DeepCopy());
  policy_not_merged.Set(policy::key::kExtensionInstallForcelist,
                        cloud_machine_recommended.DeepCopy());

  PolicyMap group_merged;
  group_merged.CopyFrom(policy_not_merged);
  PolicyGroupMerger group_merger;
  group_merged.MergeValues({&group_merger});

  PolicyMap expected_group_merged;
  expected_group_merged.Set(kTestPolicyName1,
                            platform_user_mandatory.DeepCopy());
  expected_group_merged.Set(policy::key::kExtensionInstallBlacklist,
                            cloud_machine_mandatory.DeepCopy());
  expected_group_merged.Set(policy::key::kExtensionInstallWhitelist,
                            ad_machine_mandatory_ignored.DeepCopy());
  expected_group_merged.Set(policy::key::kExtensionInstallForcelist,
                            cloud_machine_recommended.DeepCopy());

  EXPECT_TRUE(group_merged.Equals(expected_group_merged));
}

TEST_F(PolicyMapTest, GetDifferingKeys) {
  PolicyMap a;
  a.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("google.com"),
        nullptr);
  a.Set(kTestPolicyName2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD, nullptr, CreateExternalDataFetcher("dummy"));
  a.Set(kTestPolicyName3, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true), nullptr);
  a.Set(kTestPolicyName4, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD, nullptr, CreateExternalDataFetcher("a"));
  a.Set(kTestPolicyName5, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(false), nullptr);
  a.Set(kTestPolicyName6, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("google.com/q={x}"),
        nullptr);
  a.Set(kTestPolicyName7, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true), nullptr);

  PolicyMap b;
  b.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("google.com"),
        nullptr);
  b.Set(kTestPolicyName2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD, nullptr, CreateExternalDataFetcher("dummy"));
  b.Set(kTestPolicyName3, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(false), nullptr);
  b.Set(kTestPolicyName4, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD, nullptr, CreateExternalDataFetcher("b"));
  b.Set(kTestPolicyName5, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(false), nullptr);
  b.Set(kTestPolicyName6, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("google.com/q={x}"),
        nullptr);
  b.Set(kTestPolicyName8, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true), nullptr);

  std::set<std::string> diff;
  std::set<std::string> diff2;
  a.GetDifferingKeys(b, &diff);
  b.GetDifferingKeys(a, &diff2);
  // Order shouldn't matter.
  EXPECT_EQ(diff, diff2);
  // No change.
  EXPECT_TRUE(diff.find(kTestPolicyName1) == diff.end());
  EXPECT_TRUE(diff.find(kTestPolicyName2) == diff.end());
  // Different values.
  EXPECT_TRUE(diff.find(kTestPolicyName3) != diff.end());
  // Different external data references.
  EXPECT_TRUE(diff.find(kTestPolicyName4) != diff.end());
  // Different levels.
  EXPECT_TRUE(diff.find(kTestPolicyName5) != diff.end());
  // Different scopes.
  EXPECT_TRUE(diff.find(kTestPolicyName6) != diff.end());
  // Not in |a|.
  EXPECT_TRUE(diff.find(kTestPolicyName8) != diff.end());
  // Not in |b|.
  EXPECT_TRUE(diff.find(kTestPolicyName7) != diff.end());
  // No surprises.
  EXPECT_EQ(6u, diff.size());
}

TEST_F(PolicyMapTest, LoadFromSetsLevelScopeAndSource) {
  base::DictionaryValue policies;
  policies.SetString("TestPolicy1", "google.com");
  policies.SetBoolean("TestPolicy2", true);
  policies.SetInteger("TestPolicy3", -12321);

  PolicyMap loaded;
  loaded.LoadFrom(&policies,
                  POLICY_LEVEL_MANDATORY,
                  POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM);

  PolicyMap expected;
  expected.Set("TestPolicy1", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_PLATFORM,
               std::make_unique<base::Value>("google.com"), nullptr);
  expected.Set("TestPolicy2", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_PLATFORM, std::make_unique<base::Value>(true),
               nullptr);
  expected.Set("TestPolicy3", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_PLATFORM, std::make_unique<base::Value>(-12321),
               nullptr);
  EXPECT_TRUE(loaded.Equals(expected));
}

bool IsMandatory(const PolicyMap::PolicyMapType::const_iterator iter) {
  return iter->second.level == POLICY_LEVEL_MANDATORY;
}

TEST_F(PolicyMapTest, EraseNonmatching) {
  PolicyMap a;
  a.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("google.com"),
        nullptr);
  a.Set(kTestPolicyName2, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>(true), nullptr);

  a.EraseNonmatching(base::Bind(&IsMandatory));

  PolicyMap b;
  b.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD, std::make_unique<base::Value>("google.com"),
        nullptr);
  EXPECT_TRUE(a.Equals(b));
}

TEST_F(PolicyMapTest, EntryAddConflict) {
  PolicyMap::Entry entry_a;
  entry_a.level = POLICY_LEVEL_MANDATORY;
  entry_a.source = POLICY_SOURCE_CLOUD;
  entry_a.value = std::make_unique<base::Value>(true);
  entry_a.scope = POLICY_SCOPE_USER;
  PolicyMap::Entry entry_b = entry_a.DeepCopy();
  entry_b.value = std::make_unique<base::Value>(false);
  PolicyMap::Entry entry_b_no_conflicts = entry_b.DeepCopy();
  PolicyMap::Entry entry_c = entry_a.DeepCopy();
  entry_c.source = POLICY_SOURCE_PLATFORM;

  entry_b.AddConflictingPolicy(entry_c.DeepCopy());
  entry_a.AddConflictingPolicy(entry_b.DeepCopy());

  EXPECT_TRUE(entry_a.conflicts.size() == 2);
  EXPECT_TRUE(entry_b.conflicts.size() == 1);
  EXPECT_TRUE(entry_c.conflicts.empty());

  EXPECT_TRUE(entry_a.conflicts[0].Equals(entry_c));
  EXPECT_TRUE(entry_a.conflicts[1].Equals(entry_b_no_conflicts));
  EXPECT_TRUE(entry_b.conflicts[0].Equals(entry_c));
}

TEST_F(PolicyMapTest, BlockedEntry) {
  PolicyMap::Entry entry_a(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                           POLICY_SOURCE_CLOUD,
                           std::make_unique<base::Value>("a"), nullptr);
  PolicyMap::Entry entry_b = entry_a.DeepCopy();
  entry_b.value = std::make_unique<base::Value>("b");
  PolicyMap::Entry entry_c_blocked = entry_a.DeepCopy();
  entry_c_blocked.value = std::make_unique<base::Value>("c");
  entry_c_blocked.SetBlocked();

  PolicyMap policies;
  policies.Set("a", entry_a.DeepCopy());
  policies.Set("b", entry_b.DeepCopy());
  policies.Set("c", entry_c_blocked.DeepCopy());

  const size_t expected_size = 3;
  EXPECT_TRUE(policies.size() == expected_size);

  EXPECT_TRUE(policies.Get("a")->Equals(entry_a));
  EXPECT_TRUE(policies.Get("b")->Equals(entry_b));
  EXPECT_TRUE(policies.Get("c") == nullptr);

  EXPECT_TRUE(policies.GetMutable("a")->Equals(entry_a));
  EXPECT_TRUE(policies.GetMutable("b")->Equals(entry_b));
  EXPECT_TRUE(policies.GetMutable("c") == nullptr);

  EXPECT_TRUE(policies.GetValue("a")->Equals(entry_a.value.get()));
  EXPECT_TRUE(policies.GetValue("b")->Equals(entry_b.value.get()));
  EXPECT_TRUE(policies.GetValue("c") == nullptr);

  EXPECT_TRUE(policies.GetMutableValue("a")->Equals(entry_a.value.get()));
  EXPECT_TRUE(policies.GetMutableValue("b")->Equals(entry_b.value.get()));
  EXPECT_TRUE(policies.GetMutableValue("c") == nullptr);

  EXPECT_TRUE(policies.GetUntrusted("a")->Equals(entry_a));
  EXPECT_TRUE(policies.GetUntrusted("b")->Equals(entry_b));
  EXPECT_TRUE(policies.GetUntrusted("c")->Equals(entry_c_blocked));

  EXPECT_TRUE(policies.GetMutableUntrusted("a")->Equals(entry_a));
  EXPECT_TRUE(policies.GetMutableUntrusted("b")->Equals(entry_b));
  EXPECT_TRUE(policies.GetMutableUntrusted("c")->Equals(entry_c_blocked));

  size_t iterated_values = 0;
  for (auto it = policies.begin(); it != policies.end();
       ++it, ++iterated_values) {
  }
  EXPECT_TRUE(iterated_values == expected_size);
}

}  // namespace policy
