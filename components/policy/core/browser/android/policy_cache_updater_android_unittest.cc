// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/browser/android/policy_cache_updater_android.h"

#include <jni.h>
#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/values.h"
#include "components/policy/core/browser/configuration_policy_handler.h"
#include "components/policy/core/browser/policy_conversions_client.h"
#include "components/policy/core/browser/policy_error_map.h"
#include "components/policy/core/common/configuration_policy_provider.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/core/common/policy_service_impl.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/policy/android/test_jni_headers/PolicyCacheUpdaterTestSupporter_jni.h"

using ::testing::_;
using ::testing::Return;

namespace policy {
namespace android {
namespace {
// The list of policies that can be cached is controlled by Java library. Hence
// we use real policy name for testing.
constexpr char kPolicyName[] = "BrowserSignin";
constexpr int kPolicyValue = 1;

class StubPolicyHandler : public ConfigurationPolicyHandler {
 public:
  StubPolicyHandler(const std::string& policy_name,
                    PolicyMap::MessageType error_level)
      : StubPolicyHandler(policy_name,
                          /*has_error=*/true,
                          error_level) {}
  StubPolicyHandler(
      const std::string& policy_name,
      bool has_error,
      PolicyMap::MessageType error_level = PolicyMap::MessageType::kError)
      : policy_name_(policy_name),
        has_error_(has_error),
        error_level_(error_level) {}
  StubPolicyHandler(const StubPolicyHandler&) = delete;
  StubPolicyHandler& operator=(const StubPolicyHandler&) = delete;
  ~StubPolicyHandler() override = default;

  bool CheckPolicySettings(const PolicyMap& policies,
                           PolicyErrorMap* errors) override {
    if (has_error_) {
      errors->AddError(policy_name_, IDS_POLICY_BLOCKED, /*error_path=*/{},
                       error_level_);
    }
    return policies.GetValue(policy_name_, base::Value::Type::INTEGER) &&
           error_level_ != PolicyMap::MessageType::kError;
  }

 private:
  void ApplyPolicySettings(const PolicyMap& policies,
                           PrefValueMap* prefs) override {}
  std::string policy_name_;
  bool has_error_;
  PolicyMap::MessageType error_level_;
};
}  // namespace

class PolicyCacheUpdaterAndroidTest : public ::testing::Test {
 public:
  PolicyCacheUpdaterAndroidTest() {
    policy_provider_.SetDefaultReturns(
        true /* is_initialization_complete_return */,
        true /* is_first_policy_load_complete_return */);
    j_support_ = Java_PolicyCacheUpdaterTestSupporter_Constructor(env_);
    policy_service_ = std::make_unique<policy::PolicyServiceImpl>(
        std::vector<raw_ptr<ConfigurationPolicyProvider, VectorExperimental>>{
            &policy_provider_});
    policy_handler_list_ = std::make_unique<ConfigurationPolicyHandlerList>(
        ConfigurationPolicyHandlerList::
            PopulatePolicyHandlerParametersCallback(),
        GetChromePolicyDetailsCallback(),
        /*are_future_policies_allowed_by_default=*/false);
  }
  ~PolicyCacheUpdaterAndroidTest() override = default;

  void TearDown() override {
    Java_PolicyCacheUpdaterTestSupporter_resetPolicyCache(env_, j_support_);
  }

  void SetPolicy(const std::string& policy, int policy_value) {
    policy_map_.Set(policy, PolicyLevel::POLICY_LEVEL_MANDATORY,
                    PolicyScope::POLICY_SCOPE_MACHINE,
                    PolicySource::POLICY_SOURCE_PLATFORM,
                    base::Value(policy_value),
                    /*external_data_fetcher=*/nullptr);
  }

  void UpdatePolicy() { policy_provider_.UpdateChromePolicy(policy_map_); }

  void VerifyIntPolicyNotCached(const std::string& policy) {
    Java_PolicyCacheUpdaterTestSupporter_verifyIntPolicyNotCached(
        env_, j_support_, base::android::ConvertUTF8ToJavaString(env_, policy));
  }

  void VerifyIntPolicyHasValue(const std::string& policy, int expected_value) {
    Java_PolicyCacheUpdaterTestSupporter_verifyIntPolicyHasValue(
        env_, j_support_, base::android::ConvertUTF8ToJavaString(env_, policy),
        expected_value);
  }

  ConfigurationPolicyHandlerList* policy_handler_list() {
    return policy_handler_list_.get();
  }

  PolicyService* policy_service() { return policy_service_.get(); }

  PolicyMap* policy_map() { return &policy_map_; }

 private:
  raw_ptr<JNIEnv> env_ = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> j_support_;
  PolicyMap policy_map_;
  testing::NiceMock<MockConfigurationPolicyProvider> policy_provider_;
  std::unique_ptr<PolicyService> policy_service_;
  std::unique_ptr<ConfigurationPolicyHandlerList> policy_handler_list_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(PolicyCacheUpdaterAndroidTest, TestCachePolicy) {
  policy_handler_list()->AddHandler(
      std::make_unique<StubPolicyHandler>(kPolicyName, /*has_error=*/false));

  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  SetPolicy(kPolicyName, kPolicyValue);
  UpdatePolicy();
  VerifyIntPolicyHasValue(kPolicyName, kPolicyValue);
}

TEST_F(PolicyCacheUpdaterAndroidTest, TestPolicyNotExist) {
  policy_handler_list()->AddHandler(
      std::make_unique<StubPolicyHandler>(kPolicyName, /*has_error=*/false));

  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  UpdatePolicy();
  VerifyIntPolicyNotCached(kPolicyName);
}

TEST_F(PolicyCacheUpdaterAndroidTest, TestPolicyErrorPolicy) {
  policy_handler_list()->AddHandler(
      std::make_unique<StubPolicyHandler>(kPolicyName, /*has_error=*/true));

  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  SetPolicy(kPolicyName, kPolicyValue);
  UpdatePolicy();
  VerifyIntPolicyNotCached(kPolicyName);
}

TEST_F(PolicyCacheUpdaterAndroidTest, TestPolicyMapIgnoredPolicy) {
  policy_handler_list()->AddHandler(
      std::make_unique<StubPolicyHandler>(kPolicyName, /*has_error=*/false));

  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  SetPolicy(kPolicyName, kPolicyValue);
  policy_map()->GetMutable(kPolicyName)->SetIgnored();
  UpdatePolicy();
  VerifyIntPolicyNotCached(kPolicyName);
}

TEST_F(PolicyCacheUpdaterAndroidTest, TestPolicyMapErrorMessagePolicy) {
  policy_handler_list()->AddHandler(
      std::make_unique<StubPolicyHandler>(kPolicyName, /*has_error=*/false));

  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  SetPolicy(kPolicyName, kPolicyValue);
  policy_map()
      ->GetMutable(kPolicyName)
      ->AddMessage(PolicyMap::MessageType::kError, IDS_POLICY_BLOCKED);
  UpdatePolicy();
  VerifyIntPolicyNotCached(kPolicyName);
}

TEST_F(PolicyCacheUpdaterAndroidTest, TestPolicyMapWarningMessagePolicy) {
  policy_handler_list()->AddHandler(
      std::make_unique<StubPolicyHandler>(kPolicyName, /*has_error=*/false));

  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  SetPolicy(kPolicyName, kPolicyValue);
  policy_map()
      ->GetMutable(kPolicyName)
      ->AddMessage(PolicyMap::MessageType::kWarning, IDS_POLICY_BLOCKED);
  UpdatePolicy();
  VerifyIntPolicyHasValue(kPolicyName, kPolicyValue);
}

TEST_F(PolicyCacheUpdaterAndroidTest, TestPolicyUpdatedBeforeUpdaterCreated) {
  policy_handler_list()->AddHandler(
      std::make_unique<StubPolicyHandler>(kPolicyName, /*has_error=*/false));

  SetPolicy(kPolicyName, kPolicyValue);
  UpdatePolicy();
  VerifyIntPolicyNotCached(kPolicyName);
  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  VerifyIntPolicyHasValue(kPolicyName, kPolicyValue);
}

TEST_F(PolicyCacheUpdaterAndroidTest,
       TestWithFatalError_PolicyDoesntHaveValue) {
  policy_handler_list()->AddHandler(std::make_unique<StubPolicyHandler>(
      kPolicyName, PolicyMap::MessageType::kError));

  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  SetPolicy(kPolicyName, kPolicyValue);
  UpdatePolicy();
  VerifyIntPolicyNotCached(kPolicyName);
}

TEST_F(PolicyCacheUpdaterAndroidTest, TestWithWarningError_PolicyHasValue) {
  policy_handler_list()->AddHandler(std::make_unique<StubPolicyHandler>(
      kPolicyName, PolicyMap::MessageType::kWarning));

  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  SetPolicy(kPolicyName, kPolicyValue);
  UpdatePolicy();
  VerifyIntPolicyHasValue(kPolicyName, kPolicyValue);
}

TEST_F(PolicyCacheUpdaterAndroidTest, TestWithInfoError_PolicyHasValue) {
  policy_handler_list()->AddHandler(std::make_unique<StubPolicyHandler>(
      kPolicyName, PolicyMap::MessageType::kInfo));

  PolicyCacheUpdater updater(policy_service(), policy_handler_list());
  SetPolicy(kPolicyName, kPolicyValue);
  UpdatePolicy();
  VerifyIntPolicyHasValue(kPolicyName, kPolicyValue);
}

}  // namespace android
}  // namespace policy
