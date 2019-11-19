// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/configuration_policy_provider_test.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/values.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/extension_policy_migrator.h"
#include "components/policy/core/common/external_data_fetcher.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_types.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::Mock;
using ::testing::_;

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

PolicyTestBase::PolicyTestBase() {}

PolicyTestBase::~PolicyTestBase() {}

void PolicyTestBase::SetUp() {
  const PolicyNamespace ns(POLICY_DOMAIN_CHROME, "");
  ASSERT_TRUE(RegisterSchema(ns, kTestChromeSchema));
}

void PolicyTestBase::TearDown() {
  task_environment_.RunUntilIdle();
}

bool PolicyTestBase::RegisterSchema(const PolicyNamespace& ns,
                                    const std::string& schema_string) {
  std::string error;
  Schema schema = Schema::Parse(schema_string, &error);
  if (schema.valid()) {
    schema_registry_.RegisterComponent(ns, schema);
    return true;
  }
  ADD_FAILURE() << error;
  return false;
}

PolicyProviderTestHarness::PolicyProviderTestHarness(PolicyLevel level,
                                                     PolicyScope scope,
                                                     PolicySource source)
    : level_(level), scope_(scope), source_(source) {}

PolicyProviderTestHarness::~PolicyProviderTestHarness() {}

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
    const base::DictionaryValue* policies) {
  FAIL();
}

ConfigurationPolicyProviderTest::ConfigurationPolicyProviderTest() {}

ConfigurationPolicyProviderTest::~ConfigurationPolicyProviderTest() {}

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
      base::CreateSequencedTaskRunner({base::ThreadPool(), base::MayBlock()})));
  provider_->Init(&schema_registry_);
  // Some providers do a reload on init. Make sure any notifications generated
  // are fired now.
  task_environment_.RunUntilIdle();

  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(provider_->policies().Equals(kEmptyBundle));
}

void ConfigurationPolicyProviderTest::TearDown() {
  // Give providers the chance to clean up after themselves on the file thread.
  provider_->Shutdown();
  provider_.reset();

  PolicyTestBase::TearDown();
}

void ConfigurationPolicyProviderTest::CheckValue(
    const char* policy_name,
    const base::Value& expected_value,
    base::Closure install_value) {
  // Install the value, reload policy and check the provider for the value.
  install_value.Run();
  provider_->RefreshPolicies();
  task_environment_.RunUntilIdle();
  PolicyBundle expected_bundle;
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(policy_name, test_harness_->policy_level(),
           test_harness_->policy_scope(), test_harness_->policy_source(),
           expected_value.CreateDeepCopy(), nullptr);
  EXPECT_TRUE(provider_->policies().Equals(expected_bundle));
  // TODO(joaodasilva): set the policy in the POLICY_DOMAIN_EXTENSIONS too,
  // and extend the |expected_bundle|, once all providers are ready.
}

TEST_P(ConfigurationPolicyProviderTest, Empty) {
  provider_->RefreshPolicies();
  task_environment_.RunUntilIdle();
  const PolicyBundle kEmptyBundle;
  EXPECT_TRUE(provider_->policies().Equals(kEmptyBundle));
}

TEST_P(ConfigurationPolicyProviderTest, StringValue) {
  const char kTestString[] = "string_value";
  base::Value expected_value(kTestString);
  CheckValue(test_keys::kKeyString,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallStringPolicy,
                        base::Unretained(test_harness_.get()),
                        test_keys::kKeyString,
                        kTestString));
}

TEST_P(ConfigurationPolicyProviderTest, BooleanValue) {
  base::Value expected_value(true);
  CheckValue(test_keys::kKeyBoolean,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallBooleanPolicy,
                        base::Unretained(test_harness_.get()),
                        test_keys::kKeyBoolean,
                        true));
}

TEST_P(ConfigurationPolicyProviderTest, IntegerValue) {
  base::Value expected_value(42);
  CheckValue(test_keys::kKeyInteger,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallIntegerPolicy,
                        base::Unretained(test_harness_.get()),
                        test_keys::kKeyInteger,
                        42));
}

TEST_P(ConfigurationPolicyProviderTest, StringListValue) {
  base::ListValue expected_value;
  expected_value.AppendString("first");
  expected_value.AppendString("second");
  CheckValue(test_keys::kKeyStringList,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallStringListPolicy,
                        base::Unretained(test_harness_.get()),
                        test_keys::kKeyStringList,
                        &expected_value));
}

TEST_P(ConfigurationPolicyProviderTest, DictionaryValue) {
  base::DictionaryValue expected_value;
  expected_value.SetBoolean("bool", true);
  expected_value.SetDouble("double", 123.456);
  expected_value.SetInteger("int", 123);
  expected_value.SetString("string", "omg");

  auto list = std::make_unique<base::ListValue>();
  list->AppendString("first");
  list->AppendString("second");
  expected_value.Set("array", std::move(list));

  auto dict = std::make_unique<base::DictionaryValue>();
  dict->SetString("sub", "value");
  list = std::make_unique<base::ListValue>();
  auto sub = std::make_unique<base::DictionaryValue>();
  sub->SetInteger("aaa", 111);
  sub->SetInteger("bbb", 222);
  list->Append(std::move(sub));
  sub = std::make_unique<base::DictionaryValue>();
  sub->SetString("ccc", "333");
  sub->SetString("ddd", "444");
  list->Append(std::move(sub));
  dict->Set("sublist", std::move(list));
  expected_value.Set("dictionary", std::move(dict));

  CheckValue(test_keys::kKeyDictionary,
             expected_value,
             base::Bind(&PolicyProviderTestHarness::InstallDictionaryPolicy,
                        base::Unretained(test_harness_.get()),
                        test_keys::kKeyDictionary,
                        &expected_value));
}

TEST_P(ConfigurationPolicyProviderTest, RefreshPolicies) {
  PolicyBundle bundle;
  EXPECT_TRUE(provider_->policies().Equals(bundle));

  // OnUpdatePolicy is called even when there are no changes.
  MockConfigurationPolicyObserver observer;
  provider_->AddObserver(&observer);
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  provider_->RefreshPolicies();
  task_environment_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  EXPECT_TRUE(provider_->policies().Equals(bundle));

  // OnUpdatePolicy is called when there are changes.
  test_harness_->InstallStringPolicy(test_keys::kKeyString, "value");
  EXPECT_CALL(observer, OnUpdatePolicy(provider_.get())).Times(1);
  provider_->RefreshPolicies();
  task_environment_.RunUntilIdle();
  Mock::VerifyAndClearExpectations(&observer);

  bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .Set(test_keys::kKeyString, test_harness_->policy_level(),
           test_harness_->policy_scope(), test_harness_->policy_source(),
           std::make_unique<base::Value>("value"), nullptr);
  EXPECT_TRUE(provider_->policies().Equals(bundle));
  provider_->RemoveObserver(&observer);
}

class MockPolicyMigrator : public ExtensionPolicyMigrator {
 public:
  MOCK_METHOD1(Migrate, void(PolicyBundle* bundle));
};

TEST_P(ConfigurationPolicyProviderTest, AddMigrator) {
  MockPolicyMigrator* migrator = new MockPolicyMigrator;
  EXPECT_CALL(*migrator, Migrate(_));
  provider_->AddMigrator(std::unique_ptr<ExtensionPolicyMigrator>(migrator));
  provider_->RefreshPolicies();
  task_environment_.RunUntilIdle();
}

Configuration3rdPartyPolicyProviderTest::
    Configuration3rdPartyPolicyProviderTest() {}

Configuration3rdPartyPolicyProviderTest::
    ~Configuration3rdPartyPolicyProviderTest() {}

TEST_P(Configuration3rdPartyPolicyProviderTest, Load3rdParty) {
  base::DictionaryValue policy_dict;
  policy_dict.SetBoolean("bool", true);
  policy_dict.SetDouble("double", 123.456);
  policy_dict.SetInteger("int", 789);
  policy_dict.SetString("string", "string value");

  auto list = std::make_unique<base::ListValue>();
  for (int i = 0; i < 2; ++i) {
    auto dict = std::make_unique<base::DictionaryValue>();
    dict->SetInteger("subdictindex", i);
    dict->SetKey("subdict", policy_dict.Clone());
    list->Append(std::move(dict));
  }
  policy_dict.Set("list", std::move(list));
  policy_dict.SetKey("dict", policy_dict.Clone());

  // Install these policies as a Chrome policy.
  test_harness_->InstallDictionaryPolicy(test_keys::kKeyDictionary,
                                         &policy_dict);
  // Install them as 3rd party policies too.
  base::DictionaryValue policy_3rdparty;
  policy_3rdparty.SetPath({"extensions", "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"},
                          policy_dict.Clone());
  policy_3rdparty.SetPath({"extensions", "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"},
                          policy_dict.Clone());
  // Install invalid 3rd party policies that shouldn't be loaded. These also
  // help detecting memory leaks in the code paths that detect invalid input.
  policy_3rdparty.SetPath({"invalid-domain", "component"}, policy_dict.Clone());
  policy_3rdparty.SetString("extensions.cccccccccccccccccccccccccccccccc",
                            "invalid-value");
  test_harness_->Install3rdPartyPolicy(&policy_3rdparty);

  provider_->RefreshPolicies();
  task_environment_.RunUntilIdle();

  PolicyMap expected_policy;
  expected_policy.Set(test_keys::kKeyDictionary, test_harness_->policy_level(),
                      test_harness_->policy_scope(),
                      test_harness_->policy_source(),
                      policy_dict.CreateDeepCopy(), nullptr);
  PolicyBundle expected_bundle;
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()))
      .CopyFrom(expected_policy);
  expected_policy.Clear();
  expected_policy.LoadFrom(&policy_dict,
                           test_harness_->policy_level(),
                           test_harness_->policy_scope(),
                           test_harness_->policy_source());
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                                      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"))
      .CopyFrom(expected_policy);
  expected_bundle.Get(PolicyNamespace(POLICY_DOMAIN_EXTENSIONS,
                                      "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"))
      .CopyFrom(expected_policy);
  EXPECT_TRUE(provider_->policies().Equals(expected_bundle));
}

}  // namespace policy
