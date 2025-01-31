// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REFERRING_APP_INFO_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REFERRING_APP_INFO_H_

#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "url/gurl.h"

namespace safe_browsing::internal {

// This struct contains data needed to compute the ReferringAppInfo proto and
// other fields related to the referring app. For Android app referrers, these
// fields are obtained in SafeBrowsingReferringAppBridge.java which passes an
// analog of this struct over jni.
struct ReferringAppInfo {
  ReferringAppInfo();
  ~ReferringAppInfo();

  ReferringAppInfo(const ReferringAppInfo&);
  ReferringAppInfo(ReferringAppInfo&&);
  ReferringAppInfo& operator=(const ReferringAppInfo&);
  ReferringAppInfo& operator=(ReferringAppInfo&&);

  safe_browsing::ReferringAppInfo::ReferringAppSource referring_app_source =
      safe_browsing::ReferringAppInfo::REFERRING_APP_SOURCE_UNSPECIFIED;

  std::string referring_app_name;
  GURL target_url;

  // These fields are populated if the referring app is a WebAPK. They
  // are used to compute the SafeBrowsingWebAppKey in ReferringAppInfo.
  GURL referring_webapk_start_url;
  GURL referring_webapk_manifest_id;
};

}  // namespace safe_browsing::internal

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REFERRING_APP_INFO_H_
