// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_map.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/external_data_manager.h"
#include "components/policy/core/common/features.h"
#include "components/policy/core/common/policy_details.h"
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
const char16_t kTestError[] = u"Test error message";

const PolicyDetails kExternalDetails_ = {false, false, kProfile, 0, 10, {}};
const PolicyDetails kNonExternalDetails_ = {false, false, kProfile, 0, 0, {}};
#if !BUILDFLAG(IS_CHROMEOS)
const PolicyDetails kUserCloudDetails = {false, false, kSingleProfile,
                                         0,     0,     {}};
#endif

// Utility functions for the tests.
void SetPolicy(PolicyMap* map, const char* name, base::Value value) {
  map->Set(name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
           std::move(value), nullptr);
}

void SetPolicy(PolicyMap* map,
               const char* name,
               std::unique_ptr<ExternalDataFetcher> external_data_fetcher) {
  map->Set(name, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
           std::nullopt, std::move(external_data_fetcher));
}

template <class T>
base::Value::List GetList(const std::vector<T>& entry) {
  base::Value::List result;
  for (const auto& it : entry)
    result.Append(it);
  return result;
}

}  // namespace

class PolicyMapTestBase {
 protected:
  std::unique_ptr<ExternalDataFetcher> CreateExternalDataFetcher(
      const std::string& policy) const;
};

std::unique_ptr<ExternalDataFetcher>
PolicyMapTestBase::CreateExternalDataFetcher(const std::string& policy) const {
  return std::make_unique<ExternalDataFetcher>(
      base::WeakPtr<ExternalDataManager>(), policy);
}

class PolicyMapTest : public PolicyMapTestBase, public testing::Test {
 public:
  const PolicyDetails* GetPolicyDetailsExternalCallback(
      const std::string& policy_name) {
    return &kExternalDetails_;
  }

  const PolicyDetails* GetPolicyDetailsNonExternalCallback(
      const std::string& policy_name) {
    return &kNonExternalDetails_;
  }
};

TEST_F(PolicyMapTest, SetAndGet) {
  PolicyMap map;
  EXPECT_FALSE(map.IsPolicySet(kTestPolicyName1));
  SetPolicy(&map, kTestPolicyName1, base::Value("aaa"));
  EXPECT_TRUE(map.IsPolicySet(kTestPolicyName1));
  const base::Value kExpectedStringA("aaa");
  EXPECT_EQ(kExpectedStringA, *map.GetValueUnsafe(kTestPolicyName1));
  EXPECT_EQ(kExpectedStringA,
            *map.GetValue(kTestPolicyName1, base::Value::Type::STRING));
  EXPECT_EQ(nullptr,
            map.GetValue(kTestPolicyName1, base::Value::Type::BOOLEAN));

  SetPolicy(&map, kTestPolicyName1, base::Value("bbb"));
  EXPECT_TRUE(map.IsPolicySet(kTestPolicyName1));
  const base::Value kExpectedStringB("bbb");
  EXPECT_EQ(kExpectedStringB, *map.GetValueUnsafe(kTestPolicyName1));
  EXPECT_EQ(kExpectedStringB,
            *map.GetValue(kTestPolicyName1, base::Value::Type::STRING));
  EXPECT_EQ(nullptr,
            map.GetValue(kTestPolicyName1, base::Value::Type::BOOLEAN));

  SetPolicy(&map, kTestPolicyName1, base::Value(true));
  EXPECT_TRUE(map.IsPolicySet(kTestPolicyName1));
  const base::Value kExpectedBool(true);
  EXPECT_EQ(kExpectedBool, *map.GetValueUnsafe(kTestPolicyName1));
  EXPECT_EQ(nullptr, map.GetValue(kTestPolicyName1, base::Value::Type::STRING));
  EXPECT_EQ(kExpectedBool,
            *map.GetValue(kTestPolicyName1, base::Value::Type::BOOLEAN));

  SetPolicy(&map, kTestPolicyName1, CreateExternalDataFetcher("dummy"));
  EXPECT_FALSE(map.IsPolicySet(kTestPolicyName1));
  map.AddMessage(kTestPolicyName1, PolicyMap::MessageType::kError,
                 IDS_POLICY_STORE_STATUS_VALIDATION_ERROR, {kTestError});
  EXPECT_EQ(nullptr, map.GetValueUnsafe(kTestPolicyName1));
  const PolicyMap::Entry* entry = map.Get(kTestPolicyName1);
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(POLICY_LEVEL_MANDATORY, entry->level);
  EXPECT_EQ(POLICY_SCOPE_USER, entry->scope);
  EXPECT_EQ(POLICY_SOURCE_CLOUD, entry->source);
  std::u16string error_string =
      base::StrCat({u"Validation error: ", kTestError});
  PolicyMap::Entry::L10nLookupFunction lookup = base::BindRepeating(
      static_cast<std::u16string (*)(int)>(&base::NumberToString16));
  EXPECT_EQ(error_string, entry->GetLocalizedMessages(
                              PolicyMap::MessageType::kError, lookup));
  EXPECT_TRUE(
      ExternalDataFetcher::Equals(entry->external_data_fetcher.get(),
                                  CreateExternalDataFetcher("dummy").get()));
  map.Set(kTestPolicyName1, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
          POLICY_SOURCE_ENTERPRISE_DEFAULT, std::nullopt, nullptr);
  EXPECT_FALSE(map.IsPolicySet(kTestPolicyName1));
  EXPECT_EQ(nullptr, map.GetValueUnsafe(kTestPolicyName1));
  entry = map.Get(kTestPolicyName1);
  ASSERT_NE(nullptr, entry);
  EXPECT_EQ(POLICY_LEVEL_RECOMMENDED, entry->level);
  EXPECT_EQ(POLICY_SCOPE_MACHINE, entry->scope);
  EXPECT_EQ(POLICY_SOURCE_ENTERPRISE_DEFAULT, entry->source);
  EXPECT_EQ(std::u16string(), entry->GetLocalizedMessages(
                                  PolicyMap::MessageType::kError, lookup));
  EXPECT_FALSE(entry->external_data_fetcher);
}

TEST_F(PolicyMapTest, AddMessage_Error) {
  PolicyMap map;
  SetPolicy(&map, kTestPolicyName1, base::Value(0));
  PolicyMap::Entry* entry1 = map.GetMutable(kTestPolicyName1);
  PolicyMap::Entry::L10nLookupFunction lookup = base::BindRepeating(
      static_cast<std::u16string (*)(int)>(&base::NumberToString16));
  EXPECT_EQ(std::u16string(), entry1->GetLocalizedMessages(
                                  PolicyMap::MessageType::kError, lookup));
  map.AddMessage(kTestPolicyName1, PolicyMap::MessageType::kError, 1234);
  EXPECT_EQ(u"1234", entry1->GetLocalizedMessages(
                         PolicyMap::MessageType::kError, lookup));
  map.AddMessage(kTestPolicyName1, PolicyMap::MessageType::kError, 5678);
  EXPECT_EQ(u"1234\n5678", entry1->GetLocalizedMessages(
                               PolicyMap::MessageType::kError, lookup));

  // Add second entry to make sure errors are added individually.
  SetPolicy(&map, kTestPolicyName2, base::Value(0));
  PolicyMap::Entry* entry2 = map.GetMutable(kTestPolicyName2);
  // Test adding Error message with placeholder replacement (one arg)
  map.AddMessage(kTestPolicyName2, PolicyMap::MessageType::kError,
                 IDS_POLICY_MIGRATED_OLD_POLICY, {u"SomeNewPolicy"});
  EXPECT_EQ(u"1234\n5678", entry1->GetLocalizedMessages(
                               PolicyMap::MessageType::kError, lookup));
  EXPECT_EQ(
      u"This policy is deprecated. You should use the "
      u"SomeNewPolicy policy instead.",
      entry2->GetLocalizedMessages(PolicyMap::MessageType::kError, lookup));
  map.AddMessage(kTestPolicyName2, PolicyMap::MessageType::kError, 1357);
  EXPECT_EQ(u"1234\n5678", entry1->GetLocalizedMessages(
                               PolicyMap::MessageType::kError, lookup));
  EXPECT_EQ(
      u"1357\nThis policy is deprecated. You should use "
      u"the SomeNewPolicy policy instead.",
      entry2->GetLocalizedMessages(PolicyMap::MessageType::kError, lookup));
  // Test adding Error message with placeholder replacement (two args)
  map.AddMessage(kTestPolicyName1, PolicyMap::MessageType::kError,
                 IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM,
                 {u"SomeSource", u"SomeDestination"});
  EXPECT_EQ(
      u"1234\n5678\nSharing from SomeSource to SomeDestination has "
      u"been blocked by administrator policy",
      entry1->GetLocalizedMessages(PolicyMap::MessageType::kError, lookup));
  EXPECT_EQ(
      u"1357\nThis policy is deprecated. You should use "
      u"the SomeNewPolicy policy instead.",
      entry2->GetLocalizedMessages(PolicyMap::MessageType::kError, lookup));

  // Ensure other message types are empty
  EXPECT_EQ(std::u16string(), entry2->GetLocalizedMessages(
                                  PolicyMap::MessageType::kWarning, lookup));
  EXPECT_EQ(std::u16string(), entry2->GetLocalizedMessages(
                                  PolicyMap::MessageType::kInfo, lookup));
}

TEST_F(PolicyMapTest, AddMessage_Warning) {
  PolicyMap map;
  SetPolicy(&map, kTestPolicyName1, base::Value(0));
  PolicyMap::Entry* entry1 = map.GetMutable(kTestPolicyName1);
  PolicyMap::Entry::L10nLookupFunction lookup = base::BindRepeating(
      static_cast<std::u16string (*)(int)>(&base::NumberToString16));
  EXPECT_EQ(std::u16string(), entry1->GetLocalizedMessages(
                                  PolicyMap::MessageType::kWarning, lookup));
  entry1->AddMessage(PolicyMap::MessageType::kWarning, 1234);
  EXPECT_EQ(u"1234", entry1->GetLocalizedMessages(
                         PolicyMap::MessageType::kWarning, lookup));
  entry1->AddMessage(PolicyMap::MessageType::kWarning, 5678);
  EXPECT_EQ(u"1234\n5678", entry1->GetLocalizedMessages(
                               PolicyMap::MessageType::kWarning, lookup));

  // Add second entry to make sure warnings are added individually.
  SetPolicy(&map, kTestPolicyName2, base::Value(0));
  PolicyMap::Entry* entry2 = map.GetMutable(kTestPolicyName2);
  // Test adding Warning message with placeholder replacement (one arg)
  entry2->AddMessage(PolicyMap::MessageType::kWarning,
                     IDS_POLICY_MIGRATED_OLD_POLICY, {u"SomeNewPolicy"});
  EXPECT_EQ(u"1234\n5678", entry1->GetLocalizedMessages(
                               PolicyMap::MessageType::kWarning, lookup));
  EXPECT_EQ(
      u"This policy is deprecated. You should use the "
      u"SomeNewPolicy policy instead.",
      entry2->GetLocalizedMessages(PolicyMap::MessageType::kWarning, lookup));
  entry2->AddMessage(PolicyMap::MessageType::kWarning, 1357);
  EXPECT_EQ(u"1234\n5678", entry1->GetLocalizedMessages(
                               PolicyMap::MessageType::kWarning, lookup));
  EXPECT_EQ(
      u"1357\nThis policy is deprecated. You should use "
      u"the SomeNewPolicy policy instead.",
      entry2->GetLocalizedMessages(PolicyMap::MessageType::kWarning, lookup));
  // Test adding Warning message with placeholder replacement (two args)
  entry1->AddMessage(PolicyMap::MessageType::kWarning,
                     IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM,
                     {u"SomeSource", u"SomeDestination"});
  EXPECT_EQ(
      u"1234\n5678\nSharing from SomeSource to SomeDestination has "
      u"been blocked by administrator policy",
      entry1->GetLocalizedMessages(PolicyMap::MessageType::kWarning, lookup));
  EXPECT_EQ(
      u"1357\nThis policy is deprecated. You should use "
      u"the SomeNewPolicy policy instead.",
      entry2->GetLocalizedMessages(PolicyMap::MessageType::kWarning, lookup));

  // Ensure other message types are empty
  EXPECT_EQ(std::u16string(), entry2->GetLocalizedMessages(
                                  PolicyMap::MessageType::kError, lookup));
  EXPECT_EQ(std::u16string(), entry2->GetLocalizedMessages(
                                  PolicyMap::MessageType::kInfo, lookup));
}

TEST_F(PolicyMapTest, AddMessage_Info) {
  PolicyMap map;
  SetPolicy(&map, kTestPolicyName1, base::Value(0));
  PolicyMap::Entry* entry1 = map.GetMutable(kTestPolicyName1);
  PolicyMap::Entry::L10nLookupFunction lookup = base::BindRepeating(
      static_cast<std::u16string (*)(int)>(&base::NumberToString16));
  EXPECT_EQ(std::u16string(), entry1->GetLocalizedMessages(
                                  PolicyMap::MessageType::kInfo, lookup));
  entry1->AddMessage(PolicyMap::MessageType::kInfo, 1234);
  EXPECT_EQ(u"1234", entry1->GetLocalizedMessages(PolicyMap::MessageType::kInfo,
                                                  lookup));
  entry1->AddMessage(PolicyMap::MessageType::kInfo, 5678);
  EXPECT_EQ(u"1234\n5678", entry1->GetLocalizedMessages(
                               PolicyMap::MessageType::kInfo, lookup));

  // Add second entry to make sure messages are added individually.
  SetPolicy(&map, kTestPolicyName2, base::Value(0));
  PolicyMap::Entry* entry2 = map.GetMutable(kTestPolicyName2);
  // Test adding Info message with placeholder replacement (one arg)
  entry2->AddMessage(PolicyMap::MessageType::kInfo,
                     IDS_POLICY_MIGRATED_OLD_POLICY, {u"SomeNewPolicy"});
  EXPECT_EQ(u"1234\n5678", entry1->GetLocalizedMessages(
                               PolicyMap::MessageType::kInfo, lookup));
  EXPECT_EQ(
      u"This policy is deprecated. You should use the "
      u"SomeNewPolicy policy instead.",
      entry2->GetLocalizedMessages(PolicyMap::MessageType::kInfo, lookup));
  entry2->AddMessage(PolicyMap::MessageType::kInfo, 1357);
  EXPECT_EQ(u"1234\n5678", entry1->GetLocalizedMessages(
                               PolicyMap::MessageType::kInfo, lookup));
  EXPECT_EQ(
      u"1357\nThis policy is deprecated. You should use "
      u"the SomeNewPolicy policy instead.",
      entry2->GetLocalizedMessages(PolicyMap::MessageType::kInfo, lookup));
  // Test adding Info message with placeholder replacement (two args)
  entry1->AddMessage(PolicyMap::MessageType::kInfo,
                     IDS_POLICY_DLP_CLIPBOARD_BLOCKED_ON_COPY_VM,
                     {u"SomeSource", u"SomeDestination"});
  EXPECT_EQ(
      u"1234\n5678\nSharing from SomeSource to SomeDestination has "
      u"been blocked by administrator policy",
      entry1->GetLocalizedMessages(PolicyMap::MessageType::kInfo, lookup));
  EXPECT_EQ(
      u"1357\nThis policy is deprecated. You should use "
      u"the SomeNewPolicy policy instead.",
      entry2->GetLocalizedMessages(PolicyMap::MessageType::kInfo, lookup));

  // Ensure other message types are empty
  EXPECT_EQ(std::u16string(), entry2->GetLocalizedMessages(
                                  PolicyMap::MessageType::kError, lookup));
  EXPECT_EQ(std::u16string(), entry2->GetLocalizedMessages(
                                  PolicyMap::MessageType::kWarning, lookup));
}

TEST_F(PolicyMapTest, Equals) {
  PolicyMap a;
  SetPolicy(&a, kTestPolicyName1, base::Value("aaa"));
  PolicyMap a2;
  SetPolicy(&a2, kTestPolicyName1, base::Value("aaa"));
  PolicyMap b;
  SetPolicy(&b, kTestPolicyName1, base::Value("bbb"));
  PolicyMap c;
  SetPolicy(&c, kTestPolicyName1, base::Value("aaa"));
  SetPolicy(&c, kTestPolicyName2, base::Value(true));
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
  SetPolicy(&a, kTestPolicyName1, base::Value("aaa"));
  SetPolicy(&a, kTestPolicyName2, CreateExternalDataFetcher("dummy"));
  PolicyMap b;
  SetPolicy(&b, kTestPolicyName1, base::Value("bbb"));
  SetPolicy(&b, kTestPolicyName3, base::Value(true));

  a.Swap(&b);
  const base::Value kExpectedStringB("bbb");
  EXPECT_EQ(kExpectedStringB,
            *a.GetValue(kTestPolicyName1, base::Value::Type::STRING));
  const base::Value kExpectedBool(true);
  EXPECT_EQ(kExpectedBool,
            *a.GetValue(kTestPolicyName3, base::Value::Type::BOOLEAN));
  EXPECT_EQ(nullptr, a.GetValueUnsafe(kTestPolicyName2));
  EXPECT_EQ(nullptr, a.Get(kTestPolicyName2));
  const base::Value kExpectedStringA("aaa");
  EXPECT_EQ(kExpectedStringA,
            *b.GetValue(kTestPolicyName1, base::Value::Type::STRING));
  EXPECT_EQ(nullptr, b.GetValueUnsafe(kTestPolicyName3));
  EXPECT_EQ(nullptr, a.GetValueUnsafe(kTestPolicyName2));
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

#if !BUILDFLAG(IS_CHROMEOS)
// Policy precedence changes are not supported on Chrome OS.
TEST_F(PolicyMapTest, MergeFrom_CloudMetapolicies) {
  // The two precedence metapolicies, CloudPolicyOverridesPlatformPolicy and
  // CloudUserPolicyOverridesCloudMachinePolicy, are set as cloud policies in
  // the incoming |policy_map_2|.
  PolicyMap policy_map_1;
  policy_map_1.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                   base::Value("platform_machine"), nullptr);
  policy_map_1.Set(kTestPolicyName2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, base::Value("cloud_user"), nullptr);
  policy_map_1.Set(kTestPolicyName3, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, base::Value("cloud_user"), nullptr);
  policy_map_1.Set(kTestPolicyName4, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                   base::Value("cloud_machine"), nullptr);

  PolicyMap policy_map_2;
  // Set matching user and device affiliation IDs to allow cloud user policies
  // to take precedence over cloud machine policies.
  base::flat_set<std::string> affiliation_ids;
  affiliation_ids.insert("a");
  policy_map_2.SetUserAffiliationIds(affiliation_ids);
  policy_map_2.SetDeviceAffiliationIds(affiliation_ids);

  policy_map_2.Set(key::kCloudPolicyOverridesPlatformPolicy,
                   POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                   POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  policy_map_2.Set(key::kCloudUserPolicyOverridesCloudMachinePolicy,
                   POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                   POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  policy_map_2.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                   base::Value("cloud_machine"), nullptr);
  policy_map_2.Set(kTestPolicyName2, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_PLATFORM, base::Value("platform_user"),
                   nullptr);
  policy_map_2.Set(kTestPolicyName3, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                   base::Value("platform_machine"), nullptr);
  policy_map_2.Set(kTestPolicyName4, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, base::Value("cloud_user"), nullptr);

  auto conflicting_policy_1 = policy_map_1.Get(kTestPolicyName1)->DeepCopy();
  auto conflicting_policy_2 = policy_map_2.Get(kTestPolicyName2)->DeepCopy();
  auto conflicting_policy_3 = policy_map_2.Get(kTestPolicyName3)->DeepCopy();
  auto conflicting_policy_4 = policy_map_1.Get(kTestPolicyName4)->DeepCopy();

  policy_map_1.MergeFrom(policy_map_2);

  PolicyMap policy_map_expected;

  policy_map_expected.SetUserAffiliationIds(affiliation_ids);
  policy_map_expected.SetDeviceAffiliationIds(affiliation_ids);
  policy_map_expected.Set(key::kCloudPolicyOverridesPlatformPolicy,
                          POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                          POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  policy_map_expected.Set(key::kCloudUserPolicyOverridesCloudMachinePolicy,
                          POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                          POLICY_SOURCE_CLOUD, base::Value(true), nullptr);
  // Cloud machine overrides platform machine.
  policy_map_expected.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY,
                          POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                          base::Value("cloud_machine"), nullptr);
  policy_map_expected.GetMutable(kTestPolicyName1)
      ->AddMessage(PolicyMap::MessageType::kWarning,
                   IDS_POLICY_CONFLICT_DIFF_VALUE);
  policy_map_expected.GetMutable(kTestPolicyName1)
      ->AddConflictingPolicy(std::move(conflicting_policy_1));
  // Cloud user overrides platform user.
  policy_map_expected.Set(kTestPolicyName2, POLICY_LEVEL_MANDATORY,
                          POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                          base::Value("cloud_user"), nullptr);
  policy_map_expected.GetMutable(kTestPolicyName2)
      ->AddMessage(PolicyMap::MessageType::kWarning,
                   IDS_POLICY_CONFLICT_DIFF_VALUE);
  policy_map_expected.GetMutable(kTestPolicyName2)
      ->AddConflictingPolicy(std::move(conflicting_policy_2));
  // Cloud user overrides platform machine.
  policy_map_expected.Set(kTestPolicyName3, POLICY_LEVEL_MANDATORY,
                          POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                          base::Value("cloud_user"), nullptr);
  policy_map_expected.GetMutable(kTestPolicyName3)
      ->AddMessage(PolicyMap::MessageType::kWarning,
                   IDS_POLICY_CONFLICT_DIFF_VALUE);
  policy_map_expected.GetMutable(kTestPolicyName3)
      ->AddConflictingPolicy(std::move(conflicting_policy_3));
  // Cloud user overrides cloud machine.
  policy_map_expected.Set(kTestPolicyName4, POLICY_LEVEL_MANDATORY,
                          POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                          base::Value("cloud_user"), nullptr);
  policy_map_expected.GetMutable(kTestPolicyName4)
      ->AddMessage(PolicyMap::MessageType::kWarning,
                   IDS_POLICY_CONFLICT_DIFF_VALUE);
  policy_map_expected.GetMutable(kTestPolicyName4)
      ->AddConflictingPolicy(std::move(conflicting_policy_4));

  EXPECT_TRUE(policy_map_1.Equals(policy_map_expected));
}
#endif  // BUILDFLAG(IS_CHROMEOS)

TEST_F(PolicyMapTest, MergeValuesList) {
  base::Value::List abcd = GetList<std::string>({"a", "b", "c", "d"});
  base::Value::List abc = GetList<std::string>({"a", "b", "c"});
  base::Value::List ab = GetList<std::string>({"a", "b"});
  base::Value::List cd = GetList<std::string>({"c", "d"});
  base::Value::List ef = GetList<std::string>({"e", "f"});

  base::Value::List int12 = GetList<int>({1, 2});
  base::Value::List int34 = GetList<int>({3, 4});
  base::Value::List int56 = GetList<int>({5, 6});
  base::Value::List int1234 = GetList<int>({1, 2, 3, 4});

  base::Value::Dict dict_ab;
  dict_ab.Set("a", true);
  dict_ab.Set("b", false);
  base::Value::Dict dict_c;
  dict_c.Set("c", false);
  base::Value::Dict dict_d;
  dict_d.Set("d", false);

  base::Value::List list_dict_abd;
  list_dict_abd.Append(dict_ab.Clone());
  list_dict_abd.Append(dict_d.Clone());
  base::Value::List list_dict_c;
  list_dict_c.Append(dict_c.Clone());

  base::Value::List list_dict_abcd;
  list_dict_abcd.Append(dict_ab.Clone());
  list_dict_abcd.Append(dict_d.Clone());
  list_dict_abcd.Append(dict_c.Clone());

  // Case 1 - kTestPolicyName1
  // Enterprise default policies should not be merged with other sources.
  PolicyMap::Entry case1(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_PLATFORM, base::Value(abc.Clone()),
                         nullptr);

  case1.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_COMMAND_LINE,
      base::Value(cd.Clone()), nullptr));

  case1.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
      POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(ef.Clone()), nullptr));

  case1.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
      POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(ef.Clone()), nullptr));

  PolicyMap::Entry expected_case1(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                  POLICY_SOURCE_MERGED,
                                  base::Value(abcd.Clone()), nullptr);
  expected_case1.AddConflictingPolicy(case1.DeepCopy());

  // Case 2 - kTestPolicyName2
  // Policies should only be merged with other policies with the same target,
  // level and scope.
  PolicyMap::Entry case2(POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_CLOUD, base::Value(int12.Clone()),
                         nullptr);

  case2.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      base::Value(int34.Clone()), nullptr));

  case2.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
      base::Value(int56.Clone()), nullptr));

  PolicyMap::Entry expected_case2(POLICY_LEVEL_RECOMMENDED,
                                  POLICY_SCOPE_MACHINE, POLICY_SOURCE_MERGED,
                                  base::Value(int1234.Clone()), nullptr);
  expected_case2.AddConflictingPolicy(case2.DeepCopy());

  // Case 3 - kTestPolicyName3
  // Trivial case with 2 sources.
  PolicyMap::Entry case3(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_CLOUD, base::Value(ab.Clone()), nullptr);

  case3.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      base::Value(cd.Clone()), nullptr));

  PolicyMap::Entry expected_case3(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                  POLICY_SOURCE_MERGED,
                                  base::Value(abcd.Clone()), nullptr);
  auto case3_blocked_by_group = expected_case3.DeepCopy();
  case3_blocked_by_group.SetIgnoredByPolicyAtomicGroup();
  expected_case3.AddConflictingPolicy(case3.DeepCopy());

  // Case 4 - kTestPolicyName4
  // Policies with a single source should stay the same.
  PolicyMap::Entry case4(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_CLOUD, base::Value(ef.Clone()), nullptr);
  PolicyMap::Entry expected_case4 = case4.DeepCopy();

  // Case 5 - kTestPolicyName5
  // Policies that are not lists should not be merged.
  // If such a policy is explicitly in the list of policies to merge, an error
  // is added to the entry and the policy stays intact.
  PolicyMap::Entry case5(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_PLATFORM, base::Value("bad stuff"),
                         nullptr);

  PolicyMap::Entry expected_case5 = case5.DeepCopy();
  expected_case5.AddMessage(
      PolicyMap::MessageType::kError,
      IDS_POLICY_LIST_MERGING_WRONG_POLICY_TYPE_SPECIFIED);

  // Case 6 - kTestPolicyName6
  // User cloud policies should not be merged with other sources.
  PolicyMap::Entry case6(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         POLICY_SOURCE_PLATFORM, base::Value(ab.Clone()),
                         nullptr);
  case6.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       POLICY_SOURCE_CLOUD, base::Value(cd.Clone()), nullptr));
  case6.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       POLICY_SOURCE_CLOUD, base::Value(ef.Clone()), nullptr));
  PolicyMap::Entry expected_case6(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                                  POLICY_SOURCE_MERGED, base::Value(ab.Clone()),
                                  nullptr);
  expected_case6.AddConflictingPolicy(case6.DeepCopy());

  // Case 7 - kTestPolicyName7
  // User platform policies should not be merged under any circumstances.
  PolicyMap::Entry case7(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         POLICY_SOURCE_PLATFORM, base::Value(ab.Clone()),
                         nullptr);
  case7.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
      base::Value(cd.Clone()), nullptr));
  case7.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       POLICY_SOURCE_CLOUD, base::Value(cd.Clone()), nullptr));
  case7.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      base::Value(ef.Clone()), nullptr));
  case7.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_COMMAND_LINE,
      base::Value(ef.Clone()), nullptr));
  PolicyMap::Entry expected_case7(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                                  POLICY_SOURCE_MERGED, base::Value(ab.Clone()),
                                  nullptr);
  expected_case7.AddConflictingPolicy(case7.DeepCopy());

  // Case 8 - kTestPolicyName8
  // Lists of dictionaries should not have duplicates.
  PolicyMap::Entry case8(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_PLATFORM,
                         base::Value(list_dict_abd.Clone()), nullptr);

  case8.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
      base::Value(list_dict_abd.Clone()), nullptr));

  case8.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_COMMAND_LINE,
      base::Value(list_dict_c.Clone()), nullptr));

  PolicyMap::Entry expected_case8(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                  POLICY_SOURCE_MERGED,
                                  base::Value(list_dict_abcd.Clone()), nullptr);
  expected_case8.AddConflictingPolicy(case8.DeepCopy());

  PolicyMap policy_not_merged;
  policy_not_merged.Set(kTestPolicyName1, case1.DeepCopy());
  policy_not_merged.Set(kTestPolicyName2, case2.DeepCopy());
  policy_not_merged.Set(kTestPolicyName3, case3.DeepCopy());
  policy_not_merged.Set(kTestPolicyName4, case4.DeepCopy());
  policy_not_merged.Set(kTestPolicyName5, case5.DeepCopy());
  policy_not_merged.Set(kTestPolicyName6, case6.DeepCopy());
  policy_not_merged.Set(kTestPolicyName7, case7.DeepCopy());
  policy_not_merged.Set(kTestPolicyName8, case8.DeepCopy());

  PolicyMap expected_list_merged;
  expected_list_merged.Set(kTestPolicyName1, expected_case1.DeepCopy());
  expected_list_merged.Set(kTestPolicyName2, expected_case2.DeepCopy());
  expected_list_merged.Set(kTestPolicyName3, expected_case3.DeepCopy());
  expected_list_merged.Set(kTestPolicyName4, expected_case4.DeepCopy());
  expected_list_merged.Set(kTestPolicyName5, expected_case5.DeepCopy());
  expected_list_merged.Set(kTestPolicyName6, expected_case6.DeepCopy());
  expected_list_merged.Set(kTestPolicyName7, expected_case7.DeepCopy());
  expected_list_merged.Set(kTestPolicyName8, expected_case8.DeepCopy());

  PolicyMap list_merged = policy_not_merged.Clone();

  PolicyMap list_merged_wildcard = policy_not_merged.Clone();

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
       kTestPolicyName5, kTestPolicyName6, kTestPolicyName7, kTestPolicyName8});
  PolicyListMerger wildcard_policy_list({"*"});
  list_merged.MergeValues({&good_policy_list});
  EXPECT_TRUE(list_merged.Equals(expected_list_merged));

  PolicyMap expected_list_merged_wildcard = expected_list_merged.Clone();
  expected_list_merged_wildcard.Set(kTestPolicyName5, case5.DeepCopy());
  list_merged_wildcard.MergeValues({&wildcard_policy_list});
  EXPECT_TRUE(list_merged_wildcard.Equals(expected_list_merged_wildcard));
}

TEST_F(PolicyMapTest, MergeValuesDictionary) {
  base::Value::Dict dict_a;
  dict_a.Set("keyA", true);

  base::Value::Dict dict_b;
  dict_b.Set("keyB", "ValueB2");
  dict_b.Set("keyC", "ValueC2");
  dict_b.Set("keyD", "ValueD2");

  base::Value::Dict dict_c;
  dict_c.Set("keyA", "ValueA");
  dict_c.Set("keyB", "ValueB");
  dict_c.Set("keyC", "ValueC");
  dict_c.Set("keyD", "ValueD");
  dict_c.Set("keyZ", "ValueZ");

  base::Value::Dict dict_d;
  dict_d.Set("keyC", "ValueC3");

  base::Value::Dict dict_e;
  dict_e.Set("keyD", "ValueD4");
  dict_e.Set("keyE", 123);

  base::Value::Dict dict_f;
  dict_f.Set("keyX", "ValueX");
  dict_f.Set("keyE", "ValueE5");

  // Case 1: kTestPolicyName1 - Merging should only keep keys with the highest
  // priority
  PolicyMap::Entry case1(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_PLATFORM, base::Value(dict_a.Clone()),
                         nullptr);
  case1.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
      base::Value(dict_b.Clone()), nullptr));
  case1.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_COMMAND_LINE,
      base::Value(dict_c.Clone()), nullptr));

  base::Value::Dict merged_dict_case1;
  merged_dict_case1.Merge(dict_c.Clone());
  merged_dict_case1.Merge(dict_b.Clone());
  merged_dict_case1.Merge(dict_a.Clone());

  PolicyMap::Entry expected_case1(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_MERGED,
      base::Value(merged_dict_case1.Clone()), nullptr);
  expected_case1.AddConflictingPolicy(case1.DeepCopy());

  // Case 2 - kTestPolicyName2
  // Policies should only be merged with other policies with the same target,
  // level and scope.
  PolicyMap::Entry case2(POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_PLATFORM, base::Value(dict_e.Clone()),
                         nullptr);

  case2.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
      base::Value(dict_f.Clone()), nullptr));

  case2.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
      base::Value(dict_a.Clone()), nullptr));

  base::Value::Dict merged_dict_case2;
  merged_dict_case2.Merge(dict_f.Clone());
  merged_dict_case2.Merge(dict_e.Clone());

  PolicyMap::Entry expected_case2(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE, POLICY_SOURCE_MERGED,
      base::Value(merged_dict_case2.Clone()), nullptr);
  expected_case2.AddConflictingPolicy(case2.DeepCopy());

  // Case 3 - kTestPolicyName3
  // Enterprise default policies should not be merged with other sources.
  PolicyMap::Entry case3(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_PLATFORM, base::Value(dict_a.Clone()),
                         nullptr);

  case3.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_COMMAND_LINE,
      base::Value(dict_b.Clone()), nullptr));

  case3.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
      POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(dict_e.Clone()), nullptr));

  case3.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
      POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(dict_f.Clone()), nullptr));

  base::Value::Dict merged_dict_case3;
  merged_dict_case3.Merge(dict_b.Clone());
  merged_dict_case3.Merge(dict_a.Clone());

  PolicyMap::Entry expected_case3(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_MERGED,
      base::Value(merged_dict_case3.Clone()), nullptr);
  expected_case3.AddConflictingPolicy(case3.DeepCopy());

  // Case 4 - kTestPolicyName4
  // Policies with a single source should stay the same.
  PolicyMap::Entry case4(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_CLOUD, base::Value(dict_a.Clone()),
                         nullptr);
  PolicyMap::Entry expected_case4 = case4.DeepCopy();

  // Case 5 - kTestPolicyName5
  // Policies that are not dictionaries should not be merged.
  // If such a policy is explicitly in the list of policies to merge, an error
  // is added to the entry and the policy stays intact.
  PolicyMap::Entry case5(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_PLATFORM, base::Value("bad stuff"),
                         nullptr);

  PolicyMap::Entry expected_case5 = case5.DeepCopy();
  expected_case5.AddMessage(
      PolicyMap::MessageType::kError,
      IDS_POLICY_DICTIONARY_MERGING_WRONG_POLICY_TYPE_SPECIFIED);

  // Case 6 - kTestPolicyName6
  // User cloud policies should not be merged with other sources.
  PolicyMap::Entry case6(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         POLICY_SOURCE_PLATFORM, base::Value(dict_a.Clone()),
                         nullptr);
  case6.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      base::Value(dict_e.Clone()), nullptr));
  case6.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      base::Value(dict_f.Clone()), nullptr));
  PolicyMap::Entry expected_case6(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                                  POLICY_SOURCE_MERGED,
                                  base::Value(dict_a.Clone()), nullptr);
  expected_case6.AddConflictingPolicy(case6.DeepCopy());

  // Case 7 - kTestPolicyName7
  // User platform policies should not be merged under any circumstances.
  PolicyMap::Entry case7(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         POLICY_SOURCE_PLATFORM, base::Value(dict_a.Clone()),
                         nullptr);
  case7.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
      base::Value(dict_b.Clone()), nullptr));
  case7.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
      base::Value(dict_c.Clone()), nullptr));
  case7.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      base::Value(dict_d.Clone()), nullptr));
  case7.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_COMMAND_LINE,
      base::Value(dict_e.Clone()), nullptr));
  PolicyMap::Entry expected_case7(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                                  POLICY_SOURCE_MERGED,
                                  base::Value(dict_a.Clone()), nullptr);
  expected_case7.AddConflictingPolicy(case7.DeepCopy());

  // Case 8 - kTestPolicyName8
  // If a dictionary policy is not in the list of dictionary policies allowed to
  // be merged, an error is added to the entry and the policy stays intact.
  PolicyMap::Entry case8(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                         POLICY_SOURCE_CLOUD, base::Value(dict_a.Clone()),
                         nullptr);

  PolicyMap::Entry expected_case8 = case8.DeepCopy();

  expected_case8.AddMessage(PolicyMap::MessageType::kError,
                            IDS_POLICY_DICTIONARY_MERGING_POLICY_NOT_ALLOWED);

  PolicyMap policy_not_merged;
  policy_not_merged.Set(kTestPolicyName1, case1.DeepCopy());
  policy_not_merged.Set(kTestPolicyName2, case2.DeepCopy());
  policy_not_merged.Set(kTestPolicyName3, case3.DeepCopy());
  policy_not_merged.Set(kTestPolicyName4, case4.DeepCopy());
  policy_not_merged.Set(kTestPolicyName5, case5.DeepCopy());
  policy_not_merged.Set(kTestPolicyName6, case6.DeepCopy());
  policy_not_merged.Set(kTestPolicyName7, case7.DeepCopy());
  policy_not_merged.Set(kTestPolicyName8, case8.DeepCopy());

  PolicyMap expected_list_merged;
  expected_list_merged.Set(kTestPolicyName1, expected_case1.DeepCopy());
  expected_list_merged.Set(kTestPolicyName2, expected_case2.DeepCopy());
  expected_list_merged.Set(kTestPolicyName3, expected_case3.DeepCopy());
  expected_list_merged.Set(kTestPolicyName4, expected_case4.DeepCopy());
  expected_list_merged.Set(kTestPolicyName5, expected_case5.DeepCopy());
  expected_list_merged.Set(kTestPolicyName6, expected_case6.DeepCopy());
  expected_list_merged.Set(kTestPolicyName7, expected_case7.DeepCopy());
  expected_list_merged.Set(kTestPolicyName8, expected_case8.DeepCopy());

  PolicyMap list_merged = policy_not_merged.Clone();

  PolicyMap list_merged_wildcard = policy_not_merged.Clone();

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
       kTestPolicyName5, kTestPolicyName6, kTestPolicyName7, kTestPolicyName8});
  good_policy_list.SetAllowedPoliciesForTesting(
      {kTestPolicyName1, kTestPolicyName2, kTestPolicyName3, kTestPolicyName4,
       kTestPolicyName5, kTestPolicyName6, kTestPolicyName7});
  PolicyDictionaryMerger wildcard_policy_list({"*"});
  wildcard_policy_list.SetAllowedPoliciesForTesting(
      {kTestPolicyName1, kTestPolicyName2, kTestPolicyName3, kTestPolicyName4,
       kTestPolicyName5, kTestPolicyName6, kTestPolicyName7});
  list_merged.MergeValues({&good_policy_list});
  EXPECT_TRUE(list_merged.Equals(expected_list_merged));

  PolicyMap expected_list_merged_wildcard = expected_list_merged.Clone();
  expected_list_merged_wildcard.Set(kTestPolicyName5, case5.DeepCopy());
  expected_list_merged_wildcard.Set(kTestPolicyName8, case8.DeepCopy());
  list_merged_wildcard.MergeValues({&wildcard_policy_list});
  EXPECT_TRUE(list_merged_wildcard.Equals(expected_list_merged_wildcard));
}

TEST_F(PolicyMapTest, MergeValuesGroup) {
  base::Value::List abc = GetList<std::string>({"a", "b", "c"});
  base::Value::List ab = GetList<std::string>({"a", "b"});
  base::Value::List cd = GetList<std::string>({"c", "d"});
  base::Value::List ef = GetList<std::string>({"e", "f"});

  // Case 1 - kTestPolicyName1
  // Should not be affected by the atomic groups
  PolicyMap::Entry platform_user_mandatory(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
      base::Value(abc.Clone()), nullptr);

  platform_user_mandatory.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       POLICY_SOURCE_CLOUD, base::Value(cd.Clone()), nullptr));

  platform_user_mandatory.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
      POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(ef.Clone()), nullptr));

  platform_user_mandatory.AddConflictingPolicy(PolicyMap::Entry(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_USER,
      POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(ef.Clone()), nullptr));

  // Case 2 - policy::key::kExtensionInstallBlocklist
  // This policy is part of the atomic group "Extensions" and has the highest
  // source in its group, its value should remain the same.
  PolicyMap::Entry platform_machine_mandatory(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      base::Value(ab.Clone()), nullptr);

  platform_machine_mandatory.AddConflictingPolicy(
      PolicyMap::Entry(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                       POLICY_SOURCE_CLOUD, base::Value(cd.Clone()), nullptr));

  // Case 3 - policy::key::kExtensionInstallAllowlist
  // This policy is part of the atomic group "Extensions" and has a lower
  // source than policy::key::kExtensionInstallBlocklist from the same group,
  // its value should be ignored.
  PolicyMap::Entry cloud_machine_mandatory(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
      base::Value(ef.Clone()), nullptr);
  auto cloud_machine_mandatory_ignored = cloud_machine_mandatory.DeepCopy();
  cloud_machine_mandatory_ignored.SetIgnoredByPolicyAtomicGroup();

  // Case 4 - policy::key::kExtensionInstallBlocklist
  // This policy is part of the atomic group "Extensions" and has the highest
  // source in its group, its value should remain the same.
  PolicyMap::Entry platform_machine_recommended(
      POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      base::Value(ab.Clone()), nullptr);

  PolicyMap policy_not_merged;
  policy_not_merged.Set(kTestPolicyName1, platform_user_mandatory.DeepCopy());
  policy_not_merged.Set(policy::key::kPopupsAllowedForUrls,
                        platform_machine_mandatory.DeepCopy());
  policy_not_merged.Set(policy::key::kPopupsBlockedForUrls,
                        cloud_machine_mandatory.DeepCopy());
  policy_not_merged.Set(policy::key::kDefaultPopupsSetting,
                        platform_machine_recommended.DeepCopy());

  PolicyMap group_merged = policy_not_merged.Clone();
  PolicyGroupMerger group_merger;
  group_merged.MergeValues({&group_merger});

  PolicyMap expected_group_merged;
  expected_group_merged.Set(kTestPolicyName1,
                            platform_user_mandatory.DeepCopy());
  expected_group_merged.Set(policy::key::kPopupsAllowedForUrls,
                            platform_machine_mandatory.DeepCopy());
  expected_group_merged.Set(policy::key::kPopupsBlockedForUrls,
                            cloud_machine_mandatory_ignored.DeepCopy());
  expected_group_merged.Set(policy::key::kDefaultPopupsSetting,
                            platform_machine_recommended.DeepCopy());

  EXPECT_TRUE(group_merged.Equals(expected_group_merged));
}

TEST_F(PolicyMapTest, LoadFromSetsLevelScopeAndSource) {
  base::Value::Dict policies;
  policies.Set("TestPolicy1", "google.com");
  policies.Set("TestPolicy2", true);
  policies.Set("TestPolicy3", -12321);

  PolicyMap loaded;
  loaded.LoadFrom(policies, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM);

  PolicyMap expected;
  expected.Set("TestPolicy1", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_PLATFORM, base::Value("google.com"), nullptr);
  expected.Set("TestPolicy2", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  expected.Set("TestPolicy3", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
               POLICY_SOURCE_PLATFORM, base::Value(-12321), nullptr);
  EXPECT_TRUE(loaded.Equals(expected));
}

TEST_F(PolicyMapTest, LoadFromCheckForExternalPolicy) {
  base::Value::Dict policies;
  policies.Set("TestPolicy1", "google.com");

  PolicyMap loaded;
  loaded.set_chrome_policy_details_callback_for_test(
      base::BindRepeating(&PolicyMapTest::GetPolicyDetailsExternalCallback,
                          base::Unretained(this)));
  loaded.LoadFrom(policies, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM);
  EXPECT_TRUE(loaded.empty());
  loaded.set_chrome_policy_details_callback_for_test(
      base::BindRepeating(&PolicyMapTest::GetPolicyDetailsNonExternalCallback,
                          base::Unretained(this)));
  loaded.LoadFrom(policies, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                  POLICY_SOURCE_PLATFORM);
  EXPECT_FALSE(loaded.empty());
}

bool IsMandatory(PolicyMap::const_reference entry) {
  return entry.second.level == POLICY_LEVEL_MANDATORY;
}

TEST_F(PolicyMapTest, CloneIf) {
  PolicyMap a;
  a.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD, base::Value("google.com"), nullptr);
  a.Set(kTestPolicyName2, POLICY_LEVEL_RECOMMENDED, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_CLOUD, base::Value(true), nullptr);

  PolicyMap clone = a.CloneIf(base::BindRepeating(&IsMandatory));

  PolicyMap b;
  b.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
        POLICY_SOURCE_CLOUD, base::Value("google.com"), nullptr);
  EXPECT_TRUE(clone.Equals(b));
}

TEST_F(PolicyMapTest, EntryAddConflict) {
  base::Value::List ab = GetList<std::string>({"a", "b"});
  base::Value::List cd = GetList<std::string>({"c", "d"});
  base::Value::List ef = GetList<std::string>({"e", "f"});
  base::Value::List gh = GetList<std::string>({"g", "h"});

  // Case 1: Non-nested conflicts
  PolicyMap::Entry case1(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         POLICY_SOURCE_PLATFORM, base::Value(ab.Clone()),
                         nullptr);
  PolicyMap::Entry conflict11(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                              POLICY_SOURCE_PLATFORM, base::Value(cd.Clone()),
                              nullptr);
  PolicyMap::Entry conflict12(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                              POLICY_SOURCE_PLATFORM, base::Value(ef.Clone()),
                              nullptr);
  PolicyMap::Entry conflict13(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                              POLICY_SOURCE_PLATFORM, base::Value(gh.Clone()),
                              nullptr);
  PolicyMap::Entry conflict14(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                              POLICY_SOURCE_PLATFORM, base::Value(ab.Clone()),
                              nullptr);

  case1.AddConflictingPolicy(conflict11.DeepCopy());
  case1.AddConflictingPolicy(conflict12.DeepCopy());
  case1.AddConflictingPolicy(conflict13.DeepCopy());
  case1.AddConflictingPolicy(conflict14.DeepCopy());

  EXPECT_TRUE(case1.conflicts.size() == 4);
  EXPECT_TRUE(case1.conflicts.at(0).entry().Equals(conflict11));
  EXPECT_TRUE(case1.conflicts.at(1).entry().Equals(conflict12));
  EXPECT_TRUE(case1.conflicts.at(2).entry().Equals(conflict13));
  EXPECT_TRUE(case1.conflicts.at(3).entry().Equals(conflict14));
  EXPECT_EQ(case1.conflicts.at(0).conflict_type(),
            PolicyMap::ConflictType::Override);
  EXPECT_EQ(case1.conflicts.at(1).conflict_type(),
            PolicyMap::ConflictType::Override);
  EXPECT_EQ(case1.conflicts.at(2).conflict_type(),
            PolicyMap::ConflictType::Override);
  EXPECT_EQ(case1.conflicts.at(3).conflict_type(),
            PolicyMap::ConflictType::Supersede);

  // Case 2: Nested conflicts
  PolicyMap::Entry case2(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                         POLICY_SOURCE_PLATFORM, base::Value(ab.Clone()),
                         nullptr);
  PolicyMap::Entry conflict21(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                              POLICY_SOURCE_PLATFORM, base::Value(cd.Clone()),
                              nullptr);
  PolicyMap::Entry conflict22(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                              POLICY_SOURCE_PLATFORM, base::Value(cd.Clone()),
                              nullptr);
  PolicyMap::Entry conflict23(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                              POLICY_SOURCE_PLATFORM, base::Value(ef.Clone()),
                              nullptr);
  PolicyMap::Entry conflict24(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                              POLICY_SOURCE_PLATFORM, base::Value(gh.Clone()),
                              nullptr);

  conflict21.AddConflictingPolicy(conflict22.DeepCopy());
  conflict21.AddConflictingPolicy(conflict23.DeepCopy());
  conflict21.AddConflictingPolicy(conflict24.DeepCopy());
  case2.AddConflictingPolicy(conflict21.DeepCopy());

  EXPECT_TRUE(case2.conflicts.size() == 4);
  EXPECT_TRUE(case2.conflicts.at(0).entry().Equals(conflict22));
  EXPECT_TRUE(case2.conflicts.at(1).entry().Equals(conflict23));
  EXPECT_TRUE(case2.conflicts.at(2).entry().Equals(conflict24));
  EXPECT_TRUE(conflict21.conflicts.at(0).entry().Equals(conflict22));
  EXPECT_TRUE(conflict21.conflicts.at(1).entry().Equals(conflict23));
  EXPECT_TRUE(conflict21.conflicts.at(2).entry().Equals(conflict24));
  EXPECT_EQ(case2.conflicts.at(0).conflict_type(),
            PolicyMap::ConflictType::Supersede);
  EXPECT_EQ(case2.conflicts.at(1).conflict_type(),
            PolicyMap::ConflictType::Override);
  EXPECT_EQ(case2.conflicts.at(2).conflict_type(),
            PolicyMap::ConflictType::Override);
  EXPECT_EQ(case2.conflicts.at(3).conflict_type(),
            PolicyMap::ConflictType::Override);
  EXPECT_EQ(conflict21.conflicts.at(0).conflict_type(),
            PolicyMap::ConflictType::Supersede);
  EXPECT_EQ(conflict21.conflicts.at(1).conflict_type(),
            PolicyMap::ConflictType::Override);
  EXPECT_EQ(conflict21.conflicts.at(2).conflict_type(),
            PolicyMap::ConflictType::Override);
}

TEST_F(PolicyMapTest, BlockedEntry) {
  PolicyMap::Entry entry_a(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                           POLICY_SOURCE_CLOUD, base::Value("a"), nullptr);
  PolicyMap::Entry entry_b = entry_a.DeepCopy();
  entry_b.set_value(base::Value("b"));
  PolicyMap::Entry entry_c_blocked = entry_a.DeepCopy();
  entry_c_blocked.set_value(base::Value("c"));
  entry_c_blocked.SetBlocked();

  PolicyMap policies;
  policies.Set("a", entry_a.DeepCopy());
  policies.Set("b", entry_b.DeepCopy());
  policies.Set("c", entry_c_blocked.DeepCopy());

  const size_t expected_size = 3;
  EXPECT_EQ(policies.size(), expected_size);

  EXPECT_TRUE(policies.Get("a")->Equals(entry_a));
  EXPECT_TRUE(policies.Get("b")->Equals(entry_b));
  EXPECT_EQ(policies.Get("c"), nullptr);

  EXPECT_TRUE(policies.GetMutable("a")->Equals(entry_a));
  EXPECT_TRUE(policies.GetMutable("b")->Equals(entry_b));
  EXPECT_EQ(policies.GetMutable("c"), nullptr);

  EXPECT_EQ(*policies.GetValue("a", base::Value::Type::STRING),
            *entry_a.value(base::Value::Type::STRING));
  EXPECT_EQ(*policies.GetValue("b", base::Value::Type::STRING),
            *entry_b.value(base::Value::Type::STRING));
  EXPECT_EQ(policies.GetValue("c", base::Value::Type::STRING), nullptr);

  EXPECT_EQ(*policies.GetValueUnsafe("a"), *entry_a.value_unsafe());
  EXPECT_EQ(*policies.GetValueUnsafe("b"), *entry_b.value_unsafe());
  EXPECT_EQ(policies.GetValueUnsafe("c"), nullptr);

  EXPECT_EQ(*policies.GetMutableValue("a", base::Value::Type::STRING),
            *entry_a.value(base::Value::Type::STRING));
  EXPECT_EQ(*policies.GetMutableValue("b", base::Value::Type::STRING),
            *entry_b.value(base::Value::Type::STRING));
  EXPECT_EQ(policies.GetMutableValue("c", base::Value::Type::STRING), nullptr);

  EXPECT_EQ(*policies.GetMutableValueUnsafe("a"), *entry_a.value_unsafe());
  EXPECT_EQ(*policies.GetMutableValueUnsafe("b"), *entry_b.value_unsafe());
  EXPECT_EQ(policies.GetMutableValueUnsafe("c"), nullptr);

  EXPECT_TRUE(policies.GetUntrusted("a")->Equals(entry_a));
  EXPECT_TRUE(policies.GetUntrusted("b")->Equals(entry_b));
  EXPECT_TRUE(policies.GetUntrusted("c")->Equals(entry_c_blocked));

  EXPECT_TRUE(policies.GetMutableUntrusted("a")->Equals(entry_a));
  EXPECT_TRUE(policies.GetMutableUntrusted("b")->Equals(entry_b));
  EXPECT_TRUE(policies.GetMutableUntrusted("c")->Equals(entry_c_blocked));

  EXPECT_FALSE(policies.GetUntrusted("a")->ignored());
  EXPECT_FALSE(policies.GetUntrusted("b")->ignored());
  EXPECT_TRUE(policies.GetUntrusted("c")->ignored());

  size_t iterated_values = 0;
  for (auto it = policies.begin(); it != policies.end();
       ++it, ++iterated_values) {
  }
  EXPECT_TRUE(iterated_values == expected_size);
}

TEST_F(PolicyMapTest, InvalidEntry) {
  PolicyMap::Entry entry_a(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                           POLICY_SOURCE_CLOUD, base::Value("a"), nullptr);
  PolicyMap::Entry entry_b_invalid = entry_a.DeepCopy();
  entry_b_invalid.set_value(base::Value("b"));
  entry_b_invalid.SetInvalid();

  PolicyMap policies;
  policies.Set("a", entry_a.DeepCopy());
  policies.Set("b", entry_b_invalid.DeepCopy());

  const size_t expected_size = 2;
  EXPECT_EQ(policies.size(), expected_size);

  EXPECT_TRUE(policies.Get("a")->Equals(entry_a));
  EXPECT_EQ(policies.Get("b"), nullptr);

  EXPECT_TRUE(policies.GetMutable("a")->Equals(entry_a));
  EXPECT_EQ(policies.GetMutable("b"), nullptr);

  EXPECT_EQ(*policies.GetValue("a", base::Value::Type::STRING),
            *entry_a.value(base::Value::Type::STRING));
  EXPECT_EQ(policies.GetValue("b", base::Value::Type::STRING), nullptr);

  EXPECT_EQ(*policies.GetValueUnsafe("a"), *entry_a.value_unsafe());
  EXPECT_EQ(policies.GetValueUnsafe("b"), nullptr);

  EXPECT_EQ(*policies.GetMutableValue("a", base::Value::Type::STRING),
            *entry_a.value(base::Value::Type::STRING));
  EXPECT_EQ(policies.GetMutableValue("b", base::Value::Type::STRING), nullptr);

  EXPECT_EQ(*policies.GetMutableValueUnsafe("a"), *entry_a.value_unsafe());
  EXPECT_EQ(policies.GetMutableValueUnsafe("b"), nullptr);

  EXPECT_TRUE(policies.GetUntrusted("a")->Equals(entry_a));
  EXPECT_TRUE(policies.GetUntrusted("b")->Equals(entry_b_invalid));

  EXPECT_TRUE(policies.GetMutableUntrusted("a")->Equals(entry_a));
  EXPECT_TRUE(policies.GetMutableUntrusted("b")->Equals(entry_b_invalid));

  EXPECT_FALSE(policies.GetUntrusted("a")->ignored());
  EXPECT_TRUE(policies.GetUntrusted("b")->ignored());

  size_t iterated_values = 0;
  for (auto it = policies.begin(); it != policies.end();
       ++it, ++iterated_values) {
  }
  EXPECT_EQ(iterated_values, expected_size);

  policies.SetAllInvalid();
  EXPECT_TRUE(policies.GetUntrusted("a")->ignored());
  EXPECT_TRUE(policies.GetUntrusted("b")->ignored());
}

TEST_F(PolicyMapTest, Affiliation) {
  PolicyMap policies;
  EXPECT_FALSE(policies.IsUserAffiliated());

  base::flat_set<std::string> user_ids;
  user_ids.insert("a");
  base::flat_set<std::string> device_ids;
  device_ids.insert("b");
  policies.SetUserAffiliationIds(user_ids);
  policies.SetDeviceAffiliationIds(device_ids);

  // Affiliation check fails because user and device IDs don't have at least one
  // ID in common.
  EXPECT_FALSE(policies.IsUserAffiliated());

  user_ids.insert("b");
  device_ids.insert("c");
  policies.SetUserAffiliationIds(user_ids);
  policies.SetDeviceAffiliationIds(device_ids);

  // Affiliation check succeeds now that 'a' is present in user and device IDs.
  EXPECT_TRUE(policies.IsUserAffiliated());
}

#if !BUILDFLAG(IS_CHROMEOS)
class PolicyMapMergeTest
    : public PolicyMapTestBase,
      public testing::TestWithParam<
          std::tuple</*cloud_policy_overrides_platform_policy=*/bool,
                     /*cloud_user_policy_overrides_cloud_machine_policy=*/bool,
                     /*is_user_affiliated=*/bool,
                     /*metapolicies_are_incoming=*/bool>> {
 public:
  bool CloudPolicyOverridesPlatformPolicy() const {
    return std::get<0>(GetParam());
  }

  bool CloudUserPolicyOverridesCloudMachinePolicy() const {
    return std::get<1>(GetParam());
  }

  bool IsUserAffiliated() const { return std::get<2>(GetParam()); }

  bool MetapoliciesAreIncoming() const { return std::get<3>(GetParam()); }

  void PopulateExpectedPolicyMap(PolicyMap& policy_map_expected,
                                 const PolicyMap& policy_map_1,
                                 const PolicyMap& policy_map_2) {
    // Setting the metapolicies.
    policy_map_expected.Set(
        key::kCloudPolicyOverridesPlatformPolicy, POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
        base::Value(CloudPolicyOverridesPlatformPolicy()), nullptr);
    policy_map_expected.Set(
        key::kCloudUserPolicyOverridesCloudMachinePolicy,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
        base::Value(CloudUserPolicyOverridesCloudMachinePolicy()), nullptr);

    // Expected behavior independent of metapolicy values and affiliation.
    // -------------------------------------------------------------------
    // |policy_map_1| has precedence over |policy_map_2|.
    policy_map_expected.Set(kTestPolicyName2, POLICY_LEVEL_MANDATORY,
                            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                            base::Value(true), nullptr);
    policy_map_expected.GetMutable(kTestPolicyName2)
        ->AddMessage(PolicyMap::MessageType::kWarning,
                     IDS_POLICY_CONFLICT_DIFF_VALUE);
    policy_map_expected.GetMutable(kTestPolicyName2)
        ->AddConflictingPolicy(policy_map_2.Get(kTestPolicyName2)->DeepCopy());
    policy_map_expected.Set(kTestPolicyName3, POLICY_LEVEL_MANDATORY,
                            POLICY_SCOPE_MACHINE,
                            POLICY_SOURCE_ENTERPRISE_DEFAULT, std::nullopt,
                            CreateExternalDataFetcher("a"));
    policy_map_expected.GetMutable(kTestPolicyName3)
        ->AddMessage(PolicyMap::MessageType::kWarning,
                     IDS_POLICY_CONFLICT_DIFF_VALUE);
    policy_map_expected.GetMutable(kTestPolicyName3)
        ->AddConflictingPolicy(policy_map_2.Get(kTestPolicyName3)->DeepCopy());
    // Cloud machine over platform user for recommended policies.
    policy_map_expected.Set(kTestPolicyName4, POLICY_LEVEL_RECOMMENDED,
                            POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                            base::Value(true), nullptr);
    policy_map_expected.GetMutable(kTestPolicyName4)
        ->AddMessage(PolicyMap::MessageType::kWarning,
                     IDS_POLICY_CONFLICT_DIFF_VALUE);
    policy_map_expected.GetMutable(kTestPolicyName4)
        ->AddConflictingPolicy(policy_map_1.Get(kTestPolicyName4)->DeepCopy());
    // Mandatory over recommended level.
    policy_map_expected.Set(kTestPolicyName5, POLICY_LEVEL_MANDATORY,
                            POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                            base::Value(std::string()), nullptr);
    policy_map_expected.GetMutable(kTestPolicyName5)
        ->AddMessage(PolicyMap::MessageType::kWarning,
                     IDS_POLICY_CONFLICT_DIFF_VALUE);
    policy_map_expected.GetMutable(kTestPolicyName5)
        ->AddConflictingPolicy(policy_map_1.Get(kTestPolicyName5)->DeepCopy());
    // Merge new policy.
    policy_map_expected.Set(kTestPolicyName6, POLICY_LEVEL_RECOMMENDED,
                            POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                            base::Value(true), nullptr);
    // Platform over default source.
    policy_map_expected.Set(kTestPolicyName7, POLICY_LEVEL_MANDATORY,
                            POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
                            base::Value(true), nullptr);
    policy_map_expected.GetMutable(kTestPolicyName7)->SetBlocked();

    // Expected behavior that depends on metapolicy values and affiliation.
    // --------------------------------------------------------------------
#if !BUILDFLAG(IS_CHROMEOS)
    if (CloudPolicyOverridesPlatformPolicy() &&
        CloudUserPolicyOverridesCloudMachinePolicy() && IsUserAffiliated()) {
      // Cloud user over cloud machine source.
      policy_map_expected.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY,
                              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                              base::Value("google.com"), nullptr);
      policy_map_expected.GetMutable(kTestPolicyName1)
          ->AddMessage(PolicyMap::MessageType::kWarning,
                       IDS_POLICY_CONFLICT_DIFF_VALUE);
      policy_map_expected.GetMutable(kTestPolicyName1)
          ->AddConflictingPolicy(
              policy_map_2.Get(kTestPolicyName1)->DeepCopy());
      // Cloud machine over platform machine when platform is blocked.
      policy_map_expected.Set(kTestPolicyName8, POLICY_LEVEL_MANDATORY,
                              POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                              base::Value("non-blocked cloud policy"), nullptr);
      policy_map_expected.GetMutable(kTestPolicyName8)
          ->AddMessage(PolicyMap::MessageType::kWarning,
                       IDS_POLICY_CONFLICT_DIFF_VALUE);
      policy_map_expected.GetMutable(kTestPolicyName8)
          ->AddConflictingPolicy(
              policy_map_1.Get(kTestPolicyName8)->DeepCopy());
      // policy_map_expected.GetMutable(kTestPolicyName8)->SetBlocked();
    } else if (CloudUserPolicyOverridesCloudMachinePolicy() &&
               IsUserAffiliated()) {
      // Cloud user over cloud machine source.
      policy_map_expected.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY,
                              POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD,
                              base::Value("google.com"), nullptr);
      policy_map_expected.GetMutable(kTestPolicyName1)
          ->AddMessage(PolicyMap::MessageType::kWarning,
                       IDS_POLICY_CONFLICT_DIFF_VALUE);
      policy_map_expected.GetMutable(kTestPolicyName1)
          ->AddConflictingPolicy(
              policy_map_2.Get(kTestPolicyName1)->DeepCopy());
      // Platform machine over cloud machine even when platform is blocked.
      policy_map_expected.Set(kTestPolicyName8, POLICY_LEVEL_MANDATORY,
                              POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                              base::Value("blocked platform policy"), nullptr);
      policy_map_expected.GetMutable(kTestPolicyName8)
          ->AddMessage(PolicyMap::MessageType::kWarning,
                       IDS_POLICY_CONFLICT_DIFF_VALUE);
      policy_map_expected.GetMutable(kTestPolicyName8)
          ->AddConflictingPolicy(
              policy_map_2.Get(kTestPolicyName8)->DeepCopy());
      policy_map_expected.GetMutable(kTestPolicyName8)->SetBlocked();
    } else if (CloudPolicyOverridesPlatformPolicy()) {
      // Machine over user scope.
      policy_map_expected.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY,
                              POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                              base::Value("chromium.org"), nullptr);
      policy_map_expected.GetMutable(kTestPolicyName1)
          ->AddMessage(PolicyMap::MessageType::kWarning,
                       IDS_POLICY_CONFLICT_DIFF_VALUE);
      policy_map_expected.GetMutable(kTestPolicyName1)
          ->AddConflictingPolicy(
              policy_map_1.Get(kTestPolicyName1)->DeepCopy());
      // Cloud machine over platform machine when platform is blocked.
      policy_map_expected.Set(kTestPolicyName8, POLICY_LEVEL_MANDATORY,
                              POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                              base::Value("non-blocked cloud policy"), nullptr);
      policy_map_expected.GetMutable(kTestPolicyName8)
          ->AddMessage(PolicyMap::MessageType::kWarning,
                       IDS_POLICY_CONFLICT_DIFF_VALUE);
      policy_map_expected.GetMutable(kTestPolicyName8)
          ->AddConflictingPolicy(
              policy_map_1.Get(kTestPolicyName8)->DeepCopy());
      // policy_map_expected.GetMutable(kTestPolicyName8)->SetBlocked();
    } else {
#endif  // !BUILDFLAG(IS_CHROMEOS)
      // Machine over user scope.
      policy_map_expected.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY,
                              POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                              base::Value("chromium.org"), nullptr);
      policy_map_expected.GetMutable(kTestPolicyName1)
          ->AddMessage(PolicyMap::MessageType::kWarning,
                       IDS_POLICY_CONFLICT_DIFF_VALUE);
      policy_map_expected.GetMutable(kTestPolicyName1)
          ->AddConflictingPolicy(
              policy_map_1.Get(kTestPolicyName1)->DeepCopy());
      // Platform machine over cloud machine even when platform is blocked.
      policy_map_expected.Set(kTestPolicyName8, POLICY_LEVEL_MANDATORY,
                              POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                              base::Value("blocked platform policy"), nullptr);
      policy_map_expected.GetMutable(kTestPolicyName8)
          ->AddMessage(PolicyMap::MessageType::kWarning,
                       IDS_POLICY_CONFLICT_DIFF_VALUE);
      policy_map_expected.GetMutable(kTestPolicyName8)
          ->AddConflictingPolicy(
              policy_map_2.Get(kTestPolicyName8)->DeepCopy());
      policy_map_expected.GetMutable(kTestPolicyName8)->SetBlocked();
#if !BUILDFLAG(IS_CHROMEOS)
    }
#endif  // !BUILDFLAG(IS_CHROMEOS)
  }

  void PopulateExpectedMetapolicyMap(PolicyMap& policy_map_expected,
                                     const PolicyMap& policy_map_1,
                                     const PolicyMap& policy_map_2,
                                     base::Value::List merge_list_1,
                                     base::Value::List merge_list_2) {
    // Platform machine overrides cloud machine because modified priorities
    // don't apply to precedence metapolicies.
    policy_map_expected.Set(
        key::kCloudPolicyOverridesPlatformPolicy, POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
        base::Value(CloudPolicyOverridesPlatformPolicy()), nullptr);
    policy_map_expected.GetMutable(key::kCloudPolicyOverridesPlatformPolicy)
        ->AddMessage(CloudPolicyOverridesPlatformPolicy()
                         ? PolicyMap::MessageType::kWarning
                         : PolicyMap::MessageType::kInfo,
                     CloudPolicyOverridesPlatformPolicy()
                         ? IDS_POLICY_CONFLICT_DIFF_VALUE
                         : IDS_POLICY_CONFLICT_SAME_VALUE);
    policy_map_expected.GetMutable(key::kCloudPolicyOverridesPlatformPolicy)
        ->AddConflictingPolicy(
            policy_map_2.Get(key::kCloudPolicyOverridesPlatformPolicy)
                ->DeepCopy());
    policy_map_expected.Set(
        key::kCloudUserPolicyOverridesCloudMachinePolicy,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
        base::Value(CloudUserPolicyOverridesCloudMachinePolicy()), nullptr);
    policy_map_expected
        .GetMutable(key::kCloudUserPolicyOverridesCloudMachinePolicy)
        ->AddMessage(CloudUserPolicyOverridesCloudMachinePolicy()
                         ? PolicyMap::MessageType::kWarning
                         : PolicyMap::MessageType::kInfo,
                     CloudUserPolicyOverridesCloudMachinePolicy()
                         ? IDS_POLICY_CONFLICT_DIFF_VALUE
                         : IDS_POLICY_CONFLICT_SAME_VALUE);
    policy_map_expected
        .GetMutable(key::kCloudUserPolicyOverridesCloudMachinePolicy)
        ->AddConflictingPolicy(
            policy_map_1.Get(key::kCloudUserPolicyOverridesCloudMachinePolicy)
                ->DeepCopy());
#if !BUILDFLAG(IS_CHROMEOS)
    if (CloudPolicyOverridesPlatformPolicy()) {
      // Cloud machine overrides platform machine because modified priorities
      // apply to merging metapolicies.
      policy_map_expected.Set(key::kPolicyListMultipleSourceMergeList,
                              POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                              POLICY_SOURCE_CLOUD,
                              base::Value(std::move(merge_list_2)), nullptr);
      policy_map_expected.GetMutable(key::kPolicyListMultipleSourceMergeList)
          ->AddMessage(PolicyMap::MessageType::kWarning,
                       IDS_POLICY_CONFLICT_DIFF_VALUE);
      policy_map_expected.GetMutable(key::kPolicyListMultipleSourceMergeList)
          ->AddConflictingPolicy(
              policy_map_1.Get(key::kPolicyListMultipleSourceMergeList)
                  ->DeepCopy());
    } else {
#endif  // !BUILDFLAG(IS_CHROMEOS)
      // Platform machine overrides cloud machine with default precedence.
      policy_map_expected.Set(key::kPolicyListMultipleSourceMergeList,
                              POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                              POLICY_SOURCE_PLATFORM,
                              base::Value(std::move(merge_list_1)), nullptr);
      policy_map_expected.GetMutable(key::kPolicyListMultipleSourceMergeList)
          ->AddMessage(PolicyMap::MessageType::kWarning,
                       IDS_POLICY_CONFLICT_DIFF_VALUE);
      policy_map_expected.GetMutable(key::kPolicyListMultipleSourceMergeList)
          ->AddConflictingPolicy(
              policy_map_2.Get(key::kPolicyListMultipleSourceMergeList)
                  ->DeepCopy());
#if !BUILDFLAG(IS_CHROMEOS)
    }
#endif  // !BUILDFLAG(IS_CHROMEOS)
  }
};

TEST_P(PolicyMapMergeTest, MergeFrom) {
  PolicyMap policy_map_1;
  if (IsUserAffiliated()) {
    base::flat_set<std::string> affiliation_ids;
    affiliation_ids.insert("12345");
    // Treat user as affiliated by setting identical user and device IDs.
    policy_map_1.SetUserAffiliationIds(affiliation_ids);
    policy_map_1.SetDeviceAffiliationIds(affiliation_ids);
  }
  if (!MetapoliciesAreIncoming()) {
    // Metapolicies are set in the base PolicyMap.
    policy_map_1.Set(
        key::kCloudPolicyOverridesPlatformPolicy, POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
        base::Value(CloudPolicyOverridesPlatformPolicy()), nullptr);
    policy_map_1.Set(
        key::kCloudUserPolicyOverridesCloudMachinePolicy,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
        base::Value(CloudUserPolicyOverridesCloudMachinePolicy()), nullptr);
  }
  policy_map_1.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_CLOUD, base::Value("google.com"), nullptr);
  policy_map_1.Set(kTestPolicyName2, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, base::Value(true),
                   nullptr);
  policy_map_1.Set(kTestPolicyName3, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                   std::nullopt, CreateExternalDataFetcher("a"));
  policy_map_1.Set(kTestPolicyName4, POLICY_LEVEL_RECOMMENDED,
                   POLICY_SCOPE_USER, POLICY_SOURCE_PLATFORM,
                   base::Value(false), nullptr);
  policy_map_1.Set(kTestPolicyName5, POLICY_LEVEL_RECOMMENDED,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                   base::Value("google.com/q={x}"), nullptr);
  policy_map_1.Set(kTestPolicyName7, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(false),
                   nullptr);
  policy_map_1.Set(kTestPolicyName8, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                   base::Value("blocked platform policy"), nullptr);

  PolicyMap policy_map_2;
  if (MetapoliciesAreIncoming()) {
    // Metapolicies are set in the incoming PolicyMap.
    policy_map_2.Set(
        key::kCloudPolicyOverridesPlatformPolicy, POLICY_LEVEL_MANDATORY,
        POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
        base::Value(CloudPolicyOverridesPlatformPolicy()), nullptr);
    policy_map_2.Set(
        key::kCloudUserPolicyOverridesCloudMachinePolicy,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
        base::Value(CloudUserPolicyOverridesCloudMachinePolicy()), nullptr);
  }
  policy_map_2.Set(kTestPolicyName1, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                   base::Value("chromium.org"), nullptr);
  policy_map_2.Set(kTestPolicyName2, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                   base::Value(false), nullptr);
  policy_map_2.Set(kTestPolicyName3, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_ENTERPRISE_DEFAULT,
                   std::nullopt, CreateExternalDataFetcher("b"));
  policy_map_2.Set(kTestPolicyName4, POLICY_LEVEL_RECOMMENDED,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD, base::Value(true),
                   nullptr);
  policy_map_2.Set(kTestPolicyName5, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
                   base::Value(std::string()), nullptr);
  policy_map_2.Set(kTestPolicyName6, POLICY_LEVEL_RECOMMENDED,
                   POLICY_SCOPE_USER, POLICY_SOURCE_CLOUD, base::Value(true),
                   nullptr);
  policy_map_2.Set(kTestPolicyName7, POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                   POLICY_SOURCE_PLATFORM, base::Value(true), nullptr);
  policy_map_2.Set(kTestPolicyName8, POLICY_LEVEL_MANDATORY,
                   POLICY_SCOPE_MACHINE, POLICY_SOURCE_CLOUD,
                   base::Value("non-blocked cloud policy"), nullptr);

  PolicyMap policy_map_expected;
  PopulateExpectedPolicyMap(policy_map_expected, policy_map_1, policy_map_2);

  policy_map_1.GetMutable(kTestPolicyName7)->SetBlocked();
  policy_map_2.GetMutable(kTestPolicyName7)->SetBlocked();
  policy_map_1.GetMutable(kTestPolicyName8)->SetBlocked();
  policy_map_1.MergeFrom(policy_map_2);

  EXPECT_TRUE(policy_map_1.Equals(policy_map_expected));
}

TEST_P(PolicyMapMergeTest, MergeFrom_Metapolicies) {
  // Define the lists of policies that will be used by the merging metapolicies.
  base::Value::List merge_list_1;
  merge_list_1.Append(kTestPolicyName1);
  base::Value::List merge_list_2;
  merge_list_2.Append(kTestPolicyName2);

  PolicyMap policy_map_1;
  if (IsUserAffiliated()) {
    base::flat_set<std::string> affiliation_ids;
    affiliation_ids.insert("12345");
    policy_map_1.SetUserAffiliationIds(affiliation_ids);
    policy_map_1.SetDeviceAffiliationIds(affiliation_ids);
  }
  policy_map_1.Set(key::kCloudPolicyOverridesPlatformPolicy,
                   POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                   POLICY_SOURCE_PLATFORM,
                   base::Value(CloudPolicyOverridesPlatformPolicy()), nullptr);
  policy_map_1.Set(key::kCloudUserPolicyOverridesCloudMachinePolicy,
                   POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                   POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  policy_map_1.Set(key::kPolicyListMultipleSourceMergeList,
                   POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                   POLICY_SOURCE_PLATFORM, base::Value(merge_list_1.Clone()),
                   nullptr);

  PolicyMap policy_map_2;
  policy_map_2.Set(key::kCloudPolicyOverridesPlatformPolicy,
                   POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                   POLICY_SOURCE_CLOUD, base::Value(false), nullptr);
  policy_map_2.Set(
      key::kCloudUserPolicyOverridesCloudMachinePolicy, POLICY_LEVEL_MANDATORY,
      POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      base::Value(CloudUserPolicyOverridesCloudMachinePolicy()), nullptr);
  policy_map_2.Set(key::kPolicyListMultipleSourceMergeList,
                   POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                   POLICY_SOURCE_CLOUD, base::Value(merge_list_2.Clone()),
                   nullptr);

  PolicyMap policy_map_expected;
  PopulateExpectedMetapolicyMap(policy_map_expected, policy_map_1, policy_map_2,
                                std::move(merge_list_1),
                                std::move(merge_list_2));

  policy_map_1.MergeFrom(policy_map_2);

  EXPECT_TRUE(policy_map_1.Equals(policy_map_expected));
}

INSTANTIATE_TEST_SUITE_P(PolicyMapMergeTestInstance,
                         PolicyMapMergeTest,
                         testing::Combine(testing::Values(false, true),
                                          testing::Values(false, true),
                                          testing::Values(false, true),
                                          testing::Values(false, true)));

class PolicyMapPriorityTest
    : public testing::TestWithParam<
          std::tuple</*cloud_policy_overrides_platform_policy=*/bool,
                     /*cloud_user_policy_overrides_cloud_machine_policy=*/bool,
                     /*is_user_affiliated=*/bool>> {
 public:
  bool CloudPolicyOverridesPlatformPolicy() { return std::get<0>(GetParam()); }

  bool CloudUserPolicyOverridesCloudMachinePolicy() {
    return std::get<1>(GetParam());
  }

  bool IsUserAffiliated() { return std::get<2>(GetParam()); }

  void SetUp() override {
    // Update the metapolicy values.
    policy_map_.Set(key::kCloudPolicyOverridesPlatformPolicy,
                    POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                    POLICY_SOURCE_PLATFORM,
                    base::Value(CloudPolicyOverridesPlatformPolicy()), nullptr);
    policy_map_.Set(
        key::kCloudUserPolicyOverridesCloudMachinePolicy,
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
        base::Value(CloudUserPolicyOverridesCloudMachinePolicy()), nullptr);
    // Causes the stored metapolicy values to be updated.
    PolicyMap policy_map_empty;
    policy_map_.MergeFrom(policy_map_empty);

    if (IsUserAffiliated()) {
      base::flat_set<std::string> affiliation_ids;
      affiliation_ids.insert("a");
      policy_map_.SetUserAffiliationIds(affiliation_ids);
      policy_map_.SetDeviceAffiliationIds(affiliation_ids);
    }
  }

  void CheckPriorityConditions() {
    PolicyMap::Entry platform_machine(
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
        base::Value(), nullptr);
    PolicyMap::Entry platform_user(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                                   POLICY_SOURCE_PLATFORM, base::Value(),
                                   nullptr);
    PolicyMap::Entry cloud_machine(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                   POLICY_SOURCE_CLOUD, base::Value(), nullptr);
    PolicyMap::Entry cloud_user(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                                POLICY_SOURCE_CLOUD, base::Value(), nullptr);
    PolicyMap::Entry command_line(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                  POLICY_SOURCE_COMMAND_LINE, base::Value(),
                                  nullptr);
    PolicyMap::Entry enterprise_default(
        POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
        POLICY_SOURCE_ENTERPRISE_DEFAULT, base::Value(), nullptr);

    // The policy priority conditions depend on the precedence metapolicy values
    // and user affiliation.
    if (CloudPolicyOverridesPlatformPolicy() &&
        CloudUserPolicyOverridesCloudMachinePolicy() && IsUserAffiliated()) {
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(platform_machine, platform_user));
      EXPECT_FALSE(
          policy_map_.EntryHasHigherPriority(platform_machine, cloud_machine));
      EXPECT_FALSE(
          policy_map_.EntryHasHigherPriority(platform_machine, cloud_user));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(cloud_machine, platform_user));
      EXPECT_FALSE(
          policy_map_.EntryHasHigherPriority(cloud_machine, cloud_user));
      EXPECT_FALSE(
          policy_map_.EntryHasHigherPriority(platform_user, cloud_user));
      EXPECT_TRUE(policy_map_.EntryHasHigherPriority(cloud_user, command_line));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(cloud_user, enterprise_default));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(command_line, enterprise_default));
    } else if (CloudUserPolicyOverridesCloudMachinePolicy() &&
               IsUserAffiliated()) {
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(platform_machine, platform_user));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(platform_machine, cloud_machine));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(platform_machine, cloud_user));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(cloud_machine, platform_user));
      EXPECT_FALSE(
          policy_map_.EntryHasHigherPriority(cloud_machine, cloud_user));
      EXPECT_FALSE(
          policy_map_.EntryHasHigherPriority(platform_user, cloud_user));
      EXPECT_TRUE(policy_map_.EntryHasHigherPriority(cloud_user, command_line));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(cloud_user, enterprise_default));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(command_line, enterprise_default));
    } else if (CloudPolicyOverridesPlatformPolicy()) {
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(platform_machine, platform_user));
      EXPECT_FALSE(
          policy_map_.EntryHasHigherPriority(platform_machine, cloud_machine));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(platform_machine, cloud_user));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(cloud_machine, platform_user));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(cloud_machine, cloud_user));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(platform_user, cloud_user));
      EXPECT_TRUE(policy_map_.EntryHasHigherPriority(cloud_user, command_line));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(cloud_user, enterprise_default));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(command_line, enterprise_default));
    } else {
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(platform_machine, platform_user));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(platform_machine, cloud_machine));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(platform_machine, cloud_user));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(cloud_machine, platform_user));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(cloud_machine, cloud_user));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(platform_user, cloud_user));
      EXPECT_TRUE(policy_map_.EntryHasHigherPriority(cloud_user, command_line));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(cloud_user, enterprise_default));
      EXPECT_TRUE(
          policy_map_.EntryHasHigherPriority(command_line, enterprise_default));
    }
  }

  PolicyMap policy_map_;
};

TEST_P(PolicyMapPriorityTest, PriorityCheck) {
  CheckPriorityConditions();
}

TEST_P(PolicyMapPriorityTest, SingleProfilePolicy) {
  PolicyMap::Entry platform_machine(
      POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE, POLICY_SOURCE_PLATFORM,
      base::Value(), nullptr, &kUserCloudDetails);
  PolicyMap::Entry platform_user(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                                 POLICY_SOURCE_PLATFORM, base::Value(), nullptr,
                                 &kUserCloudDetails);
  PolicyMap::Entry cloud_machine(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                 POLICY_SOURCE_CLOUD, base::Value(), nullptr,
                                 &kUserCloudDetails);
  PolicyMap::Entry cloud_user(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                              POLICY_SOURCE_CLOUD, base::Value(), nullptr,
                              &kUserCloudDetails);
  PolicyMap::Entry command_line(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                POLICY_SOURCE_COMMAND_LINE, base::Value(),
                                nullptr, &kUserCloudDetails);

  EXPECT_TRUE(policy_map_.EntryHasHigherPriority(cloud_user, cloud_machine));
  EXPECT_TRUE(policy_map_.EntryHasHigherPriority(cloud_user, platform_user));
  EXPECT_TRUE(policy_map_.EntryHasHigherPriority(cloud_user, platform_machine));
  EXPECT_TRUE(policy_map_.EntryHasHigherPriority(cloud_user, platform_machine));
}

TEST_P(PolicyMapPriorityTest, SingleProfilePolicyWithMissingDetails) {
  PolicyMap::Entry cloud_machine(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                 POLICY_SOURCE_CLOUD, base::Value(), nullptr,
                                 &kUserCloudDetails);
  PolicyMap::Entry cloud_user(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_USER,
                              POLICY_SOURCE_CLOUD, base::Value(), nullptr,
                              nullptr);
  EXPECT_TRUE(policy_map_.EntryHasHigherPriority(cloud_user, cloud_machine));
  EXPECT_FALSE(policy_map_.EntryHasHigherPriority(cloud_machine, cloud_user));
}

INSTANTIATE_TEST_SUITE_P(PolicyMapPriorityTestInstance,
                         PolicyMapPriorityTest,
                         testing::Combine(testing::Values(false, true),
                                          testing::Values(false, true),
                                          testing::Values(false, true)));
#endif  // !BUILDFLAG(IS_CHROMEOS)

}  // namespace policy
