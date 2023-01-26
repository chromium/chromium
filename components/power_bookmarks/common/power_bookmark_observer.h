// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_COMMON_POWER_BOOKMARK_OBSERVER_H_
#define COMPONENTS_POWER_BOOKMARKS_COMMON_POWER_BOOKMARK_OBSERVER_H_

namespace power_bookmarks {

// Observer class for any changes to powers.
class PowerBookmarkObserver {
 public:
  virtual ~PowerBookmarkObserver() = default;

  // Called whenever there are changes to Powers.
  virtual void OnPowersChanged() = 0;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_COMMON_POWER_BOOKMARK_OBSERVER_H_