// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_CORE_SUGGESTED_SAVE_LOCATION_PROVIDER_H_
#define COMPONENTS_POWER_BOOKMARKS_CORE_SUGGESTED_SAVE_LOCATION_PROVIDER_H_

#include "base/time/time.h"
#include "url/gurl.h"

namespace power_bookmarks {

class SuggestedSaveLocationProvider {
 public:
  virtual ~SuggestedSaveLocationProvider() = default;

  // Gets a save suggestion based on the provided URL. The provided bookmark
  // node must either be a folder or nullptr (in the case of no suggestion).
  virtual const bookmarks::BookmarkNode* GetSuggestion(const GURL& url) = 0;

  // Gets the amount of time that a suggestion should back off for if it was
  // rejected by the user (usually this means observing a move event for the
  // saved node immediately following creation).
  virtual const base::TimeDelta GetBackoffTime() = 0;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_CORE_SUGGESTED_SAVE_LOCATION_PROVIDER_H_
