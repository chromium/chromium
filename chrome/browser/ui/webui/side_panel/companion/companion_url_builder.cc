// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/side_panel/companion/companion_url_builder.h"

#include "base/base64.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/webui/side_panel/companion/companion_permission_utils.h"
#include "chrome/browser/ui/webui/side_panel/companion/constants.h"
#include "chrome/browser/ui/webui/side_panel/companion/proto/companion_url_params.pb.h"
#include "chrome/browser/ui/webui/side_panel/companion/signin_delegate.h"
#include "components/prefs/pref_service.h"
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

// Checks to see if the page url is a valid one to be sent to companion.
bool IsValidPageURLForCompanion(const GURL& url) {
  if (!url.is_valid()) {
    return false;
  }

  if (!url.has_host()) {
    return false;
  }
  if (net::IsLocalhost(url)) {
    return false;
  }
  if (url.HostIsIPAddress()) {
    return false;
  }
  if (!url.SchemeIsHTTPOrHTTPS()) {
    return false;
  }
  if (url.has_username() || url.has_password()) {
    return false;
  }
  return true;
}

}  // namespace

CompanionUrlBuilder::CompanionUrlBuilder(PrefService* pref_service,
                                         SigninDelegate* signin_delegate)
    : pref_service_(pref_service), signin_delegate_(signin_delegate) {}

CompanionUrlBuilder::~CompanionUrlBuilder() = default;

GURL CompanionUrlBuilder::BuildCompanionURL(GURL page_url) {
  return BuildCompanionURL(page_url, /*text_query=*/"");
}

GURL CompanionUrlBuilder::BuildCompanionURL(GURL page_url,
                                            const std::string& text_query) {
  // Fill the protobuf with the required query params.
  companion::proto::QueryParams url_params;
  bool is_msbb_enabled =
      IsUserPermittedToSharePageInfoWithCompanion(pref_service_);
  if (is_msbb_enabled && IsValidPageURLForCompanion(page_url)) {
    url_params.set_page_url(page_url.spec());
  }

  url_params.set_has_msbb_enabled(is_msbb_enabled);
  url_params.set_signin_allowed_and_required(signin_delegate_->AllowedSignin());
  if (!text_query.empty()) {
    url_params.set_search_query(text_query);
  }

  companion::proto::PromoState* promo_state = url_params.mutable_promo_state();
  promo_state->set_signin_promo_denial_count(
      pref_service_->GetInteger(kSigninPromoDeclinedCountPref));
  promo_state->set_msbb_promo_denial_count(
      pref_service_->GetInteger(kMsbbPromoDeclinedCountPref));
  promo_state->set_labs_promo_denial_count(
      pref_service_->GetInteger(kLabsPromoDeclinedCountPref));

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
  if (is_msbb_enabled && IsValidPageURLForCompanion(page_url)) {
    url_with_query_params = net::AppendOrReplaceQueryParameter(
        url_with_query_params, kUrlQueryParameterKey, page_url.spec());
  }

  return url_with_query_params;
}

GURL CompanionUrlBuilder::GetHomepageURLForCompanion() {
  return GURL(features::kHomepageURLForCompanion.Get());
}

}  // namespace companion
