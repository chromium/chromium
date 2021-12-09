// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/reorder/app_list_reorder_core.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ui/app_list/app_list_model_updater.h"
#include "chrome/browser/ui/app_list/chrome_app_list_item.h"

namespace app_list {
namespace reorder {
namespace {

using SyncItem = AppListSyncableService::SyncItem;

// Indicate the status of a subsequence.
struct SubsequenceStatus {
  // The subsequence length.
  int length = 0;

  // Used to reconstruct the subsequence.
  absl::optional<int> prev_item;
};

// Returns true if `order` is increasing.
bool IsIncreasingOrder(ash::AppListSortOrder order) {
  switch (order) {
    case ash::AppListSortOrder::kCustom:
      NOTREACHED();
      return false;
    case ash::AppListSortOrder::kNameAlphabetical:
      return true;
    case ash::AppListSortOrder::kNameReverseAlphabetical:
      return false;
    case ash::AppListSortOrder::kColor:
      return true;
  }
}

// Sorts `wrappers` based on `order` and returns the longest subsequence in
// increasing ordinal order (a.k.a the longest increasing subsequence or
// "LIS"). Each element in LIS is an index of an element in `wrappers`.
template <typename T>
std::vector<int> SortAndGetLis(
    ash::AppListSortOrder order,
    std::vector<reorder::SyncItemWrapper<T>>* wrappers) {
  DCHECK(!wrappers->empty());
  std::sort(wrappers->begin(), wrappers->end(),
            [order](const reorder::SyncItemWrapper<T>& wrapper1,
                    const reorder::SyncItemWrapper<T>& wrapper2) {
              // Always put folders in front.
              // TODO(https://crbug.com/1261673): remove this code block when
              // `SyncItemWrapper` comparison operators take item type into
              // consideration.
              // TODO(crbug.com/1270898): Color sort should place folders at the
              // end of the app list.
              if (wrapper1.is_folder != wrapper2.is_folder)
                return wrapper1.is_folder;

              bool comp = false;
              if (IsIncreasingOrder(order)) {
                comp = wrapper1 < wrapper2;
              } else {
                comp = wrapper1 > wrapper2;
              }
              return comp;
            });

  // The remaining code is needed to find the longest increasing subsequence
  // (LIS) through dynamic programming. Denote the LIS ending with the j-th
  // element by s(j); denote the j-th element by elements(j). Then s(j) must
  // be the combination of s(i) and elements(j), where i < j and
  // elements(i).ordinal < elements(j).ordinal.

  // The maximum length of LIS found so far. Note that length of LIS is zero
  // if all of current ordinals are invalid.
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

    // When an increasing subsequence can be combined with i-th wrapper to
    // form a new increasing subsequence, it is called "appendable".
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
  // Although reversal isn't necessary, `lis` is reversed to make coding
  // easier.
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
      // Therefore if `index_of_lis_front_element` has reached the end, the
      // loop iterates at least once.
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
    // In addition, the case that `index_to_update` is smaller has been
    // handled. Therefore, `index_of_item_to_update` must be equal to
    // `lis_front` here.
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

// Calculates the entropy (i.e. the ratio of the out-of-order item number to
// the total number) of `items` based on the specified order. Fill
// `sorted_subsequence` if `sorted_subsequence` is not null.
// Note that `items` should not be empty.
template <typename T>
void CalculateEntropyAndGetSortedSubsequence(
    ash::AppListSortOrder order,
    std::vector<reorder::SyncItemWrapper<T>>* items,
    float* entropy,
    std::vector<reorder::SyncItemWrapper<T>>* sorted_subsequence) {
  DCHECK(!items->empty());

  std::vector<int> lis = SortAndGetLis(order, items);
  const int total_item_count = items->size();
  *entropy = (total_item_count - lis.size()) / float(total_item_count);

  if (!sorted_subsequence)
    return;

  DCHECK(sorted_subsequence->empty());
  for (const int& index : lis)
    sorted_subsequence->push_back(items->at(index));
}

// Calculate neighbors' locations so that if the new item is inserted between
// neighbors then `sorted_subsequence` after insertion still keeps `order`. Pass
// the results through parameters. If the new item should be placed at the end,
// either `prev` or `next` is empty. Note that `prev` and `next` are calculated
// based on the local items (i.e. the app list items of the device where a new
// item is added). Local items are contrast to global items which include all
// items from all sync devices.
template <typename T>
void CalculateNeighbors(
    ash::AppListSortOrder order,
    const T& new_item_key_attribute,
    const std::vector<reorder::SyncItemWrapper<T>>& sorted_subsequence,
    syncer::StringOrdinal* prev,
    syncer::StringOrdinal* next) {
  DCHECK(prev && !prev->IsValid());
  DCHECK(next && !next->IsValid());

  // Find the first local item that is not a folder. Recall that
  // `sorted_subsequence` guarantees:
  // (1) Folder items are always placed in front of non-folder items.
  // (2) The items of the same category follow `order`.
  DCHECK(!sorted_subsequence.empty());
  auto first_non_folder_iter = std::find_if(
      sorted_subsequence.cbegin(), sorted_subsequence.cend(),
      [](const reorder::SyncItemWrapper<T>& item) { return !item.is_folder; });

  // Handle the case that all items are folders. Note that the new item is
  // assumed not to be a folder. TODO(https://crbug.com/1260875): handle the
  // case that the new item is a folder.
  if (first_non_folder_iter == sorted_subsequence.cend()) {
    *prev = sorted_subsequence.back().item_ordinal;
    return;
  }

  const bool is_increasing = IsIncreasingOrder(order);

  // Find the non-folder item that should be placed right after the new item
  // when the new item is added.
  auto lower_bound = std::lower_bound(
      first_non_folder_iter, sorted_subsequence.cend(), new_item_key_attribute,
      [&is_increasing](const reorder::SyncItemWrapper<T>& item,
                       const std::string& val) {
        return is_increasing ? item.key_attribute < val
                             : item.key_attribute > val;
      });

  // Handle the case that `item` should be placed in front of all non-folder
  // items.
  if (lower_bound == first_non_folder_iter) {
    *next = first_non_folder_iter->item_ordinal;
    if (first_non_folder_iter != sorted_subsequence.cbegin()) {
      // If there are folders, the new item should be placed between the last
      // folder item and the first non-folder item
      *prev = std::prev(first_non_folder_iter)->item_ordinal;
    }
    return;
  }

  // Handle the case that `item` should be placed behind all non-folder items.
  if (lower_bound == sorted_subsequence.cend()) {
    *prev = sorted_subsequence.back().item_ordinal;
    return;
  }

  // The only scenario left is that the new item should be placed between two
  // non-folder items.
  *prev = std::prev(lower_bound)->item_ordinal;
  *next = lower_bound->item_ordinal;
}

// Adjusts `prev` and `prev` in global scope so that the sorting order is kept
// on all sync devices after placing `new_item` between adjusted neighbors.
template <typename T>
void AdjustNeighborsInGlobalScope(
    ash::AppListSortOrder order,
    const reorder::SyncItemWrapper<T>& new_item,
    const std::vector<reorder::SyncItemWrapper<T>>& global_items,
    syncer::StringOrdinal* prev,
    syncer::StringOrdinal* next) {
  // Before adjustment, `prev` and `next` are the new item's local neighbor
  // positions (see CalculateNeighbors() for more details). Recall that
  // different sync devices may have different sets of apps. This method checks
  // the existing sync items whose positions are between `prev` and `next` so as
  // to get the correct position in global scope.
  DCHECK(prev);
  DCHECK(next);

  // The left neighbor in the global scope.
  syncer::StringOrdinal global_prev;
  if (prev->IsValid())
    global_prev = *prev;

  // The right neighbor in the global scope.
  syncer::StringOrdinal global_next;
  if (next->IsValid())
    global_next = *next;

  const bool is_increasing = IsIncreasingOrder(order);
  for (const auto& item : global_items) {
    const syncer::StringOrdinal& position = item.item_ordinal;
    if (!position.IsValid())
      continue;

    // Skip the loop iteration if `position` is not in the range of
    // (global_prev, global_next) because it cannot shrink the range.
    if ((global_prev.IsValid() && !position.GreaterThan(global_prev)) ||
        (global_next.IsValid() && !position.LessThan(global_next))) {
      continue;
    }

    // TODO(https://crbug.com/1261673): clean this code block when
    // `SyncItemWrapper` comparison operators take item type into consideration.
    const bool is_item_greater =
        ((item.is_folder == new_item.is_folder &&
          item.key_attribute > new_item.key_attribute) ||
         (!item.is_folder && new_item.is_folder));
    const bool is_equal = (item.is_folder == new_item.is_folder &&
                           item.key_attribute == new_item.key_attribute);

    if (is_equal || is_increasing == is_item_greater) {
      // Handle the case that the new item should be placed in front of `item`.
      // Note that if `item` is equal to `new_item`, `item` is always placed
      // after `new_item` to keep the consistency with `CalculateNeighbors()`.
      global_next = position;
    } else {
      // Handle the case that the new item should be placed after `item`.
      global_prev = position;
    }
  }

  // Store results.
  if (global_prev.IsValid())
    *prev = global_prev;

  if (global_next.IsValid())
    *next = global_next;
}

syncer::StringOrdinal CalculatePositionBetweenNeighbors(
    const syncer::StringOrdinal& prev,
    const syncer::StringOrdinal& next) {
  if (!prev.IsValid() && !next.IsValid()) {
    // Not sure whether this case really exists. Handle it for satefy.
    return syncer::StringOrdinal().CreateInitialOrdinal();
  }

  if (prev.IsValid() && next.IsValid()) {
    // Both left neighbor and right neighbor are valid. Return a position that
    // is between `prev` and `next`.
    return prev.CreateBetween(next);
  }

  if (prev.IsValid()) {
    // Only `prev` is valid. Return a position that is after `prev`.
    return prev.CreateAfter();
  }

  // Only `next` is valid. Return a position that is before `next`.
  return next.CreateBefore();
}

// Similar to `CalculateNewItemPosition()` but `order` is either
// kNameAlphabetical or kNameReverseAlphabetical. Read the comment of
// `CalculateNewItemPosition()` for parameters' meanings.
bool CalculatePositionInNameOrder(
    ash::AppListSortOrder order,
    const ChromeAppListItem& new_item,
    const std::vector<const ChromeAppListItem*>& local_items,
    const AppListSyncableService::SyncItemMap* global_items,
    syncer::StringOrdinal* target_position) {
  DCHECK(order == ash::AppListSortOrder::kNameAlphabetical ||
         order == ash::AppListSortOrder::kNameReverseAlphabetical);

  std::vector<reorder::SyncItemWrapper<std::string>> local_item_wrappers =
      reorder::GenerateWrappersFromAppListItems<std::string>(local_items);

  if (local_item_wrappers.empty()) {
    *target_position = syncer::StringOrdinal::CreateInitialOrdinal();
    return true;
  }

  std::vector<reorder::SyncItemWrapper<std::string>> sorted_subsequence;
  float entropy;
  CalculateEntropyAndGetSortedSubsequence(order, &local_item_wrappers, &entropy,
                                          &sorted_subsequence);

  if (entropy > reorder::kOrderResetThreshold) {
    // Do not set `target_position` if entropy is too high.
    return false;
  }

  syncer::StringOrdinal prev_neighbor;
  syncer::StringOrdinal next_neighbor;
  CalculateNeighbors(order, new_item.name(), sorted_subsequence, &prev_neighbor,
                     &next_neighbor);

  if (global_items) {
    AdjustNeighborsInGlobalScope(
        order, reorder::SyncItemWrapper<std::string>(new_item),
        reorder::GenerateWrappersFromSyncItems<std::string>(*global_items),
        &prev_neighbor, &next_neighbor);
  }

  *target_position =
      CalculatePositionBetweenNeighbors(prev_neighbor, next_neighbor);
  return true;
}

}  // namespace

std::vector<reorder::ReorderParam> GenerateReorderParamsForSyncItems(
    ash::AppListSortOrder order,
    const AppListSyncableService::SyncItemMap& sync_item_map) {
  DCHECK_GT(sync_item_map.size(), 1);
  switch (order) {
    case ash::AppListSortOrder::kNameAlphabetical:
    case ash::AppListSortOrder::kNameReverseAlphabetical: {
      std::vector<reorder::SyncItemWrapper<std::string>> wrappers =
          reorder::GenerateWrappersFromSyncItems<std::string>(sync_item_map);
      return GenerateReorderParamsImpl(order, &wrappers);
    }
    case ash::AppListSortOrder::kColor: {
      std::vector<reorder::SyncItemWrapper<ash::IconColor>> wrappers =
          reorder::GenerateWrappersFromSyncItems<ash::IconColor>(sync_item_map);
      return GenerateReorderParamsImpl(order, &wrappers);
    }
    case ash::AppListSortOrder::kCustom:
      NOTREACHED();
      return std::vector<reorder::ReorderParam>();
  }
}

std::vector<reorder::ReorderParam> GenerateReorderParamsForAppListItems(
    ash::AppListSortOrder order,
    const std::vector<const ChromeAppListItem*>& app_list_items) {
  DCHECK_GT(app_list_items.size(), 1);
  switch (order) {
    case ash::AppListSortOrder::kNameAlphabetical:
    case ash::AppListSortOrder::kNameReverseAlphabetical: {
      std::vector<reorder::SyncItemWrapper<std::string>> wrappers =
          reorder::GenerateWrappersFromAppListItems<std::string>(
              app_list_items);
      return GenerateReorderParamsImpl(order, &wrappers);
    }
    case ash::AppListSortOrder::kColor: {
      std::vector<reorder::SyncItemWrapper<ash::IconColor>> wrappers =
          reorder::GenerateWrappersFromAppListItems<ash::IconColor>(
              app_list_items);
      return GenerateReorderParamsImpl(order, &wrappers);
    }
    case ash::AppListSortOrder::kCustom:
      NOTREACHED();
      return std::vector<reorder::ReorderParam>();
  }
}

bool CalculateNewItemPosition(
    ash::AppListSortOrder order,
    const ChromeAppListItem& new_item,
    const std::vector<const ChromeAppListItem*>& local_items,
    const AppListSyncableService::SyncItemMap* global_items,
    syncer::StringOrdinal* target_position) {
  // TODO(https://crbug.com/1260875): handle the case that `new_item` is a
  // folder.
  DCHECK(!new_item.is_folder());

  switch (order) {
    case ash::AppListSortOrder::kCustom:
      // TODO(crbug.com/1270898): Color sort should handle placing a new item
      // similar to the way that is done for name sorting.
    case ash::AppListSortOrder::kColor:
      // Insert `item` at the front when the sort order is kCustom.
      DCHECK(global_items);
      *target_position = CalculateFrontPosition(*global_items);
      return true;
    case ash::AppListSortOrder::kNameAlphabetical:
    case ash::AppListSortOrder::kNameReverseAlphabetical:
      return CalculatePositionInNameOrder(order, new_item, local_items,
                                          global_items, target_position);
  }
}

syncer::StringOrdinal CalculateFrontPosition(
    const AppListSyncableService::SyncItemMap& sync_item_map) {
  syncer::StringOrdinal minimum_valid_ordinal;
  for (auto iter = sync_item_map.cbegin(); iter != sync_item_map.cend();
       ++iter) {
    const syncer::StringOrdinal& ordinal = iter->second->item_ordinal;

    // `ordinal` may be invalid (especially in tests).
    if (!ordinal.IsValid())
      continue;

    if (!minimum_valid_ordinal.IsValid() ||
        ordinal.LessThan(minimum_valid_ordinal)) {
      minimum_valid_ordinal = ordinal;
    }
  }

  if (minimum_valid_ordinal.IsValid())
    return minimum_valid_ordinal.CreateBefore();

  return syncer::StringOrdinal::CreateInitialOrdinal();
}

float CalculateEntropyForTest(ash::AppListSortOrder order,
                              AppListModelUpdater* model_updater) {
  switch (order) {
    case ash::AppListSortOrder::kCustom:
    case ash::AppListSortOrder::kColor:
      NOTREACHED();
      return 0.f;
    case ash::AppListSortOrder::kNameAlphabetical:
    case ash::AppListSortOrder::kNameReverseAlphabetical:
      std::vector<reorder::SyncItemWrapper<std::string>> local_item_wrappers =
          reorder::GenerateWrappersFromAppListItems<std::string>(
              model_updater->GetItems());
      float entropy = 0.f;
      CalculateEntropyAndGetSortedSubsequence(
          order, &local_item_wrappers, &entropy,
          static_cast<std::vector<reorder::SyncItemWrapper<std::string>>*>(
              nullptr));
      return entropy;
  }
}

}  // namespace reorder
}  // namespace app_list
