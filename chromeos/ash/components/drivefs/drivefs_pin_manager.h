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

// The periodic removal task is ran to ensure any leftover items in the syncing
// map are identified as being `available_offline` or 0 byte files.
extern const base::TimeDelta kPeriodicRemovalInterval;

// The `DriveFsPinManager` first undergoes a setup phase, where it audits the
// current disk space, pins all available files (disk space willing) then moves
// to monitoring. This enum represents the various stages the setup goes
// through.
enum class SetupStage {
  // Initial stage.
  kNotStarted,

  // In-progress stages.
  kCalculatingFreeSpace,
  kCalculatingRequiredSpace,
  kSyncing,

  // Final success stage.
  kSuccess,

  // Final error stages.
  kDisabled,
  kStopped,
  kCannotCalculateFreeSpace,
  kCannotRetrieveSearchResults,
  kNotEnoughSpace,
};

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
std::ostream& operator<<(std::ostream& out, SetupStage stage);

// When the manager is setting up, this struct maintains all the information
// gathered.
struct SetupProgress {
  // Number of free bytes on the stateful partition. Estimated at the beginning
  // of the setup process and left unchanged afterwards.
  int64_t free_space = 0;

  // Estimated number of bytes that are required to store the files to pin. This
  // is a pessimistic estimate based on the assumption that each file uses an
  // integral number of fixed-size blocks. Estimated at the beginning of the
  // setup process and left unchanged afterwards.
  int64_t required_space = 0;

  // Estimated number of bytes that are required to download the files to pin.
  // Estimated at the beginning of the setup process and left unchanged
  // afterwards.
  int64_t total_bytes = 0;

  // Number of bytes that have been downloaded so far.
  int64_t transferred_bytes = 0;

  // Number of pinned and downloaded files so far.
  int32_t pinned_files = 0;

  // Number of errors encountered so far.
  int32_t errors = 0;

  // Number of "useful" (ie non-duplicated) events received from DriveFS so far.
  int32_t useful_events = 0;

  // Number of duplicated events received from DriveFS so far.
  int32_t duplicated_events = 0;

  // Stage of the setup process.
  SetupStage stage = SetupStage::kNotStarted;
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
  using SpaceResult = base::OnceCallback<void(int64_t)>;
  using SpaceGetter =
      base::RepeatingCallback<void(const base::FilePath&, SpaceResult)>;

  DriveFsPinManager(bool enabled,
                    const base::FilePath& profile_path,
                    mojom::DriveFs* drivefs_interface,
                    SpaceGetter get_free_space = {});

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
  using CompletionCallback = base::OnceCallback<void(SetupStage)>;
  void Start(CompletionCallback complete_callback, bool should_pin = true);

  void GetFeatureAvailability(base::OnceCallback<void(SetupStage)> callback) {
    Start(std::move(callback), false);
  }

  // Stop the syncing setup.
  void Stop();

  void AddObserver(DriveFsBulkPinObserver* observer);
  void RemoveObserver(DriveFsBulkPinObserver* observer);

  // drivefs::DriveFsHostObserver
  void OnSyncingStatusUpdate(const mojom::SyncingStatus& status) override;
  void OnUnmounted() override;
  void OnFilesChanged(const std::vector<mojom::FileChange>& changes) override;
  void OnError(const mojom::DriveError& error) override;

  // Stable ID provided by DriveFS.
  enum class StableId : int64_t { kNone = 0 };

 private:
  // Adds an item to the tracked files.
  void Add(StableId id, const std::string& path, int64_t expected_size);

  // Removes an item from the map. Does nothing if the item is not in the map.
  // Updates the total number of bytes transferred so far.
  // Returns whether an item was actually removed.
  bool Remove(StableId id,
              const std::string& path,
              int64_t bytes_transferred = -1);

  // Updates an item in the map. Does nothing if the item is not in the map.
  // Updates the total number of bytes transferred so far.
  // Returns whether anything has actually been updated.
  bool Update(StableId id,
              const std::string& path,
              int64_t bytes_transferred,
              int64_t bytes_to_transfer);

  // Updates an item to mark it in progress.
  // Does nothing if the item is not in the map.
  // Returns whether anything has actually been updated.
  bool MarkInProgress(StableId id, const std::string& path);

 private:
  // Struct keeping track of the progress of a file being synced.
  struct Progress {
    // Path inside the Drive folder.
    std::string path;

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

  // Invoked on retrieval of available space in the `~/GCache` directory.
  void OnFreeSpaceRetrieved(int64_t free_space);

  // Once the free disk space has been retrieved, this method will be invoked
  // after every batch of searches to Drive complete. This is required as the
  // user may already have files pinned (which the `GetQuotaUsage` will include
  // in it's calculation).
  void OnSearchResultForSizeCalculation(
      drive::FileError error,
      absl::optional<std::vector<drivefs::mojom::QueryItemPtr>> items);

  // When the pinning has finished, this ensures appropriate cleanup happens on
  // the underlying search query mojo connection.
  void Complete(SetupStage stage);

  // Once the verification that the files to pin will not exceed available disk
  // space, the files to pin can be batch pinned.
  void StartPinning();

  // After a file has been pinned, this ensures the in progress map has the item
  // emplaced. Note the file being pinned is just an update in drivefs, not the
  // actually completion of the file being downloaded, that is monitored via
  // `OnSyncingStatusUpdate`.
  void OnFilePinned(StableId id,
                    const std::string& path,
                    drive::FileError status);

  // Invoked at a regular interval to look at the map of in progress items and
  // ensure they are all still not available offline (i.e. still syncing). In
  // certain cases (e.g. hosted docs like gdocs) they will not emit a syncing
  // status update but will get pinned.
  void CheckUnstartedFiles();

  // When an item goes to completed, it doesn't emit the final chunk of progress
  // nor it's final size, to ensure progress is adequately retrieved, this
  // method is used to get the total size to keep track of.
  void OnMetadataRetrieved(StableId id,
                           const std::string& path,
                           drive::FileError error,
                           mojom::FileMetadataPtr metadata);

  // Start or continue pinning some files.
  void PinSomeFiles();

  // Report progress to all the observers.
  void NotifyProgress();

  SEQUENCE_CHECKER(sequence_checker_);

  // Denotes whether the feature is enabled. If the feature is disabled no setup
  // nor monitoring occurs.
  bool enabled_ = false;

  // Should the feature actually pin files, or should it stop after checking the
  // space requirements?
  bool should_pin_ = false;

  CompletionCallback complete_callback_;
  const SpaceGetter get_free_space_;

  SetupProgress progress_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<DriveFsBulkPinObserver>::Unchecked observers_;

  const base::FilePath profile_path_;
  const raw_ptr<mojom::DriveFs> drivefs_interface_;
  mojo::Remote<mojom::SearchQuery> search_query_;
  base::ElapsedTimer timer_;

  // Map that tracks the in-progress files indexed by their stable ID.
  using Files = std::map<StableId, Progress>;
  Files files_to_pin_ GUARDED_BY_CONTEXT(sequence_checker_);
  Files files_to_track_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<DriveFsPinManager> weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& out, DriveFsPinManager::StableId id);

}  // namespace drivefs::pinning

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PIN_MANAGER_H_
