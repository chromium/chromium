// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/companion/companion_url_builder.h"

#include "base/base64.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/side_panel/companion/constants.h"
#include "chrome/browser/ui/webui/side_panel/companion/proto/companion_url_params.pb.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"
#include "components/unified_consent/url_keyed_data_collection_consent_helper.h"
#include "net/base/url_util.h"
#include "url/gurl.h"

namespace companion {
namespace {

// TODO(b/274714162): Update server side code soon after this change.
// Query parameter for the companion url.
inline constexpr char kQueryStringKey[] = "query";

// Query parameter for the url of the main web content.
inline constexpr char kUrlQueryParameterKey[] = "url";
// Query parameter for the Chrome WebUI origin.
inline constexpr char kOriginQueryParameterKey[] = "origin";
// Query parameter value for the Chrome WebUI origin. This needs to be different
// from the WebUI URL constant because it does not include the last '/'.
inline constexpr char kOriginQueryParameterValue[] =
    "chrome-untrusted://companion-side-panel.top-chrome";
}  // namespace

CompanionUrlBuilder::CompanionUrlBuilder(PrefService* pref_service)
    : pref_service_(pref_service) {}

CompanionUrlBuilder::~CompanionUrlBuilder() = default;

bool CompanionUrlBuilder::IsMsbbEnabled() {
  std::unique_ptr<unified_consent::UrlKeyedDataCollectionConsentHelper>
      consent_helper = unified_consent::UrlKeyedDataCollectionConsentHelper::
          NewAnonymizedDataCollectionConsentHelper(pref_service_);
  return consent_helper->IsEnabled();
}

GURL CompanionUrlBuilder::BuildCompanionURL(GURL page_url) {
  // Fill the protobuf with the required query params.
  companion::proto::QueryParams url_params;
  bool is_msbb_enabled = IsMsbbEnabled();
  if (is_msbb_enabled) {
    url_params.set_page_url(page_url.spec());
  }
  url_params.set_has_msbb_enabled(is_msbb_enabled);

  // TODO(b/273652233): Uncomment.
  // companion::proto::PromoState* promo_state =
  // url_params.mutable_promo_state();
  // promo_state->set_signin_promo_denial_count(
  //     pref_service_->GetInteger(kSigninPromoDeclinedPref));
  // promo_state->set_msbb_promo_denial_count(
  //     pref_service_->GetInteger(kMsbbPromoDeclinedPref));
  // promo_state->set_labs_promo_denial_count(
  //     pref_service_->GetInteger(kLabsPromoDeclinedPref));

  GURL url_with_query_params = GetHomepageURLForCompanion();
  std::string base64_encoded_proto;
  base::Base64Encode(url_params.SerializeAsString(), &base64_encoded_proto);
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kQueryStringKey, base64_encoded_proto);

  // Add origin as a param allowing the page to be iframed.
  url_with_query_params = net::AppendOrReplaceQueryParameter(
      url_with_query_params, kOriginQueryParameterKey,
      kOriginQueryParameterValue);

  // TODO(b/274714162): Remove URL param.
  if (is_msbb_enabled) {
    url_with_query_params = net::AppendOrReplaceQueryParameter(
        url_with_query_params, kUrlQueryParameterKey, page_url.spec());
  }

  return url_with_query_params;
}

GURL CompanionUrlBuilder::GetHomepageURLForCompanion() {
  return GURL(features::kHomepageURLForCompanion.Get());
}

}  // namespace companion
