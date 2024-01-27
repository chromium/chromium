// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_COMMON_DOWNLOAD_DB_CACHE_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_COMMON_DOWNLOAD_DB_CACHE_H_

#include <map>
#include <memory>
#include <optional>
#include <set>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/timer/timer.h"
#include "components/download/public/common/download_export.h"
#include "components/download/public/common/download_item.h"

namespace download {

class DownloadDB;
struct DownloadDBEntry;

// Responsible for caching the metadata of all in progress downloads.
class COMPONENTS_DOWNLOAD_EXPORT DownloadDBCache
    : public DownloadItem::Observer {
 public:
  explicit DownloadDBCache(std::unique_ptr<DownloadDB> download_db);

  DownloadDBCache(const DownloadDBCache&) = delete;
  DownloadDBCache& operator=(const DownloadDBCache&) = delete;

  ~DownloadDBCache() override;

  using InitializeCallback =
      base::OnceCallback<void(bool /* success */,
                              std::unique_ptr<std::vector<DownloadDBEntry>>)>;
  void Initialize(InitializeCallback callback);

  std::optional<DownloadDBEntry> RetrieveEntry(const std::string& guid);
  void AddOrReplaceEntry(const DownloadDBEntry& entry);

  // Remove an entry from the DownloadDB.
  void RemoveEntry(const std::string& guid);

 private:
  friend class DownloadDBCacheTest;
  friend class InProgressDownloadManager;

  // Update all the entries in |download_db_|.
  void UpdateDownloadDB();

  // DownloadItem::Observer
  void OnDownloadUpdated(DownloadItem* download) override;
  void OnDownloadRemoved(DownloadItem* download) override;

  // Called when the |download_db_| is initialized.
  void OnDownloadDBInitialized(InitializeCallback callback, bool success);

  // Called when all the download db entries are loaded.
  void OnDownloadDBEntriesLoaded(
      InitializeCallback callback,
      bool success,
      std::unique_ptr<std::vector<DownloadDBEntry>> entries);

  void SetTimerTaskRunnerForTesting(
      scoped_refptr<base::SequencedTaskRunner> task_runner);

  // Whether this object has already been initialized.
  bool initialized_;

  // Database for storing in-progress metadata.
  std::unique_ptr<DownloadDB> download_db_;

  using DownloadDBEntryMap = std::map<std::string, DownloadDBEntry>;
  // Entries that are currently being updated will be cached to reduce disk
  // writing.
  DownloadDBEntryMap cached_entries_;

  // GUIDs of updated entries.
  std::set<std::string> updated_guids_;

  // Used to trigger db updates.
  base::OneShotTimer update_timer_;

  base::WeakPtrFactory<DownloadDBCache> weak_factory_{this};
};

}  //  namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_COMMON_DOWNLOAD_DB_CACHE_H_
