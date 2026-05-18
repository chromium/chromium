// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_BROWSER_URL_LIST_URL_LIST_POLICY_PREF_NAMES_H_
#define COMPONENTS_POLICY_CORE_BROWSER_URL_LIST_URL_LIST_POLICY_PREF_NAMES_H_

namespace policy::policy_prefs {

// Blocks access to the listed host patterns for incognito mode.
inline constexpr char kIncognitoModeUrlBlocklist[] =
    "policy.incognito_mode_url_blocklist";

// Allows access to the listed host patterns for incognito mode.
inline constexpr char kIncognitoModeUrlAllowlist[] =
    "policy.incognito_mode_url_allowlist";

// Blocks access to the listed host patterns.
inline constexpr char kUrlBlocklist[] = "policy.url_blocklist";

// Allows access to the listed host patterns, as exceptions to the blacklist.
inline constexpr char kUrlAllowlist[] = "policy.url_allowlist";

}  // namespace policy::policy_prefs

#endif  // COMPONENTS_POLICY_CORE_BROWSER_URL_LIST_URL_LIST_POLICY_PREF_NAMES_H_
