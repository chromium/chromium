// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/chrome_connected_header_helper.h"

#include <vector>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/google/core/common/google_util.h"
#include "components/signin/core/browser/cookie_settings_util.h"
#include "components/signin/public/identity_manager/tribool.h"
#include "google_apis/gaia/gaia_auth_util.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "url/gurl.h"

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/crosapi/mojom/crosapi.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#endif

namespace signin {

namespace {

const char kConsistencyEnabledByDefaultAttrName[] =
    "consistency_enabled_by_default";
const char kContinueUrlAttrName[] = "continue_url";
const char kEmailAttrName[] = "email";
const char kEnableAccountConsistencyAttrName[] = "enable_account_consistency";
const char kGaiaIdAttrName[] = "id";
const char kIsSameTabAttrName[] = "is_same_tab";
const char kIsSamlAttrName[] = "is_saml";
const char kProfileModeAttrName[] = "mode";
const char kServiceTypeAttrName[] = "action";
const char kSupervisedAttrName[] = "supervised";
const char kSourceAttrName[] = "source";
#if BUILDFLAG(IS_ANDROID)
const char kEligibleForConsistency[] = "eligible_for_consistency";
const char kShowConsistencyPromo[] = "show_consistency_promo";
#endif

// Determines the service type that has been passed from Gaia in the header.
GAIAServiceType GetGAIAServiceTypeFromHeader(const std::string& header_value) {
  if (header_value == "SIGNOUT")
    return GAIA_SERVICE_TYPE_SIGNOUT;
  else if (header_value == "INCOGNITO")
    return GAIA_SERVICE_TYPE_INCOGNITO;
  else if (header_value == "ADDSESSION")
    return GAIA_SERVICE_TYPE_ADDSESSION;
  else if (header_value == "SIGNUP")
    return GAIA_SERVICE_TYPE_SIGNUP;
  else if (header_value == "DEFAULT")
    return GAIA_SERVICE_TYPE_DEFAULT;
  else
    return GAIA_SERVICE_TYPE_NONE;
}

}  // namespace

const char kChromeConnectedCookieName[] = "CHROME_CONNECTED";

ChromeConnectedHeaderHelper::ChromeConnectedHeaderHelper(
    AccountConsistencyMethod account_consistency)
    : account_consistency_(account_consistency) {}

// static
std::string ChromeConnectedHeaderHelper::BuildRequestCookieIfPossible(
    const GURL& url,
    const std::string& gaia_id,
    AccountConsistencyMethod account_consistency,
    const content_settings::CookieSettings* cookie_settings,
    int profile_mode_mask) {
  ChromeConnectedHeaderHelper chrome_connected_helper(account_consistency);
  if (!chrome_connected_helper.ShouldBuildRequestHeader(url, cookie_settings))
    return "";

  // Child accounts are not supported on iOS, so it is preferred to not include
  // this information in the ChromeConnected cookie.
  return chrome_connected_helper.BuildRequestHeader(
      /*is_header_request=*/false, url, gaia_id,
      /*is_child_account=*/Tribool::kUnknown, profile_mode_mask,
      /*source=*/std::string(), /*force_account_consistency=*/false);
}

// static
ManageAccountsParams ChromeConnectedHeaderHelper::BuildManageAccountsParams(
    const std::string& header_value) {
  DCHECK(!header_value.empty());
  ManageAccountsParams params;
  ResponseHeaderDictionary header_dictionary =
      ParseAccountConsistencyResponseHeader(header_value);
  ResponseHeaderDictionary::const_iterator it = header_dictionary.begin();
  for (; it != header_dictionary.end(); ++it) {
    const std::string key_name(it->first);
    const std::string value(it->second);
    if (key_name == kServiceTypeAttrName) {
      params.service_type = GetGAIAServiceTypeFromHeader(value);
    } else if (key_name == kEmailAttrName) {
      params.email = value;
    } else if (key_name == kIsSamlAttrName) {
      params.is_saml = value == "true";
    } else if (key_name == kContinueUrlAttrName) {
      params.continue_url = value;
    } else if (key_name == kIsSameTabAttrName) {
      params.is_same_tab = value == "true";
#if BUILDFLAG(IS_ANDROID)
    } else if (key_name == kShowConsistencyPromo) {
      params.show_consistency_promo = value == "true";
#endif
    } else {
      DLOG(WARNING) << "Unexpected Gaia header attribute '" << key_name << "'.";
    }
  }
  return params;
}

bool ChromeConnectedHeaderHelper::ShouldBuildRequestHeader(
    const GURL& url,
    const content_settings::CookieSettings* cookie_settings) {
  // Check if url is eligible for the header.
  if (!IsUrlEligibleForRequestHeader(url)) {
    return false;
  }

  // If signin cookies are not allowed, don't add the header.
  return SettingsAllowSigninCookies(cookie_settings);
}

bool ChromeConnectedHeaderHelper::IsUrlEligibleToIncludeGaiaId(
    const GURL& url,
    bool is_header_request) {
  // Gaia ID is only used by Google Drive on desktop to auto-enable offline
  // mode. As Gaia ID  is personal identifiable information, we restrict its
  // usage:
  // * Avoid sending it in the cookie as not needed on iOS.
  // * Only send it in the header to Drive URLs.
  return is_header_request ? IsDriveOrigin(url.DeprecatedGetOriginAsURL())
                           : false;
}

bool ChromeConnectedHeaderHelper::IsDriveOrigin(const GURL& url) {
  if (!url.SchemeIsCryptographic())
    return false;

  const GURL kGoogleDriveURL("https://drive.google.com");
  const GURL kGoogleDocsURL("https://docs.google.com");
  return url == kGoogleDriveURL || url == kGoogleDocsURL;
}

bool ChromeConnectedHeaderHelper::IsUrlEligibleForRequestHeader(
    const GURL& url) {
  // Consider the account ID sensitive and limit it to secure domains.
  if (!url.SchemeIsCryptographic())
    return false;

  switch (account_consistency_) {
    case AccountConsistencyMethod::kDisabled:
      return false;
    case AccountConsistencyMethod::kDice:
      // Google Drive uses the sync account ID present in the X-Chrome-Connected
      // header to automatically turn on offline mode. So Chrome needs to send
      // this header to Google Drive when Dice is enabled.
      return IsDriveOrigin(url.DeprecatedGetOriginAsURL());
    case AccountConsistencyMethod::kMirror: {
      // Set the X-Chrome-Connected header for all Google web properties if
      // Mirror account consistency is enabled. Vasquette, which is integrated
      // with most Google properties, needs the header to redirect certain user
      // actions to Chrome native UI.
      return google_util::IsGoogleDomainUrl(
                 url, google_util::ALLOW_SUBDOMAIN,
                 google_util::DISALLOW_NON_STANDARD_PORTS) ||
             google_util::IsYoutubeDomainUrl(
                 url, google_util::ALLOW_SUBDOMAIN,
                 google_util::DISALLOW_NON_STANDARD_PORTS) ||
             gaia::HasGaiaSchemeHostPort(url);
    }
  }
}

std::string ChromeConnectedHeaderHelper::BuildRequestHeader(
    bool is_header_request,
    const GURL& url,
    const std::string& gaia_id,
    Tribool is_child_account,
    int profile_mode_mask,
    const std::string& source,
    bool force_account_consistency) {
  std::vector<std::string> parts;
  if (!source.empty()) {
    parts.push_back(
        base::StringPrintf("%s=%s", kSourceAttrName, source.c_str()));
  }
// If we are on mobile or desktop, an empty |account_id| corresponds to the user
// not signed into Sync. Do not enforce account consistency, unless Mice is
// enabled on mobile (Android or iOS).
// On Chrome OS, an empty |account_id| corresponds to Public Sessions, Guest
// Sessions and Active Directory logins. Guest Sessions have already been
// filtered upstream and we want to enforce account consistency in Public
// Sessions and Active Directory logins.
#if BUILDFLAG(IS_CHROMEOS)
  force_account_consistency = true;
#endif

  if (!force_account_consistency && gaia_id.empty()) {
#if BUILDFLAG(IS_ANDROID)
    if (gaia::HasGaiaSchemeHostPort(url)) {
      parts.push_back(
          base::StringPrintf("%s=%s", kEligibleForConsistency, "true"));
      return base::JoinString(parts, is_header_request ? "," : ":");
    }
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
    return std::string();
  }

  if (!gaia_id.empty() &&
      IsUrlEligibleToIncludeGaiaId(url, is_header_request)) {
    // Only set the Gaia ID on domains that actually require it.
    parts.push_back(
        base::StringPrintf("%s=%s", kGaiaIdAttrName, gaia_id.c_str()));
  }
  parts.push_back(
      base::StringPrintf("%s=%s", kProfileModeAttrName,
                         base::NumberToString(profile_mode_mask).c_str()));
  bool is_mirror_enabled =
      account_consistency_ == AccountConsistencyMethod::kMirror;
  parts.push_back(base::StringPrintf("%s=%s", kEnableAccountConsistencyAttrName,
                                     is_mirror_enabled ? "true" : "false"));
  switch (is_child_account) {
    case Tribool::kTrue:
      parts.push_back(base::StringPrintf("%s=%s", kSupervisedAttrName, "true"));
      break;
    case Tribool::kFalse:
      parts.push_back(
          base::StringPrintf("%s=%s", kSupervisedAttrName, "false"));
      break;
    case Tribool::kUnknown:
      // Do not add the supervised parameter.
      break;
  }

  parts.push_back(base::StringPrintf("%s=%s",
                                     kConsistencyEnabledByDefaultAttrName,
#if BUILDFLAG(IS_CHROMEOS_LACROS)
                                     "true"));
#else
                                     "false"));
#endif

  return base::JoinString(parts, is_header_request ? "," : ":");
}

}  // namespace signin
