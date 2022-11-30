// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/safe_browsing/content/browser/safe_browsing_navigation_observer_manager_util.h"

namespace safe_browsing {

void MaybeRemoveNonUserGestureReferrerEntries(ReferrerChain* referrer_chain,
                                              int max_allowed_length) {
  int extra_entries = referrer_chain->size() - max_allowed_length;
  if (extra_entries > 0) {
    RemoveNonUserGestureReferrerEntries(referrer_chain, max_allowed_length);
  }
}

void RemoveNonUserGestureReferrerEntries(ReferrerChain* referrer_chain_entries,
                                         int max_allowed_length) {
  std::vector<int> user_gesture_indices =
      GetUserGestureNavigationEntriesIndices(referrer_chain_entries);

  // "Middle indices" means the entries adjacent to this entry are both non user
  // gesture. "Non middle indices" means there is at least one adjacent entry
  // that is non user gesture. The first and the last entries are non middle.
  std::vector<int> middle_indices;
  std::vector<int> non_middle_indices;
  size_t entry_index = 0;
  size_t user_gesture_index = 0;
  for (ReferrerChainEntry& entry : *referrer_chain_entries) {
    if (entry.navigation_initiation() !=
        ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE) {
      entry_index++;
      continue;
    }
    // If all the user gesture indices have been gone through, set the value to
    // -2 because the user gesture index is checked +-1. This works because the
    // index values in |user_gesture_indices| are in order from smallest to
    // greatest.
    size_t current_user_gesture_index_value =
        (user_gesture_index < user_gesture_indices.size())
            ? user_gesture_indices.at(user_gesture_index)
            : -2;
    // An index is considered a middle index if it is not next to a user gesture
    // index.
    if ((current_user_gesture_index_value == entry_index) ||
        (entry_index == (current_user_gesture_index_value + 1u)) ||
        (entry_index == (current_user_gesture_index_value - 1u))) {
      non_middle_indices.push_back(entry_index);
      // If the current index is right after a user gesture index, move the user
      // gesture index to the next index in the |user_gesture_indices|.
      if (entry_index == (current_user_gesture_index_value + 1u)) {
        user_gesture_index++;
      }
    } else {
      middle_indices.push_back(entry_index);
    }
    entry_index++;
  }

  size_t extra_entries = referrer_chain_entries->size() - max_allowed_length;
  int middle_omit_entries = middle_indices.size() < extra_entries
                                ? middle_indices.size()
                                : extra_entries;
  extra_entries = extra_entries - middle_omit_entries;
  int non_middle_omit_entries = non_middle_indices.size() < extra_entries
                                    ? non_middle_indices.size()
                                    : extra_entries;
  RemoveExtraIndicesInReferrerChain(referrer_chain_entries, middle_indices,
                                    middle_omit_entries);

  RemoveExtraIndicesInReferrerChain(referrer_chain_entries, non_middle_indices,
                                    non_middle_omit_entries);
}

std::vector<int> GetUserGestureNavigationEntriesIndices(
    ReferrerChain* referrer_chain) {
  std::vector<int> indices;
  int index = 0;
  for (ReferrerChainEntry& entry : *referrer_chain) {
    if (entry.navigation_initiation() !=
        ReferrerChainEntry::RENDERER_INITIATED_WITHOUT_USER_GESTURE) {
      indices.push_back(index);
    }
    index++;
  }
  return indices;
}

void RemoveExtraIndicesInReferrerChain(ReferrerChain* referrer_chain_entries,
                                       std::vector<int> indices_to_remove,
                                       int extra_entries) {
  if (extra_entries == 0) {
    return;
  }
  size_t beginning_index = indices_to_remove.size() / 2 - extra_entries / 2;
  if (beginning_index < 0) {
    beginning_index = 0;
  }
  size_t end_index = beginning_index + (extra_entries - 1);
  // There could be more |extra_entries| than the length of the
  // |indices_to_remove|.
  if (end_index > indices_to_remove.size()) {
    end_index = indices_to_remove.size() - 1;
  }
  ReferrerChain entries_copy;
  int removal_index;
  int index = 0;
  for (ReferrerChainEntry& entry : *referrer_chain_entries) {
    if ((beginning_index <= end_index)) {
      removal_index = indices_to_remove.at(beginning_index);
    }
    if (index == removal_index) {
      // Replace with empty referrer_chain_entry.
      std::unique_ptr<ReferrerChainEntry> referrer_chain_entry =
          std::make_unique<ReferrerChainEntry>();
      entries_copy.Add()->Swap(referrer_chain_entry.get());
      beginning_index++;
    } else {
      entries_copy.Add()->Swap(&entry);
    }
    index++;
  }
  referrer_chain_entries->Swap(&entries_copy);
}

}  // namespace safe_browsing
