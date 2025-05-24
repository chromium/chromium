// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/facilitated_payments/android/secure_payload_android.h"

#include <utility>

#include "base/android/jni_android.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace payments::facilitated {

using SecurePayloadAndroidJniTest = testing::Test;

TEST_F(SecurePayloadAndroidJniTest, SecurePayloadJavaObjectCreated) {
  SecurePayload secure_payload;
  secure_payload.action_token = {1, 2, 3};
  secure_payload.secure_data.emplace_back(1, "value_1");
  secure_payload.secure_data.emplace_back(2, "value_2");

  base::android::ScopedJavaLocalRef<jobject> java_secure_payload =
      ConvertSecurePayloadToJavaObject(std::move(secure_payload));

  // Verify that the java object is created.
  EXPECT_TRUE(java_secure_payload.obj());
}

}  // namespace payments::facilitated
