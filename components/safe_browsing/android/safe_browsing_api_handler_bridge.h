// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Glue to pass Safe Browsing API requests between Chrome and GMSCore.

#ifndef COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_BRIDGE_H_
#define COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "components/safe_browsing/android/safe_browsing_api_handler_util.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"

class GURL;

namespace safe_browsing {

class UrlCheckInterceptor;
struct ThreatMetadata;

class SafeBrowsingApiHandlerBridge {
 public:
  using ResponseCallback =
      base::OnceCallback<void(SBThreatType, const ThreatMetadata&)>;
  using VerifyAppsResponseCallback =
      base::OnceCallback<void(VerifyAppsEnabledResult)>;
  using HasHarmfulAppsResponseCallback =
      base::OnceCallback<void(HasHarmfulAppsResultStatus,
                              /*num_of_apps=*/int,
                              /*status_code=*/int)>;
  using GetSafetyNetIdResponseCallback =
      base::OnceCallback<void(const std::string&)>;

  SafeBrowsingApiHandlerBridge();

  ~SafeBrowsingApiHandlerBridge();

  SafeBrowsingApiHandlerBridge(const SafeBrowsingApiHandlerBridge&) = delete;
  SafeBrowsingApiHandlerBridge& operator=(const SafeBrowsingApiHandlerBridge&) =
      delete;

  // Returns a reference to the singleton.
  static SafeBrowsingApiHandlerBridge& GetInstance();

  // Clear any URLs retained from the command-line.
  void ClearArtificialDatabase();

  // Populates any URLs specified at the command-line.
  void PopulateArtificialDatabase();

  // Makes Native-to-Java call to perform the hash-prefix database check.
  void StartHashDatabaseUrlCheck(ResponseCallback callback,
                                 const GURL& url,
                                 const SBThreatTypeSet& threat_types);

  // Makes Native-to-Java call to perform the privacy-preserving hash real-time
  // check.
  void StartHashRealTimeUrlCheck(ResponseCallback callback,
                                 const GURL& url,
                                 const SBThreatTypeSet& threat_types);

  // Check whether `url` matches a local allowlist.
  bool StartCSDAllowlistCheck(const GURL& url);
  bool StartCSDDownloadAllowlistCheck(const GURL& url);

  // Query whether app verification is enabled. Will run `callback` with
  // the result of the query.
  void StartIsVerifyAppsEnabled(VerifyAppsResponseCallback callback);

  // Prompt the user to enable app verification. Will run `callback`
  // with the result of the query.
  void StartEnableVerifyApps(VerifyAppsResponseCallback callback);

  // TODO(crbug.com/446681100): This function is not fully implemented yet and
  // `callback` won't be invoked. Query whether any potentially harmful app is
  // present.
  void StartHasPotentiallyHarmfulApps(HasHarmfulAppsResponseCallback callback);

  // Get the SafetyNet ID for the device. Will run `callback` with the result
  // of the query or a cached result, or an empty string if unsuccessful.
  void StartGetSafetyNetId(GetSafetyNetIdResponseCallback callback);

  // Called when a non-recoverable failure is encountered from SafeBrowsing API.
  void OnSafeBrowsingApiNonRecoverableFailure();

  void SetInterceptorForTesting(UrlCheckInterceptor* interceptor) {
    interceptor_for_testing_ = interceptor;
  }

  void ResetSafeBrowsingApiAvailableForTesting() {
    is_safe_browsing_api_available_ = true;
  }

  void SetVerifyAppsEnableResultForTesting(VerifyAppsEnabledResult result) {
    verify_apps_enabled_for_testing_ = result;
  }

  void SetHarmfulAppsResultForTesting(HasHarmfulAppsResultStatus result,
                                      int num_of_apps,
                                      int status_code) {
    harmful_apps_result_for_testing_ =
        std::make_tuple(result, num_of_apps, status_code);
  }

  // Resets the cached value and callback subscriptions list.
  void ResetSafetyNetIdForTesting();

  std::optional<std::string> GetCachedSafetyNetIdForTesting() const {
    return safety_net_id_;
  }

 private:
  // Makes Native-to-Java call to check the URL through GMSCore SafeBrowsing
  // API.
  void StartUrlCheckBySafeBrowsing(ResponseCallback callback,
                                   const GURL& url,
                                   const SBThreatTypeSet& threat_types,
                                   const SafeBrowsingJavaProtocol& protocol);

  // Stores the `result` of a call to get the SafetyNet ID from Java, including
  // an empty result which indicates non-recoverable error.
  void CacheSafetyNetId(const std::string& result);

  // Used as a key to identify unique requests sent to Java to get Safe Browsing
  // reputation from GmsCore SafeBrowsing API.
  jlong next_safe_browsing_callback_id_ = 0;

  // Used as a key to identify unique requests sent to Java related to
  // SafetyNet app verification.
  jlong next_verify_apps_callback_id_ = 0;

  // Used as a key to identify unique requests sent to Java related to
  // SafetyNet harmful app detection.
  jlong next_harmful_apps_callback_id_ = 0;

  // Whether SafeBrowsing API is available. Set to false if previous call to
  // SafeBrowsing API has encountered a non-recoverable failure. If set to
  // false, future calls to SafeBrowsing API will return safe immediately.
  // Once set to false, it will remain false until browser restarts.
  bool is_safe_browsing_api_available_ = true;

  // Cached SafetyNet ID. The SafetyNet ID for the device does not change, so
  // once it is obtained from Java, it is cached here for any future calls to
  // StartGetSafetyNetId(). An empty value may be cached here, which indicates
  // an error that is not likely recoverable during this process lifetime. Note
  // that a non-empty value may still be an incorrect/default value.
  std::optional<std::string> safety_net_id_ = std::nullopt;

  // Callback subscriptions to enable cancelling any pending
  // GetSafetyNetIdResponseCallbacks when destroying `this`. This should not
  // grow unboundedly, because the first invocation of getSafetyNetId() that
  // returns a non-empty value should cache the value and subsequent calls will
  // not add a callback to this list.
  std::vector<base::CallbackListSubscription>
      pending_get_safety_net_id_callbacks_;

  raw_ptr<UrlCheckInterceptor> interceptor_for_testing_ = nullptr;

  std::optional<VerifyAppsEnabledResult> verify_apps_enabled_for_testing_ =
      std::nullopt;

  std::optional<std::tuple<HasHarmfulAppsResultStatus,
                           /*num_of_apps=*/int,
                           /*status_code=*/int>>
      harmful_apps_result_for_testing_ = std::nullopt;

  // Set of URLs specified at the command-line to be enforced on as phishing.
  std::set<GURL> artificially_marked_phishing_urls_;
};

// Interface allowing simplified interception of calls to
// SafeBrowsingApiHandlerBridge. Intended for use only in tests.
class UrlCheckInterceptor {
 public:
  virtual ~UrlCheckInterceptor() = default;
  virtual void CheckBySafeBrowsing(
      SafeBrowsingApiHandlerBridge::ResponseCallback callback,
      const GURL& url) = 0;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_BRIDGE_H_
