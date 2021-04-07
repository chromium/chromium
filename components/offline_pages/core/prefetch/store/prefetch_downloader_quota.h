// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_DOWNLOADER_QUOTA_H_
#define COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_DOWNLOADER_QUOTA_H_

#include <cstdint>

#include "base/macros.h"

namespace base {
class Clock;
}  // namespace base

namespace sql {
class Database;
}  // namespace sql

namespace offline_pages {

// Handles retrieval, storage and calculation of quota for |PrefetchDownloader|.
class PrefetchDownloaderQuota {
 public:
  // Public for unit tests.
  static const int64_t kDefaultMaxDailyQuotaBytes;

  PrefetchDownloaderQuota(sql::Database* db, const base::Clock* clock);
  ~PrefetchDownloaderQuota();

  // Gets the max daily quota from Finch.
  static int64_t GetMaxDailyQuotaBytes();

  // Gets the currently available quota, as read from the DB and adjusted for
  // time elapsed since quota was last updated.
  int64_t GetAvailableQuotaBytes();

  // Sets available quota to the provided |quota| value, capped by
  // [0, max daily quota].
  bool SetAvailableQuotaBytes(int64_t quota);

 private:
  // DB connection. Not owned.
  sql::Database* db_;

  // Clock used for time related calculation and quota updates in DB. Not owned.
  const base::Clock* clock_;

  DISALLOW_COPY_AND_ASSIGN(PrefetchDownloaderQuota);
};
}  // namespace offline_pages

#endif  // COMPONENTS_OFFLINE_PAGES_CORE_PREFETCH_STORE_PREFETCH_DOWNLOADER_QUOTA_H_
