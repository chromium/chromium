// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_DATABASE_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_DATABASE_HELPER_H_

#include <stddef.h>
#include <stdint.h>

#include <list>
#include <set>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "storage/browser/database/database_tracker.h"
#include "storage/common/database/database_identifier.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class BrowserContext;
struct StorageUsageInfo;
}  // namespace content

namespace browsing_data {

// This class fetches database information in the FILE thread, and notifies
// the UI thread upon completion.
// A client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI
// thread at some later point.
class DatabaseHelper : public base::RefCountedThreadSafe<DatabaseHelper> {
 public:
  using FetchCallback =
      base::OnceCallback<void(const std::list<content::StorageUsageInfo>&)>;

  explicit DatabaseHelper(content::BrowserContext* browser_context);

  DatabaseHelper(const DatabaseHelper&) = delete;
  DatabaseHelper& operator=(const DatabaseHelper&) = delete;

  // Starts the fetching process, which will notify its completion via
  // callback.
  // This must be called only in the UI thread.
  virtual void StartFetching(FetchCallback callback);

  // Deletes all databases associated with an origin. This must be called in the
  // UI thread.
  virtual void DeleteDatabase(const url::Origin& origin);

 protected:
  friend class base::RefCountedThreadSafe<DatabaseHelper>;
  virtual ~DatabaseHelper();

 private:
  scoped_refptr<storage::DatabaseTracker> tracker_;
};

// This class is a thin wrapper around DatabaseHelper that does not
// fetch its information from the database tracker, but gets them passed by
// a call when accessed.
class CannedDatabaseHelper : public DatabaseHelper {
 public:
  explicit CannedDatabaseHelper(content::BrowserContext* browser_context);

  CannedDatabaseHelper(const CannedDatabaseHelper&) = delete;
  CannedDatabaseHelper& operator=(const CannedDatabaseHelper&) = delete;

  // Add a database to the set of canned databases that is returned by this
  // helper.
  void Add(const url::Origin& origin);

  // Clear the list of canned databases.
  void Reset();

  // True if no databases are currently stored.
  bool empty() const;

  // Returns the number of currently stored databases.
  size_t GetCount() const;

  // Returns the current list of web databases.
  const std::set<url::Origin>& GetOrigins();

  // DatabaseHelper implementation.
  void StartFetching(FetchCallback callback) override;
  void DeleteDatabase(const url::Origin& origin) override;

 private:
  ~CannedDatabaseHelper() override;

  std::set<url::Origin> pending_origins_;
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_DATABASE_HELPER_H_
