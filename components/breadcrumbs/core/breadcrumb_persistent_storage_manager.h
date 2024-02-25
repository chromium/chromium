// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_PERSISTENT_STORAGE_MANAGER_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_PERSISTENT_STORAGE_MANAGER_H_

#include <optional>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/breadcrumb_manager_observer.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"

namespace breadcrumbs {

// The filesize for the file at |breadcrumbs_file_path_|. The file will always
// be this constant size because it is accessed using a memory mapped file. The
// file is twice as large as |kMaxDataLength| which leaves room for appending
// breadcrumb events. Once the file is full of events, the contents will be
// reduced to kMaxDataLength.
constexpr size_t kPersistedFilesizeInBytes = kMaxDataLength * 2;

// Stores breadcrumb events to and retrieves them from a file on disk.
// Persisting these events allows access to breadcrumb events from previous
// application sessions.
class BreadcrumbPersistentStorageManager : public BreadcrumbManagerObserver {
 public:
  // Observes the BreadcrumbManager and stores observed breadcrumb events to a
  // file in `directory`. If a previous session's events are already stored in
  // the file, reads them and passes them to the BreadcrumbManager to be
  // prepended to the event log.
  explicit BreadcrumbPersistentStorageManager(
      const base::FilePath& directory,
      base::RepeatingCallback<bool()> is_metrics_enabled_callback,
      base::OnceClosure initialization_done_callback = base::DoNothing());
  ~BreadcrumbPersistentStorageManager() override;
  BreadcrumbPersistentStorageManager(
      const BreadcrumbPersistentStorageManager&) = delete;
  BreadcrumbPersistentStorageManager& operator=(
      const BreadcrumbPersistentStorageManager&) = delete;

 private:
  // Sets `file_position_` based on the given `previous_session_events`, and
  // passes them to the BreadcrumbManager. If any events have already been
  // logged it then writes them to the file.
  void Initialize(base::OnceClosure initialization_done_callback,
                  const std::string& previous_session_events);

  // Returns whether metrics consent has been provided and the persistent
  // storage manager can therefore create its breadcrumbs files. Deletes any
  // existing breadcrumbs files if consent has been revoked.
  bool CheckForFileConsent();

  // Writes |pending_breadcrumbs_| to |breadcrumbs_file_| if it fits, otherwise
  // rewrites the file. NOTE: Writing may be delayed if the file has recently
  // been written into.
  void WriteEvents(base::OnceClosure done_callback = base::DoNothing());

  // Writes the given `events` to `breadcrumbs_file_`. If `append` is false,
  // overwrites the file.
  void Write(const std::string& events, bool append);

  // BreadcrumbManagerObserver
  void EventAdded(const std::string& event) override;

  // Individual breadcrumbs that have not yet been written to disk.
  std::string pending_breadcrumbs_;

  // The last time a breadcrumb was written to |breadcrumbs_file_|. This
  // timestamp prevents breadcrumbs from being written to disk too often.
  base::TimeTicks last_written_time_;

  // A timer to delay writing to disk too often.
  base::OneShotTimer write_timer_;

  // The path to the file for storing persisted breadcrumbs.
  const base::FilePath breadcrumbs_file_path_;

  // The current size of breadcrumbs written to |breadcrumbs_file_path_|.
  // NOTE: The optional will not have a value until the size of the existing
  // file, if any, is retrieved.
  std::optional<size_t> file_position_;

  // Used to check whether the user has consented to metrics reporting.
  // Breadcrumbs should only be written to persistent storage if true.
  base::RepeatingCallback<bool()> is_metrics_enabled_callback_;

  // The SequencedTaskRunner on which File IO operations are performed.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<BreadcrumbPersistentStorageManager> weak_ptr_factory_;
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_PERSISTENT_STORAGE_MANAGER_H_
