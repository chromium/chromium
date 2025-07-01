// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_UI_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_UI_UTIL_H_

#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/db/hit_report.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"

namespace safe_browsing {
class SafeBrowsingUIHandler;

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
struct DeepScanDebugData {
  DeepScanDebugData();
  DeepScanDebugData(const DeepScanDebugData&);
  ~DeepScanDebugData();

  base::Time request_time;
  std::optional<enterprise_connectors::ContentAnalysisRequest> request;
  bool per_profile_request;
  std::string access_token_truncated;
  std::string upload_info;
  std::string upload_url;

  base::Time response_time;
  std::string response_status;
  std::optional<enterprise_connectors::ContentAnalysisResponse> response;
};

// Local override of a download TailoredVerdict.
struct TailoredVerdictOverrideData {
  // Identifies the SafeBrowsingUIHandler it was set from, it is derived from
  // a SafeBrowsingUIHandler* pointer but is only used in comparison and never
  // dereferenced, to avoid dangling pointer.
  using SourceId = std::uintptr_t;

  TailoredVerdictOverrideData();
  TailoredVerdictOverrideData(const TailoredVerdictOverrideData&) = delete;
  ~TailoredVerdictOverrideData();

  void Set(ClientDownloadResponse::TailoredVerdict new_value,
           const SafeBrowsingUIHandler* new_source);
  bool IsFromSource(const SafeBrowsingUIHandler* maybe_source) const;
  void Clear();

  std::optional<ClientDownloadResponse::TailoredVerdict> override_value;
  SourceId source = 0u;
};
#endif

// The struct to combine a PhishGuard request and the token associated
// with it. The token is not part of the request proto because it is sent in the
// header. The token will be displayed along with the request in the safe
// browsing page.
struct LoginReputationClientRequestAndToken {
  LoginReputationClientRequest request;
  std::string token;
};

// The struct to combine a URL real time lookup request and the token associated
// with it. The token is not part of the request proto because it is sent in the
// header. The token will be displayed along with the request in the safe
// browsing page.
struct URTLookupRequest {
  RTLookupRequest request;
  std::string token;
};

// Combines the inner request (SearchHashesRequest) sent to Safe Browsing with
// other details about the outer request (the relay URL + the OHTTP key used
// for encryption). All are displayed on chrome://safe-browsing.
struct HPRTLookupRequest {
  V5::SearchHashesRequest inner_request;
  std::string relay_url_spec;
  std::string ohttp_key;
};

// The struct to combine a client-side phishing request and the token associated
// with it. The token is not part of the request proto because it is sent in the
// header. The token will be displayed along with the request in the safe
// browsing page.
struct ClientPhishingRequestAndToken {
  ClientPhishingRequest request;
  std::string token;
};

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_WEB_UI_SAFE_BROWSING_UI_UTIL_H_
