// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/signin/signin_url_utils.h"

#include <string>

#include "base/strings/string_number_conversions.h"
#include "chrome/common/webui_url_constants.h"
#include "net/base/url_util.h"

namespace {

// Query parameter names of the sync confirmation and the profile customization
// URL.
constexpr char kStyleParamKey[] = "style";

constexpr char kIsSyncPromoParamKey[] = "is_sync_promo";

// Query parameter names of the reauth confirmation URL.
constexpr char kAccessPointParamKey[] = "access_point";

// URL tag to specify that the source is the ProfilePicker.
constexpr char kFromProfilePickerParamKey[] = "from_profile_picker";

}  // namespace

SyncConfirmationStyle GetSyncConfirmationStyle(const GURL& url) {
  std::string style_str;
  int style_int;
  // Default to modal if the parameter is missing.
  SyncConfirmationStyle style = SyncConfirmationStyle::kDefaultModal;
  if (net::GetValueForKeyInQuery(url, kStyleParamKey, &style_str) &&
      base::StringToInt(style_str, &style_int)) {
    style = static_cast<SyncConfirmationStyle>(style_int);
  }
  return style;
}

bool IsSyncConfirmationPromo(const GURL& url) {
  std::string is_promo_str;
  return net::GetValueForKeyInQuery(url, kIsSyncPromoParamKey, &is_promo_str) &&
         is_promo_str == "true";
}

GURL AppendSyncConfirmationQueryParams(const GURL& url,
                                       SyncConfirmationStyle style,
                                       bool is_sync_promo) {
  GURL url_with_params = net::AppendQueryParameter(
      url, kStyleParamKey, base::NumberToString(static_cast<int>(style)));
  if (is_sync_promo) {
    url_with_params = net::AppendQueryParameter(url_with_params,
                                                kIsSyncPromoParamKey, "true");
  }
  return url_with_params;
}

signin_metrics::ReauthAccessPoint GetReauthAccessPointForReauthConfirmationURL(
    const GURL& url) {
  std::string value;
  if (!net::GetValueForKeyInQuery(url, kAccessPointParamKey, &value))
    return signin_metrics::ReauthAccessPoint::kUnknown;

  int access_point = -1;
  base::StringToInt(value, &access_point);
  if (access_point <=
          static_cast<int>(signin_metrics::ReauthAccessPoint::kUnknown) ||
      access_point >
          static_cast<int>(signin_metrics::ReauthAccessPoint::kMaxValue)) {
    return signin_metrics::ReauthAccessPoint::kUnknown;
  }

  return static_cast<signin_metrics::ReauthAccessPoint>(access_point);
}

GURL GetReauthConfirmationURL(signin_metrics::ReauthAccessPoint access_point) {
  GURL url = GURL(chrome::kChromeUISigninReauthURL);
  url = net::AppendQueryParameter(
      url, kAccessPointParamKey,
      base::NumberToString(static_cast<int>(access_point)));
  return url;
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)
ProfileCustomizationStyle GetProfileCustomizationStyle(const GURL& url) {
  std::string style_str;
  int style_int;
  // Default style if the parameter is missing.
  ProfileCustomizationStyle style = ProfileCustomizationStyle::kDefault;
  if (net::GetValueForKeyInQuery(url, kStyleParamKey, &style_str) &&
      base::StringToInt(style_str, &style_int)) {
    style = static_cast<ProfileCustomizationStyle>(style_int);
  }
  return style;
}

GURL AppendProfileCustomizationQueryParams(const GURL& url,
                                           ProfileCustomizationStyle style) {
  GURL url_with_params = net::AppendQueryParameter(
      url, kStyleParamKey, base::NumberToString(static_cast<int>(style)));
  return url_with_params;
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT) || BUILDFLAG(IS_CHROMEOS_LACROS)

bool HasFromProfilePickerURLParameter(const GURL& url) {
  std::string from_profile_picker;
  bool found = net::GetValueForKeyInQuery(url, kFromProfilePickerParamKey,
                                          &from_profile_picker);
  return found && from_profile_picker == "true";
}

GURL AddFromProfilePickerURLParameter(const GURL& url) {
  return net::AppendOrReplaceQueryParameter(url, kFromProfilePickerParamKey,
                                            "true");
}
