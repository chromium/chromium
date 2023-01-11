// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_H_
#define COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "components/download/database/download_namespace.h"

namespace download {

struct DownloadDBEntry;

// A backing storage for persisting DownloadDBEntry objects.
class DownloadDB {
 public:
  using LoadEntriesCallback = base::OnceCallback<void(
      bool success,
      std::unique_ptr<std::vector<DownloadDBEntry>> entries)>;
  using DownloadDBCallback = base::OnceCallback<void(bool success)>;

  DownloadDB();
  virtual ~DownloadDB();

  // Initializes this db asynchronously, callback will be run on completion.
  virtual void Initialize(DownloadDBCallback callback);

  // Adds or updates |entry| in the storage.
  virtual void AddOrReplace(const DownloadDBEntry& entry);

  // Adds or updates multiple entries in the storage.
  virtual void AddOrReplaceEntries(const std::vector<DownloadDBEntry>& entry,
                                   DownloadDBCallback callback);

  // Retrieves all entries with the given |download_namespace|.
  virtual void LoadEntries(LoadEntriesCallback callback);

  // Removes the Entry associated with |guid| from the storage.
  virtual void Remove(const std::string& guid);
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_DATABASE_DOWNLOAD_DB_H_
