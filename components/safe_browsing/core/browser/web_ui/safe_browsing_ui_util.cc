// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/web_ui/safe_browsing_ui_util.h"

#include "base/base64.h"
#include "base/base64url.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "components/enterprise/connectors/core/reporting_constants.h"
#include "components/safe_browsing/core/browser/referring_app_info.h"
#include "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton_event_observer.h"
#include "components/safe_browsing/core/common/proto/csd.to_value.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.to_value.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.to_value.h"

#if BUILDFLAG(IS_ANDROID)
#include "base/strings/escape.h"
#else
#include "components/enterprise/common/proto/upload_request_response.to_value.h"  // nogncheck crbug.com/1125897
#endif

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/common/proto/connectors.to_value.h"
#endif

using sync_pb::GaiaPasswordReuse;

namespace safe_browsing::web_ui {

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
DeepScanDebugData::DeepScanDebugData() = default;
DeepScanDebugData::DeepScanDebugData(const DeepScanDebugData&) = default;
DeepScanDebugData::~DeepScanDebugData() = default;

TailoredVerdictOverrideData::TailoredVerdictOverrideData() = default;
TailoredVerdictOverrideData::~TailoredVerdictOverrideData() = default;

void TailoredVerdictOverrideData::Set(
    ClientDownloadResponse::TailoredVerdict new_value,
    const WebUIInfoSingletonEventObserver* new_source) {
  override_value = std::move(new_value);
  source = reinterpret_cast<SourceId>(new_source);
}

bool TailoredVerdictOverrideData::IsFromSource(
    const WebUIInfoSingletonEventObserver* maybe_source) const {
  return reinterpret_cast<SourceId>(maybe_source) == source;
}

void TailoredVerdictOverrideData::Clear() {
  override_value.reset();
  source = 0u;
}
#endif

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)

std::string UserReadableTimeFromMillisSinceEpoch(int64_t time_in_milliseconds) {
  base::Time time =
      base::Time::UnixEpoch() + base::Milliseconds(time_in_milliseconds);
  return base::UTF16ToUTF8(base::TimeFormatShortDateAndTime(time));
}

void AddStoreInfo(
    const DatabaseManagerInfo::DatabaseInfo::StoreInfo& store_info,
    base::Value::List& database_info_list) {
  if (store_info.has_file_name()) {
    database_info_list.Append(store_info.file_name());
  } else {
    database_info_list.Append("Unknown store");
  }

  base::Value::List store_info_list;
  if (store_info.has_file_size_bytes()) {
    store_info_list.Append(
        "Size (in bytes): " +
        base::UTF16ToUTF8(base::FormatNumber(store_info.file_size_bytes())));
  }

  if (store_info.has_update_status()) {
    store_info_list.Append(
        "Update status: " +
        base::UTF16ToUTF8(base::FormatNumber(store_info.update_status())));
  }

  if (store_info.has_last_apply_update_time_millis()) {
    store_info_list.Append("Last update time: " +
                           UserReadableTimeFromMillisSinceEpoch(
                               store_info.last_apply_update_time_millis()));
  }

  if (store_info.has_checks_attempted()) {
    store_info_list.Append(
        "Number of database checks: " +
        base::UTF16ToUTF8(base::FormatNumber(store_info.checks_attempted())));
  }

  if (store_info.has_state()) {
    std::string state_base64 = base::Base64Encode(store_info.state());
    store_info_list.Append("State: " + state_base64);
  }

  for (const auto& prefix_set : store_info.prefix_sets()) {
    std::string size = base::UTF16ToUTF8(base::FormatNumber(prefix_set.size()));
    std::string count =
        base::UTF16ToUTF8(base::FormatNumber(prefix_set.count()));
    store_info_list.Append(count + " prefixes of size " + size);
  }

  database_info_list.Append(std::move(store_info_list));
}

void AddDatabaseInfo(const DatabaseManagerInfo::DatabaseInfo& database_info,
                     base::Value::List& database_info_list) {
  if (database_info.has_database_size_bytes()) {
    database_info_list.Append("Database size (in bytes)");
    database_info_list.Append(
        static_cast<double>(database_info.database_size_bytes()));
  }

  // Add the information specific to each store.
  for (int i = 0; i < database_info.store_info_size(); i++) {
    AddStoreInfo(database_info.store_info(i), database_info_list);
  }
}

void AddUpdateInfo(const DatabaseManagerInfo::UpdateInfo& update_info,
                   base::Value::List& database_info_list) {
  if (update_info.has_network_status_code()) {
    // Network status of the last GetUpdate().
    database_info_list.Append("Last update network status code");
    database_info_list.Append(update_info.network_status_code());
  }
  if (update_info.has_last_update_time_millis()) {
    database_info_list.Append("Last update time");
    database_info_list.Append(UserReadableTimeFromMillisSinceEpoch(
        update_info.last_update_time_millis()));
  }
  if (update_info.has_next_update_time_millis()) {
    database_info_list.Append("Next update time");
    database_info_list.Append(UserReadableTimeFromMillisSinceEpoch(
        update_info.next_update_time_millis()));
  }
}

void ParseFullHashInfo(
    const FullHashCacheInfo::FullHashCache::CachedHashPrefixInfo::FullHashInfo&
        full_hash_info,
    base::Value::Dict& full_hash_info_dict) {
  if (full_hash_info.has_positive_expiry()) {
    full_hash_info_dict.Set(
        "Positive expiry",
        UserReadableTimeFromMillisSinceEpoch(full_hash_info.positive_expiry()));
  }
  if (full_hash_info.has_full_hash()) {
    std::string full_hash;
    base::Base64UrlEncode(full_hash_info.full_hash(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &full_hash);
    full_hash_info_dict.Set("Full hash (base64)", std::move(full_hash));
  }
  if (full_hash_info.list_identifier().has_platform_type()) {
    full_hash_info_dict.Set("platform_type",
                            full_hash_info.list_identifier().platform_type());
  }
  if (full_hash_info.list_identifier().has_threat_entry_type()) {
    full_hash_info_dict.Set(
        "threat_entry_type",
        full_hash_info.list_identifier().threat_entry_type());
  }
  if (full_hash_info.list_identifier().has_threat_type()) {
    full_hash_info_dict.Set("threat_type",
                            full_hash_info.list_identifier().threat_type());
  }
}

void ParseFullHashCache(const FullHashCacheInfo::FullHashCache& full_hash_cache,
                        base::Value::List& full_hash_cache_list) {
  base::Value::Dict full_hash_cache_parsed;

  if (full_hash_cache.has_hash_prefix()) {
    std::string hash_prefix;
    base::Base64UrlEncode(full_hash_cache.hash_prefix(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &hash_prefix);
    full_hash_cache_parsed.Set("Hash prefix (base64)", std::move(hash_prefix));
  }
  if (full_hash_cache.cached_hash_prefix_info().has_negative_expiry()) {
    full_hash_cache_parsed.Set(
        "Negative expiry",
        UserReadableTimeFromMillisSinceEpoch(
            full_hash_cache.cached_hash_prefix_info().negative_expiry()));
  }

  full_hash_cache_list.Append(std::move(full_hash_cache_parsed));

  for (const auto& full_hash_info_it :
       full_hash_cache.cached_hash_prefix_info().full_hash_info()) {
    base::Value::Dict full_hash_info_dict;
    ParseFullHashInfo(full_hash_info_it, full_hash_info_dict);
    full_hash_cache_list.Append(std::move(full_hash_info_dict));
  }
}

void ParseFullHashCacheInfo(const FullHashCacheInfo& full_hash_cache_info_proto,
                            base::Value::List& full_hash_cache_info) {
  if (full_hash_cache_info_proto.has_number_of_hits()) {
    base::Value::Dict number_of_hits;
    number_of_hits.Set("Number of cache hits",
                       full_hash_cache_info_proto.number_of_hits());
    full_hash_cache_info.Append(std::move(number_of_hits));
  }

  // Record FullHashCache list.
  for (const auto& full_hash_cache_it :
       full_hash_cache_info_proto.full_hash_cache()) {
    base::Value::List full_hash_cache_list;
    ParseFullHashCache(full_hash_cache_it, full_hash_cache_list);
    full_hash_cache_info.Append(std::move(full_hash_cache_list));
  }
}

std::string AddFullHashCacheInfo(
    const FullHashCacheInfo& full_hash_cache_info_proto) {
  base::Value::List full_hash_cache;
  ParseFullHashCacheInfo(full_hash_cache_info_proto, full_hash_cache);
  return SerializeJson(full_hash_cache);
}

#endif

std::string SerializeClientDownloadRequest(const ClientDownloadRequest& cdr) {
  return SerializeJson(ToValue(cdr));
}

std::string SerializeClientDownloadResponse(const ClientDownloadResponse& cdr) {
  return SerializeJson(ToValue(cdr));
}

std::string SerializeClientPhishingRequest(
    const ClientPhishingRequestAndToken& cprat) {
  base::Value value = ToValue(cprat.request);
  CHECK(value.is_dict());
  value.GetDict().Set("scoped_oauthtoken", cprat.token);
  return SerializeJson(value);
}

std::string SerializeClientPhishingResponse(const ClientPhishingResponse& cpr) {
  return SerializeJson(ToValue(cpr));
}

std::string SerializeCSBRR(const ClientSafeBrowsingReportRequest& report) {
  return SerializeJson(ToValue(report));
}

std::string SerializeDownloadUrlChecked(const std::vector<GURL>& urls,
                                        DownloadCheckResult result) {
  base::Value::Dict url_and_result;
  base::Value::List urls_value;
  for (const GURL& url : urls) {
    urls_value.Append(url.spec());
  }
  url_and_result.Set("download_url_chain", std::move(urls_value));
  url_and_result.Set("result", DownloadCheckResultToString(result));

  return web_ui::SerializeJson(url_and_result);
}

std::string SerializeHitReport(const HitReport& hit_report) {
  base::Value::Dict hit_report_dict;
  hit_report_dict.Set("malicious_url", hit_report.malicious_url.spec());
  hit_report_dict.Set("page_url", hit_report.page_url.spec());
  hit_report_dict.Set("referrer_url", hit_report.referrer_url.spec());
  hit_report_dict.Set("is_subresource", hit_report.is_subresource);
  std::string threat_type;
  switch (hit_report.threat_type) {
    case SBThreatType::SB_THREAT_TYPE_URL_PHISHING:
      threat_type = "SB_THREAT_TYPE_URL_PHISHING";
      break;
    case SBThreatType::SB_THREAT_TYPE_URL_MALWARE:
      threat_type = "SB_THREAT_TYPE_URL_MALWARE";
      break;
    case SBThreatType::SB_THREAT_TYPE_URL_UNWANTED:
      threat_type = "SB_THREAT_TYPE_URL_UNWANTED";
      break;
    case SBThreatType::SB_THREAT_TYPE_URL_BINARY_MALWARE:
      threat_type = "SB_THREAT_TYPE_URL_BINARY_MALWARE";
      break;
    default:
      threat_type = "OTHER";
  }
  hit_report_dict.Set("threat_type", std::move(threat_type));
  std::string threat_source;
  switch (hit_report.threat_source) {
    case ThreatSource::LOCAL_PVER4:
      threat_source = "LOCAL_PVER4";
      break;
    case ThreatSource::CLIENT_SIDE_DETECTION:
      threat_source = "CLIENT_SIDE_DETECTION";
      break;
    case ThreatSource::URL_REAL_TIME_CHECK:
      threat_source = "URL_REAL_TIME_CHECK";
      break;
    case ThreatSource::NATIVE_PVER5_REAL_TIME:
      threat_source = "NATIVE_PVER5_REAL_TIME";
      break;
    case ThreatSource::ANDROID_SAFEBROWSING_REAL_TIME:
      threat_source = "ANDROID_SAFEBROWSING_REAL_TIME";
      break;
    case ThreatSource::ANDROID_SAFEBROWSING:
      threat_source = "ANDROID_SAFEBROWSING";
      break;
    case ThreatSource::UNKNOWN:
      threat_source = "UNKNOWN";
      break;
  }
  hit_report_dict.Set("threat_source", std::move(threat_source));
  std::string extended_reporting_level;
  switch (hit_report.extended_reporting_level) {
    case ExtendedReportingLevel::SBER_LEVEL_OFF:
      extended_reporting_level = "SBER_LEVEL_OFF";
      break;
    case ExtendedReportingLevel::SBER_LEVEL_LEGACY:
      extended_reporting_level = "SBER_LEVEL_LEGACY";
      break;
    case ExtendedReportingLevel::SBER_LEVEL_SCOUT:
      extended_reporting_level = "SBER_LEVEL_SCOUT";
      break;
    case ExtendedReportingLevel::SBER_LEVEL_ENHANCED_PROTECTION:
      extended_reporting_level = "SBER_LEVEL_ENHANCED_PROTECTION";
      break;
  }
  hit_report_dict.Set("extended_reporting_level",
                      std::move(extended_reporting_level));
  hit_report_dict.Set("is_enhanced_protection",
                      hit_report.is_enhanced_protection);
  hit_report_dict.Set("is_metrics_reporting_active",
                      hit_report.is_metrics_reporting_active);
  hit_report_dict.Set("post_data", hit_report.post_data);
  return SerializeJson(hit_report_dict);
}

std::string SerializeJson(base::ValueView value) {
  return base::WriteJsonWithOptions(value,
                                    base::JSONWriter::OPTIONS_PRETTY_PRINT)
      .value_or(std::string());
}

base::Value::Dict SerializePGEvent(const sync_pb::UserEventSpecifics& event) {
  base::Value::Dict result;

  base::Time timestamp = base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(event.event_time_usec()));
  result.Set("time", timestamp.InMillisecondsFSinceUnixEpoch());

  base::Value::Dict event_dict;

  // Nominally only one of the following if() statements would be true.
  // Note that top-level path is either password_captured, or one of the fields
  // under GaiaPasswordReuse (ie. we've flattened the namespace for simplicity).

  if (event.has_gaia_password_captured_event()) {
    event_dict.SetByDottedPath(
        "password_captured.event_trigger",
        sync_pb::UserEventSpecifics_GaiaPasswordCaptured_EventTrigger_Name(
            event.gaia_password_captured_event().event_trigger()));
  }

  GaiaPasswordReuse reuse = event.gaia_password_reuse_event();
  if (reuse.has_reuse_detected()) {
    event_dict.SetByDottedPath("reuse_detected.status.enabled",
                               reuse.reuse_detected().status().enabled());
    event_dict.SetByDottedPath(
        "reuse_detected.status.reporting_population",
        GaiaPasswordReuse_PasswordReuseDetected_SafeBrowsingStatus_ReportingPopulation_Name(
            reuse.reuse_detected()
                .status()
                .safe_browsing_reporting_population()));
  }

  if (reuse.has_reuse_lookup()) {
    event_dict.SetByDottedPath(
        "reuse_lookup.lookup_result",
        GaiaPasswordReuse_PasswordReuseLookup_LookupResult_Name(
            reuse.reuse_lookup().lookup_result()));
    event_dict.SetByDottedPath(
        "reuse_lookup.verdict",
        GaiaPasswordReuse_PasswordReuseLookup_ReputationVerdict_Name(
            reuse.reuse_lookup().verdict()));
    event_dict.SetByDottedPath("reuse_lookup.verdict_token",
                               reuse.reuse_lookup().verdict_token());
  }

  if (reuse.has_dialog_interaction()) {
    event_dict.SetByDottedPath(
        "dialog_interaction.interaction_result",
        GaiaPasswordReuse_PasswordReuseDialogInteraction_InteractionResult_Name(
            reuse.dialog_interaction().interaction_result()));
  }

  result.Set("message", SerializeJson(event_dict));
  return result;
}

base::Value::Dict SerializeSecurityEvent(
    const sync_pb::GaiaPasswordReuse& event) {
  base::Value::Dict result;

  base::Value::Dict event_dict;
  if (event.has_reuse_lookup()) {
    event_dict.SetByDottedPath(
        "reuse_lookup.lookup_result",
        GaiaPasswordReuse_PasswordReuseLookup_LookupResult_Name(
            event.reuse_lookup().lookup_result()));
    event_dict.SetByDottedPath(
        "reuse_lookup.verdict",
        GaiaPasswordReuse_PasswordReuseLookup_ReputationVerdict_Name(
            event.reuse_lookup().verdict()));
    event_dict.SetByDottedPath("reuse_lookup.verdict_token",
                               event.reuse_lookup().verdict_token());
  }

  result.Set("message", SerializeJson(event_dict));
  return result;
}

#if BUILDFLAG(IS_ANDROID)
base::Value::Dict SerializeReferringAppInfo(
    const internal::ReferringAppInfo& info) {
  base::Value::Dict dict;
  dict.Set("referring_app_source",
           ReferringAppInfo_ReferringAppSource_Name(info.referring_app_source));
  dict.Set("referring_app_info", info.referring_app_name);
  dict.Set("target_url", info.target_url.spec());
  // Do not bother serializing referring_webapk_* here, because they are only
  // populated for a WebAPK, and it is not possible to launch
  // chrome://safe-browsing in a WebAPK, so they will never show up here.
  return dict;
}
#endif

std::string SerializePGPing(
    const LoginReputationClientRequestAndToken& request_and_token) {
  base::Value request = ToValue(request_and_token.request);
  CHECK(request.is_dict());
  request.GetDict().Set("scoped_oauth_token", request_and_token.token);
  return SerializeJson(request);
}

std::string SerializePGResponse(const LoginReputationClientResponse& response) {
  return SerializeJson(ToValue(response));
}

std::string SerializeURTLookupPing(const URTLookupRequest& ping) {
  base::Value request = ToValue(ping.request);
  CHECK(request.is_dict());
  request.GetDict().Set("scoped_oauth_token", ping.token);
  return SerializeJson(request);
}

std::string SerializeURTLookupResponse(const RTLookupResponse& response) {
  return SerializeJson(ToValue(response));
}

std::string SerializeHPRTLookupPing(const HPRTLookupRequest& ping) {
  base::Value::Dict request_dict;

  base::Value::Dict inner_request_dict;
  base::Value::List encoded_hash_prefixes;
  for (const auto& hash_prefix : ping.inner_request.hash_prefixes()) {
    std::string encoded_hash_prefix;
    base::Base64UrlEncode(hash_prefix,
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &encoded_hash_prefix);
    encoded_hash_prefixes.Append(std::move(encoded_hash_prefix));
  }
  inner_request_dict.Set("hash_prefixes (base64)",
                         std::move(encoded_hash_prefixes));

  request_dict.Set("inner_request", std::move(inner_request_dict));
  request_dict.Set("relay_url", ping.relay_url_spec);
  std::string encoded_ohttp_key;
  base::Base64UrlEncode(ping.ohttp_key,
                        base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                        &encoded_ohttp_key);
  request_dict.Set("ohttp_public_key (base64)", std::move(encoded_ohttp_key));

  return SerializeJson(request_dict);
}

std::string SerializeHPRTLookupResponse(
    const V5::SearchHashesResponse& response) {
  return SerializeJson(ToValue(response));
}

base::Value::Dict SerializeLogMessage(base::Time timestamp,
                                      const std::string& message) {
  base::Value::Dict result;
  result.Set("time", timestamp.InMillisecondsFSinceUnixEpoch());
  result.Set("message", message);
  return result;
}

// TODO(crbug.com/443997643): Delete when
// UploadRealtimeReportingEventsUsingProto is cleaned up.
base::Value::Dict SerializeReportingEvent(const base::Value::Dict& event) {
  base::Value::Dict result;
  result.Set("message", SerializeJson(event));
  return result;
}

base::Value::Dict SerializeUploadEventsRequest(
    const ::chrome::cros::reporting::proto::UploadEventsRequest&
        upload_events_request,
    const base::Value::Dict& result) {
  base::Value::Dict message;
#if BUILDFLAG(IS_ANDROID)
  message.Set("request",
              base::EscapeNonASCII(upload_events_request.SerializeAsString()));
#else
  message.Set("request",
              ::chrome::cros::reporting::proto::ToValue(upload_events_request));
#endif
  message.Set("response", result.Clone());

  base::Value::Dict wrapper;
  wrapper.Set("message", SerializeJson(message));
  auto& event = upload_events_request.events()[0];
  wrapper.Set("timeMillis",
              event.time().seconds() * 1000 + event.time().nanos() / 1000000.0);
  wrapper.Set("event_type",
              enterprise_connectors::GetEventName(event.event_case()));
  wrapper.Set("profile", upload_events_request.has_profile());
  wrapper.Set("device", upload_events_request.has_device());
  wrapper.Set("success",
              result.FindBool("uploaded_successfully").value_or(false));
  return wrapper;
}

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
std::string SerializeContentAnalysisRequest(
    bool per_profile_request,
    const std::string& access_token_truncated,
    const std::string& upload_info,
    const std::string& upload_url,
    const enterprise_connectors::ContentAnalysisRequest& request) {
  base::Value request_value = ToValue(request);
  CHECK(request_value.is_dict());
  base::Value::Dict& request_dict = request_value.GetDict();
  request_dict.Set("access_token", access_token_truncated);
  request_dict.Set("upload_info", upload_info);
  request_dict.Set("upload_url", upload_url);
  return SerializeJson(request_dict);
}

std::string SerializeContentAnalysisResponse(
    const enterprise_connectors::ContentAnalysisResponse& response) {
  return SerializeJson(ToValue(response));
}

base::Value::Dict SerializeDeepScanDebugData(const std::string& token,
                                             const DeepScanDebugData& data) {
  base::Value::Dict value;
  value.Set("token", token);

  if (!data.request_time.is_null()) {
    value.Set("request_time",
              data.request_time.InMillisecondsFSinceUnixEpoch());
  }

  if (data.request.has_value()) {
    value.Set("request",
              SerializeContentAnalysisRequest(
                  data.per_profile_request, data.access_token_truncated,
                  data.upload_info, data.upload_url, data.request.value()));
  }

  if (!data.response_time.is_null()) {
    value.Set("response_time",
              data.response_time.InMillisecondsFSinceUnixEpoch());
  }

  if (!data.response_status.empty()) {
    value.Set("response_status", data.response_status);
  }

  if (data.response.has_value()) {
    value.Set("response",
              SerializeContentAnalysisResponse(data.response.value()));
  }

  return value;
}
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)

}  // namespace safe_browsing::web_ui
