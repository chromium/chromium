// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Glue to pass Safe Browsing API requests between
// RemoteSafeBrowsingDatabaseManager and Java-based API to check URLs.

#ifndef COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_H_
#define COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "components/safe_browsing/db/util.h"
#include "components/safe_browsing/db/v4_protocol_manager_util.h"
#include "url/gurl.h"

namespace safe_browsing {

class SafeBrowsingApiHandler {
 public:
  // Singleton interface.
  static void SetInstance(SafeBrowsingApiHandler* instance);
  static SafeBrowsingApiHandler* GetInstance();

  typedef base::OnceCallback<void(SBThreatType sb_threat_type,
                                  const ThreatMetadata& metadata)>
      URLCheckCallbackMeta;

  // Returns the Safety Net ID of the device.
  virtual std::string GetSafetyNetId() = 0;
  // Makes Native->Java call and invokes callback when check is done.
  virtual void StartURLCheck(std::unique_ptr<URLCheckCallbackMeta> callback,
                             const GURL& url,
                             const SBThreatTypeSet& threat_types) = 0;

  virtual bool StartCSDAllowlistCheck(const GURL& url) = 0;

  virtual bool StartHighConfidenceAllowlistCheck(const GURL& url) = 0;

  virtual ~SafeBrowsingApiHandler() {}

 private:
  // Pointer not owned.
  static SafeBrowsingApiHandler* instance_;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_ANDROID_SAFE_BROWSING_API_HANDLER_H_
