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
#include "base/gtest_prod_util.h"
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

// The PinManager first undergoes a setup phase, where it audits the current
// disk space, pins all available files (disk space willing) then moves to
// monitoring. This enum represents the various stages the setup goes through.
enum class Stage {
  // Initial stage.
  kNotStarted,

  // In-progress stages.
  kGettingFreeSpace,
  kListingFiles,
  kSyncing,

  // Final success stage.
  kSuccess,

  // Final error stages.
  kStopped,
  kCannotGetFreeSpace,
  kCannotListFiles,
  kNotEnoughSpace,
};

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
std::ostream& operator<<(std::ostream& out, Stage stage);

// When the manager is setting up, this struct maintains all the information
// gathered.
struct COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS) Progress {
  // Number of free bytes on the stateful partition. Estimated at the beginning
  // of the setup process and left unchanged afterwards.
  int64_t free_space = 0;

  // Estimated number of bytes that are required to store the files to pin. This
  // is a pessimistic estimate based on the assumption that each file uses an
  // integral number of fixed-size blocks. Estimated at the beginning of the
  // setup process and updated if necessary afterwards.
  int64_t required_space = 0;

  // Estimated number of bytes that are required to download the files to pin.
  // Estimated at the beginning of the setup process and updated if necessary
  // afterwards.
  int64_t bytes_to_pin = 0;

  // Number of bytes that have been downloaded so far.
  int64_t pinned_bytes = 0;

  // Total number of files to pin.
  int32_t files_to_pin = 0;

  // Number of pinned and downloaded files so far.
  int32_t pinned_files = 0;

  // Number of errors encountered so far.
  int32_t failed_files = 0;

  // Number of "useful" (ie non-duplicated) events received from DriveFS so far.
  int32_t useful_events = 0;

  // Number of duplicated events received from DriveFS so far.
  int32_t duplicated_events = 0;

  // Stage of the setup process.
  Stage stage = Stage::kNotStarted;

  Progress();
  Progress(const Progress&);
  Progress& operator=(const Progress&);
};

// Manages bulk pinning of items via DriveFS. This class handles the following:
//  - Manage batching of pin actions to avoid sending too many events at once.
//  - Ensure disk space is not being exceeded whilst pinning files.
//  - Maintain pinning of files that are newly created.
//  - Rebuild the progress of bulk pinned items (if turned off mid way through a
//    bulk pinning event).
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS) PinManager
    : public DriveFsHostObserver {
 public:
  PinManager(base::FilePath profile_path, mojom::DriveFs* drivefs);

  PinManager(const PinManager&) = delete;
  PinManager& operator=(const PinManager&) = delete;

  ~PinManager() override;

  // Starts up the manager, which will first search for any unpinned items and
  // pin them (within the users My drive) then turn to a "monitoring" phase
  // which will ensure any new files created and switched to pinned state
  // automatically.
  void Start();

  // Stops the syncing setup.
  void Stop();

  // Starts or stops the syncing engine if necessary.
  void Enable(bool enabled);

  // Gets the current progress status.
  Progress GetProgress() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return progress_;
  }

  // Observer interface.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the setup progresses.
    virtual void OnProgress(const Progress& progress) {}

    // Called when the PinManager is getting deleted.
    virtual void OnDrop() {}
  };

  void AddObserver(Observer* const observer) {
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* const observer) {
    DCHECK(observers_.HasObserver(observer));
    observers_.RemoveObserver(observer);
  }

  // Processes a syncing status event. Returns true if the event was useful.
  bool OnSyncingEvent(mojom::ItemEvent& event);

  // drivefs::DriveFsHostObserver
  void OnSyncingStatusUpdate(const mojom::SyncingStatus& status) override;
  void OnUnmounted() override;
  void OnFilesChanged(const std::vector<mojom::FileChange>& changes) override;
  void OnError(const mojom::DriveError& error) override;

  // Stable ID provided by DriveFS.
  enum class Id : int64_t { kNone = 0 };

  base::WeakPtr<PinManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Sets the function that retrieves the free space. For tests only.
  using SpaceResult = base::OnceCallback<void(int64_t)>;
  using SpaceGetter =
      base::RepeatingCallback<void(const base::FilePath&, SpaceResult)>;
  void SetSpaceGetter(SpaceGetter f) { space_getter_ = std::move(f); }

  // Sets the completion callback, which will be called once the initial pinning
  // has completed.
  using CompletionCallback = base::OnceCallback<void(Stage)>;
  void SetCompletionCallback(CompletionCallback f) {
    completion_callback_ = std::move(f);
  }

  // Sets the flag controlling whether the feature should actually pin files
  // (default), or whether it should stop after checking the space requirements.
  void ShouldPin(const bool b) { should_pin_ = b; }

  // Sets the flag controlling whether the feature should regularly check the
  // status of files that have been pinned but that haven't seen any progress
  // yet.
  void ShouldCheckStalledFiles(const bool b) {
    should_check_stalled_files_ = b;
  }

 private:
  // Struct keeping track of the progress of a file being synced.
  struct File {
    // Path inside the Drive folder.
    // TODO(b/265209836) Remove this field when not needed anymore.
    std::string path;

    // Number of bytes that have been transferred so far.
    int64_t transferred = 0;

    // Total expected number of bytes for this file.
    int64_t total = 0;

    // Have we received in-progress events for this file?
    bool in_progress = false;

    friend std::ostream& operator<<(std::ostream& out, const File& p) {
      return out << "{transferred: " << HumanReadableSize(p.transferred)
                 << ", total: " << HumanReadableSize(p.total)
                 << ", in_progress: " << p.in_progress << "}";
    }
  };

  using Files = std::map<Id, File>;

  // Adds an item to the files to pin.  Does nothing if an item with the same ID
  // already exists in files_to_pin_. Updates the total number of bytes to
  // transfer and the required space. Returns whether an item was actually
  // added.
  bool Add(Id id, const std::string& path, int64_t size);

  // Removes an item from the map. Does nothing if the item is not in the map.
  // Updates the total number of bytes transferred so far. If `transferred` is
  // negative, use the total expected size. Returns whether an item was actually
  // removed.
  bool Remove(Id id, const std::string& path, int64_t transferred = -1);

  // Updates an item in the map. Does nothing if the item is not in the map.
  // Updates the total number of bytes transferred so far. Updates the required
  // space. If `transferred` or `total` is negative, then the matching argument
  // is ignored. Returns whether anything has actually been updated.
  bool Update(Id id,
              const std::string& path,
              int64_t transferred,
              int64_t total);
  bool Update(Files::value_type& entry,
              const std::string& path,
              int64_t transferred,
              int64_t total);

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
  void Complete(Stage stage);

  // Once the verification that the files to pin will not exceed available disk
  // space, the files to pin can be batch pinned.
  void StartPinning();

  // After a file has been pinned, this ensures the in progress map has the item
  // emplaced. Note the file being pinned is just an update in drivefs, not the
  // actually completion of the file being downloaded, that is monitored via
  // `OnSyncingStatusUpdate`.
  void OnFilePinned(Id id, const std::string& path, drive::FileError status);

  // Invoked at a regular interval to look at the map of in progress items and
  // ensure they are all still not available offline (i.e. still syncing). In
  // certain cases (e.g. hosted docs like gdocs) they will not emit a syncing
  // status update but will get pinned.
  void CheckStalledFiles();

  // When an item goes to completed, it doesn't emit the final chunk of progress
  // nor it's final size, to ensure progress is adequately retrieved, this
  // method is used to get the total size to keep track of.
  void OnMetadataRetrieved(Id id,
                           const std::string& path,
                           drive::FileError error,
                           mojom::FileMetadataPtr metadata);

  // Start or continue pinning some files.
  void PinSomeFiles();

  // Report progress to all the observers.
  void NotifyProgress();

  SEQUENCE_CHECKER(sequence_checker_);

  // Should the feature actually pin files, or should it stop after checking the
  // space requirements?
  bool should_pin_ = true;

  // Should the feature regularly check the status of files that have been
  // pinned but that haven't seen any progress yet?
  bool should_check_stalled_files_ = false;

  SpaceGetter space_getter_;
  CompletionCallback completion_callback_;

  Progress progress_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<Observer> observers_;

  const base::FilePath profile_path_;
  const raw_ptr<mojom::DriveFs> drivefs_;
  mojo::Remote<mojom::SearchQuery> search_query_;
  base::ElapsedTimer timer_;

  // Map that tracks the in-progress files indexed by their stable ID.
  Files files_to_pin_ GUARDED_BY_CONTEXT(sequence_checker_);
  Files files_to_track_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<PinManager> weak_ptr_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, Add);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, Update);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, Remove);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, OnSyncingEvent);
};

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
std::ostream& operator<<(std::ostream& out, PinManager::Id id);

}  // namespace drivefs::pinning

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PIN_MANAGER_H_
