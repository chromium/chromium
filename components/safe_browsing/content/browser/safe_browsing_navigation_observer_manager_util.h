// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_UTIL_H_
#define COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_UTIL_H_

#include "components/safe_browsing/core/browser/referrer_chain_provider.h"
#include "third_party/protobuf/src/google/protobuf/repeated_field.h"

namespace safe_browsing {

// Replace indices with empty entries extra non user gesture referrer entries if
// the referrer chain is greater than the |max_allowed_length|.
void MaybeRemoveNonUserGestureReferrerEntries(ReferrerChain* referrer_chain,
                                              int max_allowed_length);

// Replace indices with empty entries middle non user gesture indices until the
// entries of |referrer chain entries| meets the requirement of
// |max_allowed_length|. Middle non user gesture indices are prioritized to be
// removed. Non-middle indices non user gesture indices can also be removed when
// the middle indices are not sufficient enough to remove.
void RemoveNonUserGestureReferrerEntries(ReferrerChain* referrer_chain_entries,
                                         int max_allowed_length);

// Get all indices of user gesture navigation entries in |referrer_chain|.
// Client redirects are also considered non user gestured entries.
std::vector<int> GetUserGestureNavigationEntriesIndices(
    ReferrerChain* referrer_chain);

// Replace indices with empty entries from |indices_to_remove| up to the number
// of |extra_entries| from |referrer_chain_entries|. If the length of
// |indices_to_remove| is smaller than |extra_entries|, all of the entries in
// |indices_to_remove| will be emptied. Middle indices will be emptied first
// when the length of |indices_to_remove| is larger than |extra_entries|.
void RemoveExtraIndicesInReferrerChain(ReferrerChain* referrer_chain_entries,
                                       std::vector<int> indices_to_remove,
                                       int extra_entries);
}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_CONTENT_BROWSER_SAFE_BROWSING_NAVIGATION_OBSERVER_MANAGER_UTIL_H_
