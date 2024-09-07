// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/account_checker.h"

#include "base/json/json_writer.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/commerce/core/commerce_constants.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/pref_names.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_capabilities.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/base/features.h"
#include "components/sync/service/sync_service_utils.h"
#include "components/sync/service/sync_user_settings.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace {

constexpr base::TimeDelta kTimeout = base::Milliseconds(10000);
const char kPriceTrackEmailPref[] = "price_track_email";
const char kPreferencesKey[] = "preferences";

}  // namespace

namespace commerce {

const char kNotificationsPrefUrl[] =
    "https://memex-pa.googleapis.com/v1/notifications/preferences";

AccountChecker::AccountChecker(
    std::string country,
    std::string locale,
    PrefService* pref_service,
    signin::IdentityManager* identity_manager,
    syncer::SyncService* sync_service,
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
    : country_(country),
      locale_(locale),
      pref_service_(pref_service),
      identity_manager_(identity_manager),
      sync_service_(sync_service),
      url_loader_factory_(url_loader_factory),
      weak_ptr_factory_(this) {
  // TODO(crbug.com/40239641): Avoid pushing the fetched pref value to the
  // server again.
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
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return identity_manager_ &&
           identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSignin);
  }
  // The feature is not enabled, fallback to old behavior.
  // TODO(crbug.com/40067058): Delete ConsentLevel::kSync usage once
  // kReplaceSyncPromosWithSignInPromos is launched on all platforms. See
  // ConsentLevel::kSync documentation for details.
  return identity_manager_ &&
         identity_manager_->HasPrimaryAccount(signin::ConsentLevel::kSync);
}

bool AccountChecker::IsSyncingBookmarks() {
  if (base::FeatureList::IsEnabled(
          syncer::kReplaceSyncPromosWithSignInPromos)) {
    return sync_service_ && syncer::GetUploadToGoogleState(
                                sync_service_, syncer::DataType::BOOKMARKS) ==
                                syncer::UploadState::ACTIVE;
  }
  // The feature is not enabled, fallback to old behavior.
  // TODO(crbug.com/40067058): Delete IsSyncFeatureActive() usage once
  // kReplaceSyncPromosWithSignInPromos is launched on all platforms. See
  // ConsentLevel::kSync documentation for details.
  return sync_service_ && sync_service_->IsSyncFeatureActive() &&
         syncer::GetUploadToGoogleState(sync_service_,
                                        syncer::DataType::BOOKMARKS) !=
             syncer::UploadState::NOT_ACTIVE;
}

bool AccountChecker::IsSyncTypeEnabled(syncer::UserSelectableType type) {
  return sync_service_ && sync_service_->GetUserSettings() &&
         sync_service_->GetUserSettings()->GetSelectedTypes().Has(type);
}

bool AccountChecker::IsAnonymizedUrlDataCollectionEnabled() {
  return pref_service_ &&
         unified_consent::UrlKeyedDataCollectionConsentHelper::
             NewAnonymizedDataCollectionConsentHelper(pref_service_)
                 ->IsEnabled();
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

bool AccountChecker::CanUseModelExecutionFeatures() {
  if (!identity_manager_) {
    return false;
  }

  AccountCapabilities capabilities =
      identity_manager_
          ->FindExtendedAccountInfo(identity_manager_->GetPrimaryAccountInfo(
              signin::ConsentLevel::kSignin))
          .capabilities;

  return capabilities.can_use_model_execution_features() ==
         signin::Tribool::kTrue;
}

std::string AccountChecker::GetCountry() {
  return country_;
}

std::string AccountChecker::GetLocale() {
  return locale_;
}

PrefService* AccountChecker::GetPrefs() {
  return pref_service_.get();
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
      kOAuthName, GURL(kNotificationsPrefUrl), kGetHttpMethod, kContentType,
      std::vector<std::string>{kOAuthScope}, kTimeout, kEmptyPostData,
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
      if (std::optional<bool> price_email_pref =
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
  base::Value::Dict post_json = base::Value::Dict().Set(
      kPreferencesKey,
      base::Value::Dict().Set(
          kPriceTrackEmailPref,
          pref_service_->GetBoolean(kPriceEmailNotificationsEnabled)));
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
      kOAuthName, GURL(kNotificationsPrefUrl), kPostHttpMethod, kContentType,
      std::vector<std::string>{kOAuthScope}, kTimeout, post_data,
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
    const base::TimeDelta& timeout,
    const std::string& post_data,
    const net::NetworkTrafficAnnotationTag& annotation_tag) {
  // TODO(crbug.com/40067058): Delete ConsentLevel::kSync usage once
  // kReplaceSyncPromosWithSignInPromos is launched on all platforms. See
  // ConsentLevel::kSync documentation for details.
  signin::ConsentLevel consent_level =
      base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
          ? signin::ConsentLevel::kSignin
          : signin::ConsentLevel::kSync;
  return std::make_unique<EndpointFetcher>(
      url_loader_factory_, oauth_consumer_name, url, http_method, content_type,
      scopes, timeout, post_data, annotation_tag, identity_manager_,
      consent_level);
}

}  // namespace commerce
