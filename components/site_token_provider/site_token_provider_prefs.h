// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROVIDER_PREFS_H_
#define COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROVIDER_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace site_token_provider::prefs {

// Preference to control whether the SiteTokenProvider is enabled via Sync.
// This preference is written by the server and synced down to clients.
inline constexpr char kSiteTokenProviderPref[] = "sync.site_token_provider";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace site_token_provider::prefs

#endif  // COMPONENTS_SITE_TOKEN_PROVIDER_SITE_TOKEN_PROVIDER_PREFS_H_
