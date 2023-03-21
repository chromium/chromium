// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PIN_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PIN_MANAGER_H_

#include <algorithm>
#include <ostream>
#include <unordered_map>
#include <unordered_set>
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
#include "base/time/time.h"
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

// The PinManager first undergoes a setup phase, where it audits the current
// disk space, pins all available files (disk space willing) then moves to
// monitoring. This enum represents the various stages the setup goes through.
enum class Stage {
  // Initial stage.
  kStopped,

  // Paused because of unfavorable network conditions.
  kPaused,

  // In-progress stages.
  kGettingFreeSpace,
  kListingFiles,
  kSyncing,

  // Final success stage.
  kSuccess,

  // Final error stages.
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
  // of the setup process and regularly updated afterwards.
  int64_t free_space = 0;

  // Estimated number of extra bytes that are required to store the files to
  // pin. This is a pessimistic estimate based on the assumption that each file
  // uses an integral number of fixed-size blocks. Estimated at the beginning of
  // the setup process and updated if necessary afterwards. When everything is
  // pinned and cached, the required space is zero.
  int64_t required_space = 0;

  // Estimated number of bytes that are required to download the files to pin.
  // Estimated at the beginning of the setup process and updated if necessary
  // afterwards.
  int64_t bytes_to_pin = 0;

  // Number of bytes that have been downloaded so far.
  int64_t pinned_bytes = 0;

  // Total number of files to pin.
  int files_to_pin = 0;

  // Number of pinned and downloaded files so far.
  int pinned_files = 0;

  // Number of errors encountered so far.
  int failed_files = 0;

  // Number of files being synced right now.
  int syncing_files = 0;

  // Number of skipped items (files, directories and shortcuts).
  int skipped_items = 0;

  // Number of all listed items (files, directories and shortcuts) seen during
  // the kListingFiles stage.
  int listed_items = 0;

  // Number of "useful" (ie non-duplicated) events received from DriveFS so far.
  int useful_events = 0;

  // Number of duplicated events received from DriveFS so far.
  int duplicated_events = 0;

  // Stage of the setup process.
  Stage stage = Stage::kStopped;

  // Has the PinManager ever emptied its set of tracking items?
  bool emptied_queue = false;

  Progress();
  Progress(const Progress&);
  Progress& operator=(const Progress&);

  // Returns whether required_space + some margin is less than free_space.
  bool HasEnoughFreeSpace() const;
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
  using Path = base::FilePath;

  PinManager(Path profile_path, mojom::DriveFs* drivefs);

  PinManager(const PinManager&) = delete;
  PinManager& operator=(const PinManager&) = delete;

  ~PinManager() override;

  // Starts up the manager, which will first search for any unpinned items and
  // pin them (within the users My drive) then turn to a "monitoring" phase
  // which will ensure any new files created and switched to pinned state
  // automatically. Does nothing if this pin manager is already started.
  void Start();

  // Stops this pin manager. Does nothing if this pin manager is already
  // stopped.
  void Stop();

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
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* const observer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    DCHECK(observers_.HasObserver(observer));
    observers_.RemoveObserver(observer);
  }

  // Processes a syncing status event. Returns true if the event was useful.
  bool OnSyncingEvent(mojom::ItemEvent& event);

  // Stable ID provided by DriveFS.
  enum class Id : int64_t { kNone = 0 };

  // Notify any ongoing syncing events that a delete operation has occurred.
  void NotifyDelete(Id id, const Path& path);

  // drivefs::DriveFsHostObserver
  void OnSyncingStatusUpdate(const mojom::SyncingStatus& status) override;
  void OnUnmounted() override;
  void OnFilesChanged(const std::vector<mojom::FileChange>& changes) override;
  void OnError(const mojom::DriveError& error) override;

  base::WeakPtr<PinManager> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  // Sets the function that retrieves the free space. For tests only.
  using SpaceResult = base::OnceCallback<void(int64_t)>;
  using SpaceGetter = base::RepeatingCallback<void(const Path&, SpaceResult)>;

  void SetSpaceGetter(SpaceGetter f) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    space_getter_ = std::move(f);
  }

  // Sets the completion callback, which will be called once the initial pinning
  // has completed.
  using CompletionCallback = base::OnceCallback<void(Stage)>;

  void SetCompletionCallback(CompletionCallback f) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    completion_callback_ = std::move(f);
  }

  // Sets the flag controlling whether the feature should actually pin files
  // (default), or whether it should stop after checking the space requirements.
  void ShouldPin(const bool b) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    should_pin_ = b;
  }

  // Sets the online or offline network status, and starts or pauses the Pin
  // manager accordingly.
  void SetOnline(bool online);

 private:
  // Progress of a file being synced or to be synced.
  struct File {
    // Path inside the Drive folder.
    // TODO(b/265209836) Remove this field when not needed anymore.
    Path path;

    // Number of bytes that have been transferred so far.
    int64_t transferred = 0;

    // Total expected number of bytes for this file.
    int64_t total = 0;

    // Is this file already pinned?
    bool pinned = false;

    // Have we received in-progress events for this file?
    bool in_progress = false;

    std::ostream& PrintTo(std::ostream& out) const;

    friend std::ostream& operator<<(std::ostream& out, const File& p) {
      return p.PrintTo(out);
    }
  };

  using Files = std::unordered_map<Id, File>;

  // Check if the given item can be pinned.
  static bool CanPin(const mojom::FileMetadata& md, const Path& path);

  // Adds an item to the files to track if it is of interest. Does nothing if an
  // item with the same ID already exists in the map. Updates the total number
  // of bytes to transfer and the required space. Returns whether an item was
  // actually added.
  bool Add(const mojom::FileMetadata& md, const Path& path);

  // Removes an item from the files to track. Does nothing if the item is not in
  // the map. Updates the total number of bytes transferred so far. If
  // `transferred` is negative, use the total expected size. Returns whether an
  // item was actually removed.
  bool Remove(Id id, const Path& path, int64_t transferred = -1);
  void Remove(Files::iterator it, const Path& path, int64_t transferred);

  // Updates an item in the files to track. Does nothing if the item is not in
  // the map. Updates the total number of bytes transferred so far. Updates the
  // required space. If `transferred` or `total` is negative, then the matching
  // argument is ignored. Returns whether anything has actually been updated.
  bool Update(Id id, const Path& path, int64_t transferred, int64_t total);
  bool Update(Files::value_type& entry,
              const Path& path,
              int64_t transferred,
              int64_t total);

  void OnFileCreated(const mojom::FileChange& event);
  void OnFileDeleted(const mojom::FileChange& event);
  void OnFileModified(const mojom::FileChange& event);

  // Invoked on retrieval of free space at the beginning of the setup process.
  void OnFreeSpaceRetrieved1(int64_t free_space);

  // Periodically check for free space.
  void CheckFreeSpace();

  // Invoked on retrieval of free space during the periodic check.
  void OnFreeSpaceRetrieved2(int64_t free_space);

  // Creates the Search Query.
  void StartSearchQuery();

  // Gets the next batch of items when listing files.
  void GetNextPage();

  // Once the free disk space has been retrieved, this method will be invoked
  // after every batch of searches to Drive complete. This is required as the
  // user may already have files pinned (which the `GetQuotaUsage` will include
  // in it's calculation).
  void OnSearchResult(
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
  void OnFilePinned(Id id, const Path& path, drive::FileError status);

  // Invoked at a regular interval to look at the map of in progress items and
  // ensure they are all still not available offline (i.e. still syncing). In
  // certain cases (e.g. hosted docs like gdocs) they will not emit a syncing
  // status update but will get pinned.
  void CheckStalledFiles();

  void OnMetadataForCreatedFile(Id id,
                                const Path& path,
                                drive::FileError error,
                                mojom::FileMetadataPtr metadata);

  void OnMetadataForModifiedFile(Id id,
                                 const Path& path,
                                 drive::FileError error,
                                 mojom::FileMetadataPtr metadata);

  // Start or continue pinning some files.
  void PinSomeFiles();

  // Report progress to all the observers.
  void NotifyProgress();

  // Counts the files that have been marked as pinned and that are still being
  // tracked. Should always be equal to progress_.syncing_files. For debugging
  // only.
  int CountPinnedFiles() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return std::count_if(
        files_to_track_.cbegin(), files_to_track_.cend(),
        [](const Files::value_type& entry) { return entry.second.pinned; });
  }

  SEQUENCE_CHECKER(sequence_checker_);

  const Path profile_path_ GUARDED_BY_CONTEXT(sequence_checker_);
  const raw_ptr<mojom::DriveFs, DanglingUntriaged> drivefs_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Is the device connected to a suitable network? Assume it is online for
  // tests.
  bool is_online_ GUARDED_BY_CONTEXT(sequence_checker_) = true;

  // Should the feature actually pin files, or should it stop after checking the
  // space requirements?
  bool should_pin_ GUARDED_BY_CONTEXT(sequence_checker_) = true;

  // Interval at which the free space is periodically checked.
  base::TimeDelta space_check_interval_ GUARDED_BY_CONTEXT(sequence_checker_) =
      base::Seconds(60);

  SpaceGetter space_getter_ GUARDED_BY_CONTEXT(sequence_checker_);
  CompletionCallback completion_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  Progress progress_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);

  mojo::Remote<mojom::SearchQuery> search_query_
      GUARDED_BY_CONTEXT(sequence_checker_);
  base::ElapsedTimer timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ElapsedTimer progress_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Stable IDs of the files to pin, and which are not already marked as pinned.
  std::unordered_set<Id> files_to_pin_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Map that tracks the in-progress files indexed by their stable ID. This
  // contains all the files, either pinned or not, that are not completely
  // cached yet.
  Files files_to_track_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<PinManager> weak_ptr_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, Add);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, Update);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, Remove);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, OnSyncingEvent);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, OnSyncingStatusUpdate);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, CanPin);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, OnFileCreated);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, OnFileModified);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, OnFileDeleted);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, OnFilePinned);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, OnFilesChanged);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, OnMetadataForCreatedFile);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, OnMetadataForModifiedFile);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, CheckFreeSpace);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, CannotGetFreeSpace2);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, NotEnoughSpace2);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, OnFreeSpaceRetrieved2);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, PeriodicSpaceCheck);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, SetOnline);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, OnTransientError);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, OnError);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, StartWhenInProgress);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, StartPinning);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, PinSomeFiles);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, CheckStalledFiles);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinManagerTest, NotifyProgress);
};

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
std::ostream& operator<<(std::ostream& out, PinManager::Id id);

}  // namespace drivefs::pinning

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PIN_MANAGER_H_
