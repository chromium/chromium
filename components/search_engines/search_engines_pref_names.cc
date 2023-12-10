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

// Whether this profile should potentially show the search engine choice
// dialog before the user can proceed. Actual eligiblity is still determined
// by the `SearchEngineChoiceService`.
// Note that this has effect only if the `kSearchEngineChoiceTrigger` feature
// is enabled and if its `kSearchEngineChoiceTriggerForTaggedProfilesOnly`
// param is set to `true`.
const char kDefaultSearchProviderChoicePending[] =
    "default_search_provider.choice_pending";

// Epoch timestamp in seconds of when the user chose a search engine in
// the choice screen.
// The timestamp and the version indicate that the user has already made a
// search engine choice in the choice screen or in settings.
const char kDefaultSearchProviderChoiceScreenCompletionTimestamp[] =
    "default_search_provider.choice_screen_completion_timestamp";

// Version of Chrome when the user chose a search engine, in the format
// "6.0.490.1".
// The timestamp and the version indicate that the user has already made a
// search engine choice in the choice screen or in settings.
const char kDefaultSearchProviderChoiceScreenCompletionVersion[] =
    "default_search_provider.choice_screen_completion_version";

// Random number to use as a profile-constant seed for the random shuffling of
// the choice screen elements.
const char kDefaultSearchProviderChoiceScreenRandomShuffleSeed[] =
    "default_search_provider.choice_screen_random_shuffle_seed";

// The Chrome milestone number at which the random seed was last set.
const char kDefaultSearchProviderChoiceScreenShuffleMilestone[] =
    "default_search_provider.choice_screen_shuffle_milestone";

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

// String that refers to the study group in which this install was enrolled.
// Used to implement the first run experiment tracking.
// NOTE: Unlike most of the other preferences here, this one is stored in the
// local state, not the profile prefs.
// TODO(b/313067383): Clean up experiment setup.
const char kSearchEnginesStudyGroup[] =
    "search_engines.client_side_study_group";

}  // namespace prefs
