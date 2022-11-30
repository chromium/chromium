// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_FAVICON_SQL_HANDLER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_FAVICON_SQL_HANDLER_H_

#include "base/memory/raw_ptr.h"
#include "components/history/core/browser/android/sql_handler.h"

namespace favicon {
class FaviconDatabase;
}

namespace history {

// The SQL handler implementation for icon_mapping and favicon table.
class FaviconSQLHandler : public SQLHandler {
 public:
  explicit FaviconSQLHandler(favicon::FaviconDatabase* favicon_db);

  FaviconSQLHandler(const FaviconSQLHandler&) = delete;
  FaviconSQLHandler& operator=(const FaviconSQLHandler&) = delete;

  ~FaviconSQLHandler() override;

  // SQLHandler overrides:
  bool Update(const HistoryAndBookmarkRow& row,
              const TableIDRows& ids_set) override;
  bool Delete(const TableIDRows& ids_set) override;
  bool Insert(HistoryAndBookmarkRow* row) override;

 private:
  // Deletes the given favicons if they are not used by any pages. Returns
  // true if all unused favicons are deleted.
  bool DeleteUnusedFavicon(const std::vector<favicon_base::FaviconID>& ids);

  raw_ptr<favicon::FaviconDatabase> favicon_db_;
};

}  // namespace history.

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_FAVICON_SQL_HANDLER_H_
