// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/web_ui/safe_browsing_ui.h"

#include <stddef.h>
#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/callback.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_string_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/singleton.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/grit/components_resources.h"
#include "components/grit/components_scaled_resources.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/safe_browsing/browser/referrer_chain_provider.h"
#include "components/safe_browsing/common/safe_browsing_prefs.h"
#include "components/safe_browsing/features.h"
#include "components/safe_browsing/web_ui/constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"

#if SAFE_BROWSING_DB_LOCAL
#include "components/safe_browsing/db/v4_local_database_manager.h"
#endif

using base::Time;
using PasswordCaptured = sync_pb::UserEventSpecifics::GaiaPasswordCaptured;
using PasswordReuseLookup =
    sync_pb::UserEventSpecifics::GaiaPasswordReuse::PasswordReuseLookup;
using PasswordReuseDetected =
    sync_pb::UserEventSpecifics::GaiaPasswordReuse::PasswordReuseDetected;
using PasswordReuseDialogInteraction = sync_pb::UserEventSpecifics::
    GaiaPasswordReuse::PasswordReuseDialogInteraction;

namespace safe_browsing {
WebUIInfoSingleton::WebUIInfoSingleton() = default;

WebUIInfoSingleton::~WebUIInfoSingleton() = default;

// static
WebUIInfoSingleton* WebUIInfoSingleton::GetInstance() {
  return base::Singleton<WebUIInfoSingleton>::get();
}

// static
bool WebUIInfoSingleton::HasListener() {
  return !GetInstance()->webui_instances_.empty();
}

void WebUIInfoSingleton::AddToClientDownloadRequestsSent(
    std::unique_ptr<ClientDownloadRequest> client_download_request) {
  if (webui_instances_.empty())
    return;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyClientDownloadRequestJsListener(
        client_download_request.get());
  client_download_requests_sent_.push_back(std::move(client_download_request));
}

void WebUIInfoSingleton::ClearClientDownloadRequestsSent() {
  std::vector<std::unique_ptr<ClientDownloadRequest>>().swap(
      client_download_requests_sent_);
}

void WebUIInfoSingleton::AddToClientDownloadResponsesReceived(
    std::unique_ptr<ClientDownloadResponse> client_download_response) {
  if (webui_instances_.empty())
    return;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyClientDownloadResponseJsListener(
        client_download_response.get());
  client_download_responses_received_.push_back(
      std::move(client_download_response));
}

void WebUIInfoSingleton::ClearClientDownloadResponsesReceived() {
  std::vector<std::unique_ptr<ClientDownloadResponse>>().swap(
      client_download_responses_received_);
}

void WebUIInfoSingleton::AddToCSBRRsSent(
    std::unique_ptr<ClientSafeBrowsingReportRequest> csbrr) {
  if (webui_instances_.empty())
    return;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyCSBRRJsListener(csbrr.get());
  csbrrs_sent_.push_back(std::move(csbrr));
}

void WebUIInfoSingleton::ClearCSBRRsSent() {
  std::vector<std::unique_ptr<ClientSafeBrowsingReportRequest>>().swap(
      csbrrs_sent_);
}

void WebUIInfoSingleton::AddToPGEvents(
    const sync_pb::UserEventSpecifics& event) {
  if (webui_instances_.empty())
    return;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyPGEventJsListener(event);

  pg_event_log_.push_back(event);
}

void WebUIInfoSingleton::ClearPGEvents() {
  std::vector<sync_pb::UserEventSpecifics>().swap(pg_event_log_);
}

int WebUIInfoSingleton::AddToPGPings(
    const LoginReputationClientRequest& request) {
  if (webui_instances_.empty())
    return -1;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyPGPingJsListener(pg_pings_.size(), request);

  pg_pings_.push_back(request);

  return pg_pings_.size() - 1;
}

void WebUIInfoSingleton::AddToPGResponses(
    int token,
    const LoginReputationClientResponse& response) {
  if (webui_instances_.empty())
    return;

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyPGResponseJsListener(token, response);

  pg_responses_[token] = response;
}

void WebUIInfoSingleton::ClearPGPings() {
  std::vector<LoginReputationClientRequest>().swap(pg_pings_);
  std::map<int, LoginReputationClientResponse>().swap(pg_responses_);
}

void WebUIInfoSingleton::LogMessage(const std::string& message) {
  if (webui_instances_.empty())
    return;

  base::Time timestamp = base::Time::Now();
  log_messages_.push_back(std::make_pair(timestamp, message));

  for (auto* webui_listener : webui_instances_)
    webui_listener->NotifyLogMessageJsListener(timestamp, message);
}

void WebUIInfoSingleton::ClearLogMessages() {
  std::vector<std::pair<base::Time, std::string>>().swap(log_messages_);
}

void WebUIInfoSingleton::RegisterWebUIInstance(SafeBrowsingUIHandler* webui) {
  webui_instances_.push_back(webui);
}

void WebUIInfoSingleton::UnregisterWebUIInstance(SafeBrowsingUIHandler* webui) {
  base::Erase(webui_instances_, webui);
  if (webui_instances_.empty()) {
    ClearCSBRRsSent();
    ClearClientDownloadRequestsSent();
    ClearClientDownloadResponsesReceived();
    ClearPGEvents();
    ClearPGPings();
    ClearLogMessages();
  }
}

namespace {
#if SAFE_BROWSING_DB_LOCAL

base::Value UserReadableTimeFromMillisSinceEpoch(int64_t time_in_milliseconds) {
  base::Time time = base::Time::UnixEpoch() +
                    base::TimeDelta::FromMilliseconds(time_in_milliseconds);
  return base::Value(
      base::UTF16ToASCII(base::TimeFormatShortDateAndTime(time)));
}

void AddStoreInfo(const DatabaseManagerInfo::DatabaseInfo::StoreInfo store_info,
                  base::ListValue* database_info_list) {
  if (store_info.has_file_size_bytes() && store_info.has_file_name()) {
    database_info_list->GetList().push_back(
        base::Value(store_info.file_name()));
    database_info_list->GetList().push_back(
        base::Value(static_cast<double>(store_info.file_size_bytes())));
  }
  if (store_info.has_update_status()) {
    database_info_list->GetList().push_back(base::Value("Store update status"));
    database_info_list->GetList().push_back(
        base::Value(store_info.update_status()));
  }
  if (store_info.has_last_apply_update_time_millis()) {
    database_info_list->GetList().push_back(base::Value("Last update time"));
    database_info_list->GetList().push_back(
        UserReadableTimeFromMillisSinceEpoch(
            store_info.last_apply_update_time_millis()));
  }
  if (store_info.has_checks_attempted()) {
    database_info_list->GetList().push_back(
        base::Value("Number of database checks"));
    database_info_list->GetList().push_back(
        base::Value(static_cast<int>(store_info.checks_attempted())));
  }
}

void AddDatabaseInfo(const DatabaseManagerInfo::DatabaseInfo database_info,
                     base::ListValue* database_info_list) {
  if (database_info.has_database_size_bytes()) {
    database_info_list->GetList().push_back(
        base::Value("Database size in bytes"));
    database_info_list->GetList().push_back(
        base::Value(static_cast<double>(database_info.database_size_bytes())));
  }

  // Add the information specific to each store.
  for (int i = 0; i < database_info.store_info_size(); i++) {
    AddStoreInfo(database_info.store_info(i), database_info_list);
  }
}

void AddUpdateInfo(const DatabaseManagerInfo::UpdateInfo update_info,
                   base::ListValue* database_info_list) {
  if (update_info.has_network_status_code()) {
    // Network status of the last GetUpdate().
    database_info_list->GetList().push_back(
        base::Value("Last update network status code"));
    database_info_list->GetList().push_back(
        base::Value(update_info.network_status_code()));
  }
  if (update_info.has_last_update_time_millis()) {
    database_info_list->GetList().push_back(base::Value("Last update time"));
    database_info_list->GetList().push_back(
        UserReadableTimeFromMillisSinceEpoch(
            update_info.last_update_time_millis()));
  }
}

void ParseFullHashInfo(
    const FullHashCacheInfo::FullHashCache::CachedHashPrefixInfo::FullHashInfo
        full_hash_info,
    base::DictionaryValue* full_hash_info_dict) {
  if (full_hash_info.has_positive_expiry()) {
    full_hash_info_dict->SetString(
        "Positive expiry",
        UserReadableTimeFromMillisSinceEpoch(full_hash_info.positive_expiry())
            .GetString());
  }
  if (full_hash_info.has_full_hash()) {
    std::string full_hash;
    base::Base64UrlEncode(full_hash_info.full_hash(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &full_hash);
    full_hash_info_dict->SetString("Full hash (base64)", full_hash);
  }
  if (full_hash_info.list_identifier().has_platform_type()) {
    full_hash_info_dict->SetInteger(
        "platform_type", full_hash_info.list_identifier().platform_type());
  }
  if (full_hash_info.list_identifier().has_threat_entry_type()) {
    full_hash_info_dict->SetInteger(
        "threat_entry_type",
        full_hash_info.list_identifier().threat_entry_type());
  }
  if (full_hash_info.list_identifier().has_threat_type()) {
    full_hash_info_dict->SetInteger(
        "threat_type", full_hash_info.list_identifier().threat_type());
  }
}

void ParseFullHashCache(const FullHashCacheInfo::FullHashCache full_hash_cache,
                        base::ListValue* full_hash_cache_list) {
  base::DictionaryValue full_hash_cache_parsed;

  if (full_hash_cache.has_hash_prefix()) {
    std::string hash_prefix;
    base::Base64UrlEncode(full_hash_cache.hash_prefix(),
                          base::Base64UrlEncodePolicy::INCLUDE_PADDING,
                          &hash_prefix);
    full_hash_cache_parsed.SetString("Hash prefix (base64)", hash_prefix);
  }
  if (full_hash_cache.cached_hash_prefix_info().has_negative_expiry()) {
    full_hash_cache_parsed.SetString(
        "Negative expiry",
        UserReadableTimeFromMillisSinceEpoch(
            full_hash_cache.cached_hash_prefix_info().negative_expiry())
            .GetString());
  }

  full_hash_cache_list->GetList().push_back(std::move(full_hash_cache_parsed));

  for (auto full_hash_info_it :
       full_hash_cache.cached_hash_prefix_info().full_hash_info()) {
    base::DictionaryValue full_hash_info_dict;
    ParseFullHashInfo(full_hash_info_it, &full_hash_info_dict);
    full_hash_cache_list->GetList().push_back(std::move(full_hash_info_dict));
  }
}

void ParseFullHashCacheInfo(const FullHashCacheInfo full_hash_cache_info_proto,
                            base::ListValue* full_hash_cache_info) {
  if (full_hash_cache_info_proto.has_number_of_hits()) {
    base::DictionaryValue number_of_hits;
    number_of_hits.SetInteger("Number of cache hits",
                              full_hash_cache_info_proto.number_of_hits());
    full_hash_cache_info->GetList().push_back(std::move(number_of_hits));
  }

  // Record FullHashCache list.
  for (auto full_hash_cache_it : full_hash_cache_info_proto.full_hash_cache()) {
    base::ListValue full_hash_cache_list;
    ParseFullHashCache(full_hash_cache_it, &full_hash_cache_list);
    full_hash_cache_info->GetList().push_back(std::move(full_hash_cache_list));
  }
}

std::string AddFullHashCacheInfo(
    const FullHashCacheInfo full_hash_cache_info_proto) {
  std::string full_hash_cache_parsed;

  base::ListValue full_hash_cache;
  ParseFullHashCacheInfo(full_hash_cache_info_proto, &full_hash_cache);

  base::Value* full_hash_cache_tree = &full_hash_cache;

  JSONStringValueSerializer serializer(&full_hash_cache_parsed);
  serializer.set_pretty_print(true);
  serializer.Serialize(*full_hash_cache_tree);

  return full_hash_cache_parsed;
}

#endif

base::Value SerializeReferrer(const ReferrerChainEntry& referrer) {
  base::DictionaryValue referrer_dict;
  referrer_dict.SetKey("url", base::Value(referrer.url()));
  referrer_dict.SetKey("main_frame_url",
                       base::Value(referrer.main_frame_url()));

  std::string url_type;
  switch (referrer.type()) {
    case ReferrerChainEntry::EVENT_URL:
      url_type = "EVENT_URL";
      break;
    case ReferrerChainEntry::LANDING_PAGE:
      url_type = "LANDING_PAGE";
      break;
    case ReferrerChainEntry::LANDING_REFERRER:
      url_type = "LANDING_REFERRER";
      break;
    case ReferrerChainEntry::CLIENT_REDIRECT:
      url_type = "CLIENT_REDIRECT";
      break;
    case ReferrerChainEntry::DEPRECATED_SERVER_REDIRECT:
      url_type = "DEPRECATED_SERVER_REDIRECT";
      break;
    case ReferrerChainEntry::RECENT_NAVIGATION:
      url_type = "RECENT_NAVIGATION";
      break;
    case ReferrerChainEntry::REFERRER:
      url_type = "REFERRER";
      break;
  }
  referrer_dict.SetKey("type", base::Value(url_type));

  base::ListValue ip_addresses;
  for (const std::string& ip_address : referrer.ip_addresses()) {
    ip_addresses.GetList().push_back(base::Value(ip_address));
  }
  referrer_dict.SetKey("ip_addresses", std::move(ip_addresses));

  referrer_dict.SetKey("referrer_url", base::Value(referrer.referrer_url()));

  referrer_dict.SetKey("referrer_main_frame_url",
                       base::Value(referrer.referrer_main_frame_url()));

  referrer_dict.SetKey("is_retargeting",
                       base::Value(referrer.is_retargeting()));

  referrer_dict.SetKey("navigation_time_msec",
                       base::Value(referrer.navigation_time_msec()));

  base::ListValue server_redirects;
  for (const ReferrerChainEntry::ServerRedirect& server_redirect :
       referrer.server_redirect_chain()) {
    server_redirects.GetList().push_back(base::Value(server_redirect.url()));
  }
  referrer_dict.SetKey("server_redirect_chain", std::move(server_redirects));

  std::string navigation_initiation;
  switch (referrer.navigation_initiation()) {
    case ReferrerChainEntry::UNDEFINED:
      navigation_initiation = "UNDEFINED";
      break;
    case ReferrerChainEntry::BROWSER_INITIATED:
      navigation_initiation = "BROWSER_INITIATED";
      break;
    case ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE:
      navigation_initiation = "RENDERER_INITIATED_WITHOUT_USER_GESTURE";
      break;
    case ReferrerChainEntry::RENDERER_INITIATED_WITH_USER_GESTURE:
      navigation_initiation = "RENDERER_INITIATED_WITH_USER_GESTURE";
      break;
  }
  referrer_dict.SetKey("navigation_initiation",
                       base::Value(navigation_initiation));

  referrer_dict.SetKey(
      "maybe_launched_by_external_application",
      base::Value(referrer.maybe_launched_by_external_application()));

  return std::move(referrer_dict);
}

std::string SerializeClientDownloadRequest(const ClientDownloadRequest& cdr) {
  base::DictionaryValue dict;
  if (cdr.has_url())
    dict.SetString("url", cdr.url());
  if (cdr.has_download_type())
    dict.SetInteger("download_type", cdr.download_type());
  if (cdr.has_length())
    dict.SetInteger("length", cdr.length());
  if (cdr.has_file_basename())
    dict.SetString("file_basename", cdr.file_basename());
  if (cdr.has_archive_valid())
    dict.SetBoolean("archive_valid", cdr.archive_valid());

  auto archived_binaries = std::make_unique<base::ListValue>();
  for (const auto& archived_binary : cdr.archived_binary()) {
    auto dict_archived_binary = std::make_unique<base::DictionaryValue>();
    if (archived_binary.has_file_basename())
      dict_archived_binary->SetString("file_basename",
                                      archived_binary.file_basename());
    if (archived_binary.has_download_type())
      dict_archived_binary->SetInteger("download_type",
                                       archived_binary.download_type());
    if (archived_binary.has_length())
      dict_archived_binary->SetInteger("length", archived_binary.length());
    if (archived_binary.is_encrypted())
      dict_archived_binary->SetBoolean("is_encrypted", true);
    archived_binaries->Append(std::move(dict_archived_binary));
  }
  dict.SetList("archived_binary", std::move(archived_binaries));

  auto referrer_chain = std::make_unique<base::ListValue>();
  for (const auto& referrer_chain_entry : cdr.referrer_chain()) {
    referrer_chain->GetList().push_back(
        SerializeReferrer(referrer_chain_entry));
  }
  dict.SetList("referrer_chain", std::move(referrer_chain));

  base::Value* request_tree = &dict;
  std::string request_serialized;
  JSONStringValueSerializer serializer(&request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(*request_tree);
  return request_serialized;
}

std::string SerializeClientDownloadResponse(const ClientDownloadResponse& cdr) {
  base::DictionaryValue dict;

  switch (cdr.verdict()) {
    case ClientDownloadResponse::SAFE:
      dict.SetKey("verdict", base::Value("SAFE"));
      break;
    case ClientDownloadResponse::DANGEROUS:
      dict.SetKey("verdict", base::Value("DANGEROUS"));
      break;
    case ClientDownloadResponse::UNCOMMON:
      dict.SetKey("verdict", base::Value("UNCOMMON"));
      break;
    case ClientDownloadResponse::POTENTIALLY_UNWANTED:
      dict.SetKey("verdict", base::Value("POTENTIALLY_UNWANTED"));
      break;
    case ClientDownloadResponse::DANGEROUS_HOST:
      dict.SetKey("verdict", base::Value("DANGEROUS_HOST"));
      break;
    case ClientDownloadResponse::UNKNOWN:
      dict.SetKey("verdict", base::Value("UNKNOWN"));
      break;
  }

  if (cdr.has_more_info()) {
    dict.SetPath({"more_info", "description"},
                 base::Value(cdr.more_info().description()));
    dict.SetPath({"more_info", "url"}, base::Value(cdr.more_info().url()));
  }

  if (cdr.has_token()) {
    dict.SetKey("token", base::Value(cdr.token()));
  }

  if (cdr.has_upload()) {
    dict.SetKey("upload", base::Value(cdr.upload()));
  }

  base::Value* request_tree = &dict;
  std::string request_serialized;
  JSONStringValueSerializer serializer(&request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(*request_tree);
  return request_serialized;
}

std::string SerializeCSBRR(const ClientSafeBrowsingReportRequest& report) {
  base::DictionaryValue report_request;
  if (report.has_type()) {
    report_request.SetInteger("type", static_cast<int>(report.type()));
  }
  if (report.has_page_url())
    report_request.SetString("page_url", report.page_url());
  if (report.has_client_country()) {
    report_request.SetString("client_country", report.client_country());
  }
  if (report.has_repeat_visit()) {
    report_request.SetInteger("repeat_visit", report.repeat_visit());
  }
  if (report.has_did_proceed()) {
    report_request.SetInteger("did_proceed", report.did_proceed());
  }
  std::string serialized;
  if (report.SerializeToString(&serialized)) {
    std::string base64_encoded;
    base::Base64Encode(serialized, &base64_encoded);
    report_request.SetString("csbrr(base64)", base64_encoded);
  }

  base::Value* report_request_tree = &report_request;
  std::string report_request_serialized;
  JSONStringValueSerializer serializer(&report_request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(*report_request_tree);
  return report_request_serialized;
}

base::DictionaryValue SerializePGEvent(
    const sync_pb::UserEventSpecifics& event) {
  base::DictionaryValue result;

  base::Time timestamp = base::Time::FromDeltaSinceWindowsEpoch(
      base::TimeDelta::FromMicroseconds(event.event_time_usec()));
  result.SetDouble("time", timestamp.ToJsTime());

  base::DictionaryValue event_dict;

  // Nominally only one of the following if() statements would be true.
  // Note that top-level path is either password_captured, or one of the fields
  // under GaiaPasswordReuse (ie. we've flattened the namespace for simplicity).

  if (event.has_gaia_password_captured_event()) {
    std::string event_trigger;

    switch (event.gaia_password_captured_event().event_trigger()) {
      case PasswordCaptured::UNSPECIFIED:
        event_trigger = "UNSPECIFIED";
        break;
      case PasswordCaptured::USER_LOGGED_IN:
        event_trigger = "USER_LOGGED_IN";
        break;
      case PasswordCaptured::EXPIRED_28D_TIMER:
        event_trigger = "EXPIRED_28D_TIMER";
        break;
    }

    event_dict.SetPath({"password_captured", "event_trigger"},
                       base::Value(event_trigger));
  }

  sync_pb::UserEventSpecifics::GaiaPasswordReuse reuse =
      event.gaia_password_reuse_event();
  if (reuse.has_reuse_detected()) {
    event_dict.SetPath({"reuse_detected", "status", "enabled"},
                       base::Value(reuse.reuse_detected().status().enabled()));

    std::string reporting_population;
    switch (
        reuse.reuse_detected().status().safe_browsing_reporting_population()) {
      case PasswordReuseDetected::SafeBrowsingStatus::
          REPORTING_POPULATION_UNSPECIFIED:
        reporting_population = "REPORTING_POPULATION_UNSPECIFIED";
        break;
      case PasswordReuseDetected::SafeBrowsingStatus::NONE:
        reporting_population = "NONE";
        break;
      case PasswordReuseDetected::SafeBrowsingStatus::EXTENDED_REPORTING:
        reporting_population = "EXTENDED_REPORTING";
        break;
      case PasswordReuseDetected::SafeBrowsingStatus::SCOUT:
        reporting_population = "SCOUT";
        break;
    }
    event_dict.SetPath({"reuse_detected", "status", "reporting_population"},
                       base::Value(reporting_population));
  }

  if (reuse.has_reuse_lookup()) {
    std::string lookup_result;
    switch (reuse.reuse_lookup().lookup_result()) {
      case PasswordReuseLookup::UNSPECIFIED:
        lookup_result = "UNSPECIFIED";
        break;
      case PasswordReuseLookup::WHITELIST_HIT:
        lookup_result = "WHITELIST_HIT";
        break;
      case PasswordReuseLookup::CACHE_HIT:
        lookup_result = "CACHE_HIT";
        break;
      case PasswordReuseLookup::REQUEST_SUCCESS:
        lookup_result = "REQUEST_SUCCESS";
        break;
      case PasswordReuseLookup::REQUEST_FAILURE:
        lookup_result = "REQUEST_FAILURE";
        break;
      case PasswordReuseLookup::URL_UNSUPPORTED:
        lookup_result = "URL_UNSUPPORTED";
        break;
      case PasswordReuseLookup::ENTERPRISE_WHITELIST_HIT:
        lookup_result = "ENTERPRISE_WHITELIST_HIT";
        break;
      case PasswordReuseLookup::TURNED_OFF_BY_POLICY:
        lookup_result = "TURNED_OFF_BY_POLICY";
        break;
    }
    event_dict.SetPath({"reuse_lookup", "lookup_result"},
                       base::Value(lookup_result));

    std::string verdict;
    switch (reuse.reuse_lookup().verdict()) {
      case PasswordReuseLookup::VERDICT_UNSPECIFIED:
        verdict = "VERDICT_UNSPECIFIED";
        break;
      case PasswordReuseLookup::SAFE:
        verdict = "SAFE";
        break;
      case PasswordReuseLookup::LOW_REPUTATION:
        verdict = "LOW_REPUTATION";
        break;
      case PasswordReuseLookup::PHISHING:
        verdict = "PHISHING";
        break;
    }
    event_dict.SetPath({"reuse_lookup", "verdict"}, base::Value(verdict));
    event_dict.SetPath({"reuse_lookup", "verdict_token"},
                       base::Value(reuse.reuse_lookup().verdict_token()));
  }

  if (reuse.has_dialog_interaction()) {
    std::string interaction_result;
    switch (reuse.dialog_interaction().interaction_result()) {
      case PasswordReuseDialogInteraction::UNSPECIFIED:
        interaction_result = "UNSPECIFIED";
        break;
      case PasswordReuseDialogInteraction::WARNING_ACTION_TAKEN:
        interaction_result = "WARNING_ACTION_TAKEN";
        break;
      case PasswordReuseDialogInteraction::WARNING_ACTION_IGNORED:
        interaction_result = "WARNING_ACTION_IGNORED";
        break;
      case PasswordReuseDialogInteraction::WARNING_UI_IGNORED:
        interaction_result = "WARNING_UI_IGNORED";
        break;
      case PasswordReuseDialogInteraction::WARNING_ACTION_TAKEN_ON_SETTINGS:
        interaction_result = "WARNING_ACTION_TAKEN_ON_SETTINGS";
        break;
    }
    event_dict.SetPath({"dialog_interaction", "interaction_result"},
                       base::Value(interaction_result));
  }

  std::string event_serialized;
  JSONStringValueSerializer serializer(&event_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(event_dict);
  result.SetString("message", event_serialized);
  return result;
}

base::Value SerializeFrame(const LoginReputationClientRequest::Frame& frame) {
  base::DictionaryValue frame_dict;
  frame_dict.SetKey("frame_index", base::Value(frame.frame_index()));
  frame_dict.SetKey("parent_frame_index",
                    base::Value(frame.parent_frame_index()));
  frame_dict.SetKey("url", base::Value(frame.url()));
  frame_dict.SetKey("has_password_field",
                    base::Value(frame.has_password_field()));

  base::ListValue referrer_list;
  for (const ReferrerChainEntry& referrer : frame.referrer_chain()) {
    referrer_list.GetList().push_back(SerializeReferrer(referrer));
  }
  frame_dict.SetKey("referrer_chain", std::move(referrer_list));

  frame_dict.SetPath(
      {"referrer_chain_options", "recent_navigations_to_collect"},
      base::Value(
          frame.referrer_chain_options().recent_navigations_to_collect()));

  base::ListValue form_list;
  for (const LoginReputationClientRequest::Frame::Form& form : frame.forms()) {
    base::DictionaryValue form_dict;
    form_dict.SetKey("action_url", base::Value(form.action_url()));
    form_dict.SetKey("has_password_field",
                     base::Value(form.has_password_field()));
    form_list.GetList().push_back(std::move(form_dict));
  }
  frame_dict.SetKey("forms", std::move(form_list));

  return std::move(frame_dict);
}

base::Value SerializePasswordReuseEvent(
    const LoginReputationClientRequest::PasswordReuseEvent& event) {
  base::DictionaryValue event_dict;

  base::ListValue domains_list;
  for (const std::string& domain : event.domains_matching_password()) {
    domains_list.GetList().push_back(base::Value(domain));
  }
  event_dict.SetKey("domains_matching_password", std::move(domains_list));

  event_dict.SetKey("frame_id", base::Value(event.frame_id()));
  event_dict.SetKey("is_chrome_signin_password",
                    base::Value(event.is_chrome_signin_password()));

  std::string sync_account_type;
  switch (event.sync_account_type()) {
    case LoginReputationClientRequest::PasswordReuseEvent::NOT_SIGNED_IN:
      sync_account_type = "NOT_SIGNED_IN";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::GMAIL:
      sync_account_type = "GMAIL";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::GSUITE:
      sync_account_type = "GSUITE";
      break;
  }
  event_dict.SetKey("sync_account_type", base::Value(sync_account_type));

  std::string reused_password_type;
  switch (event.reused_password_type()) {
    case LoginReputationClientRequest::PasswordReuseEvent::
        REUSED_PASSWORD_TYPE_UNKNOWN:
      reused_password_type = "REUSED_PASSWORD_TYPE_UNKNOWN";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::SAVED_PASSWORD:
      reused_password_type = "SAVED_PASSWORD";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::SIGN_IN_PASSWORD:
      reused_password_type = "SIGN_IN_PASSWORD";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::OTHER_GAIA_PASSWORD:
      reused_password_type = "OTHER_GAIA_PASSWORD";
      break;
    case LoginReputationClientRequest::PasswordReuseEvent::ENTERPRISE_PASSWORD:
      reused_password_type = "ENTERPRISE_PASSWORD";
      break;
  }
  event_dict.SetKey("reused_password_type", base::Value(reused_password_type));

  return std::move(event_dict);
}

base::Value SerializeChromeUserPopulation(
    const ChromeUserPopulation& population) {
  base::DictionaryValue population_dict;

  std::string user_population;
  switch (population.user_population()) {
    case ChromeUserPopulation::UNKNOWN_USER_POPULATION:
      user_population = "UNKNOWN_USER_POPULATION";
      break;
    case ChromeUserPopulation::SAFE_BROWSING:
      user_population = "SAFE_BROWSING";
      break;
    case ChromeUserPopulation::EXTENDED_REPORTING:
      user_population = "EXTENDED_REPORTING";
      break;
  }
  population_dict.SetKey("user_population", base::Value(user_population));

  population_dict.SetKey("is_history_sync_enabled",
                         base::Value(population.is_history_sync_enabled()));

  base::ListValue finch_list;
  for (const std::string& finch_group : population.finch_active_groups()) {
    finch_list.GetList().push_back(base::Value(finch_group));
  }
  population_dict.SetKey("finch_active_groups", std::move(finch_list));

  std::string management_status;
  switch (population.profile_management_status()) {
    case ChromeUserPopulation::UNKNOWN:
      management_status = "UNKNOWN";
      break;
    case ChromeUserPopulation::UNAVAILABLE:
      management_status = "UNAVAILABLE";
      break;
    case ChromeUserPopulation::NOT_MANAGED:
      management_status = "NOT_MANAGED";
      break;
    case ChromeUserPopulation::ENTERPRISE_MANAGED:
      management_status = "ENTERPRISE_MANAGED";
      break;
  }
  population_dict.SetKey("profile_management_status",
                         base::Value(management_status));
  population_dict.SetKey(
      "is_under_advanced_protection",
      base::Value(population.is_under_advanced_protection()));
  population_dict.SetKey("is_incognito",
                         base::Value(population.is_incognito()));

  return std::move(population_dict);
}

std::string SerializePGPing(const LoginReputationClientRequest& request) {
  base::DictionaryValue request_dict;

  request_dict.SetKey("page_url", base::Value(request.page_url()));

  std::string trigger_type;
  switch (request.trigger_type()) {
    case LoginReputationClientRequest::TRIGGER_TYPE_UNSPECIFIED:
      trigger_type = "TRIGGER_TYPE_UNSPECIFIED";
      break;
    case LoginReputationClientRequest::UNFAMILIAR_LOGIN_PAGE:
      trigger_type = "UNFAMILIAR_LOGIN_PAGE";
      break;
    case LoginReputationClientRequest::PASSWORD_REUSE_EVENT:
      trigger_type = "PASSWORD_REUSE_EVENT";
      break;
  }
  request_dict.SetKey("trigger_type", base::Value(trigger_type));

  base::ListValue frames_list;
  for (const LoginReputationClientRequest::Frame& frame : request.frames()) {
    frames_list.GetList().push_back(SerializeFrame(frame));
  }
  request_dict.SetKey("frames", std::move(frames_list));

  request_dict.SetKey(
      "password_reuse_event",
      SerializePasswordReuseEvent(request.password_reuse_event()));
  request_dict.SetKey("stored_verdict_cnt",
                      base::Value(request.stored_verdict_cnt()));
  request_dict.SetKey("population",
                      SerializeChromeUserPopulation(request.population()));
  request_dict.SetKey("clicked_through_interstitial",
                      base::Value(request.clicked_through_interstitial()));
  request_dict.SetKey("content_type", base::Value(request.content_type()));

  if (request.has_content_area_height()) {
    request_dict.SetKey("content_area_height",
                        base::Value(request.content_area_height()));
  }
  if (request.has_content_area_width()) {
    request_dict.SetKey("content_area_width",
                        base::Value(request.content_area_width()));
  }

  std::string request_serialized;
  JSONStringValueSerializer serializer(&request_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(request_dict);
  return request_serialized;
}

std::string SerializePGResponse(const LoginReputationClientResponse& response) {
  base::DictionaryValue response_dict;

  std::string verdict;
  switch (response.verdict_type()) {
    case LoginReputationClientResponse::VERDICT_TYPE_UNSPECIFIED:
      verdict = "VERDICT_TYPE_UNSPECIFIED";
      break;
    case LoginReputationClientResponse::SAFE:
      verdict = "SAFE";
      break;
    case LoginReputationClientResponse::LOW_REPUTATION:
      verdict = "LOW_REPUTATION";
      break;
    case LoginReputationClientResponse::PHISHING:
      verdict = "PHISHING";
      break;
  }
  response_dict.SetKey("verdict_type", base::Value(verdict));
  response_dict.SetKey("cache_duration_sec",
                       base::Value(int(response.cache_duration_sec())));
  response_dict.SetKey("cache_expression",
                       base::Value(response.cache_expression()));
  response_dict.SetKey("verdict_token", base::Value(response.verdict_token()));

  std::string response_serialized;
  JSONStringValueSerializer serializer(&response_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(response_dict);
  return response_serialized;
}

base::Value SerializeLogMessage(const base::Time& timestamp,
                                const std::string& message) {
  base::DictionaryValue result;
  result.SetDouble("time", timestamp.ToJsTime());
  result.SetString("message", message);
  return std::move(result);
}

}  // namespace

SafeBrowsingUI::SafeBrowsingUI(content::WebUI* web_ui)
    : content::WebUIController(web_ui) {
  // Set up the chrome://safe-browsing source.

  content::WebUIDataSource* html_source = content::WebUIDataSource::Create(
      safe_browsing::kChromeUISafeBrowsingHost);

  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();

  // Register callback handler.
  // Handles messages from JavaScript to C++ via chrome.send().
  web_ui->AddMessageHandler(
      std::make_unique<SafeBrowsingUIHandler>(browser_context));

  // Add required resources.
  html_source->AddResourcePath("safe_browsing.css", IDR_SAFE_BROWSING_CSS);
  html_source->AddResourcePath("safe_browsing.js", IDR_SAFE_BROWSING_JS);
  html_source->SetDefaultResource(IDR_SAFE_BROWSING_HTML);
  html_source->UseGzip();

  content::WebUIDataSource::Add(browser_context, html_source);
}

SafeBrowsingUI::~SafeBrowsingUI() {}

SafeBrowsingUIHandler::SafeBrowsingUIHandler(content::BrowserContext* context)
    : browser_context_(context) {
  WebUIInfoSingleton::GetInstance()->RegisterWebUIInstance(this);
}

SafeBrowsingUIHandler::~SafeBrowsingUIHandler() {
  WebUIInfoSingleton::GetInstance()->UnregisterWebUIInstance(this);
}

void SafeBrowsingUIHandler::GetExperiments(const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  ResolveJavascriptCallback(base::Value(callback_id), GetFeatureStatusList());
}

void SafeBrowsingUIHandler::GetPrefs(const base::ListValue* args) {
  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  ResolveJavascriptCallback(base::Value(callback_id),
                            safe_browsing::GetSafeBrowsingPreferencesList(
                                user_prefs::UserPrefs::Get(browser_context_)));
}

void SafeBrowsingUIHandler::GetSavedPasswords(const base::ListValue* args) {
  password_manager::HashPasswordManager hash_manager(
      user_prefs::UserPrefs::Get(browser_context_));

  base::ListValue saved_passwords;
  for (const password_manager::PasswordHashData& hash_data :
       hash_manager.RetrieveAllPasswordHashes()) {
    saved_passwords.AppendString(hash_data.username);
    saved_passwords.AppendBoolean(hash_data.is_gaia_password);
  }

  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  ResolveJavascriptCallback(base::Value(callback_id), saved_passwords);
}

void SafeBrowsingUIHandler::GetDatabaseManagerInfo(
    const base::ListValue* args) {
  base::ListValue database_manager_info;

#if SAFE_BROWSING_DB_LOCAL
  const V4LocalDatabaseManager* local_database_manager_instance =
      V4LocalDatabaseManager::current_local_database_manager();
  if (local_database_manager_instance) {
    DatabaseManagerInfo database_manager_info_proto;
    FullHashCacheInfo full_hash_cache_info_proto;

    local_database_manager_instance->CollectDatabaseManagerInfo(
        &database_manager_info_proto, &full_hash_cache_info_proto);

    if (database_manager_info_proto.has_update_info()) {
      AddUpdateInfo(database_manager_info_proto.update_info(),
                    &database_manager_info);
    }
    if (database_manager_info_proto.has_database_info()) {
      AddDatabaseInfo(database_manager_info_proto.database_info(),
                      &database_manager_info);
    }

    database_manager_info.GetList().push_back(
        base::Value(AddFullHashCacheInfo(full_hash_cache_info_proto)));
  }
#endif

  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);

  ResolveJavascriptCallback(base::Value(callback_id), database_manager_info);
}

void SafeBrowsingUIHandler::GetSentClientDownloadRequests(
    const base::ListValue* args) {
  const std::vector<std::unique_ptr<ClientDownloadRequest>>& cdrs =
      WebUIInfoSingleton::GetInstance()->client_download_requests_sent();

  base::ListValue cdrs_sent;

  for (const auto& cdr : cdrs) {
    cdrs_sent.GetList().push_back(
        base::Value(SerializeClientDownloadRequest(*cdr)));
  }

  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  ResolveJavascriptCallback(base::Value(callback_id), cdrs_sent);
}

void SafeBrowsingUIHandler::GetReceivedClientDownloadResponses(
    const base::ListValue* args) {
  const std::vector<std::unique_ptr<ClientDownloadResponse>>& cdrs =
      WebUIInfoSingleton::GetInstance()->client_download_responses_received();

  base::ListValue cdrs_received;

  for (const auto& cdr : cdrs) {
    cdrs_received.GetList().push_back(
        base::Value(SerializeClientDownloadResponse(*cdr)));
  }

  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  ResolveJavascriptCallback(base::Value(callback_id), cdrs_received);
}

void SafeBrowsingUIHandler::GetSentCSBRRs(const base::ListValue* args) {
  const std::vector<std::unique_ptr<ClientSafeBrowsingReportRequest>>& reports =
      WebUIInfoSingleton::GetInstance()->csbrrs_sent();

  base::ListValue sent_reports;

  for (const auto& report : reports) {
    sent_reports.GetList().push_back(base::Value(SerializeCSBRR(*report)));
  }

  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  ResolveJavascriptCallback(base::Value(callback_id), sent_reports);
}

void SafeBrowsingUIHandler::GetPGEvents(const base::ListValue* args) {
  const std::vector<sync_pb::UserEventSpecifics>& events =
      WebUIInfoSingleton::GetInstance()->pg_event_log();

  base::ListValue events_sent;

  for (const sync_pb::UserEventSpecifics& event : events)
    events_sent.GetList().push_back(SerializePGEvent(event));

  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  ResolveJavascriptCallback(base::Value(callback_id), events_sent);
}

void SafeBrowsingUIHandler::GetPGPings(const base::ListValue* args) {
  const std::vector<LoginReputationClientRequest> requests =
      WebUIInfoSingleton::GetInstance()->pg_pings();

  base::ListValue pings_sent;
  for (size_t request_index = 0; request_index < requests.size();
       request_index++) {
    base::ListValue ping_entry;
    ping_entry.GetList().push_back(base::Value(int(request_index)));
    ping_entry.GetList().push_back(
        base::Value(SerializePGPing(requests[request_index])));
    pings_sent.GetList().push_back(std::move(ping_entry));
  }

  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  ResolveJavascriptCallback(base::Value(callback_id), pings_sent);
}

void SafeBrowsingUIHandler::GetPGResponses(const base::ListValue* args) {
  const std::map<int, LoginReputationClientResponse> responses =
      WebUIInfoSingleton::GetInstance()->pg_responses();

  base::ListValue responses_sent;
  for (const auto& token_and_response : responses) {
    base::ListValue response_entry;
    response_entry.GetList().push_back(base::Value(token_and_response.first));
    response_entry.GetList().push_back(
        base::Value(SerializePGResponse(token_and_response.second)));
    responses_sent.GetList().push_back(std::move(response_entry));
  }

  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  ResolveJavascriptCallback(base::Value(callback_id), responses_sent);
}

void SafeBrowsingUIHandler::GetReferrerChain(const base::ListValue* args) {
  std::string url_string;
  args->GetString(1, &url_string);

  ReferrerChainProvider* provider =
      WebUIInfoSingleton::GetInstance()->referrer_chain_provider();

  std::string callback_id;
  args->GetString(0, &callback_id);

  if (!provider) {
    AllowJavascript();
    ResolveJavascriptCallback(base::Value(callback_id), base::Value(""));
  }

  ReferrerChain referrer_chain;
  provider->IdentifyReferrerChainByEventURL(
      GURL(url_string), SessionID::InvalidValue(), 2, &referrer_chain);

  base::ListValue referrer_list;
  for (const ReferrerChainEntry& entry : referrer_chain) {
    referrer_list.GetList().push_back(SerializeReferrer(entry));
  }

  std::string referrer_chain_serialized;
  JSONStringValueSerializer serializer(&referrer_chain_serialized);
  serializer.set_pretty_print(true);
  serializer.Serialize(referrer_list);

  AllowJavascript();
  ResolveJavascriptCallback(base::Value(callback_id),
                            base::Value(referrer_chain_serialized));
}

void SafeBrowsingUIHandler::GetLogMessages(const base::ListValue* args) {
  const std::vector<std::pair<base::Time, std::string>>& log_messages =
      WebUIInfoSingleton::GetInstance()->log_messages();

  base::ListValue messages_received;
  for (const auto& message : log_messages) {
    messages_received.GetList().push_back(
        base::Value(SerializeLogMessage(message.first, message.second)));
  }

  AllowJavascript();
  std::string callback_id;
  args->GetString(0, &callback_id);
  ResolveJavascriptCallback(base::Value(callback_id), messages_received);
}

void SafeBrowsingUIHandler::NotifyClientDownloadRequestJsListener(
    ClientDownloadRequest* client_download_request) {
  AllowJavascript();
  FireWebUIListener(
      "sent-client-download-requests-update",
      base::Value(SerializeClientDownloadRequest(*client_download_request)));
}

void SafeBrowsingUIHandler::NotifyClientDownloadResponseJsListener(
    ClientDownloadResponse* client_download_response) {
  AllowJavascript();
  FireWebUIListener(
      "received-client-download-responses-update",
      base::Value(SerializeClientDownloadResponse(*client_download_response)));
}

void SafeBrowsingUIHandler::NotifyCSBRRJsListener(
    ClientSafeBrowsingReportRequest* csbrr) {
  AllowJavascript();
  FireWebUIListener("sent-csbrr-update", base::Value(SerializeCSBRR(*csbrr)));
}

void SafeBrowsingUIHandler::NotifyPGEventJsListener(
    const sync_pb::UserEventSpecifics& event) {
  AllowJavascript();
  FireWebUIListener("sent-pg-event", SerializePGEvent(event));
}

void SafeBrowsingUIHandler::NotifyPGPingJsListener(
    int token,
    const LoginReputationClientRequest& request) {
  base::ListValue request_list;
  request_list.GetList().push_back(base::Value(token));
  request_list.GetList().push_back(base::Value(SerializePGPing(request)));

  AllowJavascript();
  FireWebUIListener("pg-pings-update", request_list);
}

void SafeBrowsingUIHandler::NotifyPGResponseJsListener(
    int token,
    const LoginReputationClientResponse& response) {
  base::ListValue response_list;
  response_list.GetList().push_back(base::Value(token));
  response_list.GetList().push_back(base::Value(SerializePGResponse(response)));

  AllowJavascript();
  FireWebUIListener("pg-responses-update", response_list);
}

void SafeBrowsingUIHandler::NotifyLogMessageJsListener(
    const base::Time& timestamp,
    const std::string& message) {
  AllowJavascript();
  FireWebUIListener("log-messages-update",
                    base::Value(SerializeLogMessage(timestamp, message)));
}

void SafeBrowsingUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getExperiments",
      base::BindRepeating(&SafeBrowsingUIHandler::GetExperiments,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPrefs", base::BindRepeating(&SafeBrowsingUIHandler::GetPrefs,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSavedPasswords",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSavedPasswords,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDatabaseManagerInfo",
      base::BindRepeating(&SafeBrowsingUIHandler::GetDatabaseManagerInfo,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSentClientDownloadRequests",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSentClientDownloadRequests,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReceivedClientDownloadResponses",
      base::BindRepeating(
          &SafeBrowsingUIHandler::GetReceivedClientDownloadResponses,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSentCSBRRs",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSentCSBRRs,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPGEvents", base::BindRepeating(&SafeBrowsingUIHandler::GetPGEvents,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPGPings", base::BindRepeating(&SafeBrowsingUIHandler::GetPGPings,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPGResponses",
      base::BindRepeating(&SafeBrowsingUIHandler::GetPGResponses,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getLogMessages",
      base::BindRepeating(&SafeBrowsingUIHandler::GetLogMessages,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReferrerChain",
      base::BindRepeating(&SafeBrowsingUIHandler::GetReferrerChain,
                          base::Unretained(this)));
}

void SafeBrowsingUIHandler::SetWebUIForTesting(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

CrSBLogMessage::CrSBLogMessage() {}

CrSBLogMessage::~CrSBLogMessage() {
  WebUIInfoSingleton::GetInstance()->LogMessage(stream_.str());
}

}  // namespace safe_browsing
