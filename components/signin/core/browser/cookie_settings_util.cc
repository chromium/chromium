// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/core/browser/cookie_settings_util.h"

#include "components/content_settings/core/browser/cookie_settings.h"
#include "google_apis/gaia/gaia_urls.h"
#include "url/gurl.h"

namespace signin {

bool SettingsAllowSigninCookies(
    const content_settings::CookieSettings* cookie_settings) {
  GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  GURL google_url = GaiaUrls::GetInstance()->google_url();
  return cookie_settings &&
         cookie_settings->IsFullCookieAccessAllowed(
             gaia_url, gaia_url,
             content_settings::CookieSettings::QueryReason::kCookies) &&
         cookie_settings->IsFullCookieAccessAllowed(
             google_url, google_url,
             content_settings::CookieSettings::QueryReason::kCookies);
}

bool SettingsDeleteSigninCookiesOnExit(
    const content_settings::CookieSettings* cookie_settings) {
  GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  GURL google_url = GaiaUrls::GetInstance()->google_url();
  ContentSettingsForOneType settings = cookie_settings->GetCookieSettings();

  return !cookie_settings ||
         cookie_settings->ShouldDeleteCookieOnExit(
             settings, "." + gaia_url.host(), true) ||
         cookie_settings->ShouldDeleteCookieOnExit(
             settings, "." + google_url.host(), true);
}

}  // namespace signin
