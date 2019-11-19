// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/signin_header_helper.h"

#include <stddef.h>

#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/core/browser/chrome_connected_header_helper.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/escape.h"
#include "net/url_request/url_request.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "components/signin/core/browser/dice_header_helper.h"
#endif

namespace signin {

const char kChromeConnectedHeader[] = "X-Chrome-Connected";
const char kDiceRequestHeader[] = "X-Chrome-ID-Consistency-Request";
const char kDiceResponseHeader[] = "X-Chrome-ID-Consistency-Response";

ManageAccountsParams::ManageAccountsParams()
    : service_type(GAIA_SERVICE_TYPE_NONE),
      email(""),
      is_saml(false),
      continue_url(""),
      is_same_tab(false) {
}

ManageAccountsParams::ManageAccountsParams(const ManageAccountsParams&) =
    default;

// Trivial constructors and destructors.
DiceResponseParams::DiceResponseParams() {}
DiceResponseParams::~DiceResponseParams() {}
DiceResponseParams::DiceResponseParams(DiceResponseParams&&) = default;
DiceResponseParams& DiceResponseParams::operator=(DiceResponseParams&&) =
    default;

DiceResponseParams::AccountInfo::AccountInfo() {}
DiceResponseParams::AccountInfo::AccountInfo(const std::string& gaia_id,
                                             const std::string& email,
                                             int session_index)
    : gaia_id(gaia_id), email(email), session_index(session_index) {}
DiceResponseParams::AccountInfo::~AccountInfo() {}
DiceResponseParams::AccountInfo::AccountInfo(const AccountInfo&) = default;

DiceResponseParams::SigninInfo::SigninInfo() {}
DiceResponseParams::SigninInfo::~SigninInfo() {}
DiceResponseParams::SigninInfo::SigninInfo(const SigninInfo&) = default;

DiceResponseParams::SignoutInfo::SignoutInfo() {}
DiceResponseParams::SignoutInfo::~SignoutInfo() {}
DiceResponseParams::SignoutInfo::SignoutInfo(const SignoutInfo&) = default;

DiceResponseParams::EnableSyncInfo::EnableSyncInfo() {}
DiceResponseParams::EnableSyncInfo::~EnableSyncInfo() {}
DiceResponseParams::EnableSyncInfo::EnableSyncInfo(const EnableSyncInfo&) =
    default;

RequestAdapter::RequestAdapter(net::URLRequest* request) : request_(request) {}

RequestAdapter::~RequestAdapter() = default;

const GURL& RequestAdapter::GetUrl() {
  return request_->url();
}

bool RequestAdapter::HasHeader(const std::string& name) {
  return request_->extra_request_headers().HasHeader(name);
}

void RequestAdapter::RemoveRequestHeaderByName(const std::string& name) {
  return request_->RemoveRequestHeaderByName(name);
}

void RequestAdapter::SetExtraHeaderByName(const std::string& name,
                                          const std::string& value) {
  return request_->SetExtraRequestHeaderByName(name, value, false);
}

std::string BuildMirrorRequestCookieIfPossible(
    const GURL& url,
    const std::string& gaia_id,
    AccountConsistencyMethod account_consistency,
    const content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask) {
  return ChromeConnectedHeaderHelper::BuildRequestCookieIfPossible(
      url, gaia_id, account_consistency, cookie_settings, profile_mode_mask);
}

SigninHeaderHelper::SigninHeaderHelper() = default;
SigninHeaderHelper::~SigninHeaderHelper() = default;

bool SigninHeaderHelper::AppendOrRemoveRequestHeader(
    RequestAdapter* request,
    const GURL& redirect_url,
    const char* header_name,
    const std::string& header_value) {
  if (header_value.empty()) {
    // If the request is being redirected, and it has the account consistency
    // header, and current url is a Google URL, and the redirected one is not,
    // remove the header.
    if (!redirect_url.is_empty() && request->HasHeader(header_name) &&
        IsUrlEligibleForRequestHeader(request->GetUrl()) &&
        !IsUrlEligibleForRequestHeader(redirect_url)) {
      VLOG(1) << "Sign-in request header [" << header_name << "] removed.";
      request->RemoveRequestHeaderByName(header_name);
    }
    return false;
  }
  VLOG(1) << "Sign-in request header [" << header_name << "] added.";
  request->SetExtraHeaderByName(header_name, header_value);
  return true;
}

// static
SigninHeaderHelper::ResponseHeaderDictionary
SigninHeaderHelper::ParseAccountConsistencyResponseHeader(
    const std::string& header_value) {
  ResponseHeaderDictionary dictionary;
  for (const base::StringPiece& field :
       base::SplitStringPiece(header_value, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_NONEMPTY)) {
    size_t delim = field.find_first_of('=');
    if (delim == std::string::npos) {
      DLOG(WARNING) << "Unexpected Gaia header field '" << field << "'.";
      continue;
    }
    dictionary.insert(
        {field.substr(0, delim).as_string(),
         net::UnescapeURLComponent(
             field.substr(delim + 1).as_string(),
             net::UnescapeRule::PATH_SEPARATORS |
                 net::UnescapeRule::URL_SPECIAL_CHARS_EXCEPT_PATH_SEPARATORS)});
  }
  return dictionary;
}

void AppendOrRemoveMirrorRequestHeader(
    RequestAdapter* request,
    const GURL& redirect_url,
    const std::string& gaia_id,
    AccountConsistencyMethod account_consistency,
    const content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask) {
  const GURL& url = redirect_url.is_empty() ? request->GetUrl() : redirect_url;
  ChromeConnectedHeaderHelper chrome_connected_helper(account_consistency);
  std::string chrome_connected_header_value;
  if (chrome_connected_helper.ShouldBuildRequestHeader(url, cookie_settings)) {
    chrome_connected_header_value = chrome_connected_helper.BuildRequestHeader(
        true /* is_header_request */, url, gaia_id, profile_mode_mask);
  }
  chrome_connected_helper.AppendOrRemoveRequestHeader(
      request, redirect_url, kChromeConnectedHeader,
      chrome_connected_header_value);
}

bool AppendOrRemoveDiceRequestHeader(
    RequestAdapter* request,
    const GURL& redirect_url,
    const std::string& gaia_id,
    bool sync_enabled,
    AccountConsistencyMethod account_consistency,
    const content_settings::CookieSettings* cookie_settings,
    const std::string& device_id) {
#if BUILDFLAG(ENABLE_DICE_SUPPORT)
  const GURL& url = redirect_url.is_empty() ? request->GetUrl() : redirect_url;
  DiceHeaderHelper dice_helper(account_consistency);
  std::string dice_header_value;
  if (dice_helper.ShouldBuildRequestHeader(url, cookie_settings)) {
    dice_header_value = dice_helper.BuildRequestHeader(
        sync_enabled ? gaia_id : std::string(), device_id);
  }
  return dice_helper.AppendOrRemoveRequestHeader(
      request, redirect_url, kDiceRequestHeader, dice_header_value);
#else
  return false;
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
}

ManageAccountsParams BuildManageAccountsParams(
    const std::string& header_value) {
  return ChromeConnectedHeaderHelper::BuildManageAccountsParams(header_value);
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
DiceResponseParams BuildDiceSigninResponseParams(
    const std::string& header_value) {
  return DiceHeaderHelper::BuildDiceSigninResponseParams(header_value);
}

DiceResponseParams BuildDiceSignoutResponseParams(
    const std::string& header_value) {
  return DiceHeaderHelper::BuildDiceSignoutResponseParams(header_value);
}
#endif

}  // namespace signin
