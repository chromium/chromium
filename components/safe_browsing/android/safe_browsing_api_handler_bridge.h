// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Glue to pass Safe Browsing API requests between Chrome and GMSCore

#ifndef COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_BRIDGE_H_
#define COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_BRIDGE_H_

#include <jni.h>

#include <string>
#include <vector>

#include "base/android/jni_android.h"
#include "base/macros.h"
#include "components/safe_browsing/android/safe_browsing_api_handler.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "url/gurl.h"

namespace safe_browsing {

class SafeBrowsingApiHandlerBridge : public SafeBrowsingApiHandler {
 public:
  SafeBrowsingApiHandlerBridge();
  ~SafeBrowsingApiHandlerBridge() override;

  std::string GetSafetyNetId() override;

  // Makes Native->Java call to check the URL against Safe Browsing lists.
  void StartURLCheck(std::unique_ptr<URLCheckCallbackMeta> callback,
                     const GURL& url,
                     const SBThreatTypeSet& threat_types) override;

  bool StartCSDAllowlistCheck(const GURL& url) override;

  bool StartHighConfidenceAllowlistCheck(const GURL& url) override;

 private:
  // Creates the |j_api_handler_| if it hasn't been already.  If the API is not
  // supported, this will return false and j_api_handler_ will remain nullptr.
  bool CheckApiIsSupported();

  bool StartAllowlistCheck(const GURL& url, const SBThreatType& sb_threat_type);

  // The Java-side SafeBrowsingApiHandler. Must call CheckApiIsSupported first.
  base::android::ScopedJavaGlobalRef<jobject> j_api_handler_;

  // True if we've once tried to create the above object.
  bool checked_api_support_;

  // Used as a key to identify unique requests sent to Java to get Safe Browsing
  // reputation from GmsCore.
  jlong next_callback_id_ = 0;

  DISALLOW_COPY_AND_ASSIGN(SafeBrowsingApiHandlerBridge);
};

}  // namespace safe_browsing
#endif  // COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_BRIDGE_H_
