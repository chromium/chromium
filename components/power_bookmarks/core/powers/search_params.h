// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_CORE_POWERS_SEARCH_PARAMS_H_
#define COMPONENTS_POWER_BOOKMARKS_CORE_POWERS_SEARCH_PARAMS_H_

#include <string>

namespace power_bookmarks {

// Parameters for searching power bookmarks. Used as a parameter for
// PowerBookmarkService::Search()
struct SearchParams {
  // Specifies a plain text query that will be matched against the contents of
  // powers. The exact semantics of matching depend on the power type.
  std::string query;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_CORE_POWERS_SEARCH_PARAMS_H_
