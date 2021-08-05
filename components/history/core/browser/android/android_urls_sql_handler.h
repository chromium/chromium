// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_ANDROID_URLS_SQL_HANDLER_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_ANDROID_URLS_SQL_HANDLER_H_

#include "base/macros.h"
#include "components/history/core/browser/android/sql_handler.h"

namespace history {

class AndroidURLsDatabase;

// The SQLHanlder implementation for android_urls table.
class AndroidURLsSQLHandler : public SQLHandler {
 public:
  explicit AndroidURLsSQLHandler(AndroidURLsDatabase* android_urls_db);
  ~AndroidURLsSQLHandler() override;

  bool Update(const HistoryAndBookmarkRow& row,
              const TableIDRows& ids_set) override;

  bool Insert(HistoryAndBookmarkRow* row) override;

  bool Delete(const TableIDRows& ids_set) override;

 private:
  AndroidURLsDatabase* android_urls_db_;

  DISALLOW_COPY_AND_ASSIGN(AndroidURLsSQLHandler);
};

}  // namespace history.

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_ANDROID_ANDROID_URLS_SQL_HANDLER_H_
