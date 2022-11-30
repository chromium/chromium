// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTER_IMPORTED_BOOKMARK_ENTRY_H_
#define CHROME_COMMON_IMPORTER_IMPORTED_BOOKMARK_ENTRY_H_

#include <string>
#include <vector>

#include "base/time/time.h"
#include "url/gurl.h"

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
};

#endif  // CHROME_COMMON_IMPORTER_IMPORTED_BOOKMARK_ENTRY_H_
