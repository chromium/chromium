// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_IMPORTER_IMPORTER_URL_ROW_H_
#define CHROME_COMMON_IMPORTER_IMPORTER_URL_ROW_H_

#include <string>

#include "base/time/time.h"
#include "url/gurl.h"

// Used as the target for importing history URLs from other browser's profiles
// in the utility process. Converted to history::URLRow after being passed via
// IPC to the browser.
struct ImporterURLRow {
 public:
  ImporterURLRow();
  explicit ImporterURLRow(const GURL& url);
  ImporterURLRow(const ImporterURLRow&);
  ImporterURLRow& operator=(const ImporterURLRow&);

  GURL url;
  std::u16string title;

  // Total number of times this URL has been visited.
  int visit_count;

  // Number of times this URL has been manually entered in the URL bar.
  int typed_count;

  // The date of the last visit of this URL, which saves us from having to
  // loop up in the visit table for things like autocomplete and expiration.
  base::Time last_visit;

  // Indicates this entry should now be shown in typical UI or queries, this
  // is usually for subframes.
  bool hidden;
};

#endif  // CHROME_COMMON_IMPORTER_IMPORTER_URL_ROW_H_
