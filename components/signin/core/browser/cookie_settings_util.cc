// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/cookie_settings_util.h"

#include "components/content_settings/core/browser/cookie_settings.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_setting_override.h"
#include "net/cookies/site_for_cookies.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace signin {

// TODO(crbug.com/40247160): Consider whether the following checks should
// take in CookieSettingOverrides rather than default to none.

bool SettingsAllowSigninCookies(
    const content_settings::CookieSettings* cookie_settings) {
  GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  GURL google_url = GaiaUrls::GetInstance()->google_url();
  return cookie_settings &&
         cookie_settings->IsFullCookieAccessAllowed(
             gaia_url, net::SiteForCookies::FromUrl(gaia_url),
             url::Origin::Create(gaia_url), net::CookieSettingOverrides()) &&
         cookie_settings->IsFullCookieAccessAllowed(
             google_url, net::SiteForCookies::FromUrl(google_url),
             url::Origin::Create(google_url), net::CookieSettingOverrides());
}

bool SettingsDeleteSigninCookiesOnExit(
    const content_settings::CookieSettings* cookie_settings) {
  GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  GURL google_url = GaiaUrls::GetInstance()->google_url();
  ContentSettingsForOneType settings = cookie_settings->GetCookieSettings();

  return !cookie_settings ||
         cookie_settings->ShouldDeleteCookieOnExit(
             settings, "." + gaia_url.host(),
             net::CookieSourceScheme::kSecure) ||
         cookie_settings->ShouldDeleteCookieOnExit(
             settings, "." + google_url.host(),
             net::CookieSourceScheme::kSecure);
}

}  // namespace signin
