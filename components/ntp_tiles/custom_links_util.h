// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_NTP_TILES_CUSTOM_LINKS_UTIL_H_
#define COMPONENTS_NTP_TILES_CUSTOM_LINKS_UTIL_H_

#include <algorithm>
#include <vector>

#include "url/gurl.h"

namespace ntp_tiles::custom_links_util {

// Moves the specified link from its current index and inserts it at
// |new_pos|. Returns false and does nothing if |url| is invalid, |url| does not
// exist in the list, or |new_pos| is invalid/already the current index.
template <typename T>
bool ReorderLink(std::vector<T>& current_links,
                 std::optional<std::vector<T>>& previous_links,
                 const GURL& url,
                 size_t new_pos) {
  if (!url.is_valid() || new_pos < 0 || new_pos >= current_links.size()) {
    return false;
  }

  auto curr_it = std::ranges::find(current_links, url, &T::url);
  if (curr_it == current_links.end()) {
    return false;
  }

  auto new_it = current_links.begin() + new_pos;
  if (new_it == curr_it) {
    return false;
  }

  previous_links = current_links;

  // If the new position is to the left of the current position, left rotate the
  // range [new_pos, curr_pos] until the link is first.
  if (new_it < curr_it) {
    std::rotate(new_it, curr_it, curr_it + 1);
  }
  // If the new position is to the right, we only need to left rotate the range
  // [curr_pos, new_pos] once so that the link is last.
  else {
    std::rotate(curr_it, curr_it + 1, new_it + 1);
  }

  return true;
}

// Restores the previous state of the list of links. Used to undo the previous
// action (add, edit, delete, etc.). Returns false and does nothing if there is
// no previous state to restore.
template <typename T>
bool UndoAction(std::vector<T>& current_links,
                std::optional<std::vector<T>>& previous_links) {
  if (!previous_links.has_value()) {
    return false;
  }

  // Replace the current links with the previous state.
  current_links = *previous_links;
  previous_links = std::nullopt;
  return true;
}

/// Returns an iterator into |custom_links|.
template <typename T>
std::vector<T>::iterator FindLinkWithUrl(std::vector<T>& current_links,
                                         const GURL& url) {
  return std::ranges::find(current_links, url, &T::url);
}

}  // namespace ntp_tiles::custom_links_util

#endif  // COMPONENTS_NTP_TILES_CUSTOM_LINKS_UTIL_H_
