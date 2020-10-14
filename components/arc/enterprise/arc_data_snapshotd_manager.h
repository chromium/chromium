// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_MANAGER_H_
#define COMPONENTS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_MANAGER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/session_manager/core/session_manager_observer.h"

class PrefService;

namespace base {

class Value;

}  // namespace base

namespace arc {
namespace data_snapshotd {

class ArcDataSnapshotdBridge;

// This class manages ARC data/ directory snapshots and controls the lifetime of
// the arc-data-snapshotd daemon.
class ArcDataSnapshotdManager final
    : public session_manager::SessionManagerObserver {
 public:
  // State of the flow.
  enum class State {
    kNone,
    // Blocked UI mode is ON.
    kBlockedUi,
    // In blocked UI mode, MGS can be launched.
    kMgsToLaunch,
    // MGS is launched to create a snapshot.
    kMgsLaunched,
    // User session was restored.
    kRestored,
  };

  // This class operates with a snapshot related info either last or
  // backed-up (previous): stores and keeps in sync with an appropriate
  // preference in local state.
  class SnapshotInfo {
   public:
    SnapshotInfo(const base::Value* value, bool last);
    SnapshotInfo(const SnapshotInfo&) = delete;
    SnapshotInfo& operator=(const SnapshotInfo&) = delete;
    ~SnapshotInfo();

    // Creates from the passed arguments instead of constructing it from
    // dictionary.
    static std::unique_ptr<SnapshotInfo> CreateForTesting(
        const std::string& os_version,
        const std::string& creation_date,
        bool verified,
        bool updated,
        bool last);

    // Syncs stored snapshot info to dictionaty |value|.
    void Sync(base::Value* value);

    // Returns true if snapshot is expired.
    bool IsExpired() const;

    // Returns true if OS version is updated, since the snapshot has been taken.
    bool IsOsVersionUpdated() const;

    bool is_last() const { return is_last_; }

   private:
    SnapshotInfo(const std::string& os_version,
                 const std::string& creation_date,
                 bool verified,
                 bool updated,
                 bool last);

    // Returns dictionary path in arc.snapshot local state preference.
    std::string GetDictPath() const;

    bool is_last_;

    // Values should be kept in sync with values stored in arc.snapshot.last or
    // arc.snapshot.previous preferences.
    std::string os_version_;
    std::string creation_date_;
    bool verified_ = false;
    bool updated_ = false;
  };

  // This class operates with a snapshot related info including mode and
  // creation flow time: stores and keeps in sync with arc.snapshot preference
  // in local state.
  class Snapshot {
   public:
    // Snapshot does not own |local_state|, it must be non nullptr and must
    // outlive the instance.
    explicit Snapshot(PrefService* local_state);

    Snapshot(const Snapshot&) = delete;
    Snapshot& operator=(const Snapshot&) = delete;
    ~Snapshot();

    // Creates an instance from the passed arguments instead of reading it from
    // |local_state|.
    static std::unique_ptr<Snapshot> CreateForTesting(
        PrefService* local_state,
        bool blocked_ui_mode,
        bool started,
        std::unique_ptr<SnapshotInfo> last,
        std::unique_ptr<SnapshotInfo> previous);

    // Parses the snapshot info from arc.snapshot preference.
    void Parse();
    // Syncs stored snapshot info to local state.
    void Sync();
    // Clears snapshot related info in arc.snapshot preference either last
    // if |last| is true or previous otherwise.
    void ClearSnapshot(bool last);

    // Moves last snapshot to previous and updates a |start_date| to the current
    // date.
    void StartNewSnapshot();

    void set_blocked_ui_mode(bool blocked_ui_mode) {
      blocked_ui_mode_ = blocked_ui_mode;
    }
    bool is_blocked_ui_mode() const { return blocked_ui_mode_; }
    bool started() const { return started_; }
    SnapshotInfo* last() { return last_.get(); }
    SnapshotInfo* previous() { return previous_.get(); }

   private:
    Snapshot(PrefService* local_state,
             bool blocked_ui_mode,
             bool started,
             std::unique_ptr<SnapshotInfo> last,
             std::unique_ptr<SnapshotInfo> previous);

    // Unowned pointer - outlives this instance.
    PrefService* const local_state_;

    // Values should be kept in sync with values stored in arc.snapshot
    // preference.
    bool blocked_ui_mode_ = false;
    bool started_;
    std::unique_ptr<SnapshotInfo> last_;
    std::unique_ptr<SnapshotInfo> previous_;
  };

  ArcDataSnapshotdManager(PrefService* local_state,
                          base::OnceClosure attempt_user_exit_callback);
  ArcDataSnapshotdManager(const ArcDataSnapshotdManager&) = delete;
  ArcDataSnapshotdManager& operator=(const ArcDataSnapshotdManager&) = delete;
  ~ArcDataSnapshotdManager() override;

  static ArcDataSnapshotdManager* Get();

  // Starts arc-data-snapshotd.
  void EnsureDaemonStarted(base::OnceClosure callback);
  // Stops arc-data-snapshotd.
  void EnsureDaemonStopped(base::OnceClosure callback);

  // Returns true if autologin to public account should be performed.
  bool IsAutoLoginConfigured();
  // Returns true if autologin is allowed to be performed and manager is not
  // waiting for the response from arc-data-snapshotd daemon.
  bool IsAutoLoginAllowed();

  // session_manager::SessionManagerObserver:
  void OnSessionStateChanged() override;

  // Get |bridge_| for testing.
  ArcDataSnapshotdBridge* bridge() { return bridge_.get(); }

  State state() const { return state_; }

  void set_reset_autologin_callback(base::OnceClosure callback) {
    reset_autologin_callback_ = std::move(callback);
  }

  static void set_snapshot_enabled_for_testing(bool enabled) {
    is_snapshot_enabled_for_testing_ = enabled;
  }
  static bool is_snapshot_enabled_for_testing() {
    return is_snapshot_enabled_for_testing_;
  }
  void set_state_for_testing(State state) { state_ = state; }

 private:
  // Attempts to arc-data-snapshotd daemon regardless of state of the class.
  // Runs |callback| once finished.
  void StopDaemon(base::OnceClosure callback);

  // Attempts to clear snapshots.
  void DoClearSnapshots();
  // Attempts to clear the passed snapshot, calls |callback| once finished.
  // |success| indicates a successfully or not the previous operation has been
  // finished.
  void DoClearSnapshot(SnapshotInfo* snapshot,
                       base::OnceCallback<void(bool)> callback,
                       bool success);

  // Delegates operations to |bridge_|
  void GenerateKeyPair();
  void ClearSnapshot(bool last, base::OnceCallback<void(bool)> callback);

  // Called once the outdated snapshots were removed or ensured that there are
  // no outdated snapshots.
  void OnSnapshotsCleared(bool success);
  // Called once GenerateKeyPair is finished with a result |success|.
  void OnKeyPairGenerated(bool success);
  // Called once arc-data-snapshotd starting process is finished with result
  // |success|, runs |callback| afterwards.
  void OnDaemonStarted(base::OnceClosure callback, bool success);
  // Called once arc-data-snapshotd stopping process is finished with result
  // |success", runs |callback| afterwards.
  void OnDaemonStopped(base::OnceClosure callback, bool success);

  static bool is_snapshot_enabled_for_testing_;
  State state_ = State::kNone;
  Snapshot snapshot_;

  std::unique_ptr<ArcDataSnapshotdBridge> bridge_;

  base::OnceClosure attempt_user_exit_callback_;

  // Callback to reset an autologin timer once userless MGS is ready to start.
  base::OnceClosure reset_autologin_callback_;

  // Used for cancelling previously posted tasks to daemon.
  base::WeakPtrFactory<ArcDataSnapshotdManager> daemon_weak_ptr_factory_{this};
  // WeakPtrFactory to use for callbacks.
  base::WeakPtrFactory<ArcDataSnapshotdManager> weak_ptr_factory_{this};
};

}  // namespace data_snapshotd
}  // namespace arc

#endif  // COMPONENTS_ARC_ENTERPRISE_ARC_DATA_SNAPSHOTD_MANAGER_H_
