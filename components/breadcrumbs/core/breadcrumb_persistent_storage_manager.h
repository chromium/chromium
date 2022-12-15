// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_PERSISTENT_STORAGE_MANAGER_H_
#define COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_PERSISTENT_STORAGE_MANAGER_H_

#include <string>
#include <vector>

#include "base/callback.h"
#include "base/files/file_path.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/breadcrumbs/core/breadcrumb_manager.h"
#include "components/breadcrumbs/core/breadcrumb_manager_observer.h"
#include "components/breadcrumbs/core/breadcrumbs_status.h"
#include "components/breadcrumbs/core/crash_reporter_breadcrumb_constants.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
  // file in `directory`.
  explicit BreadcrumbPersistentStorageManager(
      const base::FilePath& directory,
      base::RepeatingCallback<bool()> is_metrics_enabled_callback);
  ~BreadcrumbPersistentStorageManager() override;
  BreadcrumbPersistentStorageManager(
      const BreadcrumbPersistentStorageManager&) = delete;
  BreadcrumbPersistentStorageManager& operator=(
      const BreadcrumbPersistentStorageManager&) = delete;

  // Returns the stored breadcrumb events from disk to |callback|.
  void GetStoredEvents(
      base::OnceCallback<void(std::vector<std::string>)> callback);

 private:
  // Returns whether metrics consent has been provided and the persistent
  // storage manager can therefore create its breadcrumbs files. Deletes any
  // existing breadcrumbs files if consent has been revoked.
  bool CheckForFileConsent();

  // Initializes |file_position_| to |file_size| and writes any events so far.
  void InitializeFilePosition(size_t file_size);

  // Writes |pending_breadcrumbs_| to |breadcrumbs_file_| if it fits, otherwise
  // rewrites the file. NOTE: Writing may be delayed if the file has recently
  // been written into.
  void WriteEvents();

  // Writes events from BreadcrumbManager to `breadcrumbs_file_`, overwriting
  // any existing persisted breadcrumbs.
  void RewriteAllExistingBreadcrumbs();

  // Writes breadcrumbs stored in |pending_breadcrumbs_| to |breadcrumbs_file_|.
  void WritePendingBreadcrumbs();

  // BreadcrumbManagerObserver
  void EventAdded(const std::string& event) override;

  // Individual breadcrumbs that have not yet been written to disk.
  std::string pending_breadcrumbs_;

  // The last time a breadcrumb was written to |breadcrumbs_file_|. This
  // timestamp prevents breadcrumbs from being written to disk too often.
  base::TimeTicks last_written_time_;

  // A timer to delay writing to disk too often.
  base::OneShotTimer write_timer_;

  // TODO(crbug.com/1327267): Remove these counters once crash is understood.
  // The number of times the breadcrumbs file has been written to. Counts from
  // the perspective of the main thread, i.e., a write is counted at the time
  // that a task to write is posted.
  size_t write_counter_ = 0;
  // The value of `write_counter_` when the file was last fully rewritten, i.e.,
  // replaced by the temp file. Intended to investigate whether replacing the
  // breadcrumbs file can sometimes cause a crash on the next write attempt.
  size_t write_counter_at_last_full_rewrite_ = 0;

  // The path to the file for storing persisted breadcrumbs.
  const base::FilePath breadcrumbs_file_path_;

  // The current size of breadcrumbs written to |breadcrumbs_file_path_|.
  // NOTE: The optional will not have a value until the size of the existing
  // file, if any, is retrieved.
  absl::optional<size_t> file_position_;

  // Used to check whether the user has consented to metrics reporting.
  // Breadcrumbs should only be written to persistent storage if true.
  base::RepeatingCallback<bool()> is_metrics_enabled_callback_;

  // The SequencedTaskRunner on which File IO operations are performed.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  base::WeakPtrFactory<BreadcrumbPersistentStorageManager> weak_ptr_factory_;
};

}  // namespace breadcrumbs

#endif  // COMPONENTS_BREADCRUMBS_CORE_BREADCRUMB_PERSISTENT_STORAGE_MANAGER_H_
