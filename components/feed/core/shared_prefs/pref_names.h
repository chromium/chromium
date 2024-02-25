// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_SHARED_PREFS_PREF_NAMES_H_
#define COMPONENTS_FEED_CORE_SHARED_PREFS_PREF_NAMES_H_

class PrefRegistrySimple;

// These prefs are shared by Feed and Zine (ntp_snippets).

namespace feed {
namespace prefs {

// If set to false, remote suggestions are completely disabled. This is set by
// an enterprise policy.
extern const char kEnableSnippets[];

// Whether the list of NTP snippets is visible in UI. This is set to false when
// the user toggles the list off.
extern const char kArticlesListVisible[];

// This is set to false if swapping out NTP is enabled and default search engine
// isn't Google.
extern const char kEnableSnippetsByDse[];

void RegisterFeedSharedProfilePrefs(PrefRegistrySimple* registry);
}  // namespace prefs
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_SHARED_PREFS_PREF_NAMES_H_
