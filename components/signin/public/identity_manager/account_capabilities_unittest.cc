// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"

#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/android/jni_android.h"
#endif

class AccountCapabilitiesTest : public testing::Test {};

TEST_F(AccountCapabilitiesTest, CanOfferExtendedChromeSyncPromos) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);
  EXPECT_EQ(capabilities.can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kTrue);

  mutator.set_can_offer_extended_chrome_sync_promos(false);
  EXPECT_EQ(capabilities.can_offer_extended_chrome_sync_promos(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, CanRunChromePrivacySandboxTrials) {
  AccountCapabilities capabilities;
  EXPECT_EQ(capabilities.can_run_chrome_privacy_sandbox_trials(),
            signin::Tribool::kUnknown);

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_run_chrome_privacy_sandbox_trials(true);
  EXPECT_EQ(capabilities.can_run_chrome_privacy_sandbox_trials(),
            signin::Tribool::kTrue);

  mutator.set_can_run_chrome_privacy_sandbox_trials(false);
  EXPECT_EQ(capabilities.can_run_chrome_privacy_sandbox_trials(),
            signin::Tribool::kFalse);
}

TEST_F(AccountCapabilitiesTest, AreAllCapabilitiesKnown_Empty) {
  AccountCapabilities capabilities;
  EXPECT_FALSE(capabilities.AreAllCapabilitiesKnown());
}

TEST_F(AccountCapabilitiesTest, AreAllCapabilitiesKnown_PartiallyFilled) {
  AccountCapabilities capabilities;

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);
  EXPECT_FALSE(capabilities.AreAllCapabilitiesKnown());
}

TEST_F(AccountCapabilitiesTest, AreAllCapabilitiesKnown_Filled) {
  AccountCapabilities capabilities;

  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.SetAllSupportedCapabilities(true);
  EXPECT_TRUE(capabilities.AreAllCapabilitiesKnown());
}

TEST_F(AccountCapabilitiesTest, UpdateWith_UnknownToKnown) {
  AccountCapabilities capabilities;

  AccountCapabilities other;
  AccountCapabilitiesTestMutator mutator(&other);
  mutator.set_can_offer_extended_chrome_sync_promos(true);

  EXPECT_TRUE(capabilities.UpdateWith(other));
  EXPECT_EQ(signin::Tribool::kTrue,
            capabilities.can_offer_extended_chrome_sync_promos());
}

TEST_F(AccountCapabilitiesTest, UpdateWith_KnownToUnknown) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);

  AccountCapabilities other;

  EXPECT_FALSE(capabilities.UpdateWith(other));
  EXPECT_EQ(signin::Tribool::kTrue,
            capabilities.can_offer_extended_chrome_sync_promos());
}

TEST_F(AccountCapabilitiesTest, UpdateWith_OverwriteKnown) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);

  AccountCapabilities other;
  AccountCapabilitiesTestMutator other_mutator(&other);
  other_mutator.set_can_offer_extended_chrome_sync_promos(false);

  EXPECT_TRUE(capabilities.UpdateWith(other));
  EXPECT_EQ(signin::Tribool::kFalse,
            capabilities.can_offer_extended_chrome_sync_promos());
}

#if BUILDFLAG(IS_ANDROID)

TEST_F(AccountCapabilitiesTest, ConversionWithJNI_TriboolTrue) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(true);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_capabilities =
      capabilities.ConvertToJavaAccountCapabilities(env);
  AccountCapabilities converted_back =
      AccountCapabilities::ConvertFromJavaAccountCapabilities(
          env, java_capabilities);

  EXPECT_EQ(capabilities, converted_back);
}

TEST_F(AccountCapabilitiesTest, ConversionWithJNI_TriboolFalse) {
  AccountCapabilities capabilities;
  AccountCapabilitiesTestMutator mutator(&capabilities);
  mutator.set_can_offer_extended_chrome_sync_promos(false);

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_capabilities =
      capabilities.ConvertToJavaAccountCapabilities(env);
  AccountCapabilities converted_back =
      AccountCapabilities::ConvertFromJavaAccountCapabilities(
          env, java_capabilities);

  EXPECT_EQ(capabilities, converted_back);
}

TEST_F(AccountCapabilitiesTest, ConversionWithJNI_TriboolUnknown) {
  AccountCapabilities capabilities;

  JNIEnv* env = base::android::AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> java_capabilities =
      capabilities.ConvertToJavaAccountCapabilities(env);
  AccountCapabilities converted_back =
      AccountCapabilities::ConvertFromJavaAccountCapabilities(
          env, java_capabilities);

  EXPECT_EQ(capabilities, converted_back);
}

#endif
