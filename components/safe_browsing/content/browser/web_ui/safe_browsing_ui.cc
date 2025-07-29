// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/web_ui/safe_browsing_ui.h"

#include <stddef.h>

#include <algorithm>
#include <memory>
#include <utility>
#include <vector>

#include "base/base64.h"
#include "base/base64url.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/i18n/number_formatting.h"
#include "base/i18n/time_formatting.h"
#include "base/json/json_writer.h"
#include "base/memory/ref_counted.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
#include "components/grit/components_scaled_resources.h"
#include "components/grit/safe_browsing_resources.h"
#include "components/grit/safe_browsing_resources_map.h"
#include "components/password_manager/core/browser/hash_password_manager.h"
#include "components/safe_browsing/buildflags.h"
#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "components/safe_browsing/core/common/features.h"
#include "components/safe_browsing/core/common/proto/csd.pb.h"
#include "components/safe_browsing/core/common/proto/csd.to_value.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.pb.h"
#include "components/safe_browsing/core/common/proto/realtimeapi.to_value.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.pb.h"
#include "components/safe_browsing/core/common/proto/safebrowsingv5.to_value.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/safe_browsing/core/common/web_ui_constants.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/browser/web_contents.h"
#include "services/network/public/mojom/content_security_policy.mojom.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"

#if BUILDFLAG(SAFE_BROWSING_DB_LOCAL)
#include "components/safe_browsing/core/browser/db/v4_local_database_manager.h"
#endif

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
#include "components/enterprise/common/proto/connectors.pb.h"
#include "components/enterprise/common/proto/connectors.to_value.h"
#endif

using base::Time;
using sync_pb::GaiaPasswordReuse;
using PasswordCaptured = sync_pb::UserEventSpecifics::GaiaPasswordCaptured;
using PasswordReuseLookup = sync_pb::GaiaPasswordReuse::PasswordReuseLookup;
using PasswordReuseDetected = sync_pb::GaiaPasswordReuse::PasswordReuseDetected;
using PasswordReuseDialogInteraction =
    sync_pb::GaiaPasswordReuse::PasswordReuseDialogInteraction;

namespace safe_browsing {

SafeBrowsingUI::SafeBrowsingUI(
    content::WebUI* web_ui,
    std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate)
    : content::WebUIController(web_ui) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  // Set up the chrome://safe-browsing source.
  content::WebUIDataSource* html_source =
      content::WebUIDataSource::CreateAndAdd(
          browser_context, safe_browsing::kChromeUISafeBrowsingHost);

  // Register callback handler.
  // Handles messages from JavaScript to C++ via chrome.send().
  web_ui->AddMessageHandler(std::make_unique<SafeBrowsingUIHandler>(
      browser_context, std::move(delegate)));

  // Add required resources.
  html_source->AddResourcePaths(kSafeBrowsingResources);
  html_source->AddResourcePath("", IDR_SAFE_BROWSING_SAFE_BROWSING_HTML);

  // Static types
  html_source->OverrideContentSecurityPolicy(
      network::mojom::CSPDirectiveName::TrustedTypes,
      "trusted-types static-types;");
}

SafeBrowsingUI::~SafeBrowsingUI() = default;

SafeBrowsingUIHandler::SafeBrowsingUIHandler(
    content::BrowserContext* context,
    std::unique_ptr<SafeBrowsingLocalStateDelegate> delegate)
    : browser_context_(context), delegate_(std::move(delegate)) {}

SafeBrowsingUIHandler::~SafeBrowsingUIHandler() {
  WebUIInfoSingleton::GetInstance()->UnregisterWebUIInstance(this);
}

void SafeBrowsingUIHandler::OnJavascriptAllowed() {
  // We don't want to register the SafeBrowsingUIHandler with the
  // WebUIInfoSingleton at construction, since this can lead to
  // messages being sent to the renderer before it's ready. So register it here.
  WebUIInfoSingleton::GetInstance()->RegisterWebUIInstance(this);
}

void SafeBrowsingUIHandler::OnJavascriptDisallowed() {
  // In certain situations, Javascript can become disallowed before the
  // destructor is called (e.g. tab refresh/renderer crash). In these situation,
  // we want to stop receiving JS messages.
  WebUIInfoSingleton::GetInstance()->UnregisterWebUIInstance(this);
}

void SafeBrowsingUIHandler::GetExperiments(const base::Value::List& args) {
  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, GetFeatureStatusList());
}

void SafeBrowsingUIHandler::GetPrefs(const base::Value::List& args) {
  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id,
                            safe_browsing::GetSafeBrowsingPreferencesList(
                                user_prefs::UserPrefs::Get(browser_context_)));
}

void SafeBrowsingUIHandler::GetPolicies(const base::Value::List& args) {
  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id,
                            safe_browsing::GetSafeBrowsingPoliciesList(
                                user_prefs::UserPrefs::Get(browser_context_)));
}

void SafeBrowsingUIHandler::GetCookie(const base::Value::List& args) {
  DCHECK(!args.empty());
  std::string callback_id = args[0].GetString();

  cookie_manager_remote_ =
      WebUIInfoSingleton::GetInstance()->GetCookieManager(browser_context_);
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

  base::Value::List response;
  response.Append(std::move(cookie));
  response.Append(time);

  AllowJavascript();
  ResolveJavascriptCallback(callback_id, response);
}

void SafeBrowsingUIHandler::GetSavedPasswords(const base::Value::List& args) {
  password_manager::HashPasswordManager hash_manager;
  hash_manager.set_prefs(user_prefs::UserPrefs::Get(browser_context_));
  hash_manager.set_local_prefs(delegate_->GetLocalState());

  base::Value::List saved_passwords;
  for (const password_manager::PasswordHashData& hash_data :
       hash_manager.RetrieveAllPasswordHashes()) {
    saved_passwords.Append(hash_data.username);
    saved_passwords.Append(hash_data.is_gaia_password);
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, saved_passwords);
}

void SafeBrowsingUIHandler::GetDatabaseManagerInfo(
    const base::Value::List& args) {
  base::Value::List database_manager_info;

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

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();

  ResolveJavascriptCallback(callback_id, database_manager_info);
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

void SafeBrowsingUIHandler::GetDownloadUrlsChecked(
    const base::Value::List& args) {
  const std::vector<std::pair<std::vector<GURL>, DownloadCheckResult>>&
      urls_checked = WebUIInfoSingleton::GetInstance()->download_urls_checked();

  base::Value::List urls_checked_value;
  for (const auto& url_and_result : urls_checked) {
    const std::vector<GURL>& urls = url_and_result.first;
    DownloadCheckResult result = url_and_result.second;
    urls_checked_value.Append(SerializeDownloadUrlChecked(urls, result));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, urls_checked_value);
}

void SafeBrowsingUIHandler::GetSentClientDownloadRequests(
    const base::Value::List& args) {
  const std::vector<std::unique_ptr<ClientDownloadRequest>>& cdrs =
      WebUIInfoSingleton::GetInstance()->client_download_requests_sent();

  base::Value::List cdrs_sent;

  for (const auto& cdr : cdrs) {
    cdrs_sent.Append(web_ui::SerializeClientDownloadRequest(*cdr));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, cdrs_sent);
}

void SafeBrowsingUIHandler::GetReceivedClientDownloadResponses(
    const base::Value::List& args) {
  const std::vector<std::unique_ptr<ClientDownloadResponse>>& cdrs =
      WebUIInfoSingleton::GetInstance()->client_download_responses_received();

  base::Value::List cdrs_received;

  for (const auto& cdr : cdrs) {
    cdrs_received.Append(web_ui::SerializeClientDownloadResponse(*cdr));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, cdrs_received);
}

void SafeBrowsingUIHandler::GetSentClientPhishingRequests(
    const base::Value::List& args) {
  const std::vector<web_ui::ClientPhishingRequestAndToken>& cprs =
      WebUIInfoSingleton::GetInstance()->client_phishing_requests_sent();

  base::Value::List cprs_sent;

  for (const auto& cpr : cprs) {
    cprs_sent.Append(web_ui::SerializeClientPhishingRequest(cpr));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, cprs_sent);
}

void SafeBrowsingUIHandler::GetReceivedClientPhishingResponses(
    const base::Value::List& args) {
  const std::vector<std::unique_ptr<ClientPhishingResponse>>& cprs =
      WebUIInfoSingleton::GetInstance()->client_phishing_responses_received();

  base::Value::List cprs_received;

  for (const auto& cpr : cprs) {
    cprs_received.Append(web_ui::SerializeClientPhishingResponse(*cpr));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, cprs_received);
}

void SafeBrowsingUIHandler::GetSentCSBRRs(const base::Value::List& args) {
  const std::vector<std::unique_ptr<ClientSafeBrowsingReportRequest>>& reports =
      WebUIInfoSingleton::GetInstance()->csbrrs_sent();

  base::Value::List sent_reports;

  for (const auto& report : reports) {
    sent_reports.Append(web_ui::SerializeCSBRR(*report));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, sent_reports);
}

void SafeBrowsingUIHandler::GetSentHitReports(const base::Value::List& args) {
  const std::vector<std::unique_ptr<HitReport>>& reports =
      WebUIInfoSingleton::GetInstance()->hit_reports_sent();

  base::Value::List sent_reports;

  for (const auto& report : reports) {
    sent_reports.Append(web_ui::SerializeHitReport(*report));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, sent_reports);
}

void SafeBrowsingUIHandler::GetPGEvents(const base::Value::List& args) {
  const std::vector<sync_pb::UserEventSpecifics>& events =
      WebUIInfoSingleton::GetInstance()->pg_event_log();

  base::Value::List events_sent;

  for (const sync_pb::UserEventSpecifics& event : events) {
    events_sent.Append(web_ui::SerializePGEvent(event));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, events_sent);
}

void SafeBrowsingUIHandler::GetSecurityEvents(const base::Value::List& args) {
  const std::vector<sync_pb::GaiaPasswordReuse>& events =
      WebUIInfoSingleton::GetInstance()->security_event_log();

  base::Value::List events_sent;

  for (const sync_pb::GaiaPasswordReuse& event : events) {
    events_sent.Append(web_ui::SerializeSecurityEvent(event));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, events_sent);
}

void SafeBrowsingUIHandler::GetPGPings(const base::Value::List& args) {
  const std::vector<web_ui::LoginReputationClientRequestAndToken> requests =
      WebUIInfoSingleton::GetInstance()->pg_pings();

  base::Value::List pings_sent;
  for (size_t request_index = 0; request_index < requests.size();
       request_index++) {
    base::Value::List ping_entry;
    ping_entry.Append(int(request_index));
    ping_entry.Append(SerializePGPing(requests[request_index]));
    pings_sent.Append(std::move(ping_entry));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, pings_sent);
}

void SafeBrowsingUIHandler::GetPGResponses(const base::Value::List& args) {
  const std::map<int, LoginReputationClientResponse> responses =
      WebUIInfoSingleton::GetInstance()->pg_responses();

  base::Value::List responses_sent;
  for (const auto& token_and_response : responses) {
    base::Value::List response_entry;
    response_entry.Append(token_and_response.first);
    response_entry.Append(
        web_ui::SerializePGResponse(token_and_response.second));
    responses_sent.Append(std::move(response_entry));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, responses_sent);
}

void SafeBrowsingUIHandler::GetURTLookupPings(const base::Value::List& args) {
  const std::vector<web_ui::URTLookupRequest> requests =
      WebUIInfoSingleton::GetInstance()->urt_lookup_pings();

  base::Value::List pings_sent;
  for (size_t request_index = 0; request_index < requests.size();
       request_index++) {
    base::Value::List ping_entry;
    ping_entry.Append(static_cast<int>(request_index));
    ping_entry.Append(SerializeURTLookupPing(requests[request_index]));
    pings_sent.Append(std::move(ping_entry));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, pings_sent);
}

void SafeBrowsingUIHandler::GetURTLookupResponses(
    const base::Value::List& args) {
  const std::map<int, RTLookupResponse> responses =
      WebUIInfoSingleton::GetInstance()->urt_lookup_responses();

  base::Value::List responses_sent;
  for (const auto& token_and_response : responses) {
    base::Value::List response_entry;
    response_entry.Append(token_and_response.first);
    response_entry.Append(
        web_ui::SerializeURTLookupResponse(token_and_response.second));
    responses_sent.Append(std::move(response_entry));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, responses_sent);
}

void SafeBrowsingUIHandler::GetHPRTLookupPings(const base::Value::List& args) {
  const std::vector<web_ui::HPRTLookupRequest> requests =
      WebUIInfoSingleton::GetInstance()->hprt_lookup_pings();

  base::Value::List pings_sent;
  for (size_t request_index = 0; request_index < requests.size();
       request_index++) {
    base::Value::List ping_entry;
    ping_entry.Append(static_cast<int>(request_index));
    ping_entry.Append(SerializeHPRTLookupPing(requests[request_index]));
    pings_sent.Append(std::move(ping_entry));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, pings_sent);
}

void SafeBrowsingUIHandler::GetHPRTLookupResponses(
    const base::Value::List& args) {
  const std::map<int, V5::SearchHashesResponse> responses =
      WebUIInfoSingleton::GetInstance()->hprt_lookup_responses();

  base::Value::List responses_sent;
  for (const auto& token_and_response : responses) {
    base::Value::List response_entry;
    response_entry.Append(token_and_response.first);
    response_entry.Append(
        web_ui::SerializeHPRTLookupResponse(token_and_response.second));
    responses_sent.Append(std::move(response_entry));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, responses_sent);
}

void SafeBrowsingUIHandler::GetReferrerChain(const base::Value::List& args) {
  DCHECK_GE(args.size(), 2U);
  const std::string& url_string = args[1].GetString();

  ReferrerChainProvider* provider =
      WebUIInfoSingleton::GetInstance()->GetReferrerChainProvider(
          browser_context_);

  const std::string& callback_id = args[0].GetString();

  if (!provider) {
    AllowJavascript();
    ResolveJavascriptCallback(callback_id, "");
    return;
  }

  ReferrerChain referrer_chain;
  provider->IdentifyReferrerChainByEventURL(
      GURL(url_string), SessionID::InvalidValue(),
      content::GlobalRenderFrameHostId(), 2, &referrer_chain);

  base::Value::List referrer_list;
  for (const ReferrerChainEntry& entry : referrer_chain) {
    referrer_list.Append(Serialize(entry));
  }

  std::string referrer_chain_serialized = web_ui::SerializeJson(referrer_list);

  AllowJavascript();
  ResolveJavascriptCallback(callback_id, referrer_chain_serialized);
}

#if BUILDFLAG(IS_ANDROID)
void SafeBrowsingUIHandler::GetReferringAppInfo(const base::Value::List& args) {
  base::Value::Dict referring_app_value;
  internal::ReferringAppInfo info =
      WebUIInfoSingleton::GetInstance()->GetReferringAppInfo(
          web_ui()->GetWebContents());
  referring_app_value = web_ui::SerializeReferringAppInfo(info);

  std::string referring_app_serialized =
      web_ui::SerializeJson(referring_app_value);

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, referring_app_serialized);
}
#endif

void SafeBrowsingUIHandler::GetReportingEvents(const base::Value::List& args) {
  base::Value::List reporting_events;
  for (const auto& reporting_event :
       WebUIInfoSingleton::GetInstance()->reporting_events()) {
    reporting_events.Append(reporting_event.Clone());
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, reporting_events);
}

void SafeBrowsingUIHandler::GetLogMessages(const base::Value::List& args) {
  const std::vector<std::pair<base::Time, std::string>>& log_messages =
      WebUIInfoSingleton::GetInstance()->log_messages();

  base::Value::List messages_received;
  for (const auto& message : log_messages) {
    messages_received.Append(
        web_ui::SerializeLogMessage(message.first, message.second));
  }

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, messages_received);
}

void SafeBrowsingUIHandler::GetDeepScans(const base::Value::List& args) {
  base::Value::List pings_sent;
#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  for (const auto& token_and_data :
       WebUIInfoSingleton::GetInstance()->deep_scan_requests()) {
    pings_sent.Append(SerializeDeepScanDebugData(token_and_data.first,
                                                 token_and_data.second));
  }
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)

  AllowJavascript();
  DCHECK(!args.empty());
  const std::string& callback_id = args[0].GetString();
  ResolveJavascriptCallback(callback_id, pings_sent);
}

base::Value::Dict SafeBrowsingUIHandler::GetFormattedTailoredVerdictOverride() {
  base::Value::Dict override_dict;
#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  const char kStatusKey[] = "status";
  const char kOverrideValueKey[] = "override_value";
  const web_ui::TailoredVerdictOverrideData& override_data =
      WebUIInfoSingleton::GetInstance()->tailored_verdict_override();
  if (!override_data.override_value) {
    override_dict.Set(kStatusKey, base::Value("No override set."));
  } else {
    if (override_data.IsFromSource(this)) {
      override_dict.Set(kStatusKey, base::Value("Override set from this tab."));
    } else {
      override_dict.Set(kStatusKey,
                        base::Value("Override set from another tab."));
    }
    override_dict.Set(kOverrideValueKey,
                      Serialize(*override_data.override_value));
  }
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)
  return override_dict;
}

void SafeBrowsingUIHandler::SetTailoredVerdictOverride(
    const base::Value::List& args) {
  DCHECK_GE(args.size(), 2U);
#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  ClientDownloadResponse::TailoredVerdict tv;
  const base::Value::Dict& input = args[1].GetDict();

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

  WebUIInfoSingleton::GetInstance()->SetTailoredVerdictOverride(std::move(tv),
                                                                this);
#endif  // BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) &&
        // !BUILDFLAG(IS_ANDROID)

  ResolveTailoredVerdictOverrideCallback(args[0].GetString());
}

void SafeBrowsingUIHandler::GetTailoredVerdictOverride(
    const base::Value::List& args) {
  ResolveTailoredVerdictOverrideCallback(args[0].GetString());
}

void SafeBrowsingUIHandler::ClearTailoredVerdictOverride(
    const base::Value::List& args) {
#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
  WebUIInfoSingleton::GetInstance()->ClearTailoredVerdictOverride();
#endif

  ResolveTailoredVerdictOverrideCallback(args[0].GetString());
}

void SafeBrowsingUIHandler::ResolveTailoredVerdictOverrideCallback(
    const std::string& callback_id) {
  AllowJavascript();
  ResolveJavascriptCallback(callback_id, GetFormattedTailoredVerdictOverride());
}

void SafeBrowsingUIHandler::NotifyTailoredVerdictOverrideJsListener() {
  AllowJavascript();
  FireWebUIListener("tailored-verdict-override-update",
                    GetFormattedTailoredVerdictOverride());
}

void SafeBrowsingUIHandler::NotifyDownloadUrlCheckedJsListener(
    const std::vector<GURL>& urls,
    DownloadCheckResult result) {
  AllowJavascript();
  FireWebUIListener("download-url-checked-update",
                    base::Value(SerializeDownloadUrlChecked(urls, result)));
}

void SafeBrowsingUIHandler::NotifyClientDownloadRequestJsListener(
    ClientDownloadRequest* client_download_request) {
  AllowJavascript();
  FireWebUIListener("sent-client-download-requests-update",
                    base::Value(web_ui::SerializeClientDownloadRequest(
                        *client_download_request)));
}

void SafeBrowsingUIHandler::NotifyClientDownloadResponseJsListener(
    ClientDownloadResponse* client_download_response) {
  AllowJavascript();
  FireWebUIListener("received-client-download-responses-update",
                    base::Value(web_ui::SerializeClientDownloadResponse(
                        *client_download_response)));
}

void SafeBrowsingUIHandler::NotifyClientPhishingRequestJsListener(
    const web_ui::ClientPhishingRequestAndToken& client_phishing_request) {
  AllowJavascript();
  FireWebUIListener(
      "sent-client-phishing-requests-update",
      base::Value(SerializeClientPhishingRequest(client_phishing_request)));
}

void SafeBrowsingUIHandler::NotifyClientPhishingResponseJsListener(
    ClientPhishingResponse* client_phishing_response) {
  AllowJavascript();
  FireWebUIListener("received-client-phishing-responses-update",
                    base::Value(web_ui::SerializeClientPhishingResponse(
                        *client_phishing_response)));
}

void SafeBrowsingUIHandler::NotifyCSBRRJsListener(
    ClientSafeBrowsingReportRequest* csbrr) {
  AllowJavascript();
  FireWebUIListener("sent-csbrr-update",
                    base::Value(web_ui::SerializeCSBRR(*csbrr)));
}

void SafeBrowsingUIHandler::NotifyHitReportJsListener(HitReport* hit_report) {
  AllowJavascript();
  FireWebUIListener("sent-hit-report-list",
                    base::Value(web_ui::SerializeHitReport(*hit_report)));
}

void SafeBrowsingUIHandler::NotifyPGEventJsListener(
    const sync_pb::UserEventSpecifics& event) {
  AllowJavascript();
  FireWebUIListener("sent-pg-event", web_ui::SerializePGEvent(event));
}

void SafeBrowsingUIHandler::NotifySecurityEventJsListener(
    const sync_pb::GaiaPasswordReuse& event) {
  AllowJavascript();
  FireWebUIListener("sent-security-event",
                    web_ui::SerializeSecurityEvent(event));
}

void SafeBrowsingUIHandler::NotifyPGPingJsListener(
    int token,
    const web_ui::LoginReputationClientRequestAndToken& request) {
  base::Value::List request_list;
  request_list.Append(token);
  request_list.Append(SerializePGPing(request));

  AllowJavascript();
  FireWebUIListener("pg-pings-update", request_list);
}

void SafeBrowsingUIHandler::NotifyPGResponseJsListener(
    int token,
    const LoginReputationClientResponse& response) {
  base::Value::List response_list;
  response_list.Append(token);
  response_list.Append(web_ui::SerializePGResponse(response));

  AllowJavascript();
  FireWebUIListener("pg-responses-update", response_list);
}

void SafeBrowsingUIHandler::NotifyURTLookupPingJsListener(
    int token,
    const web_ui::URTLookupRequest& request) {
  base::Value::List request_list;
  request_list.Append(token);
  request_list.Append(SerializeURTLookupPing(request));

  AllowJavascript();
  FireWebUIListener("urt-lookup-pings-update", request_list);
}

void SafeBrowsingUIHandler::NotifyURTLookupResponseJsListener(
    int token,
    const RTLookupResponse& response) {
  base::Value::List response_list;
  response_list.Append(token);
  response_list.Append(web_ui::SerializeURTLookupResponse(response));

  AllowJavascript();
  FireWebUIListener("urt-lookup-responses-update", response_list);
}

void SafeBrowsingUIHandler::NotifyHPRTLookupPingJsListener(
    int token,
    const web_ui::HPRTLookupRequest& request) {
  base::Value::List request_list;
  request_list.Append(token);
  request_list.Append(SerializeHPRTLookupPing(request));

  AllowJavascript();
  FireWebUIListener("hprt-lookup-pings-update", request_list);
}

void SafeBrowsingUIHandler::NotifyHPRTLookupResponseJsListener(
    int token,
    const V5::SearchHashesResponse& response) {
  base::Value::List response_list;
  response_list.Append(token);
  response_list.Append(web_ui::SerializeHPRTLookupResponse(response));

  AllowJavascript();
  FireWebUIListener("hprt-lookup-responses-update", response_list);
}

void SafeBrowsingUIHandler::NotifyLogMessageJsListener(
    const base::Time& timestamp,
    const std::string& message) {
  AllowJavascript();
  FireWebUIListener("log-messages-update",
                    web_ui::SerializeLogMessage(timestamp, message));
}

void SafeBrowsingUIHandler::NotifyReportingEventJsListener(
    const base::Value::Dict& event) {
  AllowJavascript();
  FireWebUIListener("reporting-events-update",
                    web_ui::SerializeReportingEvent(event));
}

#if BUILDFLAG(SAFE_BROWSING_DOWNLOAD_PROTECTION) && !BUILDFLAG(IS_ANDROID)
void SafeBrowsingUIHandler::NotifyDeepScanJsListener(
    const std::string& token,
    const web_ui::DeepScanDebugData& deep_scan_data) {
  AllowJavascript();
  FireWebUIListener("deep-scan-request-update",
                    SerializeDeepScanDebugData(token, deep_scan_data));
}
#endif

void SafeBrowsingUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "getExperiments",
      base::BindRepeating(&SafeBrowsingUIHandler::GetExperiments,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPolicies", base::BindRepeating(&SafeBrowsingUIHandler::GetPolicies,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPrefs", base::BindRepeating(&SafeBrowsingUIHandler::GetPrefs,
                                      base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getCookie", base::BindRepeating(&SafeBrowsingUIHandler::GetCookie,
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
      "getDownloadUrlsChecked",
      base::BindRepeating(&SafeBrowsingUIHandler::GetDownloadUrlsChecked,
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
      "getSentClientPhishingRequests",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSentClientPhishingRequests,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReceivedClientPhishingResponses",
      base::BindRepeating(
          &SafeBrowsingUIHandler::GetReceivedClientPhishingResponses,
          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSentCSBRRs",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSentCSBRRs,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSentHitReports",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSentHitReports,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPGEvents", base::BindRepeating(&SafeBrowsingUIHandler::GetPGEvents,
                                         base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getSecurityEvents",
      base::BindRepeating(&SafeBrowsingUIHandler::GetSecurityEvents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPGPings", base::BindRepeating(&SafeBrowsingUIHandler::GetPGPings,
                                        base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getPGResponses",
      base::BindRepeating(&SafeBrowsingUIHandler::GetPGResponses,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getURTLookupPings",
      base::BindRepeating(&SafeBrowsingUIHandler::GetURTLookupPings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getURTLookupResponses",
      base::BindRepeating(&SafeBrowsingUIHandler::GetURTLookupResponses,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getHPRTLookupPings",
      base::BindRepeating(&SafeBrowsingUIHandler::GetHPRTLookupPings,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getHPRTLookupResponses",
      base::BindRepeating(&SafeBrowsingUIHandler::GetHPRTLookupResponses,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getLogMessages",
      base::BindRepeating(&SafeBrowsingUIHandler::GetLogMessages,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getReferrerChain",
      base::BindRepeating(&SafeBrowsingUIHandler::GetReferrerChain,
                          base::Unretained(this)));
#if BUILDFLAG(IS_ANDROID)
  web_ui()->RegisterMessageCallback(
      "getReferringAppInfo",
      base::BindRepeating(&SafeBrowsingUIHandler::GetReferringAppInfo,
                          base::Unretained(this)));
#endif
  web_ui()->RegisterMessageCallback(
      "getReportingEvents",
      base::BindRepeating(&SafeBrowsingUIHandler::GetReportingEvents,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getDeepScans", base::BindRepeating(&SafeBrowsingUIHandler::GetDeepScans,
                                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getTailoredVerdictOverride",
      base::BindRepeating(&SafeBrowsingUIHandler::GetTailoredVerdictOverride,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "setTailoredVerdictOverride",
      base::BindRepeating(&SafeBrowsingUIHandler::SetTailoredVerdictOverride,
                          base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "clearTailoredVerdictOverride",
      base::BindRepeating(&SafeBrowsingUIHandler::ClearTailoredVerdictOverride,
                          base::Unretained(this)));
}

void SafeBrowsingUIHandler::SetWebUIForTesting(content::WebUI* web_ui) {
  set_web_ui(web_ui);
}

CrSBLogMessage::CrSBLogMessage() = default;

CrSBLogMessage::~CrSBLogMessage() {
  WebUIInfoSingleton::GetInstance()->LogMessage(stream_.str());
  DLOG(WARNING) << stream_.str();
}

}  // namespace safe_browsing
