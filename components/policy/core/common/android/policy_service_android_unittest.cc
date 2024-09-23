// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/android/policy_service_android.h"

#include <jni.h>

#include "base/android/java_exception_reporter.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "components/policy/core/common/mock_policy_service.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/policy/android/test_jni_headers/PolicyServiceTestSupporter_jni.h"

using ::testing::Return;
using ::testing::ReturnRef;

namespace policy {
namespace android {

// Tests the bridge class PolicyServiceAndroid. It uses the Java helper class in
// //components/policy/android/javatest/.../test/PolicyServiceTestSupporter.java
class PolicyServiceAndroidTest : public ::testing::Test {
 public:
  PolicyServiceAndroidTest() {
    EXPECT_CALL(policy_service_, GetPolicies(PolicyNamespace(
                                     POLICY_DOMAIN_CHROME, std::string())))
        .WillOnce(ReturnRef(policies));
    policy_service_android_ =
        std::make_unique<PolicyServiceAndroid>(&policy_service_);
    j_support_ = Java_PolicyServiceTestSupporter_Constructor(
        env_, policy_service_android_->GetJavaObject());
  }
  ~PolicyServiceAndroidTest() override {
    Java_PolicyServiceTestSupporter_verifyNoMoreInteractions(env_, j_support_);
  }

  raw_ptr<JNIEnv> env_ = base::android::AttachCurrentThread();
  MockPolicyService policy_service_;
  policy::PolicyMap policies;
  std::unique_ptr<PolicyServiceAndroid> policy_service_android_;
  base::android::ScopedJavaLocalRef<jobject> j_support_;
};

TEST_F(PolicyServiceAndroidTest, IsInitializationComplete) {
  EXPECT_CALL(policy_service_, IsInitializationComplete(POLICY_DOMAIN_CHROME))
      .Times(2)
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  EXPECT_CALL(policy_service_,
              IsInitializationComplete(POLICY_DOMAIN_EXTENSIONS))
      .Times(0);
  EXPECT_CALL(policy_service_,
              IsInitializationComplete(POLICY_DOMAIN_SIGNIN_EXTENSIONS))
      .Times(0);
  Java_PolicyServiceTestSupporter_verifyIsInitalizationComplete(
      env_, j_support_, false);
  Java_PolicyServiceTestSupporter_verifyIsInitalizationComplete(
      env_, j_support_, true);

  ::testing::Mock::VerifyAndClearExpectations(&policy_service_);
}

TEST_F(PolicyServiceAndroidTest, OneObserver) {
  EXPECT_CALL(policy_service_,
              AddObserver(POLICY_DOMAIN_CHROME, policy_service_android_.get()))
      .Times(1);
  int observer_id =
      Java_PolicyServiceTestSupporter_addObserver(env_, j_support_);

  policy_service_android_->OnPolicyServiceInitialized(POLICY_DOMAIN_CHROME);
  Java_PolicyServiceTestSupporter_verifyInitializationEvent(
      env_, j_support_, /*index*/ 0, /*times*/ 1);

  policy_service_android_->OnPolicyUpdated(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()), PolicyMap(),
      PolicyMap());
  Java_PolicyServiceTestSupporter_verifyPolicyUpdatedEvent(
      env_, j_support_, /*index*/ 0, /*times*/ 1);

  EXPECT_CALL(policy_service_, RemoveObserver(POLICY_DOMAIN_CHROME,
                                              policy_service_android_.get()))
      .Times(1);
  Java_PolicyServiceTestSupporter_removeObserver(env_, j_support_, observer_id);
  ::testing::Mock::VerifyAndClearExpectations(&policy_service_);
}

TEST_F(PolicyServiceAndroidTest, MultipleObservers) {
  // When multiple observers are added in Java, only one observer will be
  // created in C++.
  EXPECT_CALL(policy_service_,
              AddObserver(POLICY_DOMAIN_CHROME, policy_service_android_.get()))
      .Times(1);
  int observer1 = Java_PolicyServiceTestSupporter_addObserver(env_, j_support_);
  int observer2 = Java_PolicyServiceTestSupporter_addObserver(env_, j_support_);

  // And we still observing the PolicyService as long as there is one Java
  // observer.
  Java_PolicyServiceTestSupporter_removeObserver(env_, j_support_, observer2);

  ::testing::Mock::VerifyAndClearExpectations(&policy_service_);

  // Trigger the event and only the activated Java observer get notified.
  policy_service_android_->OnPolicyServiceInitialized(POLICY_DOMAIN_CHROME);
  Java_PolicyServiceTestSupporter_verifyInitializationEvent(
      env_, j_support_, observer1, /*times*/ 1);
  Java_PolicyServiceTestSupporter_verifyInitializationEvent(
      env_, j_support_, observer2, /*times*/ 0);

  // Remove the last Java observers and triggers the C++ observer cleanup too.
  EXPECT_CALL(policy_service_, RemoveObserver(POLICY_DOMAIN_CHROME,
                                              policy_service_android_.get()))
      .Times(1);
  Java_PolicyServiceTestSupporter_removeObserver(env_, j_support_, observer1);
  ::testing::Mock::VerifyAndClearExpectations(&policy_service_);
}

TEST_F(PolicyServiceAndroidTest, PolicyUpdateEvent) {
  EXPECT_CALL(policy_service_,
              AddObserver(POLICY_DOMAIN_CHROME, policy_service_android_.get()))
      .Times(1);
  int observer_id =
      Java_PolicyServiceTestSupporter_addObserver(env_, j_support_);

  PolicyMap previous;
  PolicyMap current;
  previous.Set("policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
               POLICY_SOURCE_PLATFORM, base::Value(1), nullptr);
  current.Set("policy", POLICY_LEVEL_MANDATORY, POLICY_SCOPE_MACHINE,
              POLICY_SOURCE_PLATFORM, base::Value(2), nullptr);

  // PolicyMapAndroid needs to be valid until the verification is over.
  PolicyMapAndroid previous_android(previous);
  PolicyMapAndroid current_android(current);

  Java_PolicyServiceTestSupporter_setupPolicyUpdatedEventWithValues(
      env_, j_support_, /*index*/ 0, previous_android.GetJavaObject(),
      current_android.GetJavaObject());
  policy_service_android_->OnPolicyUpdated(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()), previous, current);
  Java_PolicyServiceTestSupporter_verifyPolicyUpdatedEventWithValues(
      env_, j_support_, /*index*/ 0, /*times*/ 1);

  EXPECT_CALL(policy_service_, RemoveObserver(POLICY_DOMAIN_CHROME,
                                              policy_service_android_.get()))
      .Times(1);
  Java_PolicyServiceTestSupporter_removeObserver(env_, j_support_, observer_id);
  ::testing::Mock::VerifyAndClearExpectations(&policy_service_);
}

}  // namespace android
}  // namespace policy
