// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_USER_DATA_IMPORTER_COMMON_IMPORTED_BOOKMARK_ENTRY_H_
#define COMPONENTS_USER_DATA_IMPORTER_COMMON_IMPORTED_BOOKMARK_ENTRY_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "base/uuid.h"
#include "url/gurl.h"

namespace user_data_importer {

struct ImportedBookmarkEntry {
  ImportedBookmarkEntry();
  ImportedBookmarkEntry(const ImportedBookmarkEntry& other);
  ~ImportedBookmarkEntry();

  bool operator==(const ImportedBookmarkEntry& other) const;

  bool in_toolbar;
  bool is_folder;
  GURL url;
  std::vector<std::u16string> path;
  std::u16string title;
  base::Time creation_time;
  std::optional<base::Time> last_visit_time;
  std::optional<base::Uuid> uuid;
  std::optional<bool> synced;
};

}  // namespace user_data_importer

#endif  // COMPONENTS_USER_DATA_IMPORTER_COMMON_IMPORTED_BOOKMARK_ENTRY_H_
