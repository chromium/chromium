// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_COOKIE_SETTINGS_UTIL_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_COOKIE_SETTINGS_UTIL_H_

#include "base/functional/callback.h"
#include "net/cookies/cookie_setting_override.h"

class GURL;

namespace content_settings {
class CookieSettings;
}

namespace net {
class SiteForCookies;
}

namespace url {
class Origin;
}

namespace signin {

// Callback that abstracts CookieSettings::IsFullCookieAccessAllowed checks
// required by signin request and cookie helpers.
// Should return true if full cookie access is allowed for `url` in the context
// of `site_for_cookies` and `top_frame_origin`.
using CookieSettingsCallback =
    base::RepeatingCallback<bool(const GURL& url,
                                 const net::SiteForCookies& site_for_cookies,
                                 const url::Origin& top_frame_origin,
                                 net::CookieSettingOverrides overrides)>;

// Returns a CookieSettingsCallback that wraps a
// content_settings::CookieSettings. If `cookie_settings` is null, the callback
// returns false.
CookieSettingsCallback MakeCookieSettingsCallback(
    const content_settings::CookieSettings* cookie_settings);

// Returns true if signin cookies are allowed.
bool SettingsAllowSigninCookies(
    const CookieSettingsCallback& cookie_settings_callback,
    bool check_only_gaia_url = false);
bool SettingsAllowSigninCookies(
    const content_settings::CookieSettings* cookie_settings,
    bool check_only_gaia_url = false);

// Returns true if signin cookies are cleared on exit.
bool SettingsDeleteSigninCookiesOnExit(
    const content_settings::CookieSettings* cookie_settings);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_COOKIE_SETTINGS_UTIL_H_
