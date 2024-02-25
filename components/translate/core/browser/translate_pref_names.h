// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_PREF_NAMES_H_
#define COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_PREF_NAMES_H_

namespace translate::prefs {
// These preferences are included in java_pref_names_srcjar for access in
// Java code.

// Boolean that is true when offering translate (i.e. the automatic Full Page
// Translate bubble) is enabled. Even when this is false, the user can force
// translate from the right-click context menu unless translate is disabled by
// policy.
inline constexpr char kOfferTranslateEnabled[] = "translate.enabled";
inline constexpr char kPrefAlwaysTranslateList[] = "translate_allowlists";
inline constexpr char kPrefTranslateRecentTarget[] = "translate_recent_target";
// Languages that the user marked as "do not translate".
inline constexpr char kBlockedLanguages[] = "translate_blocked_languages";
// Sites that never prompt to translate.
inline constexpr char kPrefNeverPromptSitesWithTime[] =
    "translate_site_blocklist_with_time";

}  // namespace translate::prefs

#endif  // COMPONENTS_TRANSLATE_CORE_BROWSER_TRANSLATE_PREF_NAMES_H_
