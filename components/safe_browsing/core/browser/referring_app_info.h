// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REFERRING_APP_INFO_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REFERRING_APP_INFO_H_

#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "url/gurl.h"

namespace safe_browsing {
namespace internal {

// Describes the result of an attempt to obtain the data to fill in
// ReferringAppInfo.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(GetReferringAppInfoResult)
enum class GetReferringAppInfoResult {
  // Lookup not attempted (e.g. because no WebContents was found).
  kNotAttempted = 0,
  // There was no referring app found, and no referring WebAPK found.
  kReferringAppNotFoundWebapkNotFound = 1,
  // There was no referring app, but a referring WebAPK was found.
  kReferringAppNotFoundWebapkFound = 2,
  // A referring app was found. No referring WebAPK was found.
  kReferringAppFoundWebapkNotFound = 3,
  // Both a referring app, and a referring WebAPK were found.
  kReferringAppFoundWebapkFound = 4,

  kMaxValue = kReferringAppFoundWebapkFound,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/sb_client/enums.xml:SafeBrowsingAndroidGetReferringAppInfoResult)

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

  bool has_referring_app() const { return !referring_app_name.empty(); }

  bool has_referring_webapk() const {
    return referring_webapk_start_url.is_valid();
  }

  safe_browsing::ReferringAppInfo::ReferringAppSource referring_app_source =
      safe_browsing::ReferringAppInfo::REFERRING_APP_SOURCE_UNSPECIFIED;

  std::string referring_app_name;
  GURL target_url;

  // These fields are populated if the referring app is a WebAPK. They
  // are used to compute the SafeBrowsingWebAppKey in ReferringAppInfo.
  GURL referring_webapk_start_url;
  GURL referring_webapk_manifest_id;
};

// Gets a value of the GetReferringAppInfoResult enum that describes the values
// present in `info`.
GetReferringAppInfoResult ReferringAppInfoToResult(
    const ReferringAppInfo& info);

}  // namespace internal

// Constructs a ReferringAppInfo proto composed of the data in an
// internal::ReferringAppInfo.
ReferringAppInfo GetReferringAppInfoProto(
    const internal::ReferringAppInfo& info);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_REFERRING_APP_INFO_H_
