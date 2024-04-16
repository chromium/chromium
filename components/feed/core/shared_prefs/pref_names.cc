// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FEED_CORE_SHARED_PREFS_PREF_NAMES_CC_
#define COMPONENTS_FEED_CORE_SHARED_PREFS_PREF_NAMES_CC_

#include "components/feed/core/shared_prefs/pref_names.h"

#include "components/prefs/pref_registry_simple.h"

namespace feed {
namespace prefs {

const char kEnableSnippets[] = "ntp_snippets.enable";
// A boolean pref set to true if Feed articles are visible.
// FEED_ARTICLES_LIST_VISIBLE in ChromePreferenceKeys.java is a pre-native cache
// and should be consistent with this pref.
const char kArticlesListVisible[] = "ntp_snippets.list_visible";
// A boolean pref set to true if swapping out NTP isn't enabled or if DSE is
// Google.
// TODO(crbug.com/40282032): Inhibit loading feeds in native feeds code
// when this pref is set to false.
const char kEnableSnippetsByDse[] = "ntp_snippets_by_dse.enable";

void RegisterFeedSharedProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(kEnableSnippets, true);
  registry->RegisterBooleanPref(kArticlesListVisible, true);
  registry->RegisterBooleanPref(kEnableSnippetsByDse, true);
}

}  // namespace prefs
}  // namespace feed

#endif  // COMPONENTS_FEED_CORE_SHARED_PREFS_PREF_NAMES_CC_
