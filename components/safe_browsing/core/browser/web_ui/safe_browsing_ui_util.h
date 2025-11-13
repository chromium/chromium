// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_SAFE_BROWSING_UI_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_SAFE_BROWSING_UI_UTIL_H_

#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/enterprise/common/proto/upload_request_response.pb.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/db/hit_report.h"
#include "components/safe_browsing/core/browser/download_check_result.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/safe_browsing/core/common/proto/webui.pb.h"
#include "components/sync/protocol/user_event_specifics.pb.h"

namespace safe_browsing {
namespace internal {
struct ReferringAppInfo;
}  // namespace internal
class SafeBrowsingUIHandler;
class WebUIInfoSingletonEventObserver;
}  // namespace safe_browsing

namespace safe_browsing::web_ui {

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
           const WebUIInfoSingletonEventObserver* new_source);
  bool IsFromSource(const WebUIInfoSingletonEventObserver* maybe_source) const;
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

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)

std::string UserReadableTimeFromMillisSinceEpoch(int64_t time_in_milliseconds);
void AddStoreInfo(
    const DatabaseManagerInfo::DatabaseInfo::StoreInfo& store_info,
    base::Value::List& database_info_list);

void AddDatabaseInfo(const DatabaseManagerInfo::DatabaseInfo& database_info,
                     base::Value::List& database_info_list);

void AddUpdateInfo(const DatabaseManagerInfo::UpdateInfo& update_info,
                   base::Value::List& database_info_list);

void ParseFullHashInfo(
    const FullHashCacheInfo::FullHashCache::CachedHashPrefixInfo::FullHashInfo&
        full_hash_info,
    base::Value::Dict& full_hash_info_dict);

void ParseFullHashCache(const FullHashCacheInfo::FullHashCache& full_hash_cache,
                        base::Value::List& full_hash_cache_list);
void ParseFullHashCacheInfo(const FullHashCacheInfo& full_hash_cache_info_proto,
                            base::Value::List& full_hash_cache_info);

std::string AddFullHashCacheInfo(
    const FullHashCacheInfo& full_hash_cache_info_proto);

#endif

// Serialization helper functions.
std::string SerializeClientDownloadRequest(const ClientDownloadRequest& cdr);
std::string SerializeClientDownloadResponse(const ClientDownloadResponse& cdr);
std::string SerializeClientPhishingRequest(
    const ClientPhishingRequestAndToken& cprat);
std::string SerializeClientPhishingResponse(const ClientPhishingResponse& cpr);
std::string SerializeCSBRR(const ClientSafeBrowsingReportRequest& report);
std::string SerializeDownloadUrlChecked(const std::vector<GURL>& urls,
                                        DownloadCheckResult result);
std::string SerializeHitReport(const HitReport& hit_report);
std::string SerializeJson(base::ValueView value);
base::Value::Dict SerializePGEvent(const sync_pb::UserEventSpecifics& event);
base::Value::Dict SerializeSecurityEvent(
    const sync_pb::GaiaPasswordReuse& event);
#if BUILDFLAG(IS_ANDROID)
// This serializes the internal::ReferringAppInfo struct (not to be confused
// with the protobuf message ReferringAppInfo), which contains intermediate
// information obtained from Java.
base::Value::Dict SerializeReferringAppInfo(
    const internal::ReferringAppInfo& info);
#endif
std::string SerializePGPing(
    const LoginReputationClientRequestAndToken& request_and_token);
std::string SerializePGResponse(const LoginReputationClientResponse& response);
std::string SerializeURTLookupPing(const URTLookupRequest& ping);
std::string SerializeURTLookupResponse(const RTLookupResponse& response);
std::string SerializeHPRTLookupPing(const HPRTLookupRequest& ping);
std::string SerializeHPRTLookupResponse(
    const V5::SearchHashesResponse& response);
base::Value::Dict SerializeLogMessage(base::Time timestamp,
                                      const std::string& message);
base::Value::Dict SerializeReportingEvent(const base::Value::Dict& event);
base::Value::Dict SerializeUploadEventsRequest(
    const ::chrome::cros::reporting::proto::UploadEventsRequest&
        upload_events_request,
    const base::Value::Dict& result);
#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
std::string SerializeContentAnalysisRequest(
    bool per_profile_request,
    const std::string& access_token_truncated,
    const std::string& upload_info,
    const std::string& upload_url,
    const enterprise_connectors::ContentAnalysisRequest& request);
std::string SerializeContentAnalysisResponse(
    const enterprise_connectors::ContentAnalysisResponse& response);
base::Value::Dict SerializeDeepScanDebugData(const std::string& token,
                                             const DeepScanDebugData& data);
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)

}  // namespace safe_browsing::web_ui

#endif  // COMPONENTS_SAFE_BROWSING_CORE_BROWSER_WEB_UI_SAFE_BROWSING_UI_UTIL_H_
