// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/core/browser/web_ui/safe_browsing_ui_handler.h"

#include "components/os_crypt/async/browser/os_crypt_async.h"
#include "components/os_crypt/async/common/encryptor.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton.h"
#include "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton_event_observer.h"
#include "components/safe_browsing/core/browser/web_ui/web_ui_info_singleton_event_observer_impl.h"
#include "components/safe_browsing/core/common/proto/csd.to_value.h"
#include "components/user_prefs/user_prefs.h"

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
#include "components/safe_browsing/core/browser/db/v4_local_database_manager.h"
#endif

namespace safe_browsing {

SafeBrowsingUIHandler::SafeBrowsingUIHandler(
    std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate,
    os_crypt_async::OSCryptAsync* os_crypt_async)
    : delegate_(std::move(delegate)), os_crypt_async_(os_crypt_async) {}

SafeBrowsingUIHandler::~SafeBrowsingUIHandler() = default;

void SafeBrowsingUIHandler::GetExperiments(const base::ListValue& args) {
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, GetFeatureStatusList());
}

void SafeBrowsingUIHandler::GetPrefs(const base::ListValue& args) {
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id,
                  safe_browsing::GetSafeBrowsingPreferencesList(user_prefs()));
}

void SafeBrowsingUIHandler::GetPolicies(const base::ListValue& args) {
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id,
                  safe_browsing::GetSafeBrowsingPoliciesList(user_prefs()));
}

void SafeBrowsingUIHandler::GetCookie(const base::ListValue& args) {
  DCHECK(!args.empty());
  std::string callback_id = args[0].GetString();

  cookie_manager_remote_ = cookie_manager();
  cookie_manager_remote_->GetAllCookies(
      base::BindOnce(&SafeBrowsingUIHandler::OnGetCookie,
                     weak_factory_.GetWeakPtr(), std::move(callback_id)));
}

void SafeBrowsingUIHandler::OnGetCookie(
    const std::string& callback_id,
    const std::vector<net::CanonicalCookie>& cookies) {
  DCHECK_GE(1u, cookies.size());

  std::string cookie = "No cookie";
  double time = 0.0;
  if (!cookies.empty()) {
    cookie = cookies[0].Value();
    time = cookies[0].CreationDate().InMillisecondsFSinceUnixEpoch();
  }

  base::ListValue response;
  response.Append(std::move(cookie));
  response.Append(time);

  ResolveCallback(callback_id, response);
}

void SafeBrowsingUIHandler::GetSavedPasswords(const base::ListValue& args) {
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();

  os_crypt_async_->GetInstance(
      base::BindOnce(&SafeBrowsingUIHandler::GetSavedPasswordsImpl,
                     weak_factory_.GetWeakPtr(), callback_id));
}

void SafeBrowsingUIHandler::GetSavedPasswordsImpl(
    const std::string& callback_id,
    os_crypt_async::Encryptor encryptor) {
  password_manager::HashPasswordManager hash_manager(std::move(encryptor));
  hash_manager.set_prefs(user_prefs());
  hash_manager.set_local_prefs(delegate_->GetLocalState());

  base::ListValue saved_passwords;
  for (const password_manager::PasswordHashData& hash_data :
       hash_manager.RetrieveAllPasswordHashes()) {
    saved_passwords.Append(hash_data.username);
    saved_passwords.Append(hash_data.is_gaia_password);
  }

  ResolveCallback(callback_id, saved_passwords);
}

void SafeBrowsingUIHandler::GetDatabaseManagerInfo(
    const base::ListValue& args) {
  base::ListValue database_manager_info;

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
  const V4LocalDatabaseManager* local_database_manager_instance =
      V4LocalDatabaseManager::current_local_database_manager();
  if (local_database_manager_instance) {
    DatabaseManagerInfo database_manager_info_proto;
    FullHashCacheInfo full_hash_cache_info_proto;

    local_database_manager_instance->CollectDatabaseManagerInfo(
        &database_manager_info_proto, &full_hash_cache_info_proto);

    if (database_manager_info_proto.has_update_info()) {
      web_ui::AddUpdateInfo(database_manager_info_proto.update_info(),
                            database_manager_info);
    }
    if (database_manager_info_proto.has_database_info()) {
      web_ui::AddDatabaseInfo(database_manager_info_proto.database_info(),
                              database_manager_info);
    }

    database_manager_info.Append(
        web_ui::AddFullHashCacheInfo(full_hash_cache_info_proto));
  }
#endif

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();

  ResolveCallback(callback_id, database_manager_info);
}

void SafeBrowsingUIHandler::GetDownloadUrlsChecked(
    const base::ListValue& args) {
  const std::vector<std::pair<std::vector<GURL>, DownloadCheckResult>>&
      urls_checked = web_ui_info_singleton()->download_urls_checked();

  base::ListValue urls_checked_value;
  for (const auto& url_and_result : urls_checked) {
    const std::vector<GURL>& urls = url_and_result.first;
    DownloadCheckResult result = url_and_result.second;
    urls_checked_value.Append(
        web_ui::SerializeDownloadUrlChecked(urls, result));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, urls_checked_value);
}

void SafeBrowsingUIHandler::GetSentClientDownloadRequests(
    const base::ListValue& args) {
  const std::vector<std::unique_ptr<ClientDownloadRequest>>& cdrs =
      web_ui_info_singleton()->client_download_requests_sent();

  base::ListValue cdrs_sent;

  for (const auto& cdr : cdrs) {
    cdrs_sent.Append(web_ui::SerializeClientDownloadRequest(*cdr));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, cdrs_sent);
}

void SafeBrowsingUIHandler::GetReceivedClientDownloadResponses(
    const base::ListValue& args) {
  const std::vector<std::unique_ptr<ClientDownloadResponse>>& cdrs =
      web_ui_info_singleton()->client_download_responses_received();

  base::ListValue cdrs_received;

  for (const auto& cdr : cdrs) {
    cdrs_received.Append(web_ui::SerializeClientDownloadResponse(*cdr));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, cdrs_received);
}

void SafeBrowsingUIHandler::GetSentClientPhishingRequests(
    const base::ListValue& args) {
  const std::vector<web_ui::ClientPhishingRequestAndToken>& cprs =
      web_ui_info_singleton()->client_phishing_requests_sent();

  base::ListValue cprs_sent;

  for (const auto& cpr : cprs) {
    cprs_sent.Append(web_ui::SerializeClientPhishingRequest(cpr));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, cprs_sent);
}

void SafeBrowsingUIHandler::GetReceivedClientPhishingResponses(
    const base::ListValue& args) {
  const std::vector<std::unique_ptr<ClientPhishingResponse>>& cprs =
      web_ui_info_singleton()->client_phishing_responses_received();

  base::ListValue cprs_received;

  for (const auto& cpr : cprs) {
    cprs_received.Append(web_ui::SerializeClientPhishingResponse(*cpr));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, cprs_received);
}

void SafeBrowsingUIHandler::GetSentCSBRRs(const base::ListValue& args) {
  const std::vector<std::unique_ptr<ClientSafeBrowsingReportRequest>>& reports =
      web_ui_info_singleton()->csbrrs_sent();

  base::ListValue sent_reports;

  for (const auto& report : reports) {
    sent_reports.Append(web_ui::SerializeCSBRR(*report));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, sent_reports);
}

void SafeBrowsingUIHandler::GetPGEvents(const base::ListValue& args) {
  const std::vector<sync_pb::UserEventSpecifics>& events =
      web_ui_info_singleton()->pg_event_log();

  base::ListValue events_sent;

  for (const sync_pb::UserEventSpecifics& event : events) {
    events_sent.Append(web_ui::SerializePGEvent(event));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, events_sent);
}

void SafeBrowsingUIHandler::GetSecurityEvents(const base::ListValue& args) {
  const std::vector<sync_pb::GaiaPasswordReuse>& events =
      web_ui_info_singleton()->security_event_log();

  base::ListValue events_sent;

  for (const sync_pb::GaiaPasswordReuse& event : events) {
    events_sent.Append(web_ui::SerializeSecurityEvent(event));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, events_sent);
}

void SafeBrowsingUIHandler::GetPGPings(const base::ListValue& args) {
  const std::vector<web_ui::LoginReputationClientRequestAndToken> requests =
      web_ui_info_singleton()->pg_pings();

  base::ListValue pings_sent;
  for (size_t request_index = 0; request_index < requests.size();
       request_index++) {
    base::ListValue ping_entry;
    ping_entry.Append(int(request_index));
    ping_entry.Append(SerializePGPing(requests[request_index]));
    pings_sent.Append(std::move(ping_entry));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, pings_sent);
}

void SafeBrowsingUIHandler::GetPGResponses(const base::ListValue& args) {
  const std::map<int, LoginReputationClientResponse> responses =
      web_ui_info_singleton()->pg_responses();

  base::ListValue responses_sent;
  for (const auto& token_and_response : responses) {
    base::ListValue response_entry;
    response_entry.Append(token_and_response.first);
    response_entry.Append(
        web_ui::SerializePGResponse(token_and_response.second));
    responses_sent.Append(std::move(response_entry));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, responses_sent);
}

void SafeBrowsingUIHandler::GetURTLookupPings(const base::ListValue& args) {
  const std::vector<web_ui::URTLookupRequest> requests =
      web_ui_info_singleton()->urt_lookup_pings();

  base::ListValue pings_sent;
  for (size_t request_index = 0; request_index < requests.size();
       request_index++) {
    base::ListValue ping_entry;
    ping_entry.Append(static_cast<int>(request_index));
    ping_entry.Append(SerializeURTLookupPing(requests[request_index]));
    pings_sent.Append(std::move(ping_entry));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, pings_sent);
}

void SafeBrowsingUIHandler::GetURTLookupResponses(const base::ListValue& args) {
  const std::map<int, RTLookupResponse> responses =
      web_ui_info_singleton()->urt_lookup_responses();

  base::ListValue responses_sent;
  for (const auto& token_and_response : responses) {
    base::ListValue response_entry;
    response_entry.Append(token_and_response.first);
    response_entry.Append(
        web_ui::SerializeURTLookupResponse(token_and_response.second));
    responses_sent.Append(std::move(response_entry));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, responses_sent);
}

void SafeBrowsingUIHandler::GetHPRTLookupPings(const base::ListValue& args) {
  const std::vector<web_ui::HPRTLookupRequest> requests =
      web_ui_info_singleton()->hprt_lookup_pings();

  base::ListValue pings_sent;
  for (size_t request_index = 0; request_index < requests.size();
       request_index++) {
    base::ListValue ping_entry;
    ping_entry.Append(static_cast<int>(request_index));
    ping_entry.Append(SerializeHPRTLookupPing(requests[request_index]));
    pings_sent.Append(std::move(ping_entry));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, pings_sent);
}

void SafeBrowsingUIHandler::GetHPRTLookupResponses(
    const base::ListValue& args) {
  const std::map<int, V5::SearchHashesResponse> responses =
      web_ui_info_singleton()->hprt_lookup_responses();

  base::ListValue responses_sent;
  for (const auto& token_and_response : responses) {
    base::ListValue response_entry;
    response_entry.Append(token_and_response.first);
    response_entry.Append(
        web_ui::SerializeHPRTLookupResponse(token_and_response.second));
    responses_sent.Append(std::move(response_entry));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, responses_sent);
}

void SafeBrowsingUIHandler::GetReportingEvents(const base::ListValue& args) {
  base::ListValue reporting_events;
  for (const auto& reporting_event :
       web_ui_info_singleton()->reporting_events()) {
    reporting_events.Append(reporting_event.Clone());
  }

  for (const auto& request_result_pair :
       web_ui_info_singleton()->upload_event_requests()) {
    reporting_events.Append(web_ui::SerializeUploadEventsRequest(
        request_result_pair.first, request_result_pair.second));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, reporting_events);
}

void SafeBrowsingUIHandler::GetLogMessages(const base::ListValue& args) {
  const std::vector<std::pair<base::Time, std::string>>& log_messages =
      web_ui_info_singleton()->log_messages();

  base::ListValue messages_received;
  for (const auto& message : log_messages) {
    messages_received.Append(
        web_ui::SerializeLogMessage(message.first, message.second));
  }

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, messages_received);
}

void SafeBrowsingUIHandler::GetDeepScans(const base::ListValue& args) {
  base::ListValue pings_sent;
#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION)
  for (const auto& token_and_data :
       web_ui_info_singleton()->deep_scan_requests()) {
    pings_sent.Append(SerializeDeepScanDebugData(token_and_data.first,
                                                 token_and_data.second));
  }
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION)

  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveCallback(callback_id, pings_sent);
}

base::DictValue SafeBrowsingUIHandler::GetFormattedTailoredVerdictOverride() {
  base::DictValue override_dict;
#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  const char kStatusKey[] = "status";
  const char kOverrideValueKey[] = "override_value";
  const web_ui::TailoredVerdictOverrideData& override_data =
      web_ui_info_singleton()->tailored_verdict_override();
  if (!override_data.override_value) {
    override_dict.Set(kStatusKey, base::Value("No override set."));
  } else {
    if (override_data.IsFromSource(event_observer())) {
      override_dict.Set(kStatusKey, base::Value("Override set from this tab."));
    } else {
      override_dict.Set(kStatusKey,
                        base::Value("Override set from another tab."));
    }
    override_dict.Set(kOverrideValueKey,
                      ToValue(*override_data.override_value));
  }
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)
  return override_dict;
}

void SafeBrowsingUIHandler::SetTailoredVerdictOverride(
    const base::ListValue& args) {
  DCHECK_GE(args.size(), 2U);
#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  ClientDownloadResponse::TailoredVerdict tv;
  const base::DictValue& input = args[1].GetDict();

  const std::string* tailored_verdict_type =
      input.FindString("tailored_verdict_type");
  CHECK(tailored_verdict_type);
  if (*tailored_verdict_type == "VERDICT_TYPE_UNSPECIFIED") {
    tv.set_tailored_verdict_type(
        ClientDownloadResponse::TailoredVerdict::VERDICT_TYPE_UNSPECIFIED);
  } else if (*tailored_verdict_type == "COOKIE_THEFT") {
    tv.set_tailored_verdict_type(
        ClientDownloadResponse::TailoredVerdict::COOKIE_THEFT);
  } else if (*tailored_verdict_type == "SUSPICIOUS_ARCHIVE") {
    tv.set_tailored_verdict_type(
        ClientDownloadResponse::TailoredVerdict::SUSPICIOUS_ARCHIVE);
  }

  web_ui_info_singleton()->SetTailoredVerdictOverride(std::move(tv),
                                                      event_observer());
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)

  ResolveTailoredVerdictOverrideCallback(args[0].GetString());
}

void SafeBrowsingUIHandler::GetTailoredVerdictOverride(
    const base::ListValue& args) {
  ResolveTailoredVerdictOverrideCallback(args[0].GetString());
}

void SafeBrowsingUIHandler::ClearTailoredVerdictOverride(
    const base::ListValue& args) {
#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  web_ui_info_singleton()->ClearTailoredVerdictOverride();
#endif

  ResolveTailoredVerdictOverrideCallback(args[0].GetString());
}

void SafeBrowsingUIHandler::ResolveTailoredVerdictOverrideCallback(
    const std::string& callback_id) {
  ResolveCallback(callback_id, GetFormattedTailoredVerdictOverride());
}

}  // namespace safe_browsing
