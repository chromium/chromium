// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Glue to pass Safe Browsing API requests between
// RemoteSafeBrowsingDatabaseManager and Java-based API to check URLs.

#ifndef COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_H_
#define COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_H_

#include <memory>

#include "base/callback.h"
#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"

class GURL;

namespace safe_browsing {

class UrlCheckInterceptor;
struct ThreatMetadata;

// TODO(pasko): Fold this class into SafeBrowsingApiHandlerBridge for
// simplicity, as there are no other subclasses. This direction of folding
// eliminates confusion of this native class with the
// SafeBrowsingApiHandler.java.
class SafeBrowsingApiHandler {
 public:
  // Returns a pointer to the singleton.
  static SafeBrowsingApiHandler* GetInstance();

  typedef base::OnceCallback<void(SBThreatType, const ThreatMetadata&)>
      URLCheckCallbackMeta;

  virtual ~SafeBrowsingApiHandler(){};

  // Makes Native-to-Java call and invokes callback when the check is done.
  virtual void StartURLCheck(std::unique_ptr<URLCheckCallbackMeta> callback,
                             const GURL& url,
                             const SBThreatTypeSet& threat_types) = 0;

  virtual bool StartCSDAllowlistCheck(const GURL& url) = 0;

  virtual bool StartHighConfidenceAllowlistCheck(const GURL& url) = 0;

  void SetInterceptorForTesting(UrlCheckInterceptor* interceptor) {
    interceptor_for_testing_ = interceptor;
  }

 protected:
  UrlCheckInterceptor* interceptor_for_testing_ = nullptr;
};

// Interface allowing simplified interception of calls to
// SafeBrowsingApiHandler. Intended for use only in tests.
class UrlCheckInterceptor {
 public:
  virtual ~UrlCheckInterceptor(){};
  virtual void Check(
      std::unique_ptr<SafeBrowsingApiHandler::URLCheckCallbackMeta> callback,
      const GURL& url) const = 0;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_H_
