// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSING_DATA_CONTENT_INDEXED_DB_HELPER_H_
#define COMPONENTS_BROWSING_DATA_CONTENT_INDEXED_DB_HELPER_H_

#include <stddef.h>

#include <list>
#include <set>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/strings/string16.h"
#include "components/services/storage/public/mojom/indexed_db_control.mojom.h"
#include "url/origin.h"

namespace content {
class StoragePartition;
struct StorageUsageInfo;
}  // namespace content

namespace browsing_data {

// IndexedDBHelper is an interface for classes dealing with
// aggregating and deleting browsing data stored in indexed databases.  A
// client of this class need to call StartFetching from the UI thread to
// initiate the flow, and it'll be notified by the callback in its UI thread at
// some later point.
class IndexedDBHelper : public base::RefCountedThreadSafe<IndexedDBHelper> {
 public:
  using FetchCallback =
      base::OnceCallback<void(const std::list<content::StorageUsageInfo>&)>;

  // Create a IndexedDBHelper instance for the indexed databases
  // stored in |context|'s associated profile's user data directory.
  explicit IndexedDBHelper(content::StoragePartition* storage_partition);

  // Starts the fetching process, which will notify its completion via
  // |callback|. This must be called only on the UI thread.
  virtual void StartFetching(FetchCallback callback);
  // Requests a single indexed database to be deleted in the IndexedDB thread.
  virtual void DeleteIndexedDB(const url::Origin& origin,
                               base::OnceCallback<void(bool)> callback);

 protected:
  virtual ~IndexedDBHelper();

  content::StoragePartition* storage_partition_;

 private:
  friend class base::RefCountedThreadSafe<IndexedDBHelper>;

  // Enumerates all indexed database files in the IndexedDB thread.
  void IndexedDBUsageInfoReceived(
      FetchCallback callback,
      std::vector<storage::mojom::IndexedDBStorageUsageInfoPtr> origins);

  DISALLOW_COPY_AND_ASSIGN(IndexedDBHelper);
};

// This class is an implementation of IndexedDBHelper that does
// not fetch its information from the Indexed DB context, but gets them
// passed by a call when accessed.
class CannedIndexedDBHelper : public IndexedDBHelper {
 public:
  explicit CannedIndexedDBHelper(content::StoragePartition* storage_partition);

  // Add a indexed database to the set of canned indexed databases that is
  // returned by this helper.
  void Add(const url::Origin& origin);

  // Clear the list of canned indexed databases.
  void Reset();

  // True if no indexed databases are currently stored.
  bool empty() const;

  // Returns the number of currently stored indexed databases.
  size_t GetCount() const;

  // Returns the current list of indexed data bases.
  const std::set<url::Origin>& GetOrigins() const;

  // IndexedDBHelper methods.
  void StartFetching(FetchCallback callback) override;
  void DeleteIndexedDB(const url::Origin& origin,
                       base::OnceCallback<void(bool)> callback) override;

 private:
  ~CannedIndexedDBHelper() override;

  std::set<url::Origin> pending_origins_;

  DISALLOW_COPY_AND_ASSIGN(CannedIndexedDBHelper);
};

}  // namespace browsing_data

#endif  // COMPONENTS_BROWSING_DATA_CONTENT_INDEXED_DB_HELPER_H_
