// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_SESSIONS_CROSS_DEVICE_TABS_SERVICE_H_
#define COMPONENTS_SYNC_SESSIONS_CROSS_DEVICE_TABS_SERVICE_H_

#include <optional>
#include <string_view>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/sync_device_info/remote_device_info.h"
#include "components/sync_sessions/session_sync_prefs.h"

namespace sessions {
struct SessionTab;
}  // namespace sessions

namespace sync_sessions {

// Profile-scoped service providing a unified view of cross-device tab
// eligibility, active remote devices, and visible foreign tabs for
// desktop UI surfaces. Must be used on the UI thread.
class CrossDeviceTabsService : public KeyedService {
 public:
  // High-level availability state for cross-device tab UIs, evaluated as a
  // prioritized list in declaration order (the first matching state wins).
  enum class Status {
    // Unavailable (disabled by policy/settings, missing dependency, or shut
    // down).
    kUnavailable,
    // Signed out (`Snapshot::promo_account` is populated when an
    // eligible web-signed-in account is available).
    kSignedOut,
    // Primary account requires reauthentication.
    kSignInPending,
    // Signed in, but local tab sync is off (not opted in).
    kLocalTabSyncOff,
    // No active remote devices exist (or session sync is not yet ready).
    kNoOtherDevices,
    // All active remote devices are hidden locally.
    kAllDevicesHidden,
    // At least one visible remote device exists (check per-device
    // `tab_sync_state` and `open_tabs`).
    kAvailable,
  };

  // Tab-sync state of an individual remote device.
  enum class RemoteDeviceTabSyncState {
    // Tab sync is disabled on the remote device.
    kDisabled,
    // Tab sync is enabled (or unknown) on the remote device.
    kEnabled,
  };

  // Metadata, tab-sync status, and open tabs for one visible remote device.
  struct RemoteDeviceTabsEntry {
    RemoteDeviceTabsEntry();
    RemoteDeviceTabsEntry(const RemoteDeviceTabsEntry&);
    RemoteDeviceTabsEntry& operator=(const RemoteDeviceTabsEntry&);
    RemoteDeviceTabsEntry(RemoteDeviceTabsEntry&&) noexcept;
    RemoteDeviceTabsEntry& operator=(RemoteDeviceTabsEntry&&) noexcept;
    ~RemoteDeviceTabsEntry();

    syncer::RemoteDeviceInfo device;
    RemoteDeviceTabSyncState tab_sync_state =
        RemoteDeviceTabSyncState::kEnabled;
    // Non-owning `SessionTab` pointers valid only during the current task.
    std::vector<raw_ptr<const sessions::SessionTab>> open_tabs;
  };

  // Metadata and local visibility state for one active remote device.
  struct RemoteDeviceVisibilityEntry {
    RemoteDeviceVisibilityEntry();
    RemoteDeviceVisibilityEntry(const RemoteDeviceVisibilityEntry&);
    RemoteDeviceVisibilityEntry& operator=(const RemoteDeviceVisibilityEntry&);
    RemoteDeviceVisibilityEntry(RemoteDeviceVisibilityEntry&&) noexcept;
    RemoteDeviceVisibilityEntry& operator=(
        RemoteDeviceVisibilityEntry&&) noexcept;
    ~RemoteDeviceVisibilityEntry();

    syncer::RemoteDeviceInfo device;
    SessionVisibility visibility = SessionVisibility::kVisible;
  };

  // Point-in-time snapshot of cross-device tab state. Do not cache across
  // tasks, Sync updates, or `Shutdown()`.
  struct Snapshot {
    Snapshot();
    Snapshot(const Snapshot&);
    Snapshot& operator=(const Snapshot&);
    Snapshot(Snapshot&&) noexcept;
    Snapshot& operator=(Snapshot&&) noexcept;
    ~Snapshot();

    Status status = Status::kUnavailable;
    std::optional<AccountInfo> promo_account;
    std::vector<RemoteDeviceTabsEntry> visible_devices;
  };

  // Observer notified on state changes and service shutdown.
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnCrossDeviceTabsChanged();
    virtual void OnCrossDeviceTabsServiceShutdown();

   protected:
    ~Observer() override;
  };

  CrossDeviceTabsService();
  CrossDeviceTabsService(const CrossDeviceTabsService&) = delete;
  CrossDeviceTabsService& operator=(const CrossDeviceTabsService&) = delete;
  ~CrossDeviceTabsService() override;

  // Resolves the current cross-device tab snapshot and visible remote devices.
  virtual Snapshot GetSnapshot() const = 0;

  // Returns all active remote devices in recency order, including
  // hidden devices.
  virtual std::vector<RemoteDeviceVisibilityEntry> GetAllRemoteDevices()
      const = 0;

  // Updates local visibility for `cache_guid` (matching
  // `syncer::RemoteDeviceInfo::cache_guid`).
  virtual void SetDeviceVisibility(std::string_view cache_guid,
                                   SessionVisibility visibility) = 0;

  // Registers or unregisters `observer` for coarse-grained state updates.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;
};

}  // namespace sync_sessions

#endif  // COMPONENTS_SYNC_SESSIONS_CROSS_DEVICE_TABS_SERVICE_H_
