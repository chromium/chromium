// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/android/safe_browsing_api_handler_test_util.h"

#include "base/android/jni_android.h"
#include "base/android/jni_array.h"
#include "base/android/jni_string.h"
#include "base/containers/heap_array.h"
#include "base/run_loop.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_bridge.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// Must come after all headers that specialize FromJniType() / ToJniType().
#include "components/safe_browsing/android/native_j_unittests_jni_headers/SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_jni.h"

namespace safe_browsing::test {

void WithMockSafeBrowsingApiHandler::SetUp() {
  env_ = base::android::AttachCurrentThread();
  ASSERT_TRUE(env_);
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setUp(env_);
}

void WithMockSafeBrowsingApiHandler::TearDown() {
  SafeBrowsingApiHandlerBridge::GetInstance()
      .ResetSafeBrowsingApiAvailableForTesting();
  SafeBrowsingApiHandlerBridge::GetInstance().ResetSafetyNetIdForTesting();
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_tearDown(env_);
}

void WithMockSafeBrowsingApiHandler::SetSafetyNetApiInitializationState(
    SafetyNetApiInitializationState state) {
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setSafetyNetApiInitializationState(
      env_, static_cast<int>(state));
}

void WithMockSafeBrowsingApiHandler::SetVerifyAppsResult(
    VerifyAppsEnabledResult result) {
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setVerifyAppsResult(
      env_, static_cast<int>(result));
}

void WithMockSafeBrowsingApiHandler::SetHarmfulAppsResult(
    HasHarmfulAppsResultStatus result,
    int num_of_apps,
    int status_code) {
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setHarmfulAppsResult(
      env_, static_cast<int>(result), num_of_apps, status_code);
}

void WithMockSafeBrowsingApiHandler::SetSafetyNetIdResultEmpty() {
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setSafetyNetIdResultEmpty(
      env_);
}

void WithMockSafeBrowsingApiHandler::AddLocalAllowlistEntry(
    const GURL& url,
    bool is_download_allowlist,
    bool is_match) {
  base::android::ScopedJavaLocalRef<jstring> j_url =
      base::android::ConvertUTF8ToJavaString(env_, url.spec());

  if (is_download_allowlist) {
    Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setCsdDownloadAllowlistMatch(
        env_, j_url, is_match);
  } else {
    Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setCsdAllowlistMatch(
        env_, j_url, is_match);
  }
}

void WithMockSafeBrowsingApiHandler::AddSafeBrowsingResponse(
    const GURL& url,
    const SafeBrowsingApiLookupResult& returned_lookup_result,
    const SafeBrowsingJavaThreatType& returned_threat_type,
    const std::vector<SafeBrowsingJavaThreatAttribute>&
        returned_threat_attributes,
    const SafeBrowsingJavaResponseStatus& returned_response_status,
    const std::vector<SafeBrowsingJavaThreatType>& expected_threat_types,
    const SafeBrowsingJavaProtocol& expected_protocol) {
  base::android::ScopedJavaLocalRef<jstring> j_url =
      base::android::ConvertUTF8ToJavaString(env_, url.spec());
  auto int_threat_types =
      base::HeapArray<int>::WithSize(expected_threat_types.size());
  auto itr = int_threat_types.begin();
  for (auto expected_threat_type : expected_threat_types) {
    *itr++ = static_cast<int>(expected_threat_type);
  }
  auto int_threat_attributes =
      base::HeapArray<int>::WithSize(returned_threat_attributes.size());
  itr = int_threat_attributes.begin();
  for (auto returned_threat_attribute : returned_threat_attributes) {
    *itr++ = static_cast<int>(returned_threat_attribute);
  }
  Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_setSafeBrowsingApiHandlerResponse(
      env_, j_url, base::android::ToJavaIntArray(env_, int_threat_types),
      static_cast<int>(expected_protocol),
      static_cast<int>(returned_lookup_result),
      static_cast<int>(returned_threat_type),
      base::android::ToJavaIntArray(env_, int_threat_attributes),
      static_cast<int>(returned_response_status));
}

void WithMockSafeBrowsingApiHandler::RunHashDatabaseUrlCheck(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    SBThreatType expected_threat_type,
    SubresourceFilterMatch expected_subresource_filter_match) {
  base::RunLoop run_loop;
  auto callback = SafeBrowsingApiHandlerBridge::ResponseCallback(base::BindOnce(
      [](base::RunLoop* run_loop, SBThreatType expected_threat_type,
         SubresourceFilterMatch expected_subresource_filter_match,
         SBThreatType returned_threat_type,
         const ThreatMetadata& returned_metadata) {
        EXPECT_EQ(returned_threat_type, expected_threat_type);
        EXPECT_EQ(returned_metadata.subresource_filter_match,
                  expected_subresource_filter_match);
        run_loop->Quit();
      },
      &run_loop, expected_threat_type, expected_subresource_filter_match));
  SafeBrowsingApiHandlerBridge::GetInstance().StartHashDatabaseUrlCheck(
      std::move(callback), url, threat_types);
  run_loop.Run();
}

void WithMockSafeBrowsingApiHandler::RunHashRealTimeUrlCheck(
    const GURL& url,
    const SBThreatTypeSet& threat_types,
    SBThreatType expected_threat_type) {
  base::RunLoop run_loop;
  auto callback = SafeBrowsingApiHandlerBridge::ResponseCallback(base::BindOnce(
      [](base::RunLoop* run_loop, SBThreatType expected_threat_type,
         SBThreatType returned_threat_type,
         const ThreatMetadata& returned_metadata) {
        EXPECT_EQ(returned_threat_type, expected_threat_type);
        run_loop->Quit();
      },
      &run_loop, expected_threat_type));
  SafeBrowsingApiHandlerBridge::GetInstance().StartHashRealTimeUrlCheck(
      std::move(callback), url, threat_types);
  run_loop.Run();
  EXPECT_EQ(
      Java_SafeBrowsingApiHandlerBridgeNativeUnitTestHelper_getSafeBrowsingApiUrlCheckTimeObserverResult(
          env_),
      kExpectedSafeBrowsingCheckDeltaMicroseconds);
}

}  // namespace safe_browsing::test

DEFINE_JNI(SafeBrowsingApiHandlerBridgeNativeUnitTestHelper)
