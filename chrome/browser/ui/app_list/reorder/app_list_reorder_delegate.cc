// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/reorder/app_list_reorder_delegate.h"

#include "chrome/browser/ui/app_list/app_list_syncable_service.h"
#include "chrome/browser/ui/app_list/reorder/app_list_reorder_util.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace app_list {

namespace {

using SyncItem = AppListSyncableService::SyncItem;

// Indicate the status of a subsequence.
struct SubsequenceStatus {
  // The subsequence length.
  int length = 0;

  // Used to reconstruct the subsequence.
  absl::optional<int> prev_item;
};

// Sorts `wrappers` based on `order` and returns the longest subsequence in
// increasing ordinal order (a.k.a the longest increasing subsequence or "LIS").
template <typename T>
std::vector<int> SortAndGetLis(
    ash::AppListSortOrder order,
    std::vector<reorder::SyncItemWrapper<T>>* wrappers) {
  DCHECK(!wrappers->empty());
  std::sort(wrappers->begin(), wrappers->end(),
            [order](const reorder::SyncItemWrapper<T>& wrapper1,
                    const reorder::SyncItemWrapper<T>& wrapper2) {
              // Always put folders in front.
              if ((!wrapper1.is_folder && wrapper2.is_folder) ||
                  (wrapper1.is_folder && !wrapper2.is_folder)) {
                return wrapper1.is_folder;
              }

              bool comp = false;
              switch (order) {
                case ash::AppListSortOrder::kNameAlphabetical:
                  comp = wrapper1 < wrapper2;
                  break;
                case ash::AppListSortOrder::kNameReverseAlphabetical:
                  comp = wrapper1 > wrapper2;
                  break;
                case ash::AppListSortOrder::kEmpty:
                  NOTREACHED();
              }
              return comp;
            });

  // The remaining code is needed to find the longest increasing subsequence
  // (LIS) through dynamic programming. Denote the LIS ending with the j-th
  // element by s(j); denote the j-th element by elements(j). Then s(j) must be
  // the combination of s(i) and elements(j), where i < j and
  // elements(i).ordinal < elements(j).ordinal.

  // The maximum length of LIS found so far. Note that length of LIS is zero if
  // all of current ordinals are invalid.
  int maximum_length = 0;

  // Index of LIS's ending element.
  absl::optional<int> lis_end_index;

  // `status_array[i]` stores the status of the LIS which ends with the i-th
  // element of `wrappers`.
  std::vector<SubsequenceStatus> status_array(wrappers->size());

  for (int i = 0; i < status_array.size(); ++i) {
    const syncer::StringOrdinal& item_ordinal = (*wrappers)[i].item_ordinal;

    // The element with the invalid ordinal should not be included in the LIS.
    // Because the invalid ordinal has to be updated.
    if (!item_ordinal.IsValid())
      continue;

    // When an increasing subsequence can be combined with i-th wrapper to form
    // a new increasing subsequence, it is called "appendable".
    // `optimal_prev_index` is the index to the last element of the longest
    // appendable subsequence.
    absl::optional<int> optimal_prev_index;
    for (int prev_id_index = 0; prev_id_index < i; ++prev_id_index) {
      const syncer::StringOrdinal& prev_item_ordinal =
          (*wrappers)[prev_id_index].item_ordinal;

      // Nothing to do if LIS ending at the element corresponding to
      // `prev_id_index` is not appendable.
      if (!prev_item_ordinal.IsValid() ||
          !item_ordinal.GreaterThan(prev_item_ordinal)) {
        continue;
      }

      // Start a new appendable subsequence if none have yet been found.
      if (!optimal_prev_index) {
        optimal_prev_index = prev_id_index;
        continue;
      }

      // Update `optimal_prev_index` only when a longer appendable subsequence
      // is found.
      const int prev_subsequence_length = status_array[prev_id_index].length;
      const SubsequenceStatus& optimal_prev_subsequence_status =
          status_array[*optimal_prev_index];
      if (prev_subsequence_length > optimal_prev_subsequence_status.length)
        optimal_prev_index = prev_id_index;
    }

    // Update `status_array`.
    SubsequenceStatus& subsequence_status = status_array[i];
    if (optimal_prev_index) {
      subsequence_status.length = status_array[*optimal_prev_index].length + 1;
      subsequence_status.prev_item = optimal_prev_index;
    } else {
      subsequence_status.length = 1;
    }

    // If a longer increasing subsequence is found, record its length and its
    // last element.
    if (subsequence_status.length > maximum_length) {
      maximum_length = subsequence_status.length;
      lis_end_index = i;
    }
  }

  std::vector<int> lis;
  absl::optional<int> element_in_lis = lis_end_index;
  while (element_in_lis) {
    lis.push_back(*element_in_lis);
    element_in_lis = status_array[*element_in_lis].prev_item;
  }

  // Note that before reversal the elements in `lis` are in reverse order.
  // Although reversal isn't necessary, `lis` is reversed to make coding easier.
  std::reverse(lis.begin(), lis.end());

  return lis;
}

template <typename T>
void GenerateReorderParamsWithLis(
    const std::vector<reorder::SyncItemWrapper<T>>& wrappers,
    const std::vector<int>& lis,
    std::vector<reorder::ReorderParam>* reorder_params) {
  DCHECK(!wrappers.empty());

  // Handle the edge case that `lis` is empty, which means that all existing
  // ordinals are invalid and should be updated.
  if (lis.empty()) {
    for (int index = 0; index < wrappers.size(); ++index) {
      const syncer::StringOrdinal updated_ordinal =
          (index == 0 ? syncer::StringOrdinal::CreateInitialOrdinal()
                      : reorder_params->back().ordinal.CreateAfter());
      reorder_params->emplace_back(wrappers[index].id, updated_ordinal);
    }
    return;
  }

  // Indicate the ordinal of the previous item in the sorted list.
  absl::optional<syncer::StringOrdinal> prev_ordinal;

  // The index of the next item whose ordinal waits for update.
  int index_of_item_to_update = 0;

  // The index of the next element waiting for handling in `lis`.
  int index_of_lis_front_element = 0;

  while (index_of_item_to_update < wrappers.size()) {
    if (index_of_lis_front_element >= lis.size()) {
      // All elements in `lis` have been visited.

      // The case that `lis` is empty has been handled before the loop.
      // Therefore if `index_of_lis_front_element` has reached the end, the loop
      // iterates at least once.
      DCHECK_GT(index_of_item_to_update, 0);

      // Use a bigger ordinal to ensure the increasing order.
      syncer::StringOrdinal new_ordinal = prev_ordinal->CreateAfter();
      reorder_params->emplace_back(wrappers[index_of_item_to_update].id,
                                   new_ordinal);
      ++index_of_item_to_update;
      prev_ordinal = new_ordinal;
      continue;
    }

    const int lis_front = lis[index_of_lis_front_element];
    if (index_of_item_to_update < lis_front) {
      // The code below is for generating ordinals for the items mapped by the
      // indices among the range of [index_of_item_to_update, lis_front).

      // Use a stack to temporarily store the newly generated ordinals. It
      // helps to insert elements into `reorder_params` following the
      // increasing ordinal order, which makes debugging easier.
      std::stack<syncer::StringOrdinal> reversely_generated_ordinals;

      syncer::StringOrdinal upper_bound = wrappers[lis_front].item_ordinal;
      for (int i = lis_front - 1; i >= index_of_item_to_update; --i) {
        syncer::StringOrdinal new_ordinal =
            prev_ordinal ? prev_ordinal->CreateBetween(upper_bound)
                         : upper_bound.CreateBefore();
        reversely_generated_ordinals.push(new_ordinal);
        upper_bound = new_ordinal;
      }

      for (int i = index_of_item_to_update; i < lis_front; ++i) {
        reorder_params->emplace_back(wrappers[i].id,
                                     reversely_generated_ordinals.top());
        prev_ordinal = reversely_generated_ordinals.top();
        reversely_generated_ordinals.pop();
      }

      DCHECK(reversely_generated_ordinals.empty());
      index_of_item_to_update = lis_front;
      continue;
    }

    // Note that `index_of_item_to_update` cannot be greater than `lis_front`.
    // In addition, the case that `index_to_update` is smaller has been handled.
    // Therefore, `index_of_item_to_update` must be equal to `lis_front` here.
    CHECK_EQ(index_of_item_to_update, lis_front);

    // No need to update the items included in `lis`.
    prev_ordinal = wrappers[index_of_item_to_update].item_ordinal;
    ++index_of_lis_front_element;
    ++index_of_item_to_update;
  }
}

template <typename T>
std::vector<reorder::ReorderParam> GenerateReorderParamsImpl(
    ash::AppListSortOrder order,
    std::vector<reorder::SyncItemWrapper<T>>* wrappers) {
  std::vector<reorder::ReorderParam> reorder_params;
  GenerateReorderParamsWithLis(*wrappers, SortAndGetLis(order, wrappers),
                               &reorder_params);
  return reorder_params;
}

}  // namespace

// AppListReorderDelegate -------------------------------------------------

AppListReorderDelegate::AppListReorderDelegate(AppListSyncableService* service)
    : app_list_syncable_service_(service) {}

std::vector<reorder::ReorderParam>
AppListReorderDelegate::GenerateReorderParams(
    ash::AppListSortOrder order) const {
  const AppListSyncableService::SyncItemMap& sync_item_map =
      app_list_syncable_service_->sync_items();
  DCHECK_GT(sync_item_map.size(), 1);
  switch (order) {
    case ash::AppListSortOrder::kNameAlphabetical:
    case ash::AppListSortOrder::kNameReverseAlphabetical: {
      std::vector<reorder::SyncItemWrapper<std::string>> wrappers =
          reorder::GenerateStringWrappersFromSyncItems(sync_item_map);
      return GenerateReorderParamsImpl(order, &wrappers);
    }
    case ash::AppListSortOrder::kEmpty:
      NOTREACHED();
      return std::vector<reorder::ReorderParam>();
  }
}

}  // namespace app_list
