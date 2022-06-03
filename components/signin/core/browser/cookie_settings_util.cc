// Copyright 2018 The Chromium Authors. All rights reserved.
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
         cookie_settings->IsFullCookieAccessAllowed(gaia_url, gaia_url) &&
         cookie_settings->IsFullCookieAccessAllowed(google_url, google_url);
}

bool SettingsDeleteSigninCookiesOnExit(
    const content_settings::CookieSettings* cookie_settings) {
  GURL gaia_url = GaiaUrls::GetInstance()->gaia_url();
  GURL google_url = GaiaUrls::GetInstance()->google_url();
  ContentSettingsForOneType settings;
  cookie_settings->GetCookieSettings(&settings);

  return !cookie_settings ||
         cookie_settings->ShouldDeleteCookieOnExit(
             settings, "." + gaia_url.host(), true) ||
         cookie_settings->ShouldDeleteCookieOnExit(
             settings, "." + google_url.host(), true);
}

}  // namespace signin
