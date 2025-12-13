// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_TEST_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_TEST_UTIL_H_

#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"

class GURL;

namespace safe_browsing::test {

// Test-only mirror of SafetyNetApiHandler.SafetyNetApiState in Java.
enum class SafetyNetApiInitializationState {
  kNotAvailable = 0,
  kInitialized = 1,
  kInitializedFirstParty = 2,
};

// Mixin for unittests that need to mock various functionality of
// SafeBrowsingApiHandlerBridge, SafeBrowsingApiHandler, and
// SafetyNetApiHandler. Test fixtures should inherit from this class to add the
// mock behavior.
class WithMockSafeBrowsingApiHandler {
 public:
  // This value should be aligned with DEFAULT_CHECK_DELTA_MICROSECONDS in
  // SafeBrowsingApiHandlerBridgeNativeUnitTestHelper.MockSafeBrowsingApiHandler
  static constexpr int kExpectedSafeBrowsingCheckDeltaMicroseconds = 15;

  // Call these in SetUp/TearDown for the test fixture.
  void SetUp();
  void TearDown();

  // Fake the initialization state for SafetyNetApiHandler.
  void SetSafetyNetApiInitializationState(
      SafetyNetApiInitializationState state);

  // Set a fake result for VerifyAppsEnabled.
  void SetVerifyAppsResult(VerifyAppsEnabledResult result);

  // Set a fake result for HasPotentiallyHarmfulApps.
  void SetHarmfulAppsResult(HasHarmfulAppsResultStatus result,
                            int num_of_apps,
                            int status_code);

  // Make the SafetyNetId show up as empty string.
  void SetSafetyNetIdResultEmpty();

  // Add a fake entry to one of the local allowlists. Note that the fake
  // allowlists do not check for domain and path match; they only check that the
  // URL spec is identical.
  void AddLocalAllowlistEntry(const GURL& url,
                              bool is_download_allowlist,
                              bool is_match);

  void AddSafeBrowsingResponse(
      const GURL& url,
      const SafeBrowsingApiLookupResult& returned_lookup_result,
      const SafeBrowsingJavaThreatType& returned_threat_type,
      const std::vector<SafeBrowsingJavaThreatAttribute>&
          returned_threat_attributes,
      const SafeBrowsingJavaResponseStatus& returned_response_status,
      const std::vector<SafeBrowsingJavaThreatType>& expected_threat_types,
      const SafeBrowsingJavaProtocol& expected_protocol);

  void RunHashDatabaseUrlCheck(
      const GURL& url,
      const SBThreatTypeSet& threat_types,
      SBThreatType expected_threat_type,
      SubresourceFilterMatch expected_subresource_filter_match);

  void RunHashRealTimeUrlCheck(const GURL& url,
                               const SBThreatTypeSet& threat_types,
                               SBThreatType expected_threat_type);

 protected:
  raw_ptr<JNIEnv> env_;
};

}  // namespace safe_browsing::test

#endif  // COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_TEST_UTIL_H_
