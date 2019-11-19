// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_BACKEND_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_BACKEND_H_

#include <memory>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/history/core/browser/history_types.h"

namespace base {
class CancelableTaskTracker;
class FilePath;
class SequencedTaskRunner;
}

namespace history {

class TopSitesDatabase;

// Service used by TopSites to have db interaction happen on the DB thread.  All
// public methods are invoked on the ui thread and get funneled to the DB
// thread.
class TopSitesBackend : public base::RefCountedThreadSafe<TopSitesBackend> {
 public:
  // TODO(yiyaoliu): Remove the enums and related code when crbug/223430 is
  // fixed.
  // An enum representing whether the UpdateTopSites execution time related
  // histogram should be recorded.
  enum RecordHistogram {
    RECORD_HISTOGRAM_YES,
    RECORD_HISTOGRAM_NO
  };

  using GetMostVisitedSitesCallback =
      base::OnceCallback<void(MostVisitedURLList)>;

  TopSitesBackend();

  void Init(const base::FilePath& path);

  // Schedules the db to be shutdown.
  void Shutdown();

  // Fetches MostVisitedURLList.
  void GetMostVisitedSites(GetMostVisitedSitesCallback callback,
                           base::CancelableTaskTracker* tracker);

  // Updates top sites database from the specified delta.
  void UpdateTopSites(const TopSitesDelta& delta,
                      const RecordHistogram record_or_not);

  // Deletes the database and recreates it.
  void ResetDatabase();

 private:
  friend class base::RefCountedThreadSafe<TopSitesBackend>;

  ~TopSitesBackend();

  // Invokes Init on the db_.
  void InitDBOnDBThread(const base::FilePath& path);

  // Shuts down the db.
  void ShutdownDBOnDBThread();

  // Does the work of getting the most visited sites.
  MostVisitedURLList GetMostVisitedSitesOnDBThread();

  // Updates top sites.
  void UpdateTopSitesOnDBThread(const TopSitesDelta& delta,
                                const RecordHistogram record_or_not);

  // Resets the database.
  void ResetDatabaseOnDBThread(const base::FilePath& file_path);

  base::FilePath db_path_;

  std::unique_ptr<TopSitesDatabase> db_;
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  DISALLOW_COPY_AND_ASSIGN(TopSitesBackend);
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_BACKEND_H_
