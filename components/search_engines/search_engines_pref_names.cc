// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/search_engines/search_engines_pref_names.h"

namespace prefs {

// The GUID of the synced default search provider. Note that this acts like a
// pointer to which synced search engine should be the default, rather than the
// prefs below which describe the locally saved default search provider details
// (and are not synced). This is ignored in the case of the default search
// provider being managed by policy.
const char kSyncedDefaultSearchProviderGUID[] =
    "default_search_provider.synced_guid";

// Whether a search context menu item is allowed.
const char kDefaultSearchProviderContextMenuAccessAllowed[] =
    "default_search_provider.context_menu_access_allowed";

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

}  // namespace prefs
