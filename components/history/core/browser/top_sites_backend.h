// Copyright 2011 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_BACKEND_H_
#define COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_BACKEND_H_

#include <memory>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
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
  using GetMostVisitedSitesCallback =
      base::OnceCallback<void(MostVisitedURLList)>;

  TopSitesBackend();

  TopSitesBackend(const TopSitesBackend&) = delete;
  TopSitesBackend& operator=(const TopSitesBackend&) = delete;

  void Init(const base::FilePath& path);

  // Schedules the db to be shutdown.
  void Shutdown();

  // Fetches MostVisitedURLList.
  void GetMostVisitedSites(GetMostVisitedSitesCallback callback,
                           base::CancelableTaskTracker* tracker);

  // Updates top sites database from the specified delta.
  void UpdateTopSites(const TopSitesDelta& delta);

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
  void UpdateTopSitesOnDBThread(const TopSitesDelta& delta);

  // Resets the database.
  void ResetDatabaseOnDBThread(const base::FilePath& file_path);

  base::FilePath db_path_;

  std::unique_ptr<TopSitesDatabase> db_;
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;
};

}  // namespace history

#endif  // COMPONENTS_HISTORY_CORE_BROWSER_TOP_SITES_BACKEND_H_
