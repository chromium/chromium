// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/configuration_policy_provider_test.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/policy/core/common/chrome_schema.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_migrator.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::_;
using ::testing::Mock;

namespace policy {

const char kTestChromeSchema[] =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"StringPolicy\": { \"type\": \"string\" },"
    "    \"BooleanPolicy\": { \"type\": \"boolean\" },"
    "    \"IntegerPolicy\": { \"type\": \"integer\" },"
    "    \"StringListPolicy\": {"
    "      \"type\": \"array\","
    "      \"items\": { \"type\": \"string\" }"
    "    },"
    "    \"DictionaryPolicy\": {"
    "      \"type\": \"object\","
    "      \"properties\": {"
    "        \"bool\": { \"type\": \"boolean\" },"
    "        \"double\": { \"type\": \"number\" },"
    "        \"int\": { \"type\": \"integer\" },"
    "        \"string\": { \"type\": \"string\" },"
    "        \"array\": {"
    "          \"type\": \"array\","
    "          \"items\": { \"type\": \"string\" }"
    "        },"
    "        \"dictionary\": {"
    "          \"type\": \"object\","
    "          \"properties\": {"
    "            \"sub\": { \"type\": \"string\" },"
    "            \"sublist\": {"
    "              \"type\": \"array\","
    "              \"items\": {"
    "                \"type\": \"object\","
    "                \"properties\": {"
    "                  \"aaa\": { \"type\": \"integer\" },"
    "                  \"bbb\": { \"type\": \"integer\" },"
    "                  \"ccc\": { \"type\": \"string\" },"
    "                  \"ddd\": { \"type\": \"string\" }"
    "                }"
    "              }"
    "            }"
    "          }"
    "        },"
    "        \"list\": {"
    "          \"type\": \"array\","
    "          \"items\": {"
    "            \"type\": \"object\","
    "            \"properties\": {"
    "              \"subdictindex\": { \"type\": \"integer\" },"
    "              \"subdict\": {"
    "                \"type\": \"object\","
    "                \"properties\": {"
    "                  \"bool\": { \"type\": \"boolean\" },"
    "                  \"double\": { \"type\": \"number\" },"
    "                  \"int\": { \"type\": \"integer\" },"
    "                  \"string\": { \"type\": \"string\" }"
    "                }"
    "              }"
    "            }"
    "          }"
    "        },"
    "        \"dict\": {"
    "          \"type\": \"object\","
    "          \"properties\": {"
    "            \"bool\": { \"type\": \"boolean\" },"
    "            \"double\": { \"type\": \"number\" },"
    "            \"int\": { \"type\": \"integer\" },"
    "            \"string\": { \"type\": \"string\" },"
    "            \"list\": {"
    "              \"type\": \"array\","
    "              \"items\": {"
    "                \"type\": \"object\","
    "                \"properties\": {"
    "                  \"subdictindex\": { \"type\": \"integer\" },"
    "                  \"subdict\": {"
    "                    \"type\": \"object\","
    "                    \"properties\": {"
    "                      \"bool\": { \"type\": \"boolean\" },"
    "                      \"double\": { \"type\": \"number\" },"
    "                      \"int\": { \"type\": \"integer\" },"
    "                      \"string\": { \"type\": \"string\" }"
    "                    }"
    "                  }"
    "                }"
    "              }"
    "            }"
    "          }"
    "        }"
    "      }"
    "    }"
    "  }"
    "}";

namespace test_keys {

const char kKeyString[] = "StringPolicy";
const char kKeyBoolean[] = "BooleanPolicy";
const char kKeyInteger[] = "IntegerPolicy";
const char kKeyStringList[] = "StringListPolicy";
const char kKeyDictionary[] = "DictionaryPolicy";

}  // namespace test_keys

PolicyTestBase::PolicyTestBase() = default;

PolicyTestBase::~PolicyTestBase() = default;

void PolicyTestBase::SetUp() {
  const PolicyNamespace ns(POLICY_DOMAIN_CHROME, "");
  ASSERT_TRUE(RegisterSchema(ns, kTestChromeSchema));
}

void PolicyTestBase::TearDown() {
  task_environment_.RunUntilIdle();
}

bool PolicyTestBase::RegisterSchema(const PolicyNamespace& ns,
                                    const std::string& schema_string) {
  ASSIGN_OR_RETURN(const auto schema, Schema::Parse(schema_string),
                   [](const auto& e) {
                     ADD_FAILURE() << e;
                     return false;
                   });
  schema_registry_.RegisterComponent(ns, schema);
  return true;
}

void PolicyTestBase::RegisterChromeSchema(const PolicyNamespace& ns) {
  schema_registry_.RegisterComponent(ns, policy::GetChromeSchema());
}

PolicyProviderTestHarness::PolicyProviderTestHarness(PolicyLevel level,
                                                     PolicyScope scope,
                                                     PolicySource source)
    : level_(level), scope_(scope), source_(source) {}

PolicyProviderTestHarness::~PolicyProviderTestHarness() = default;

PolicyLevel PolicyProviderTestHarness::policy_level() const {
  return level_;
}

PolicyScope PolicyProviderTestHarness::policy_scope() const {
  return scope_;
}

PolicySource PolicyProviderTestHarness::policy_source() const {
  return source_;
}

void PolicyProviderTestHarness::Install3rdPartyPolicy(
    const base::Value::Dict& policies) {
  FAIL();
}

ConfigurationPolicyProviderTest::ConfigurationPolicyProviderTest() = default;

ConfigurationPolicyProviderTest::~ConfigurationPolicyProviderTest() = default;

void ConfigurationPolicyProviderTest::SetUp() {
  PolicyTestBase::SetUp();

  test_harness_.reset((*GetParam())());
  ASSERT_NO_FATAL_FAILURE(test_harness_->SetUp());

  const PolicyNamespace chrome_ns(POLICY_DOMAIN_CHROME, "");
  Schema chrome_schema = *schema_registry_.schema_map()->GetSchema(chrome_ns);
  Schema extension_schema =
      chrome_schema.GetKnownProperty(test_keys::kKeyDictionary);
  ASSERT_TRUE(extension_schema.valid());
  schema_registry_.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"),
      extension_schema);
  schema_registry_.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"),
      extension_schema);
  schema_registry_.RegisterComponent(
      PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                      "cccccccccccccccccccccccccccccccc"),
      extension_schema);

  provider_.reset(test_harness_->CreateProvider(
      &schema_registry_,
      base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()})));
  provider_->Init(&schema_registry_);
  // Some providers do a reload on init. Make sure any notifications generated
  // are fired now.
  task_environment_.RunUntilIdle();

  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(provider_->policies().Equals(kEmptyBundle));
}

void ConfigurationPolicyProviderTest::TearDown() {
  test_harness_->TearDown();

  // Give providers the chance to clean up after themselves on the file thread.
  provider_->Shutdown();
  provider_.reset();

  PolicyTestBase::TearDown();
}

void ConfigurationPolicyProviderTest::CheckValue(
    const char* policy_name,
    const base::Value& expected_value,
    base::OnceClosure install_value) {
  // Install the value, reload policy and check the provider for the value.
  std::move(install_value).Run();
  provider_->RefreshPolicies(PolicyFetchReason::kTest);
  task_environment_.RunUntilIdle();
  PolicyBundle expected_bundle;
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(policy_name, test_harness_->policy_level(),
           test_harness_->policy_scope(), test_harness_->policy_source(),
           expected_value.Clone(), nullptr);
  EXPECT_TRUE(provider_->policies().Equals(expected_bundle));
}

TEST_P(ConfigurationPolicyProviderTest, Empty) {
  provider_->RefreshPolicies(PolicyFetchReason::kTest);
  task_environment_.RunUntilIdle();
  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(provider_->policies().Equals(kEmptyBundle));
}

TEST_P(ConfigurationPolicyProviderTest, StringValue) {
  const char kTestString[] = "string_value";
  base::Value expected_value(kTestString);
  CheckValue(test_keys::kKeyString, expected_value,
             base::BindOnce(&PolicyProviderTestHarness::InstallStringPolicy,
                            base::Unretained(test_harness_.get()),
                            test_keys::kKeyString, kTestString));
}

TEST_P(ConfigurationPolicyProviderTest, BooleanValue) {
  base::Value expected_value(true);
  CheckValue(test_keys::kKeyBoolean, expected_value,
             base::BindOnce(&PolicyProviderTestHarness::InstallBooleanPolicy,
                            base::Unretained(test_harness_.get()),
                            test_keys::kKeyBoolean, true));
}

TEST_P(ConfigurationPolicyProviderTest, IntegerValue) {
  base::Value expected_value(42);
  CheckValue(test_keys::kKeyInteger, expected_value,
             base::BindOnce(&PolicyProviderTestHarness::InstallIntegerPolicy,
                            base::Unretained(test_harness_.get()),
                            test_keys::kKeyInteger, 42));
}

TEST_P(ConfigurationPolicyProviderTest, StringListValue) {
  base::Value::List expected_value;
  expected_value.Append("first");
  expected_value.Append("second");
  CheckValue(test_keys::kKeyStringList, base::Value(expected_value.Clone()),
             base::BindOnce(&PolicyProviderTestHarness::InstallStringListPolicy,
                            base::Unretained(test_harness_.get()),
                            test_keys::kKeyStringList, expected_value.Clone()));
}

TEST_P(ConfigurationPolicyProviderTest, DictionaryValue) {
  base::Value::Dict expected_value;
  expected_value.Set("bool", true);
  expected_value.Set("double", 123.456);
  expected_value.Set("int", 123);
  expected_value.Set("string", "omg");

  {
    base::Value::List list;
    list.Append("first");
    list.Append("second");
    expected_value.Set("array", std::move(list));
  }

  base::Value::List sublist;
  {
    base::Value::Dict sub;
    sub.Set("aaa", 111);
    sub.Set("bbb", 222);
    sublist.Append(std::move(sub));
  }

  {
    base::Value::Dict sub;
    sub.Set("ccc", "333");
    sub.Set("ddd", "444");
    sublist.Append(std::move(sub));
  }

  base::Value::Dict dict;
  dict.Set("sub", "value");
  dict.Set("sublist", std::move(sublist));
  expected_value.Set("dictionary", std::move(dict));

  CheckValue(test_keys::kKeyDictionary, base::Value(expected_value.Clone()),
             base::BindOnce(&PolicyProviderTestHarness::InstallDictionaryPolicy,
                            base::Unretained(test_harness_.get()),
                            test_keys::kKeyDictionary, expected_value.Clone()));
}

TEST_P(ConfigurationPolicyProviderTest, RefreshPolicies) {
  PolicyBundle bundle;
  EXPECT_TRUE(provider_->policies().Equals(bundle));

  // OnUpdatePolicy is called even when there are no changes.
  MockConfigurationPolicyObserver observer;
  provider_->AddObserver(&observer);
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  provider_->RefreshPolicies(PolicyFetchReason::kTest);
  task_environment_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_TRUE(provider_->policies().Equals(bundle));

  // OnUpdatePolicy is called when there are changes.
  test_harness_->InstallStringPolicy(test_keys::kKeyString, "value");
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  provider_->RefreshPolicies(PolicyFetchReason::kTest);
  task_environment_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(test_keys::kKeyString, test_harness_->policy_level(),
           test_harness_->policy_scope(), test_harness_->policy_source(),
           base::Value("value"), nullptr);
  EXPECT_TRUE(provider_->policies().Equals(bundle));
  provider_->RemoveObserver(&observer);
}

Configuration3rdPartyPolicyProviderTest::
    Configuration3rdPartyPolicyProviderTest() = default;

Configuration3rdPartyPolicyProviderTest::
    ~Configuration3rdPartyPolicyProviderTest() = default;

TEST_P(Configuration3rdPartyPolicyProviderTest, Load3rdParty) {
  base::Value::Dict policy_dict;
  policy_dict.Set("bool", true);
  policy_dict.Set("double", 123.456);
  policy_dict.Set("int", 789);
  policy_dict.Set("string", "string value");

  base::Value::List list;
  for (int i = 0; i < 2; ++i) {
    base::Value::Dict dict;
    dict.Set("subdictindex", i);
    dict.Set("subdict", policy_dict.Clone());
    list.Append(std::move(dict));
  }
  policy_dict.Set("list", std::move(list));
  policy_dict.Set("dict", policy_dict.Clone());

  // Install these policies as a Chrome policy.
  test_harness_->InstallDictionaryPolicy(test_keys::kKeyDictionary,
                                         policy_dict.Clone());
  // Install them as 3rd party policies too.
  base::Value::Dict policy_3rdparty;
  policy_3rdparty.SetByDottedPath("extensions.aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
                                  base::Value(policy_dict.Clone()));
  policy_3rdparty.SetByDottedPath("extensions.bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
                                  base::Value(policy_dict.Clone()));
  // Install invalid 3rd party policies that shouldn't be loaded. These also
  // help detecting memory leaks in the code paths that detect invalid input.
  policy_3rdparty.SetByDottedPath("invalid-domain.component",
                                  base::Value(policy_dict.Clone()));
  policy_3rdparty.SetByDottedPath("extensions.cccccccccccccccccccccccccccccccc",
                                  "invalid-value");
  test_harness_->Install3rdPartyPolicy(policy_3rdparty);

  provider_->RefreshPolicies(PolicyFetchReason::kTest);
  task_environment_.RunUntilIdle();

  PolicyMap expected_policy;
  expected_policy.Set(test_keys::kKeyDictionary, test_harness_->policy_level(),
                      test_harness_->policy_scope(),
                      test_harness_->policy_source(),
                      base::Value(policy_dict.Clone()), nullptr);
  PolicyBundle expected_bundle;
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())) =
      expected_policy.Clone();
  expected_policy.Clear();
  expected_policy.LoadFrom(policy_dict, test_harness_->policy_level(),
                           test_harness_->policy_scope(),
                           test_harness_->policy_source());
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa")) =
      expected_policy.Clone();
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                                      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb")) =
      expected_policy.Clone();
  EXPECT_TRUE(provider_->policies().Equals(expected_bundle));
}

// These are abstract policy provider tests, which are instantiated for each
// policy provider implementation.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ConfigurationPolicyProviderTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(
    Configuration3rdPartyPolicyProviderTest);

}  // namespace policy
