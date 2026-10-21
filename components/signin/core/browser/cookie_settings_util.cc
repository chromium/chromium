// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/cookie_settings_util.h"

#include <optional>

#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/site_for_cookies.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace signin {

namespace {

bool IsFullCookieAccessAllowed(
    const scoped_refptr<const content_settings::CookieSettings> cookie_settings,
    const GURL& url,
    const net::SiteForCookies& site_for_cookies,
    const url::Origin& top_frame_origin,
    net::CookieSettingOverrides overrides) {
  if (!cookie_settings) {
    return false;
  }
  return cookie_settings->IsFullCookieAccessAllowed(
      url, site_for_cookies, top_frame_origin, overrides,
      /*cookie_partition_key=*/std::nullopt);
}

}  // namespace

CookieSettingsCallback MakeCookieSettingsCallback(
    const content_settings::CookieSettings* cookie_settings) {
  // Take a ref-counted pointer to ensure lifetime of cookie_settings.
  // nullptr will be handled by IsFullCookieAccessAllowed.
  return base::BindRepeating(&IsFullCookieAccessAllowed,
                             base::WrapRefCounted(cookie_settings));
}

// TODO(crbug.com/40247160): Consider whether the following checks should
// take in CookieSettingOverrides rather than default to none.

bool SettingsAllowSigninCookies(
    const CookieSettingsCallback& cookie_settings_callback,
    bool check_only_gaia_url) {
#if BUILDFLAG(IS_IOS)
  // There is no way for users to set the cookies content setting in iOS so
  // sign-in cookies will always be allowed.
  return true;
#else
  GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  if (!cookie_settings_callback.Run(
          gaia_url, net::SiteForCookies::FromUrl(gaia_url),
          url::Origin::Create(gaia_url), net::CookieSettingOverrides())) {
    return false;
  }

  GURL google_url = GaiaUrls::GetInstance()->secure_google_url();
  if (!check_only_gaia_url &&
      !cookie_settings_callback.Run(
          google_url, net::SiteForCookies::FromUrl(google_url),
          url::Origin::Create(google_url), net::CookieSettingOverrides())) {
    return false;
  }

  return true;
#endif
}

bool SettingsAllowSigninCookies(
    const content_settings::CookieSettings* cookie_settings,
    bool check_only_gaia_url) {
  return SettingsAllowSigninCookies(MakeCookieSettingsCallback(cookie_settings),
                                    check_only_gaia_url);
}

bool SettingsDeleteSigninCookiesOnExit(
    const content_settings::CookieSettings* cookie_settings) {
#if BUILDFLAG(IS_IOS)
  // There is no way for users to set the cookies content setting in iOS so
  // sign-in cookies will not be deleted.
  return false;
#else
  GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  GURL google_url = GaiaUrls::GetInstance()->secure_google_url();
  ContentSettingsForOneType settings = cookie_settings->GetCookieSettings();

  return !cookie_settings ||
         cookie_settings->ShouldDeleteCookieOnExit(
             settings, "." + gaia_url.GetHost(),
             net::CookieSourceScheme::kSecure) ||
         cookie_settings->ShouldDeleteCookieOnExit(
             settings, "." + google_url.GetHost(),
             net::CookieSourceScheme::kSecure);
#endif
}

}  // namespace signin
