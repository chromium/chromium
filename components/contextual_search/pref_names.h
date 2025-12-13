// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CONTEXTUAL_SEARCH_PREF_NAMES_H_
#define COMPONENTS_CONTEXTUAL_SEARCH_PREF_NAMES_H_

namespace contextual_search {

// The possible values for the Search Context Sharing policy. The value is an
// integer rather than a boolean to allow for additional states to be added in
// the future.
enum class SearchContentSharingSettingsValue {
  kEnabled = 0,
  kDisabled = 1,
};

// Integer that specifies whether the search backend is able to access page and
// file content for contextual search.
inline constexpr char kSearchContentSharingSettings[] =
    "contextual_search.search_content_sharing_settings";

}  // namespace contextual_search

#endif  // COMPONENTS_CONTEXTUAL_SEARCH_PREF_NAMES_H_
