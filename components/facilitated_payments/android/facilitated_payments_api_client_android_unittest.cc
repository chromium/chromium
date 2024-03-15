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
#include "components/facilitated_payments/core/browser/facilitated_payments_api_client_delegate.h"
#include "content/public/test/test_renderer_host.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

using FacilitatedPaymentsApiClientAndroidTest =
    content::RenderViewHostTestHarness;

class TestFacilitatedPaymentsApiClientDelegate
    : public FacilitatedPaymentsApiClientDelegate {
 public:
  TestFacilitatedPaymentsApiClientDelegate() = default;
  ~TestFacilitatedPaymentsApiClientDelegate() override = default;

  // FacilitatedPaymentsApiClientDelegate implementation:
  void OnIsAvailable(bool is_available) override {
    is_available_ = is_available;
    is_available_checked_ = true;
  }

  // FacilitatedPaymentsApiClientDelegate implementation:
  void OnGetClientToken(std::vector<uint8_t> client_token) override {
    client_token_ = std::move(client_token);
    is_client_token_retrieved_ = true;
  }

  // FacilitatedPaymentsApiClientDelegate implementation:
  void OnPurchaseActionResult(bool is_purchase_action_successful) override {
    is_purchase_action_successful_ = is_purchase_action_successful;
    is_purchase_action_invoked_ = true;
  }

  base::WeakPtr<TestFacilitatedPaymentsApiClientDelegate> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  bool is_available_ = false;
  bool is_available_checked_ = false;

  std::vector<uint8_t> client_token_;
  bool is_client_token_retrieved_ = false;

  bool is_purchase_action_successful_ = false;
  bool is_purchase_action_invoked_ = false;

  base::WeakPtrFactory<TestFacilitatedPaymentsApiClientDelegate>
      weak_ptr_factory_{this};
};

TEST_F(FacilitatedPaymentsApiClientAndroidTest, IsAvailable) {
  TestFacilitatedPaymentsApiClientDelegate delegate;
  std::unique_ptr<FacilitatedPaymentsApiClient> apiClient =
      FacilitatedPaymentsApiClient::Create(delegate.GetWeakPtr());

  apiClient->IsAvailable();

  EXPECT_TRUE(delegate.is_available_checked_);
  EXPECT_FALSE(delegate.is_available_);
}

TEST_F(FacilitatedPaymentsApiClientAndroidTest,
       NoIsAvailableCallbackWithoutDelegate) {
  TestFacilitatedPaymentsApiClientDelegate delegate;
  std::unique_ptr<FacilitatedPaymentsApiClient> apiClient =
      FacilitatedPaymentsApiClient::Create(delegate.GetWeakPtr());
  delegate.weak_ptr_factory_.InvalidateWeakPtrs();

  apiClient->IsAvailable();

  EXPECT_FALSE(delegate.is_available_checked_);
}

TEST_F(FacilitatedPaymentsApiClientAndroidTest, GetClientToken) {
  TestFacilitatedPaymentsApiClientDelegate delegate;
  std::unique_ptr<FacilitatedPaymentsApiClient> apiClient =
      FacilitatedPaymentsApiClient::Create(delegate.GetWeakPtr());

  apiClient->GetClientToken();

  EXPECT_TRUE(delegate.is_client_token_retrieved_);
  EXPECT_TRUE(delegate.client_token_.empty());
}

TEST_F(FacilitatedPaymentsApiClientAndroidTest,
       GetClientTokenWithNonEmptyByteArray) {
  TestFacilitatedPaymentsApiClientDelegate delegate;
  std::unique_ptr<FacilitatedPaymentsApiClientAndroid> apiClient =
      std::make_unique<FacilitatedPaymentsApiClientAndroid>(
          delegate.GetWeakPtr());

  JNIEnv* env = base::android::AttachCurrentThread();
  apiClient->OnGetClientToken(
      env, base::android::ToJavaByteArray(
               env, std::vector<uint8_t>{'C', 'l', 'i', 'e', 'n', 't'}));

  EXPECT_TRUE(delegate.is_client_token_retrieved_);
  EXPECT_EQ((std::vector<uint8_t>{'C', 'l', 'i', 'e', 'n', 't'}),
            delegate.client_token_);
}

TEST_F(FacilitatedPaymentsApiClientAndroidTest,
       NoGetClientTokenCallbackWithoutDelegate) {
  TestFacilitatedPaymentsApiClientDelegate delegate;
  std::unique_ptr<FacilitatedPaymentsApiClient> apiClient =
      FacilitatedPaymentsApiClient::Create(delegate.GetWeakPtr());
  delegate.weak_ptr_factory_.InvalidateWeakPtrs();

  apiClient->GetClientToken();

  EXPECT_FALSE(delegate.is_client_token_retrieved_);
}

TEST_F(FacilitatedPaymentsApiClientAndroidTest, InvokePurchaseAction) {
  TestFacilitatedPaymentsApiClientDelegate delegate;
  std::unique_ptr<FacilitatedPaymentsApiClient> apiClient =
      FacilitatedPaymentsApiClient::Create(delegate.GetWeakPtr());

  apiClient->InvokePurchaseAction(
      std::vector<uint8_t>{'A', 'c', 't', 'i', 'o', 'n'});

  EXPECT_TRUE(delegate.is_purchase_action_invoked_);
  EXPECT_FALSE(delegate.is_purchase_action_successful_);
}

TEST_F(FacilitatedPaymentsApiClientAndroidTest,
       NoInvokePurchaseActionCallbackWithoutDelegate) {
  TestFacilitatedPaymentsApiClientDelegate delegate;
  std::unique_ptr<FacilitatedPaymentsApiClient> apiClient =
      FacilitatedPaymentsApiClient::Create(delegate.GetWeakPtr());
  delegate.weak_ptr_factory_.InvalidateWeakPtrs();

  apiClient->InvokePurchaseAction(
      std::vector<uint8_t>{'A', 'c', 't', 'i', 'o', 'n'});

  EXPECT_FALSE(delegate.is_purchase_action_invoked_);
}

}  // namespace
}  // namespace payments::facilitated
