// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/android/facilitated_payments_api_client_android.h"

#include <jni.h>

#include <cstdint>
#include <utility>
#include <vector>

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/functional/bind.h"
#include "components/facilitated_payments/core/utils/facilitated_payments_utils.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

using FacilitatedPaymentsApiClientAndroidTest =
    content::RenderViewHostTestHarness;

void CaptureBoolean(bool* was_callback_invoked, bool* output, bool input) {
  *was_callback_invoked = true;
  *output = input;
}

void CaptureByteArray(bool* was_callback_invoked,
                      std::vector<uint8_t>* output,
                      std::vector<uint8_t> input) {
  *was_callback_invoked = true;
  *output = std::move(input);
}

void CaptureResultEnum(bool* was_callback_invoked,
                       PurchaseActionResult* output,
                       PurchaseActionResult input) {
  *was_callback_invoked = true;
  *output = input;
}

TEST_F(FacilitatedPaymentsApiClientAndroidTest,
       IsAvailableResultIsFalseByDefault) {
  FacilitatedPaymentsApiClientAndroid apiClient(main_rfh());
  bool was_callback_invoked = false;
  bool is_available_result = false;

  apiClient.IsAvailable(base::BindOnce(&CaptureBoolean, &was_callback_invoked,
                                       &is_available_result));

  EXPECT_TRUE(was_callback_invoked);
  EXPECT_FALSE(is_available_result);
}

TEST_F(FacilitatedPaymentsApiClientAndroidTest,
       IsAvailableSyncResultIsFalseByDefault) {
  FacilitatedPaymentsApiClientAndroid apiClient(main_rfh());

  EXPECT_FALSE(apiClient.IsAvailableSync());
}

TEST_F(FacilitatedPaymentsApiClientAndroidTest,
       GetClientTokenResultIsEmptyByDefault) {
  FacilitatedPaymentsApiClientAndroid apiClient(main_rfh());
  bool was_callback_invoked = false;
  std::vector<uint8_t> client_token_result;

  apiClient.GetClientToken(base::BindOnce(
      &CaptureByteArray, &was_callback_invoked, &client_token_result));

  EXPECT_TRUE(was_callback_invoked);
  EXPECT_TRUE(client_token_result.empty());
}

TEST_F(FacilitatedPaymentsApiClientAndroidTest,
       InvokePurchaseActionResultIsFalseByDefault) {
  FacilitatedPaymentsApiClientAndroid apiClient(main_rfh());
  bool was_callback_invoked = false;
  PurchaseActionResult purchase_action_result = PurchaseActionResult::kResultOk;
  signin::IdentityTestEnvironment identity_test_environment;
  SecurePayload secure_payload;
  secure_payload.action_token = {'A', 'c', 't', 'i', 'o', 'n'};
  secure_payload.secure_data.emplace_back(1, "value_1");
  secure_payload.secure_data.emplace_back(2, "value_2");

  apiClient.InvokePurchaseAction(
      identity_test_environment.MakeAccountAvailable("test@example.test"),
      secure_payload,
      base::BindOnce(&CaptureResultEnum, &was_callback_invoked,
                     &purchase_action_result));

  EXPECT_TRUE(was_callback_invoked);
  EXPECT_EQ(PurchaseActionResult::kCouldNotInvoke, purchase_action_result);
}

// Java bridge should invoke exactly one callback per method, but if it does
// not, then the Android API client should not crash.
TEST_F(FacilitatedPaymentsApiClientAndroidTest,
       SpuriousJavaCallbacksDoNotCrash) {
  FacilitatedPaymentsApiClientAndroid apiClient(main_rfh());
  JNIEnv* env = base::android::AttachCurrentThread();

  apiClient.OnIsAvailable(env, false);
  apiClient.OnGetClientToken(env, nullptr);
  apiClient.OnPurchaseActionResultEnum(
      env, static_cast<jint>(PurchaseActionResult::kResultOk));
}

}  // namespace
}  // namespace payments::facilitated
