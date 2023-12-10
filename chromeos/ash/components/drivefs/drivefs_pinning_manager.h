// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PINNING_MANAGER_H_
#define CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PINNING_MANAGER_H_

#include <algorithm>
#include <optional>
#include <ostream>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "base/component_export.h"
#include "base/containers/span.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_observation.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "base/time/time.h"
#include "base/timer/elapsed_timer.h"
#include "chromeos/ash/components/dbus/spaced/spaced_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/drivefs/drivefs_host.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom.h"
#include "chromeos/ash/components/drivefs/mojom/pinning_manager_types.mojom.h"
#include "chromeos/ash/components/file_manager/speedometer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/drive/file_errors.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace drivefs::pinning {

// These values are logged to UMA. Entries should not be renumbered and
// numeric values should never be reused. Please keep in sync with
// "FileManagerGoogleDriveBulkPinningEnabledSource" in
// src/tools/metrics/histograms/enums.xml.
enum class BulkPinningEnabledSource {
  kBanner = 0,
  kSystemSettings = 1,
  kDriveInternal = 2,
  kChoobe = 3,
  kMaxValue = kChoobe,
};

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
void RecordBulkPinningEnabledSource(BulkPinningEnabledSource source);

// Imbue the output stream with a locale that prints numbers with thousands
// separators.
COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
std::ostream& NiceNum(std::ostream& out);

// Prints a size in bytes in a human-readable way.
enum HumanReadableSize : int64_t;

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
std::ostream& operator<<(std::ostream& out, HumanReadableSize size);

using pinning_manager_types::mojom::Stage;

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
std::string ToString(Stage stage);

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
std::string ToString(base::TimeDelta time_delta);

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

  // Number of broken shortcuts (ie shortcuts to deleted or inaccessible files)
  // found by search queries during the kListingFiles stage. This does not count
  // shortcuts to directories.
  int broken_shortcuts = 0;

  // Numbers of items, directories, files, hosted documents and shortcuts found
  // by search queries during the kListingFiles stage.
  int listed_items = 0;
  int listed_dirs = 0;
  int listed_files = 0;
  int listed_docs = 0;
  int listed_shortcuts = 0;

  // Number of "useful" (ie non-duplicated) events received from DriveFS so far.
  int useful_events = 0;

  // Number of duplicated events received from DriveFS so far.
  int duplicated_events = 0;

  // Total number of search queries that have been started.
  int total_queries = 0;

  // Number of currently active search queries (ie started but not finished
  // yet).
  int active_queries = 0;

  // High watermark of the number of active queries.
  int max_active_queries = 0;

  // Stage of the setup process.
  Stage stage = Stage::kStopped;

  // Time spent listing and enumerating items.
  base::TimeDelta time_spent_listing_items;

  // Time spent pinning and caching files.
  base::TimeDelta time_spent_pinning_files;

  // Estimated time remaining to pin and cache all the files.
  base::TimeDelta remaining_time = base::TimeDelta::Max();

  // Should the PinningManager actually pin files, or should it stop after
  // checking the space requirements?
  bool should_pin = true;

  // Has the PinningManager ever emptied its set of tracking items?
  bool emptied_queue = false;

  Progress();
  Progress(const Progress&);
  Progress& operator=(const Progress&);

  // Returns whether required_space + some margin is less than free_space.
  bool HasEnoughFreeSpace() const;

  // Returns whether the stage is a final error stage.
  bool IsError() const;
};

// Manages bulk pinning of items via DriveFS. This class handles the following:
//  - Manage batching of pin actions to avoid sending too many events at once.
//  - Ensure disk space is not being exceeded whilst pinning files.
//  - Maintain pinning of files that are newly created.
//  - Rebuild the progress of bulk pinned items (if turned off mid way through a
//    bulk pinning event).
class COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS) PinningManager
    : DriveFsHost::Observer,
      ash::UserDataAuthClient::Observer,
      ash::SpacedClient::Observer,
      chromeos::PowerManagerClient::Observer {
 public:
  using Path = base::FilePath;

  PinningManager(Path profile_path,
                 Path mount_path,
                 mojom::DriveFs* drivefs,
                 int64_t queue_size);

  PinningManager(const PinningManager&) = delete;
  PinningManager& operator=(const PinningManager&) = delete;

  ~PinningManager() override;

  void SetDriveFsHost(DriveFsHost* const host) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    Observe(host);
  }

  // Starts up the manager, which will first search for any unpinned items and
  // pin them (within the users My drive) then turn to a "monitoring" phase
  // which will ensure any new files created and switched to pinned state
  // automatically. Does nothing if this pin manager is already started.
  void Start();

  // Stops this pin manager. Does nothing if this pin manager is already
  // stopped.
  void Stop();

  // Lists the files and calculates the required space and free disk space. This
  // doesn't pin any files and doesn't keep the space calculations up to date.
  // Does nothing and returns false if this pin manager is not in the right
  // stage to start calculating the required space.
  [[nodiscard]] bool CalculateRequiredSpace();

  // Gets the current progress status.
  Progress GetProgress() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return progress_;
  }

  // Observer interface.
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override;

    // Called when the setup progresses.
    virtual void OnProgress(const Progress& progress) {}
  };

  void AddObserver(Observer* const observer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observers_.AddObserver(observer);
  }

  void RemoveObserver(Observer* const observer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    observers_.RemoveObserver(observer);
  }

  bool HasObserver(Observer* const observer) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return observers_.HasObserver(observer);
  }

  // Stable ID provided by DriveFS.
  enum class Id : int64_t { kNone = 0 };

  // Notify any ongoing syncing events that a delete operation has occurred.
  void NotifyDelete(Id id, const Path& path);

  // DriveFsHost::Observer implementation.
  void OnUnmounted() override;
  void OnFilesChanged(const std::vector<mojom::FileChange>& changes) override;
  void OnError(const mojom::DriveError& error) override;
  void OnItemProgress(const mojom::ProgressEvent& event) override;

  base::WeakPtr<PinningManager> GetWeakPtr() {
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

  // Sets the flag controlling whether the feature should actually pin files.
  void ShouldPin() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    progress_.should_pin = true;
  }

  // Sets the online or offline network status, and starts or pauses the Pin
  // manager accordingly.
  void SetOnline(bool online);

  // Starts checking for free space.
  void CheckFreeSpace();

  // Whether the supplied `id` is currently being tracked by the PinningManager
  // and that it is unpinned.
  bool IsTrackedAndUnpinned(Id id) const;

  // For tests don't pin files after enumerating.
  void SetShouldPinFilesForTesting(bool should_pin_files_for_testing) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    should_pin_files_for_testing_ = should_pin_files_for_testing;
  }

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
  bool Update(Id id, const Path& path, int8_t progress_percent);

  void OnFileCreated(const mojom::FileChange& event);
  void OnFileDeleted(const mojom::FileChange& event);
  void OnFileModified(const mojom::FileChange& event);

  // Invoked on retrieval of free space at the beginning of the setup process.
  void OnFreeSpaceRetrieved1(int64_t free_space);

  // Invoked on retrieval of free space during the periodic check.
  void OnFreeSpaceRetrieved2(int64_t free_space);

  // Starts listing the items located in the given directory.
  void ListItems(Id dir_id, Path dir_path);

  // Gets the next batch of items when listing items.
  using Query = mojo::Remote<mojom::SearchQuery>;
  void GetNextPage(Id dir_id, Path dir_path, Query query);

  // Once the free disk space has been retrieved, this method will be invoked
  // after every batch of searches to Drive complete. This is required as the
  // user may already have files pinned (which the `GetQuotaUsage` will include
  // in it's calculation).
  void OnSearchResult(Id dir_id,
                      Path dir_path,
                      Query query,
                      drive::FileError error,
                      base::span<const drivefs::mojom::QueryItemPtr> items);

  // Handles one query item retrieved by a search query.
  void HandleQueryItem(Id dir_id,
                       const Path& dir_path,
                       const drivefs::mojom::QueryItem& item);

  // When the pinning has finished, this ensures appropriate cleanup happens on
  // the underlying search query mojo connection.
  void Complete(Stage stage);

  // Once the verification that the files to pin will not exceed available disk
  // space, the files to pin can be batch pinned.
  void StartPinning();

  // After a file has been pinned, this ensures the in progress map has the item
  // emplaced. Note the file being pinned is just an update in drivefs, not the
  // actually completion of the file being downloaded, that is monitored via
  // `OnItemProgress`.
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

  // UserDataAuthClient::Observer implementation.
  void LowDiskSpace(const ::user_data_auth::LowDiskSpace& status) override;

  // SpacedClient::Observer implementation.
  void OnSpaceUpdate(const SpaceEvent& event) override;

  // PowerManagerClient::Observer implementation.
  void BatterySaverModeStateChanged(
      const power_manager::BatterySaverModeState& state) override;

  // Callback used to query battery saver state from PowerManagerClient on
  // startup.
  void OnGotBatterySaverState(
      std::optional<power_manager::BatterySaverModeState> state);

  // Starts and stops monitoring space using the SpacedClient::Observer.
  bool StartMonitoringSpace();
  void StopMonitoringSpace();

  // Sends a message to the Docs offline extension telling it to enable offline
  // mode.
  void EnableDocsOffline();

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
  const Path mount_path_ GUARDED_BY_CONTEXT(sequence_checker_);
  const raw_ptr<mojom::DriveFs, DanglingUntriaged> drivefs_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Maximum number of items that can be pinned but not cached yet at the same
  // time.
  const int64_t queue_size_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Is the device connected to a suitable network? Assume it is online for
  // tests.
  bool is_online_ GUARDED_BY_CONTEXT(sequence_checker_) = true;

  // Is the device battery ok for doing sync (e.g. not in battery saver mode).
  bool is_battery_ok_ GUARDED_BY_CONTEXT(sequence_checker_) = true;

  // Is this the first full sync after the size estimation?
  bool is_first_sync_ GUARDED_BY_CONTEXT(sequence_checker_) = false;

  // Stop at the `PinSomeFiles` stage during testing to perform assertions, this
  // should always be true and only overridden in browser tests.
  bool should_pin_files_for_testing_ GUARDED_BY_CONTEXT(sequence_checker_) =
      true;

  SpaceGetter space_getter_ GUARDED_BY_CONTEXT(sequence_checker_);
  CompletionCallback completion_callback_ GUARDED_BY_CONTEXT(sequence_checker_);

  Progress progress_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ObserverList<Observer> observers_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::ElapsedTimer timer_ GUARDED_BY_CONTEXT(sequence_checker_);
  base::ElapsedTimer progress_timer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Stable IDs of items that got listed during the kListingFiles stage. The
  // mapped ID is the stable ID of the parent directory in which the given item
  // was first encountered.
  using ListedItems = std::unordered_map<Id, Id>;
  ListedItems listed_items_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Stable IDs of the files to pin, and which are not already marked as pinned.
  std::unordered_set<Id> files_to_pin_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Map that tracks the in-progress files indexed by their stable ID. This
  // contains all the files, either pinned or not, that are not completely
  // cached yet.
  Files files_to_track_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Tracks the remaining seconds for the current syncing operation to complete.
  file_manager::Speedometer speedometer_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Tracks the last time a LOG was output indicating the listing files stage is
  // taking a long time. Used to avoid emitting the WARNING log too frequently.
  base::Time last_long_listing_files_warning_time_;

  GUARDED_BY_CONTEXT(sequence_checker_)
  base::ScopedObservation<chromeos::PowerManagerClient,
                          chromeos::PowerManagerClient::Observer>
      power_manager_{this};

  GUARDED_BY_CONTEXT(sequence_checker_)
  base::ScopedObservation<ash::UserDataAuthClient,
                          ash::UserDataAuthClient::Observer>
      user_data_auth_client_{this};

  GUARDED_BY_CONTEXT(sequence_checker_)
  base::ScopedObservation<ash::SpacedClient, ash::SpacedClient::Observer>
      spaced_client_{this};

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<PinningManager> weak_ptr_factory_{this};

  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, Add);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, Update);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, Remove);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, OnSyncingEvent);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, OnItemProgress);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, CanPin);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, OnFileCreated);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, OnFileModified);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, OnFileDeleted);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, OnFilePinned);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, OnFilesChanged);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, OnMetadataForCreatedFile);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest,
                           OnMetadataForModifiedFile);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, CheckFreeSpace);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, CannotGetFreeSpace2);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, NotEnoughSpace2);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, NotEnoughSpace3);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, OnSpaceUpdate);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, StartMonitoringSpace);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, SetOnlineAndBatteryOk);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, OnTransientError);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, OnError);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, StartWhenInProgress);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, StartPinning);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, PinSomeFiles);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, CheckStalledFiles);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, NotifyProgress);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, OnSearchResult);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, HandleQueryItem);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, DropQuery);
  FRIEND_TEST_ALL_PREFIXES(DriveFsPinningManagerTest, CalculateRequiredSpace);
};

COMPONENT_EXPORT(CHROMEOS_ASH_COMPONENTS_DRIVEFS)
std::ostream& operator<<(std::ostream& out, PinningManager::Id id);

}  // namespace drivefs::pinning

#endif  // CHROMEOS_ASH_COMPONENTS_DRIVEFS_DRIVEFS_PINNING_MANAGER_H_
