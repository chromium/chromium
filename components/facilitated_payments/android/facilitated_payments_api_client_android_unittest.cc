// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/android/facilitated_payments_api_client_android.h"

#include <jni.h>
#include <cstdint>
#include <vector>

#include "content/public/test/test_renderer_host.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {
namespace {

using FacilitatedPaymentsApiClientAndroidTest =
    content::RenderViewHostTestHarness;

class MockFacilitatedPaymentsApiClientAndroid
    : public FacilitatedPaymentsApiClientAndroid {
 public:
  MockFacilitatedPaymentsApiClientAndroid() = default;
  ~MockFacilitatedPaymentsApiClientAndroid() override = default;
  MOCK_METHOD(void, OnIsAvailable, (JNIEnv*, jboolean), (override));
  MOCK_METHOD(void, OnGetClientToken, (JNIEnv*, jobject), (override));
  MOCK_METHOD(void, OnPurchaseActionResult, (JNIEnv*, jboolean), (override));
};

TEST_F(FacilitatedPaymentsApiClientAndroidTest, IsAvailable) {
  MockFacilitatedPaymentsApiClientAndroid apiClientAndroid;

  EXPECT_CALL(apiClientAndroid, OnIsAvailable(testing::_, false));

  apiClientAndroid.IsAvailable();
}

TEST_F(FacilitatedPaymentsApiClientAndroidTest, GetClientToken) {
  MockFacilitatedPaymentsApiClientAndroid apiClientAndroid;

  EXPECT_CALL(apiClientAndroid, OnGetClientToken(testing::_, nullptr));

  apiClientAndroid.GetClientToken();
}

TEST_F(FacilitatedPaymentsApiClientAndroidTest, InvokePurchaseAction) {
  MockFacilitatedPaymentsApiClientAndroid apiClientAndroid;

  EXPECT_CALL(apiClientAndroid, OnPurchaseActionResult(testing::_, false));

  apiClientAndroid.InvokePurchaseAction(
      std::vector<const uint8_t>{'A', 'c', 't', 'i', 'o', 'n'});
}

}  // namespace
}  // namespace payments::facilitated
