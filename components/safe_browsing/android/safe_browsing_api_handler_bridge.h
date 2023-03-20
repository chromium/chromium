// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Glue to pass Safe Browsing API requests between Chrome and GMSCore.

#ifndef COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_BRIDGE_H_
#define COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_BRIDGE_H_

#include <jni.h>

#include "base/android/jni_android.h"
#include "base/functional/callback.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"

class GURL;

namespace safe_browsing {

class UrlCheckInterceptor;
struct ThreatMetadata;

class SafeBrowsingApiHandlerBridge {
 public:
  using ResponseCallback =
      base::OnceCallback<void(SBThreatType, const ThreatMetadata&)>;

  SafeBrowsingApiHandlerBridge() = default;

  ~SafeBrowsingApiHandlerBridge();

  SafeBrowsingApiHandlerBridge(const SafeBrowsingApiHandlerBridge&) = delete;
  SafeBrowsingApiHandlerBridge& operator=(const SafeBrowsingApiHandlerBridge&) =
      delete;

  // Returns a reference to the singleton.
  static SafeBrowsingApiHandlerBridge& GetInstance();

  // Makes Native-to-Java call to check the URL against Safe Browsing lists.
  void StartURLCheck(std::unique_ptr<ResponseCallback> callback,
                     const GURL& url,
                     const SBThreatTypeSet& threat_types);

  bool StartCSDAllowlistCheck(const GURL& url);

  // Return nullopt when the JNI env is not initialized. If the JNI env is
  // initialized, then return whether the URL is in the allowlist.
  absl::optional<bool> StartHighConfidenceAllowlistCheck(const GURL& url);

  void SetInterceptorForTesting(UrlCheckInterceptor* interceptor) {
    interceptor_for_testing_ = interceptor;
  }

 private:
  // Used as a key to identify unique requests sent to Java to get Safe Browsing
  // reputation from GmsCore.
  jlong next_callback_id_ = 0;

  UrlCheckInterceptor* interceptor_for_testing_ = nullptr;
};

// Interface allowing simplified interception of calls to
// SafeBrowsingApiHandlerBridge. Intended for use only in tests.
class UrlCheckInterceptor {
 public:
  virtual ~UrlCheckInterceptor() {}
  virtual void Check(
      std::unique_ptr<SafeBrowsingApiHandlerBridge::ResponseCallback> callback,
      const GURL& url) const = 0;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_BRIDGE_H_
