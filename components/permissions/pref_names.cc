// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/permissions/pref_names.h"
#include "components/permissions/permission_actions_history.h"
#include "components/pref_registry/pref_registry_syncable.h"

#include "build/build_config.h"

namespace permissions {
namespace prefs {

// List containing a history of past permission actions, for all permission
// types.
const char kPermissionActions[] = "profile.content_settings.permission_actions";

#if BUILDFLAG(IS_ANDROID)
// The current level of backoff for showing the location settings dialog for the
// default search engine.
const char kLocationSettingsBackoffLevelDSE[] =
    "location_settings_backoff_level_dse";

// The current level of backoff for showing the location settings dialog for
// sites other than the default search engine.
const char kLocationSettingsBackoffLevelDefault[] =
    "location_settings_backoff_level_default";

// The next time the location settings dialog can be shown for the default
// search engine.
const char kLocationSettingsNextShowDSE[] = "location_settings_next_show_dse";

// The next time the location settings dialog can be shown for sites other than
// the default search engine.
const char kLocationSettingsNextShowDefault[] =
    "location_settings_next_show_default";
#endif  // BUILDFLAG(IS_ANDROID)

// The number of one time permission prompts a user has seen.
const char kOneTimePermissionPromptsDecidedCount[] =
    "profile.one_time_permission_prompts_decided_count";

// Boolean that specifies whether or not unused site permissions should be
// revoked by Safety Hub. It is used only when kSafetyHub flag is on.
// Conditioned because currently Safety Hub is available only on desktop.
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
const char kUnusedSitePermissionsRevocationEnabled[] =
    "safety_hub.unused_site_permissions_revocation.enabled";
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}  // namespace prefs

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  PermissionActionsHistory::RegisterProfilePrefs(registry);

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  registry->RegisterBooleanPref(prefs::kUnusedSitePermissionsRevocationEnabled,
                                true);
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
}

}  // namespace permissions
