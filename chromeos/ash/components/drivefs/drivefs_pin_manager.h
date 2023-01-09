// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PIN_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PIN_MANAGER_H_

#include <ostream>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/ash/components/drivefs/drivefs_host_observer.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace drivefs::pinning {

// Prints a size in bytes in a human-readable way.
enum HumanReadableSize : int64_t;

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
std::ostream& operator<<(std::ostream& out, HumanReadableSize size);

// Constant representing the GCache folder name.
extern const char kGCacheFolderName[];

// The periodic removal task is ran to ensure any leftover items in the syncing
// map are identified as being `available_offline` or 0 byte files.
extern const base::TimeDelta kPeriodicRemovalInterval;

// Errors that are returned via the completion callback that indicate either
// which stage the failure was at or whether the initial setup was a success.
enum class SetupError {
  kSuccess,
  kManagerDisabled,
  kManagerStopped,
  kCannotCalculateFreeSpace,
  kCannotRetrieveSearchResults,
  kCannotPinItem,
  kNotEnoughSpace,
};

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
std::ostream& operator<<(std::ostream& out, SetupError error);

// The `DriveFsPinManager` first undergoes a setup phase, where it audits the
// current disk space, pins all available files (disk space willing) then moves
// to monitoring. This enum represents the various stages the setup goes
// through.
enum class SetupStage {
  kError = -1,
  kNotStarted,
  kStarted,
  kCalculatedFreeSpace,
  kCalculatedRequiredSpace,
  kFinished,
};

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
std::ostream& operator<<(std::ostream& out, SetupStage stage);

// A delegate to aid in mocking the free disk scenarios for testing, in non-test
// scenarios this simply calls `base::SysInfo::AmountOfFreeDiskSpace`.
class FreeDiskSpaceDelegate {
 public:
  // Invokes the `base::SysInfo::AmountOfFreeDiskSpace` method on a blocking
  // thread.
  virtual void AmountOfFreeDiskSpace(
      const base::FilePath& path,
      base::OnceCallback<void(int64_t)> callback) = 0;

  virtual ~FreeDiskSpaceDelegate() = default;
};

struct DrivePathAndStatus {
  base::FilePath path;
  drive::FileError status;
};

// When the manager is setting up, this struct maintains all the information
// gathered.
struct SetupProgress {
  int64_t required_disk_space = 0;
  int64_t available_disk_space = 0;
  int64_t pinned_disk_space = 0;
  int32_t batch_size = 50;
  int32_t error_count = 0;
  SetupStage stage = SetupStage::kNotStarted;
  SetupError error = SetupError::kSuccess;

  // Sets the `SetupProgress` back to the initial values above.
  void Reset() { *this = SetupProgress(); }
};

// The managers current state.
// TODO(b/261633796): Represent the monitoring stage here after setup has
// finished.
struct ManagerState {
  SetupProgress progress;

  bool SetupInProgress() const;
};

// Observe the setup progress via subscribing as an observer on the
// `DriveFsPinManager`.
// TODO(b/261633796): Send back monitoring events to the observers.
class DriveFsBulkPinObserver {
 public:
  // When the setup progresses, this returns back the information gathered and
  // the current stage of setup.
  virtual void OnSetupProgress(const SetupProgress& progress) = 0;

 protected:
  virtual ~DriveFsBulkPinObserver() = default;
};

// Manages bulk pinning of items via DriveFS. This class handles the following:
//  - Manage batching of pin actions to avoid sending too many events at once.
//  - Ensure disk space is not being exceeded whilst pinning files.
//  - Maintain pinning of files that are newly created.
//  - Rebuild the progress of bulk pinned items (if turned off mid way through a
//    bulk pinning event).
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS) DriveFsPinManager
    : public DriveFsHostObserver {
 public:
  DriveFsPinManager(
      bool enabled,
      const base::FilePath& profile_path,
      mojom::DriveFs* drivefs_interface,
      std::unique_ptr<FreeDiskSpaceDelegate> free_disk_space = nullptr);

  DriveFsPinManager(const DriveFsPinManager&) = delete;
  DriveFsPinManager& operator=(const DriveFsPinManager&) = delete;

  ~DriveFsPinManager() override;

  // Enable or disable the bulk pinning.
  void SetBulkPinningEnabled(bool enabled) { enabled_ = enabled; }

  // Start up the manager, which will first search for any unpinned items and
  // pin them (within the users My drive) then turn to a "monitoring" phase
  // which will ensure any new files created and switched to pinned state
  // automatically. The complete callback will be called once the initial
  // pinning has completed.
  void Start(base::OnceCallback<void(SetupError)> complete_callback);

  // Stop the syncing setup.
  void Stop();

  void AddObserver(DriveFsBulkPinObserver* observer);
  void RemoveObserver(DriveFsBulkPinObserver* observer);

  // drivefs::DriveFsHostObserver
  void OnSyncingStatusUpdate(const mojom::SyncingStatus& status) override;
  void OnUnmounted() override;
  void OnFilesChanged(const std::vector<mojom::FileChange>& changes) override;
  void OnError(const mojom::DriveError& error) override;

 private:
  // A wrapper to maintain sequence-affinity on the `InProgressMap`. The
  // instance of this is owned by `DriveFsPinManager`, is created and destroyed
  // on the same task runner.
  class TrackedFiles {
   public:
    ~TrackedFiles();
    TrackedFiles();

    // Adds an item to the map.
    void Add(const std::string& path, int64_t expected_size);

    // Removes an item from the map. Does nothing if the item is not in the map.
    // Updates the total number of bytes transferred so far.
    // Returns whether an item was actually removed.
    bool Remove(const std::string& path, int64_t bytes_transferred = -1);

    // Adds or updates the item keyed at `path` with the new progress bytes.
    // Updates the total number of bytes transferred so far.
    // Returns whether anything has actually been updated.
    bool Update(const std::string& path,
                int64_t bytes_transferred,
                int64_t bytes_to_transfer);

    // Adds or updates the item to mark it in progress.
    // Returns whether anything has actually been updated.
    bool MarkInProgress(const std::string& path);

    // Returns the number of items currently being tracked as in progress.
    size_t GetCount() const;

    // Returns the paths of the tracked files that haven't started syncing yet.
    std::vector<std::string> GetUnstartedPaths() const;

    // Returns the total number of bytes that have been transferred so far.
    int64_t GetTotalBytesTransferred() const {
      DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
      return total_bytes_transferred_;
    }

    // Resets this object.
    void Reset();

   private:
    SEQUENCE_CHECKER(sequence_checker_);

    // Struct keeping track of the progress of a file being synced.
    struct Progress {
      // Number of bytes that have been transferred so far.
      int64_t transferred = 0;

      // Total expected number of bytes for this file.
      int64_t total = 0;

      // Have we received in-progress events for this file?
      bool in_progress = false;

      friend std::ostream& operator<<(std::ostream& out, const Progress& p) {
        return out << "{transferred: " << HumanReadableSize(p.transferred)
                   << ", total: " << HumanReadableSize(p.total)
                   << ", in_progress: " << p.in_progress << "}";
      }
    };

    // Map that tracks the in-progress files indexed by their path.
    using Files = std::map<std::string, Progress>;
    Files files_ GUARDED_BY_CONTEXT(sequence_checker_);

    // Keeps track of the total bytes transferred by all the in progress syncing
    // items.
    int64_t total_bytes_transferred_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
  };

  // Invoked on retrieval of available space in the `~/GCache` directory.
  void OnFreeDiskSpaceRetrieved(int64_t free_space);

  // Once the free disk space has been retrieved, this method will be invoked
  // after every batch of searches to Drive complete. This is required as the
  // user may already have files pinned (which the `GetQuotaUsage` will include
  // in it's calculation).
  void OnSearchResultForSizeCalculation(
      drive::FileError error,
      absl::optional<std::vector<drivefs::mojom::QueryItemPtr>> items);

  // When the pinning has finished, this ensures appropriate cleanup happens on
  // the underlying search query mojo connection.
  void Complete(SetupError status);

  // Once the verification that the files to pin will not exceed available disk
  // space, the files to pin can be batch pinned.
  void StartBatchPinning();

  void OnSearchResultsForPinning(
      drive::FileError error,
      absl::optional<std::vector<drivefs::mojom::QueryItemPtr>> items);

  // After a file has been pinned, this ensures the in progress map has the item
  // emplaced. Note the file being pinned is just an update in drivefs, not the
  // actually completion of the file being downloaded, that is monitored via
  // `OnSyncingStatusUpdate`.
  void OnFilePinned(const std::string& path, drive::FileError status);

  // Invoked at a regular interval to look at the map of in progress items and
  // ensure they are all still not available offline (i.e. still syncing). In
  // certain cases (e.g. hosted docs like gdocs) they will not emit a syncing
  // status update but will get pinned.
  void CheckUnstartedFiles();

  // When an item goes to completed, it doesn't emit the final chunk of progress
  // nor it's final size, to ensure progress is adequately retrieved, this
  // method is used to get the total size to keep track of.
  void OnMetadataRetrieved(const std::string& path,
                           drive::FileError error,
                           mojom::FileMetadataPtr metadata);

  // If there are no remaining items left, get the next search query page.
  void MaybeContinueSearch();

  // The `total_bytes_transferred` is guarded by a sequence in the
  // `InProgressMap`, so after updating a single item the total bytes is
  // notified via `NotifyProgress`.
  void ReportTotalBytesTransferred();

  // Report progress to all the observers.
  void NotifyProgress();

  // Denotes whether the feature is enabled. If the feature is disabled no setup
  // nor monitoring occurs.
  bool enabled_ = false;

  base::OnceCallback<void(SetupError)> complete_callback_;
  const std::unique_ptr<FreeDiskSpaceDelegate> free_disk_space_;

  ManagerState state_;
  base::ObserverList<DriveFsBulkPinObserver>::Unchecked observers_;

  const base::FilePath profile_path_;
  const raw_ptr<mojom::DriveFs> drivefs_interface_;
  mojo::Remote<mojom::SearchQuery> search_query_;
  base::ElapsedTimer timer_;
  TrackedFiles tracked_files_;

  base::WeakPtrFactory<DriveFsPinManager> weak_ptr_factory_{this};
};

}  // namespace drivefs::pinning

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PIN_MANAGER_H_
