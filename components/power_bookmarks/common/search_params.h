// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POWER_BOOKMARKS_COMMON_SEARCH_PARAMS_H_
#define COMPONENTS_POWER_BOOKMARKS_COMMON_SEARCH_PARAMS_H_

#include <string>
#include "components/sync/protocol/power_bookmark_specifics.pb.h"

namespace power_bookmarks {

// Parameters for searching power bookmarks. Used as a parameter for
// PowerBookmarkService::Search()
struct SearchParams {
  // Specifies a plain text query that will be matched against the contents of
  // powers. The exact semantics of matching depend on the power type.
  //
  // TODO(crbug.com/40243263): add an option to *not* look for matches in the
  // URL string.
  std::string query;

  // Unless equal to POWER_TYPE_UNSPECIFIED, narrows the search to a single
  // power type.
  sync_pb::PowerBookmarkSpecifics::PowerType power_type;

  // Whether the search is case sensitive.
  bool case_sensitive = false;
};

}  // namespace power_bookmarks

#endif  // COMPONENTS_POWER_BOOKMARKS_COMMON_SEARCH_PARAMS_H_
