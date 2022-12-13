// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PIN_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PIN_MANAGER_H_

#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/sequence_bound.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/ash/components/drivefs/drivefs_host_observer.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace drivefs::pinning {

// Constant representing the GCache folder name.
extern const char kGCacheFolderName[];

// The periodic removal task is ran to ensure any leftover items in the syncing
// map are identified as being `available_offline` or 0 byte files.
extern const base::TimeDelta kPeriodicRemovalInterval;

// Errors that are returned via the completion callback that indicate either
// which stage the failure was at or whether the initial setup was a success.
enum class SetupError {
  kSuccess = 0,
  kManagerDisabled = 1,
  kErrorCalculatingFreeDiskSpace = 2,
  kErrorRetrievingSearchResults = 3,
  kErrorResultsReturnedInvalid = 4,
  kErrorNotEnoughFreeSpace = 5,
  kErrorRetrievingSearchResultsForPinning = 6,
  kErrorResultsReturnedInvalidForPinning = 7,
  kErrorFailedToPinItem = 8,
  kErrorSearchQueryNotBound = 9,
  kErrorManagerStopped = 10,
};

// The `DriveFsPinManager` first undergoes a setup phase, where it audits the
// current disk space, pins all available files (disk space willing) then moves
// to monitoring. This enum represents the various stages the setup goes
// through.
enum class SetupStage {
  kNotStarted = 0,
  kStarted = 1,
  kCalculatedFreeLocalDiskSpace = 2,
  kCalculatedRequiredDiskSpace = 3,
  kFinishedSetupWithError = 4,
  kFinishedSetup = 5,
};

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
  SetupStage stage = SetupStage::kNotStarted;
  SetupError error;

  // Sets the `SetupProgress` back to the initial values above.
  void Reset();
};

// The managers current state.
// TODO(b/261633796): Represent the monitoring stage here after setup has
// finished.
struct ManagerState {
  SetupProgress progress;

  bool SetupInProgress();
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
  DriveFsPinManager(bool enabled,
                    const base::FilePath& profile_path,
                    mojom::DriveFs* drivefs_interface);
  DriveFsPinManager(bool enabled,
                    const base::FilePath& profile_path,
                    mojom::DriveFs* drivefs_interface,
                    std::unique_ptr<FreeDiskSpaceDelegate> free_disk_space);

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
  class InProgressSyncingItems {
   public:
    InProgressSyncingItems();

    InProgressSyncingItems(const InProgressSyncingItems&) = delete;
    InProgressSyncingItems& operator=(const InProgressSyncingItems&) = delete;

    ~InProgressSyncingItems();

    // Adds an item to the map.
    void AddItem(const std::string path);

    // Removes an item from the map, if the item doesn't exist ignores the
    // removal. Returns the total bytes transferred on every removal.
    int64_t RemoveItem(const std::string path, int64_t total_bytes);

    // Update the item keyed at `path` with the new progress bytes. Returns the
    // total bytes transferred on every update.
    int64_t UpdateItem(const std::string path,
                       int64_t bytes_transferred,
                       int64_t bytes_to_transfer);

    // Return the number of items currently being tracked as in progress.
    size_t GetItemCount();

    // Returns any items that have 0 `bytes_to_transfer` which corresponds to
    // items that haven't received a syncing status update.
    std::vector<std::string> GetUnstartedItems();

   private:
    SEQUENCE_CHECKER(sequence_checker_);
    // A map that tracks the in progress items by their key to a pair of
    // `int64_t` with `first` being the number of bytes transferred and `second`
    // being the `bytes_to_transfer` i.e. the total bytes of the syncing file.
    using InProgressMap = std::map<std::string, std::pair<int64_t, int64_t>>;
    InProgressMap in_progress_items_ GUARDED_BY_CONTEXT(sequence_checker_);

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
  void PeriodicallyRemovePinnedItems();

  // For any paths that are in the unstarted phase (i.e. no `bytes_to_transfer`
  // registered), the metadata must be retrieved to verify their
  // `available_offline` boolean is true OR the size is 0.
  void GetMetadata(const std::vector<std::string> unstarted_paths);

  // When an item goes to completed, it doesn't emit the final chunk of progress
  // nor it's final size, to ensure progress is adequately retrieved, this
  // method is used to get the total size to keep track of.
  void GetMetadataForPath(const base::FilePath& path);
  void OnMetadataRetrieved(const std::string path,
                           drive::FileError error,
                           mojom::FileMetadataPtr metadata);

  // If there are no remaining items left, get the next search query page.
  void MaybeStartSearch(size_t remaining_items);

  // The `total_bytes_transferred` is guarded by a sequence in the
  // `InProgressMap`, so after updating a single item the total bytes is
  // notified via `NotifyProgress`.
  void ReportTotalBytesTransferred(int64_t total_bytes_transferred);

  // Report progress to all the observers.
  void NotifyProgress();

  // Denotes whether the feature is enabled. if the feature is disabled no setup
  // nor monitoring occurs.
  bool enabled_ = false;

  base::OnceCallback<void(SetupError)> complete_callback_;
  std::unique_ptr<FreeDiskSpaceDelegate> free_disk_space_;

  ManagerState state_;
  base::ObserverList<DriveFsBulkPinObserver>::Unchecked observers_;

  base::FilePath profile_path_;
  raw_ptr<mojom::DriveFs> drivefs_interface_;
  mojo::Remote<mojom::SearchQuery> search_query_;
  base::ElapsedTimer timer_;

  // The in progress syncing items and the task runner which guarantees items
  // are added / removed / updated in sequence.
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;
  base::SequenceBound<InProgressSyncingItems> syncing_items_;

  base::WeakPtrFactory<DriveFsPinManager> weak_ptr_factory_{this};
};

}  // namespace drivefs::pinning

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PIN_MANAGER_H_
