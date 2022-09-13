// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/commerce/core/account_checker.h"
#include "base/feature_list.h"
#include "base/json/values_util.h"
#include "base/values.h"
#include "components/commerce/core/commerce_feature_list.h"
#include "components/commerce/core/pref_names.h"
#include "components/endpoint_fetcher/endpoint_fetcher.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/data_decoder/public/cpp/data_decoder.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

namespace commerce {

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

void AccountChecker::OnPrimaryAccountChanged(
    const signin::PrimaryAccountChangeEvent& event_details) {
  FetchWaaStatus();
}

void AccountChecker::FetchWaaStatus() {
  // For now we need to update users' consent status on web and app activity
  // only when ShoppingList feature is enabled.
  if (!base::FeatureList::IsEnabled(kShoppingList) || !IsSignedIn())
    return;
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
  auto endpoint_fetcher = std::make_unique<EndpointFetcher>(
      url_loader_factory_, waa_oauth_name, GURL(waa_query_url), waa_get_method,
      waa_content_type, std::vector<std::string>{waa_oauth_scope},
      waa_timeout_ms, waa_post_data, traffic_annotation, identity_manager_);
  endpoint_fetcher.get()->Fetch(base::BindOnce(
      &AccountChecker::HandleFetchWaaResponse, weak_ptr_factory_.GetWeakPtr(),
      pref_service_, std::move(endpoint_fetcher)));
}

void AccountChecker::HandleFetchWaaResponse(
    PrefService* pref_service,
    std::unique_ptr<EndpointFetcher> endpoint_fetcher,
    std::unique_ptr<EndpointResponse> responses) {
  data_decoder::DataDecoder::ParseJsonIsolated(
      responses->response,
      base::BindOnce(
          [](PrefService* pref_service,
             data_decoder::DataDecoder::ValueOrError result) {
            if (pref_service && result.has_value() && result->is_dict()) {
              const char waa_response_key[] = "history_recording_enabled";
              if (auto waa_enabled = result->FindBoolKey(waa_response_key)) {
                pref_service->SetBoolean(kWebAndAppActivityEnabledForShopping,
                                         *waa_enabled);
              }
            }
          },
          pref_service));
}

}  // namespace commerce
