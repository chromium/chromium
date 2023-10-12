// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engines_pref_names.h"

namespace prefs {

// The GUID of the locally saved default search provider. Note that this acts
// like a pointer to which synced search engine should be the default, rather
// than the prefs below which describe the locally saved default search provider
// details. This is ignored in the case of the default search provider being
// managed by policy. This pref is in the process of replacing
// `kSyncedDefaultSearchProviderGUID`.
const char kDefaultSearchProviderGUID[] = "default_search_provider.guid";

// The GUID of the synced default search provider. Note that this acts like a
// pointer to which synced search engine should be the default, rather than the
// prefs below which describe the locally saved default search provider details
// (and are not synced). This is ignored in the case of the default search
// provider being managed by policy.
// This pref is in the process of being replaced by
// `kDefaultSearchProviderGUID`.
const char kSyncedDefaultSearchProviderGUID[] =
    "default_search_provider.synced_guid";

// Epoch timestamp in seconds of when the user chose a search engine in
// the choice screen.
const char kDefaultSearchProviderChoiceScreenCompletionTimestamp[] =
    "default_search_provider.choice_screen_completion_timestamp";

// Random number to use as a profile-constant seed for the random shuffling of
// the choice screen elements.
const char kDefaultSearchProviderChoiceScreenRandomShuffleSeed[] =
    "default_search_provider.choice_screen_random_shuffle_seed";

// Whether a search context menu item is allowed.
const char kDefaultSearchProviderContextMenuAccessAllowed[] =
    "default_search_provider.context_menu_access_allowed";

// Whether the prepopulated data from which the keywords were loaded is the
// extended list that is not limited to just 5 engines.
// This pref helps versioning the keyword data in an orthogonal way from the
// prepopulated data version numbers, as this is dependent on runtime feature
// state.
// TODO(b/304947278): Deprecate when the SearchEngineChoice feature launches.
const char kDefaultSearchProviderKeywordsUseExtendedList[] =
    "default_search_provider.keywords_use_extended_list";

// Whether having a default search provider is enabled.
const char kDefaultSearchProviderEnabled[] =
    "default_search_provider.enabled";

// The dictionary key used when the default search providers are given
// in the preferences file. Normally they are copied from the main
// preferences file.
const char kSearchProviderOverrides[] = "search_provider_overrides";
// The format version for the dictionary above.
const char kSearchProviderOverridesVersion[] =
    "search_provider_overrides_version";

// Path to the profile selected to show the search engine choice prompt.
// NOTE: Unlike most of the other preferences here, this one is stored in the
// local state, not the profile prefs.
const char kSearchEnginesChoiceProfile[] = "search_engines.choice_profile";

}  // namespace prefs
