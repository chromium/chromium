// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/account_checker.h"
#include "base/feature_list.h"
#include "base/json/json_writer.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/pref_names.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

const int64_t kTimeoutMs = 10000;
const char kPriceTrackEmailPref[] = "price_track_email";
const char kPreferencesKey[] = "preferences";

}  // namespace

namespace commerce {

const char kOAuthScope[] = "https://www.googleapis.com/auth/chromememex";
const char kOAuthName[] = "chromememex_svc";
const char kGetHttpMethod[] = "GET";
const char kPostHttpMethod[] = "POST";
const char kContentType[] = "application/json; charset=UTF-8";
const char kEmptyPostData[] = "";
const char kNotificationsPrefUrl[] =
    "https://memex-pa.googleapis.com/v1/notifications/preferences";

AccountChecker::AccountChecker(
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : pref_service_(pref_service),
      identity_manager_(identity_manager),
      url_loader_factory_(url_loader_factory),
      weak_ptr_factory_(this) {
  if (identity_manager) {
    FetchWaaStatus();
    scoped_identity_manager_observation_.Observe(identity_manager);
  }
  // TODO(crbug.com/1366165): Avoid pushing the fetched pref value to the server
  // again.
  if (pref_service) {
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(pref_service);
    pref_change_registrar_->Add(
        kPriceEmailNotificationsEnabled,
        base::BindRepeating(&AccountChecker::OnPriceEmailPrefChanged,
                            weak_ptr_factory_.GetWeakPtr()));
  }
}

AccountChecker::~AccountChecker() = default;

bool AccountChecker::IsSignedIn() {
  return identity_manager_ &&
         identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync);
}

bool AccountChecker::IsAnonymizedUrlDataCollectionEnabled() {
  return pref_service_ &&
         unified_consent::UrlKeyedDataCollectionConsentHelper::
             NewAnonymizedDataCollectionConsentHelper(pref_service_)
                 ->IsEnabled();
}

bool AccountChecker::IsWebAndAppActivityEnabled() {
  return pref_service_ &&
         pref_service_->GetBoolean(kWebAndAppActivityEnabledForShopping);
}

bool AccountChecker::IsSubjectToParentalControls() {
  if (!identity_manager_) {
    return false;
  }

  AccountCapabilities capabilities =
      identity_manager_
          ->FindExtendedAccountInfo(identity_manager_->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin))
          .capabilities;

  return capabilities.is_subject_to_parental_controls() ==
         signin::Tribool::kTrue;
}

void AccountChecker::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  FetchWaaStatus();
}

void AccountChecker::FetchWaaStatus() {
  // For now we need to update users' consent status on web and app activity.
  if (!IsSignedIn()) {
    return;
  }
  // TODO(crbug.com/1311754): These parameters (url, oauth_scope, etc.) are
  // copied from web_history_service.cc directly, it works now but we should
  // figure out a better way to keep these parameters in sync.
  const char waa_oauth_name[] = "web_history";
  const char waa_query_url[] =
      "https://history.google.com/history/api/lookup?client=web_app";
  const char waa_oauth_scope[] = "https://www.googleapis.com/auth/chromesync";
  const char waa_content_type[] = "application/json; charset=UTF-8";
  const char waa_get_method[] = "GET";
  const int64_t waa_timeout_ms = 30000;
  const char waa_post_data[] = "";
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("chrome_commerce_waa_fetcher",
                                          R"(
        semantics {
          sender: "Chrome Shopping"
          description:
            "Check whether Web & App Activity is paused in My Google Activity."
            "If it is paused, some Chrome Shopping features such as Price "
            "Tracking Notifications become disabled."
          trigger:
            "On account checker initialization or every time after the user "
            "changes their primary account."
          data:
            "The request includes an OAuth2 token authenticating the user. The "
            "response includes a boolean indicating whether the feature is "
            "enabled."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This fetch is only enabled for signed-in users. There's no "
            "direct Chromium's setting to disable this, but users can manage "
            "their preferences by visiting myactivity.google.com."
          chrome_policy {
            BrowserSignin {
              policy_options {mode: MANDATORY}
              BrowserSignin: 0
            }
          }
        })");
  auto endpoint_fetcher = CreateEndpointFetcher(
      waa_oauth_name, GURL(waa_query_url), waa_get_method, waa_content_type,
      std::vector<std::string>{waa_oauth_scope}, waa_timeout_ms, waa_post_data,
      traffic_annotation);
  endpoint_fetcher.get()->Fetch(base::BindOnce(
      &AccountChecker::HandleFetchWaaResponse, weak_ptr_factory_.GetWeakPtr(),
      std::move(endpoint_fetcher)));
}

void AccountChecker::HandleFetchWaaResponse(
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> responses) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      responses->response, base::BindOnce(&AccountChecker::OnFetchWaaJsonParsed,
                                          weak_ptr_factory_.GetWeakPtr()));
}

void AccountChecker::OnFetchWaaJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (pref_service_ && result.has_value() && result->is_dict()) {
    const char waa_response_key[] = "history_recording_enabled";
    if (auto waa_enabled = result->FindBoolKey(waa_response_key)) {
      pref_service_->SetBoolean(kWebAndAppActivityEnabledForShopping,
                                *waa_enabled);
    }
  }
}

void AccountChecker::FetchPriceEmailPref() {
  if (!IsSignedIn()) {
    return;
  }

  is_waiting_for_pref_fetch_completion_ = true;
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "chrome_commerce_price_email_pref_fetcher",
          R"(
        semantics {
          sender: "Chrome Shopping"
          description:
            "Check whether the user paused receiving price drop emails."
            "If it is paused, we need to update the preference value to "
            "correctly reflect the user's choice in Chrome settings."
          trigger:
            "Every time when the user opens the Chrome settings."
          data:
            "The request includes an OAuth2 token authenticating the user. The "
            "response includes a map of commerce notification preference key "
            "strings to current user opt-in status."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This fetch is only enabled for users with Sync turned on. "
            "There's no direct Chromium's setting to disable this, but users "
            "can manage their preferences in Chrome settings."
          chrome_policy {
            SyncDisabled {
              policy_options {mode: MANDATORY}
              SyncDisabled: true
            }
          }
        })");
  auto endpoint_fetcher = CreateEndpointFetcher(
      kOAuthName, GURL(kNotificationsPrefUrl), kGetHttpMethod, kContentType,
      std::vector<std::string>{kOAuthScope}, kTimeoutMs, kEmptyPostData,
      traffic_annotation);
  endpoint_fetcher.get()->Fetch(base::BindOnce(
      &AccountChecker::HandleFetchPriceEmailPrefResponse,
      weak_ptr_factory_.GetWeakPtr(), std::move(endpoint_fetcher)));
}

void AccountChecker::HandleFetchPriceEmailPrefResponse(
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> responses) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      responses->response,
      base::BindOnce(&AccountChecker::OnFetchPriceEmailPrefJsonParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccountChecker::OnFetchPriceEmailPrefJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  // Only update the pref if we're still waiting for the pref fetch completion.
  // If users update the pref faster than we hear back from the server fetch,
  // the fetched result should be discarded.
  if (pref_service_ && is_waiting_for_pref_fetch_completion_ &&
      result.has_value() && result->is_dict()) {
    if (auto* preferences_map = result->GetDict().FindDict(kPreferencesKey)) {
      if (absl::optional<bool> price_email_pref =
              preferences_map->FindBool(kPriceTrackEmailPref)) {
        // Only set the pref value when necessary since it could affect
        // PrefService::Preference::IsDefaultValue().
        if (pref_service_->GetBoolean(kPriceEmailNotificationsEnabled) !=
            *price_email_pref) {
          ignore_next_email_pref_change_ = true;
          pref_service_->SetBoolean(kPriceEmailNotificationsEnabled,
                                    *price_email_pref);
        }
      }
    }
  }
  is_waiting_for_pref_fetch_completion_ = false;
}

void AccountChecker::OnPriceEmailPrefChanged() {
  // If users update the pref faster than we hear back from the server fetch,
  // the fetched result should be discarded.
  is_waiting_for_pref_fetch_completion_ = false;
  if (ignore_next_email_pref_change_) {
    ignore_next_email_pref_change_ = false;
    return;
  }

  if (!IsSignedIn() || !pref_service_) {
    return;
  }

  // Send the new value to server.
  base::Value preferences_map(base::Value::Type::DICT);
  preferences_map.SetBoolKey(
      kPriceTrackEmailPref,
      pref_service_->GetBoolean(kPriceEmailNotificationsEnabled));
  base::Value post_json(base::Value::Type::DICT);
  post_json.SetKey(kPreferencesKey, std::move(preferences_map));
  std::string post_data;
  base::JSONWriter::Write(post_json, &post_data);

  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation(
          "chrome_commerce_price_email_pref_sender",
          R"(
        semantics {
          sender: "Chrome Shopping"
          description:
            "Send the user's choice on whether to receive price drop emails."
          trigger:
            "Every time when the user changes their preference in the Chrome "
            "settings."
          data:
            "The map of commerce notification preference key strings to the "
            "new opt-in status. The request also includes an OAuth2 token "
            "authenticating the user."
          destination: GOOGLE_OWNED_SERVICE
        }
        policy {
          cookies_allowed: NO
          setting:
            "This request is only enabled for users with Sync turned on. "
            "There's no direct Chromium's setting to disable this, but users "
            "can manage their preferences in Chrome settings."
          chrome_policy {
            SyncDisabled {
              policy_options {mode: MANDATORY}
              SyncDisabled: true
            }
          }
        })");
  auto endpoint_fetcher = CreateEndpointFetcher(
      kOAuthName, GURL(kNotificationsPrefUrl), kPostHttpMethod, kContentType,
      std::vector<std::string>{kOAuthScope}, kTimeoutMs, post_data,
      traffic_annotation);
  endpoint_fetcher.get()->Fetch(base::BindOnce(
      &AccountChecker::HandleSendPriceEmailPrefResponse,
      weak_ptr_factory_.GetWeakPtr(), std::move(endpoint_fetcher)));
}

void AccountChecker::HandleSendPriceEmailPrefResponse(
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> responses) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      responses->response,
      base::BindOnce(&AccountChecker::OnSendPriceEmailPrefJsonParsed,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AccountChecker::OnSendPriceEmailPrefJsonParsed(
    data_decoder::DataDecoder::ValueOrError result) {
  if (pref_service_ && result.has_value() && result->is_dict()) {
    if (auto* preferences_map = result->GetDict().FindDict(kPreferencesKey)) {
      if (auto price_email_pref =
              preferences_map->FindBool(kPriceTrackEmailPref)) {
        if (pref_service_->GetBoolean(kPriceEmailNotificationsEnabled) !=
            *price_email_pref) {
          VLOG(1) << "Fail to update the price email pref";
        }
      }
    }
  }
}

std::unique_ptr<EndpointFetcher> AccountChecker::CreateEndpointFetcher(
    const std::string& oauth_consumer_name,
    const GURL& url,
    const std::string& http_method,
    const std::string& content_type,
    const std::vector<std::string>& scopes,
    int64_t timeout_ms,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  return std::make_unique<EndpointFetcher>(
      url_loader_factory_, oauth_consumer_name, url, http_method, content_type,
      scopes, timeout_ms, post_data, annotation_tag, identity_manager_);
}

}  // namespace commerce
