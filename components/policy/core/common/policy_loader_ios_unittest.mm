// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_ios.h"

#import <UIKit/UIKit.h>

#include <memory>

#include "base/apple/scoped_cftyperef.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/json/json_string_value_serializer.h"
#include "base/strings/sys_string_conversions.h"
#import "base/task/sequenced_task_runner.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#import "base/time/time.h"
#include "base/types/expected.h"
#include "base/types/expected_macros.h"
#include "base/values.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/configuration_policy_provider_test.h"
#include "components/policy/core/common/policy_bundle.h"
#import "components/policy/core/common/policy_loader_ios_constants.h"
#include "components/policy/core/common/policy_map.h"
#import "components/policy/core/common/policy_namespace.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace policy {

namespace {

class TestHarness : public PolicyProviderTestHarness {
 public:
  TestHarness(bool encode_complex_data_as_json);
  TestHarness(const TestHarness&) = delete;
  TestHarness& operator=(const TestHarness&) = delete;
  ~TestHarness() override;

  void SetUp() override;

  ConfigurationPolicyProvider* CreateProvider(
      SchemaRegistry* registry,
      scoped_refptr<base::SequencedTaskRunner> task_runner) override;

  void InstallEmptyPolicy() override;
  void InstallStringPolicy(const std::string& policy_name,
                           const std::string& policy_value) override;
  void InstallIntegerPolicy(const std::string& policy_name,
                            int policy_value) override;
  void InstallBooleanPolicy(const std::string& policy_name,
                            bool policy_value) override;
  void InstallStringListPolicy(const std::string& policy_name,
                               const base::Value::List& policy_value) override;
  void InstallDictionaryPolicy(const std::string& policy_name,
                               const base::Value::Dict& policy_value) override;

  static PolicyProviderTestHarness* Create();
  static PolicyProviderTestHarness* CreateWithJSONEncoding();

 private:
  // Merges the policies in |policy| into the current policy dictionary
  // in NSUserDefaults, after making sure that the policy dictionary
  // exists.
  void AddPolicies(NSDictionary* policy);

  // If true, the test harness will encode complex data (dicts and lists) as
  // JSON strings.
  bool encode_complex_data_as_json_;
};

TestHarness::TestHarness(bool encode_complex_data_as_json)
    : PolicyProviderTestHarness(POLICY_LEVEL_MANDATORY,
                                POLICY_SCOPE_MACHINE,
                                POLICY_SOURCE_PLATFORM),
      encode_complex_data_as_json_(encode_complex_data_as_json) {}

TestHarness::~TestHarness() {
  // Cleanup any policies left from the test.
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
}

void TestHarness::SetUp() {
  // Make sure there is no pre-existing policy present.
  [[NSUserDefaults standardUserDefaults]
      removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
}

ConfigurationPolicyProvider* TestHarness::CreateProvider(
    SchemaRegistry* registry,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return new AsyncPolicyProvider(
      registry, std::make_unique<PolicyLoaderIOS>(registry, task_runner));
}

void TestHarness::InstallEmptyPolicy() {
  AddPolicies(@{});
}

void TestHarness::InstallStringPolicy(const std::string& policy_name,
                                      const std::string& policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  NSString* value = base::SysUTF8ToNSString(policy_value);
  AddPolicies(@{
      key: value
  });
}

void TestHarness::InstallIntegerPolicy(const std::string& policy_name,
                                       int policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  AddPolicies(@{
      key: [NSNumber numberWithInt:policy_value]
  });
}

void TestHarness::InstallBooleanPolicy(const std::string& policy_name,
                                       bool policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  AddPolicies(@{
      key: [NSNumber numberWithBool:policy_value]
  });
}

void TestHarness::InstallStringListPolicy(
    const std::string& policy_name,
    const base::Value::List& policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  base::apple::ScopedCFTypeRef<CFPropertyListRef> value =
      ValueToProperty(base::Value(policy_value.Clone()));

  if (encode_complex_data_as_json_) {
    // Convert |policy_value| to a JSON-encoded string.
    std::string json_string;
    JSONStringValueSerializer serializer(&json_string);
    ASSERT_TRUE(serializer.Serialize(policy_value));

    AddPolicies(@{key : base::SysUTF8ToNSString(json_string)});
  } else {
    AddPolicies(@{key : (__bridge NSArray*)(value.get())});
  }
}

void TestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::Value::Dict& policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);

  if (encode_complex_data_as_json_) {
    // Convert |policy_value| to a JSON-encoded string.
    std::string json_string;
    JSONStringValueSerializer serializer(&json_string);
    ASSERT_TRUE(serializer.Serialize(policy_value));

    AddPolicies(@{key : base::SysUTF8ToNSString(json_string)});
  } else {
    base::apple::ScopedCFTypeRef<CFPropertyListRef> value =
        ValueToProperty(base::Value(policy_value.Clone()));
    AddPolicies(@{key : (__bridge NSDictionary*)(value.get())});
  }
}

// static
PolicyProviderTestHarness* TestHarness::Create() {
  return new TestHarness(false);
}

// static
PolicyProviderTestHarness* TestHarness::CreateWithJSONEncoding() {
  return new TestHarness(true);
}

void TestHarness::AddPolicies(NSDictionary* policy) {
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  NSMutableDictionary* chromePolicy = [[NSMutableDictionary alloc] init];

  NSDictionary* previous =
      [defaults dictionaryForKey:kPolicyLoaderIOSConfigurationKey];
  if (previous) {
    [chromePolicy addEntriesFromDictionary:previous];
  }

  [chromePolicy addEntriesFromDictionary:policy];
  [[NSUserDefaults standardUserDefaults]
      setObject:chromePolicy
         forKey:kPolicyLoaderIOSConfigurationKey];
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(PolicyProviderIOSChromePolicyTest,
                         ConfigurationPolicyProviderTest,
                         testing::Values(TestHarness::Create));

INSTANTIATE_TEST_SUITE_P(PolicyProviderIOSChromePolicyJSONTest,
                         ConfigurationPolicyProviderTest,
                         testing::Values(TestHarness::CreateWithJSONEncoding));

const char kTestChromeSchema[] =
    "{"
    "  \"type\": \"object\","
    "  \"properties\": {"
    "    \"StringPolicy\": { \"type\": \"string\" },"
    "  }"
    "}";

PolicyNamespace GetPolicyNamespace() {
  return PolicyNamespace(POLICY_DOMAIN_CHROME, "");
}

// Tests cases not covered by the test harnest.
class PolicyLoaderIosTest : public PlatformTest {
 protected:
  PolicyLoaderIosTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override {
    auto result = RegisterSchema(GetPolicyNamespace(), kTestChromeSchema);
    ASSERT_TRUE(result.has_value())
        << "Registration of schema failed with error: " << result.error();
  }

  void TearDown() override {
    task_environment_.RunUntilIdle();
    provider_->Shutdown();
    // Clear App Config.
    [[NSUserDefaults standardUserDefaults]
        removeObjectForKey:kPolicyLoaderIOSConfigurationKey];
  }

  void InitProvider() {
    std::unique_ptr<PolicyLoaderIOS> loader = std::make_unique<PolicyLoaderIOS>(
        &schema_registry_, task_environment_.GetMainThreadTaskRunner());
    provider_ = std::make_unique<AsyncPolicyProvider>(&schema_registry_,
                                                      std::move(loader));
    provider_->Init(&schema_registry_);
    task_environment_.RunUntilIdle();
  }

  base::test::TaskEnvironment task_environment_;
  SchemaRegistry schema_registry_;
  std::unique_ptr<AsyncPolicyProvider> provider_;

 private:
  base::expected<void, std::string> RegisterSchema(const PolicyNamespace& ns,
                                                   const std::string& schema) {
    ASSIGN_OR_RETURN(const auto parsed_schema, Schema::Parse(schema));
    schema_registry_.RegisterComponent(ns, parsed_schema);
    return base::ok();
  }
};

TEST_F(PolicyLoaderIosTest, ReloadIntervalWhenBrowserManagedPostStartup) {
  InitProvider();

  // Verify that there are no policies loaded at startup.
  EXPECT_TRUE(provider_->policies().Equals(PolicyBundle()));

  // Set a policy in the App Config at runtime to replicate the situation where
  // the browser is put under management after startup.
  NSDictionary* policies = @{@"StringPolicy" : @"string_value"};
  [[NSUserDefaults standardUserDefaults]
      setObject:policies
         forKey:kPolicyLoaderIOSConfigurationKey];

  // Verify that the new policy changes aren't picked up after 10 minutes
  // when the browser wasn't managed at startup. This is to make sure that the
  // first reload was scheduled with an interval of 15 minutes.
  task_environment_.FastForwardBy(base::Minutes(10));
  EXPECT_TRUE(provider_->policies().Equals(PolicyBundle()));

  // Verify that the new policy changes are picked up after 15 minutes +
  // epsilon.
  task_environment_.FastForwardBy(base::Minutes(5) + base::Seconds(1));
  ASSERT_TRUE(task_environment_.MainThreadIsIdle());
  PolicyBundle bundle;
  bundle.Get(GetPolicyNamespace())
      .Set("StringPolicy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
           POLICY_SOURCE_PLATFORM, base::Value("string_value"), nullptr);
  EXPECT_TRUE(provider_->policies().Equals(bundle));

  // Simulate the situation where the App Config is updated with new values
  // before the next reload.
  NSDictionary* new_policies = @{@"StringPolicy" : @"string_value_new"};
  [[NSUserDefaults standardUserDefaults]
      setObject:new_policies
         forKey:kPolicyLoaderIOSConfigurationKey];

  // Verify that the interval was adjusted to 30 seconds after first reload.
  task_environment_.FastForwardBy(base::Seconds(31));
  ASSERT_TRUE(task_environment_.MainThreadIsIdle());
  PolicyBundle new_bundle;
  new_bundle.Get(GetPolicyNamespace())
      .Set("StringPolicy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
           POLICY_SOURCE_PLATFORM, base::Value("string_value_new"), nullptr);
  EXPECT_TRUE(provider_->policies().Equals(new_bundle));
}

TEST_F(PolicyLoaderIosTest, ReloadIntervalWhenManagedByPlatformBeforeStartup) {
  // Set a policy in the App Config to replicate the situation where the browser
  // is put under management before startup.
  NSDictionary* policies = @{@"StringPolicy" : @"string_value"};
  [[NSUserDefaults standardUserDefaults]
      setObject:policies
         forKey:kPolicyLoaderIOSConfigurationKey];

  InitProvider();

  // Verify that the new policy changes are picked up after 30 seconds +
  // epsilon.
  task_environment_.FastForwardBy(base::Seconds(31));
  ASSERT_TRUE(task_environment_.MainThreadIsIdle());
  PolicyBundle bundle;
  bundle.Get(GetPolicyNamespace())
      .Set("StringPolicy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
           POLICY_SOURCE_PLATFORM, base::Value("string_value"), nullptr);
  EXPECT_TRUE(provider_->policies().Equals(bundle));
}

}  // namespace policy
