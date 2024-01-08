// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_CONFIGURATION_POLICY_PROVIDER_TEST_H_
#define COMPONENTS_POLICY_CORE_COMMON_CONFIGURATION_POLICY_PROVIDER_TEST_H_

#include <memory>
#include <string>

#include "base/functional/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/policy/core/common/policy_types.h"
#include "components/policy/core/common/schema.h"
#include "components/policy/core/common/schema_registry.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace policy {

class ConfigurationPolicyProvider;

namespace test_keys {

extern const char kKeyString[];
extern const char kKeyBoolean[];
extern const char kKeyInteger[];
extern const char kKeyStringList[];
extern const char kKeyDictionary[];

}  // namespace test_keys

class PolicyTestBase : public testing::Test {
 public:
  PolicyTestBase();
  PolicyTestBase(const PolicyTestBase&) = delete;
  PolicyTestBase& operator=(const PolicyTestBase&) = delete;
  ~PolicyTestBase() override;

  // testing::Test:
  void SetUp() override;
  void TearDown() override;

 protected:
  bool RegisterSchema(const PolicyNamespace& ns,
                      const std::string& schema);

  // Register the actual Chrome schema containing supported policies.
  void RegisterChromeSchema(const PolicyNamespace& ns);

  // Needs to be the first member
  base::test::TaskEnvironment task_environment_;
  SchemaRegistry schema_registry_;
};

// An interface for creating a test policy provider and creating a policy
// provider instance for testing. Used as the parameter to the abstract
// ConfigurationPolicyProviderTest below.
class PolicyProviderTestHarness {
 public:
  // |level|, |scope| and |source| are the level, scope and source of the
  // policies returned by the providers from CreateProvider().
  PolicyProviderTestHarness(PolicyLevel level,
                            PolicyScope scope,
                            PolicySource source);
  PolicyProviderTestHarness(const PolicyProviderTestHarness&) = delete;
  PolicyProviderTestHarness& operator=(const PolicyProviderTestHarness&) =
      delete;
  virtual ~PolicyProviderTestHarness();

  // Actions to run at gtest SetUp() time.
  virtual void SetUp() = 0;

  // Actions to run at gtest TearDown() time.
  virtual void TearDown() {}

  // Create a new policy provider.
  virtual ConfigurationPolicyProvider* CreateProvider(
      SchemaRegistry* registry,
      scoped_refptr<base::SequencedTaskRunner> task_runner) = 0;

  // Returns the policy level, scope and source set by the policy provider.
  PolicyLevel policy_level() const;
  PolicyScope policy_scope() const;
  PolicySource policy_source() const;

  // Helpers to configure the environment the policy provider reads from.
  virtual void InstallEmptyPolicy() = 0;
  virtual void InstallStringPolicy(const std::string& policy_name,
                                   const std::string& policy_value) = 0;
  virtual void InstallIntegerPolicy(const std::string& policy_name,
                                    int policy_value) = 0;
  virtual void InstallBooleanPolicy(const std::string& policy_name,
                                    bool policy_value) = 0;
  virtual void InstallStringListPolicy(
      const std::string& policy_name,
      const base::Value::List& policy_value) = 0;
  virtual void InstallDictionaryPolicy(
      const std::string& policy_name,
      const base::Value::Dict& policy_value) = 0;

  // Not every provider supports installing 3rd party policy. Those who do
  // should override this method; the default just makes the test fail.
  virtual void Install3rdPartyPolicy(const base::Value::Dict& policies);

 private:
  PolicyLevel level_;
  PolicyScope scope_;
  PolicySource source_;
};

// A factory method for creating a test harness.
typedef PolicyProviderTestHarness* (*CreatePolicyProviderTestHarness)();

// Abstract policy provider test. This is meant to be instantiated for each
// policy provider implementation, passing in a suitable harness factory
// function as the test parameter.
class ConfigurationPolicyProviderTest
    : public PolicyTestBase,
      public testing::WithParamInterface<CreatePolicyProviderTestHarness> {
 public:
  ConfigurationPolicyProviderTest(const ConfigurationPolicyProviderTest&) =
      delete;
  ConfigurationPolicyProviderTest& operator=(
      const ConfigurationPolicyProviderTest&) = delete;

 protected:
  ConfigurationPolicyProviderTest();
  ~ConfigurationPolicyProviderTest() override;

  void SetUp() override;
  void TearDown() override;

  // Installs a valid policy and checks whether the provider returns the
  // |expected_value|.
  void CheckValue(const char* policy_name,
                  const base::Value& expected_value,
                  base::OnceClosure install_value);

  std::unique_ptr<PolicyProviderTestHarness> test_harness_;
  std::unique_ptr<ConfigurationPolicyProvider> provider_;
};

// An extension of ConfigurationPolicyProviderTest that also tests loading of
// 3rd party policy. Policy provider implementations that support loading of
// 3rd party policy should also instantiate these tests.
class Configuration3rdPartyPolicyProviderTest
    : public ConfigurationPolicyProviderTest {
 public:
  Configuration3rdPartyPolicyProviderTest(
      const Configuration3rdPartyPolicyProviderTest&) = delete;
  Configuration3rdPartyPolicyProviderTest& operator=(
      const Configuration3rdPartyPolicyProviderTest&) = delete;

 protected:
  Configuration3rdPartyPolicyProviderTest();
  virtual ~Configuration3rdPartyPolicyProviderTest();
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_CONFIGURATION_POLICY_PROVIDER_TEST_H_
