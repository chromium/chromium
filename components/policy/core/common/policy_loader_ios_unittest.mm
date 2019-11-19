// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/policy_loader_ios.h"

#import <UIKit/UIKit.h>

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/mac/scoped_cftyperef.h"
#include "base/macros.h"
#include "base/sequenced_task_runner.h"
#include "base/strings/sys_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/values.h"
#include "components/policy/core/common/async_policy_provider.h"
#include "components/policy/core/common/configuration_policy_provider_test.h"
#include "components/policy/core/common/policy_bundle.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_test_utils.h"
#include "components/policy/core/common/policy_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

// Key in the NSUserDefaults that contains the managed app configuration.
NSString* const kConfigurationKey = @"com.apple.configuration.managed";

class TestHarness : public PolicyProviderTestHarness {
 public:
  // If |use_encoded_key| is true then AddPolicies() serializes and encodes
  // the policies, and publishes them under the EncodedChromePolicy key.
  explicit TestHarness(bool use_encoded_key);
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
                               const base::ListValue* policy_value) override;
  void InstallDictionaryPolicy(
      const std::string& policy_name,
      const base::DictionaryValue* policy_value) override;

  static PolicyProviderTestHarness* Create();
  static PolicyProviderTestHarness* CreateWithEncodedKey();

 private:
  // Merges the policies in |policy| into the current policy dictionary
  // in NSUserDefaults, after making sure that the policy dictionary
  // exists.
  void AddPolicies(NSDictionary* policy);
  void AddChromePolicy(NSDictionary* policy);
  void AddEncodedChromePolicy(NSDictionary* policy);

  bool use_encoded_key_;

  DISALLOW_COPY_AND_ASSIGN(TestHarness);
};

TestHarness::TestHarness(bool use_encoded_key)
    : PolicyProviderTestHarness(POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                                POLICY_SOURCE_PLATFORM),
      use_encoded_key_(use_encoded_key) {}

TestHarness::~TestHarness() {
  // Cleanup any policies left from the test.
  [[NSUserDefaults standardUserDefaults] removeObjectForKey:kConfigurationKey];
}

void TestHarness::SetUp() {
  // Make sure there is no pre-existing policy present.
  [[NSUserDefaults standardUserDefaults] removeObjectForKey:kConfigurationKey];
}

ConfigurationPolicyProvider* TestHarness::CreateProvider(
    SchemaRegistry* registry,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  return new AsyncPolicyProvider(
      registry, std::make_unique<PolicyLoaderIOS>(task_runner));
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

void TestHarness::InstallStringListPolicy(const std::string& policy_name,
                                          const base::ListValue* policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  base::ScopedCFTypeRef<CFPropertyListRef> value(
      ValueToProperty(*policy_value));
  AddPolicies(@{
      key: static_cast<NSArray*>(value.get())
  });
}

void TestHarness::InstallDictionaryPolicy(
    const std::string& policy_name,
    const base::DictionaryValue* policy_value) {
  NSString* key = base::SysUTF8ToNSString(policy_name);
  base::ScopedCFTypeRef<CFPropertyListRef> value(
      ValueToProperty(*policy_value));
  AddPolicies(@{
      key: static_cast<NSDictionary*>(value.get())
  });
}

// static
PolicyProviderTestHarness* TestHarness::Create() {
  return new TestHarness(false);
}

// static
PolicyProviderTestHarness* TestHarness::CreateWithEncodedKey() {
  return new TestHarness(true);
}

void TestHarness::AddPolicies(NSDictionary* policy) {
  if (use_encoded_key_)
    AddEncodedChromePolicy(policy);
  else
    AddChromePolicy(policy);
}

void TestHarness::AddChromePolicy(NSDictionary* policy) {
  NSString* key = @"ChromePolicy";
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
  base::scoped_nsobject<NSMutableDictionary> chromePolicy(
      [[NSMutableDictionary alloc] init]);

  NSDictionary* previous = [defaults dictionaryForKey:key];
  if (previous)
    [chromePolicy addEntriesFromDictionary:previous];

  [chromePolicy addEntriesFromDictionary:policy];

  NSDictionary* wrapper = @{
      key: chromePolicy
  };
  [[NSUserDefaults standardUserDefaults] setObject:wrapper
                                            forKey:kConfigurationKey];
}

void TestHarness::AddEncodedChromePolicy(NSDictionary* policy) {
  NSString* key = @"EncodedChromePolicy";
  NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];

  base::scoped_nsobject<NSMutableDictionary> chromePolicy(
      [[NSMutableDictionary alloc] init]);

  NSString* previous = [defaults stringForKey:key];
  if (previous) {
    base::scoped_nsobject<NSData> data(
        [[NSData alloc] initWithBase64EncodedString:previous options:0]);
    NSDictionary* properties = [NSPropertyListSerialization
        propertyListWithData:data.get()
                     options:NSPropertyListImmutable
                      format:NULL
                       error:NULL];
    [chromePolicy addEntriesFromDictionary:properties];
  }

  [chromePolicy addEntriesFromDictionary:policy];

  NSData* data = [NSPropertyListSerialization
      dataWithPropertyList:chromePolicy
                    format:NSPropertyListXMLFormat_v1_0
                   options:0
                     error:NULL];
  NSString* encoded = [data base64EncodedStringWithOptions:0];

  NSDictionary* wrapper = @{
      key: encoded
  };
  [[NSUserDefaults standardUserDefaults] setObject:wrapper
                                            forKey:kConfigurationKey];
}

}  // namespace

INSTANTIATE_TEST_SUITE_P(PolicyProviderIOSChromePolicyTest,
                         ConfigurationPolicyProviderTest,
                         testing::Values(TestHarness::Create));

INSTANTIATE_TEST_SUITE_P(PolicyProviderIOSEncodedChromePolicyTest,
                         ConfigurationPolicyProviderTest,
                         testing::Values(TestHarness::CreateWithEncodedKey));

TEST(PolicyProviderIOSTest, ChromePolicyOverEncodedChromePolicy) {
  // This test verifies that if the "ChromePolicy" key is present then the
  // "EncodedChromePolicy" key is ignored.

  NSDictionary* policy = @{
    @"shared": @"wrong",
    @"key1": @"value1",
  };
  NSData* data = [NSPropertyListSerialization
      dataWithPropertyList:policy
                    format:NSPropertyListXMLFormat_v1_0
                   options:0
                     error:NULL];
  NSString* encodedChromePolicy = [data base64EncodedStringWithOptions:0];

  NSDictionary* chromePolicy = @{
    @"shared": @"right",
    @"key2": @"value2",
  };

  NSDictionary* wrapper = @{
    @"ChromePolicy": chromePolicy,
    @"EncodedChromePolicy": encodedChromePolicy,
  };

  [[NSUserDefaults standardUserDefaults] setObject:wrapper
                                            forKey:kConfigurationKey];

  PolicyBundle expected;
  PolicyMap& expectedMap =
      expected.Get(PolicyNamespace(POLICY_DOMAIN_CHROME, ""));
  expectedMap.Set("shared", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_PLATFORM,
                  std::make_unique<base::Value>("right"), nullptr);
  expectedMap.Set("key2", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
                  POLICY_SOURCE_PLATFORM,
                  std::make_unique<base::Value>("value2"), nullptr);

  scoped_refptr<base::TestSimpleTaskRunner> taskRunner =
      new base::TestSimpleTaskRunner();
  PolicyLoaderIOS loader(taskRunner);
  std::unique_ptr<PolicyBundle> bundle = loader.Load();
  ASSERT_TRUE(bundle);
  EXPECT_TRUE(bundle->Equals(expected));
}

}  // namespace policy
