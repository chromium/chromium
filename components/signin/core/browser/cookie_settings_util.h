// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_COOKIE_SETTINGS_UTIL_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_COOKIE_SETTINGS_UTIL_H_

namespace content_settings {
class CookieSettings;
}

namespace signin {

// Returns true if signin cookies are allowed.
bool SettingsAllowSigninCookies(
    const content_settings::CookieSettings* cookie_settings);

// Returns true if signin cookies are cleared on exit.
bool SettingsDeleteSigninCookiesOnExit(
    const content_settings::CookieSettings* cookie_settings);

}  // namespace signin

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_COOKIE_SETTINGS_UTIL_H_
