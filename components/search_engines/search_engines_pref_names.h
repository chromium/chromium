// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_PREF_NAMES_H_
#define COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_PREF_NAMES_H_

#include "build/build_config.h"

namespace prefs {

// The GUID of the locally saved default search provider. Note that this acts
// like a pointer to which synced search engine should be the default, rather
// than the prefs below which describe the locally saved default search provider
// details. This is ignored in the case of the default search provider being
// managed by policy. This pref is in the process of replacing
// `kSyncedDefaultSearchProviderGUID`.
inline constexpr char kDefaultSearchProviderGUID[] =
    "default_search_provider.guid";

// The GUID of the synced default search provider. Note that this acts like a
// pointer to which synced search engine should be the default, rather than the
// prefs below which describe the locally saved default search provider details
// (and are not synced). This is ignored in the case of the default search
// provider being managed by policy.
// This pref is in the process of being replaced by
// `kDefaultSearchProviderGUID`.
inline constexpr char kSyncedDefaultSearchProviderGUID[] =
    "default_search_provider.synced_guid";

// Epoch timestamp in seconds of when the user chose a search engine in
// the choice screen.
// The timestamp and the version indicate that the user has already made a
// search engine choice in the choice screen or in settings.
inline constexpr char kDefaultSearchProviderChoiceScreenCompletionTimestamp[] =
    "default_search_provider.choice_screen_completion_timestamp";

// Version of Chrome when the user chose a search engine, in the format
// "6.0.490.1".
// The timestamp and the version indicate that the user has already made a
// search engine choice in the choice screen or in settings.
inline constexpr char kDefaultSearchProviderChoiceScreenCompletionVersion[] =
    "default_search_provider.choice_screen_completion_version";

// Prepopulated id of the search engine chosen in a guest session if the user
// decides to propagate the default search engine to all guest sessions. The
// prepopulated id indicates that the search engine choice dialog should not be
// displayed in the next guest sessions and should be used to set the guest
// sessions default search engine.
// Defaults to 0;
inline constexpr char kDefaultSearchProviderGuestModePrepopulatedId[] =
    "default_search_provider.guest_mode_prepopulated_id";

// Display state of the choice screen from which the user selected their
// default search engine. It is stored for logging purposes, only for a limited
// time, and cleared when that time runs out, or when we are able to report
// the choice screen display state.
// The preference is stored as a dictionary, see
// `ChoiceScreenDisplayState::FromDict()`.
inline constexpr char kDefaultSearchProviderPendingChoiceScreenDisplayState[] =
    "default_search_provider.pending_choice_screen_display_state";

// Random number to use as a profile-constant seed for the random shuffling of
// the choice screen elements.
inline constexpr char kDefaultSearchProviderChoiceScreenRandomShuffleSeed[] =
    "default_search_provider.choice_screen_random_shuffle_seed";

// The Chrome milestone number at which the random seed was last set.
inline constexpr char kDefaultSearchProviderChoiceScreenShuffleMilestone[] =
    "default_search_provider.choice_screen_shuffle_milestone";

// Whether a search context menu item is allowed.
inline constexpr char kDefaultSearchProviderContextMenuAccessAllowed[] =
    "default_search_provider.context_menu_access_allowed";

// Whether the prepopulated data from which the keywords were loaded is the
// extended list that is not limited to just 5 engines.
// This pref helps versioning the keyword data in an orthogonal way from the
// prepopulated data version numbers, as this is dependent on runtime feature
// state.
// TODO(b/304947278): Deprecate when the SearchEngineChoice feature launches.
inline constexpr char kDefaultSearchProviderKeywordsUseExtendedList[] =
    "default_search_provider.keywords_use_extended_list";

// Whether having a default search provider is enabled.
inline constexpr char kDefaultSearchProviderEnabled[] =
    "default_search_provider.enabled";

// The dictionary key used when the default search providers are given
// in the preferences file. Normally they are copied from the main
// preferences file.
inline constexpr char kSearchProviderOverrides[] = "search_provider_overrides";
// The format version for the dictionary above.
inline constexpr char kSearchProviderOverridesVersion[] =
    "search_provider_overrides_version";

// String that refers to the study group in which this install was enrolled.
// Used to implement the first run experiment tracking.
// NOTE: Unlike most of the other preferences here, this one is stored in the
// local state, not the profile prefs.
// TODO(b/313067383): Clean up experiment setup.
inline constexpr char kSearchEnginesStudyGroup[] =
    "search_engines.client_side_study_group";

#if BUILDFLAG(IS_IOS)
// Number of time the search engine choice screen was skipped because the app
// was started via an external intent.
inline constexpr char kDefaultSearchProviderChoiceScreenSkippedCount[] =
    "default_search_provider.skip_count";
#endif

}  // namespace prefs

#endif  // COMPONENTS_SEARCH_ENGINES_SEARCH_ENGINES_PREF_NAMES_H_
