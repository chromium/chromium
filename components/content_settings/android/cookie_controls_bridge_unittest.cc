// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/content_settings/android/cookie_controls_bridge.h"

#include "base/android/jni_android.h"
#include "testing/gtest/include/gtest/gtest.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/content_settings/android/native_j_unittests_jni_headers/CookieControlsBridgeUnitTest_jni.h"

using base::android::AttachCurrentThread;

namespace content_settings {
namespace {

class CookieControlsBridgeTest : public ::testing::Test {
 public:
  CookieControlsBridgeTest()
      : j_test_(Java_CookieControlsBridgeUnitTest_Constructor(
            AttachCurrentThread())) {}

  const base::android::ScopedJavaGlobalRef<jobject>& j_test() {
    return j_test_;
  }

 private:
  base::android::ScopedJavaGlobalRef<jobject> j_test_;
};

TEST_F(CookieControlsBridgeTest, CreateTpFeaturesList) {
  auto* env = AttachCurrentThread();
  base::android::ScopedJavaLocalRef<jobject> jfeatures =
      CookieControlsBridge::CreateTpFeaturesList(env);
  TrackingProtectionFeature feature;
  feature.feature_type = TrackingProtectionFeatureType::kThirdPartyCookies;
  feature.enforcement = CookieControlsEnforcement::kNoEnforcement;
  feature.status = TrackingProtectionBlockingStatus::kAllowed;
  CookieControlsBridge::CreateTpFeatureAndAddToList(env, jfeatures, feature);
  Java_CookieControlsBridgeUnitTest_testTpList(env, j_test(), jfeatures);
}

}  // namespace
}  // namespace content_settings
