// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/enterprise/arc_data_snapshotd_manager.h"

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chromeos/constants/chromeos_switches.h"
#include "chromeos/dbus/upstart/upstart_client.h"
#include "components/arc/arc_prefs.h"
#include "components/arc/enterprise/arc_data_snapshotd_bridge.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"
#include "ui/ozone/public/ozone_switches.h"

namespace arc {
namespace data_snapshotd {

namespace {

// SnapshotInfo related keys.
constexpr char kOsVersion[] = "os_version";
constexpr char kCreationDate[] = "creation_date";
constexpr char kVerified[] = "verified";
constexpr char kUpdated[] = "updated";

// Snapshot related keys.
constexpr char kPrevious[] = "previous";
constexpr char kLast[] = "last";
constexpr char kBlockedUiReboot[] = "blocked_ui_reboot";
constexpr char kStarted[] = "started";

bool IsSnapshotEnabled() {
  // TODO(pbond): implement policy processing.
  return ArcDataSnapshotdManager::is_snapshot_enabled_for_testing();
}

// Returns true if the Chrome session is restored after crash.
bool IsRestoredSession() {
  auto* command_line = base::CommandLine::ForCurrentProcess();

  return command_line->HasSwitch(chromeos::switches::kLoginUser) &&
         !command_line->HasSwitch(chromeos::switches::kLoginManager);
}

// Enables ozone platform headless via command line.
void EnableHeadlessMode() {
  auto* command_line = base::CommandLine::ForCurrentProcess();
  command_line->AppendSwitchASCII(switches::kOzonePlatform, "headless");
}

}  // namespace

bool ArcDataSnapshotdManager::is_snapshot_enabled_for_testing_ = false;

// This class is owned by ChromeBrowserMainPartsChromeos.
static ArcDataSnapshotdManager* g_arc_data_snapshotd_manager = nullptr;

ArcDataSnapshotdManager::SnapshotInfo::SnapshotInfo(const base::Value* value,
                                                    bool last)
    : is_last_(last) {
  const base::DictionaryValue* dict;
  if (!value || !value->GetAsDictionary(&dict) || !dict)
    return;
  {
    auto* found = dict->FindStringPath(kOsVersion);
    if (found)
      os_version_ = *found;
  }
  {
    auto* found = dict->FindStringPath(kCreationDate);
    if (found)
      creation_date_ = *found;
  }
  {
    auto found = dict->FindBoolPath(kVerified);
    if (found.has_value())
      verified_ = found.value();
  }

  {
    auto found = dict->FindBoolPath(kUpdated);
    if (found.has_value())
      updated_ = found.value();
  }
}

ArcDataSnapshotdManager::SnapshotInfo::~SnapshotInfo() = default;

// static
std::unique_ptr<ArcDataSnapshotdManager::SnapshotInfo>
ArcDataSnapshotdManager::SnapshotInfo::CreateForTesting(
    const std::string& os_version,
    const std::string& creation_date,
    bool verified,
    bool updated,
    bool last) {
  return base::WrapUnique(new ArcDataSnapshotdManager::SnapshotInfo(
      os_version, creation_date, verified, updated, last));
}

void ArcDataSnapshotdManager::SnapshotInfo::Sync(base::Value* dict) {
  if (!dict)
    return;

  base::DictionaryValue value;
  value.SetStringKey(kOsVersion, os_version_);
  value.SetStringKey(kCreationDate, creation_date_);
  value.SetBoolKey(kVerified, verified_);
  value.SetBoolKey(kUpdated, updated_);

  dict->SetKey(GetDictPath(), std::move(value));
}

bool ArcDataSnapshotdManager::SnapshotInfo::IsExpired() const {
  // TODO(pbond): implement;
  return false;
}

bool ArcDataSnapshotdManager::SnapshotInfo::IsOsVersionUpdated() const {
  // TODO(pbond): implement;
  return false;
}

ArcDataSnapshotdManager::SnapshotInfo::SnapshotInfo(
    const std::string& os_version,
    const std::string& creation_date,
    bool verified,
    bool updated,
    bool last)
    : is_last_(last),
      os_version_(os_version),
      creation_date_(creation_date),
      verified_(verified),
      updated_(updated) {}

std::string ArcDataSnapshotdManager::SnapshotInfo::GetDictPath() const {
  return is_last_ ? kLast : kPrevious;
}

ArcDataSnapshotdManager::Snapshot::Snapshot(PrefService* local_state)
    : local_state_(local_state) {
  DCHECK(local_state_);
}

ArcDataSnapshotdManager::Snapshot::~Snapshot() = default;

// static
std::unique_ptr<ArcDataSnapshotdManager::Snapshot>
ArcDataSnapshotdManager::Snapshot::CreateForTesting(
    PrefService* local_state,
    bool blocked_ui_mode,
    bool started,
    std::unique_ptr<SnapshotInfo> last,
    std::unique_ptr<SnapshotInfo> previous) {
  return base::WrapUnique(new ArcDataSnapshotdManager::Snapshot(
      local_state, blocked_ui_mode, started, std::move(last),
      std::move(previous)));
}

void ArcDataSnapshotdManager::Snapshot::Parse() {
  const base::DictionaryValue* dict =
      local_state_->GetDictionary(arc::prefs::kArcSnapshotInfo);
  if (!dict)
    return;
  {
    const auto* found = dict->FindDictPath(kPrevious);
    if (found)
      previous_ = std::make_unique<SnapshotInfo>(found, false);
  }
  {
    const auto* found = dict->FindDictPath(kLast);
    if (found)
      last_ = std::make_unique<SnapshotInfo>(found, true);
  }
  {
    auto found = dict->FindBoolPath(kBlockedUiReboot);
    if (found.has_value())
      blocked_ui_mode_ = found.value();
  }
  {
    auto found = dict->FindBoolPath(kStarted);
    if (found.has_value())
      started_ = found.value();
  }
}

void ArcDataSnapshotdManager::Snapshot::Sync() {
  base::DictionaryValue dict;
  if (previous_)
    previous_->Sync(&dict);
  if (last_)
    last_->Sync(&dict);
  dict.SetBoolKey(kBlockedUiReboot, blocked_ui_mode_);
  dict.SetBoolKey(kStarted, started_);
  local_state_->Set(arc::prefs::kArcSnapshotInfo, std::move(dict));
}

void ArcDataSnapshotdManager::Snapshot::ClearSnapshot(bool last) {
  std::unique_ptr<SnapshotInfo>* snapshot = (last ? &last_ : &previous_);
  snapshot->reset();
  Sync();
}

void ArcDataSnapshotdManager::Snapshot::StartNewSnapshot() {
  previous_ = std::move(last_);
  last_ = nullptr;

  started_ = true;
  Sync();
}

ArcDataSnapshotdManager::Snapshot::Snapshot(
    PrefService* local_state,
    bool blocked_ui_mode,
    bool started,
    std::unique_ptr<SnapshotInfo> last,
    std::unique_ptr<SnapshotInfo> previous)
    : local_state_(local_state),
      blocked_ui_mode_(blocked_ui_mode),
      started_(started),
      last_(std::move(last)),
      previous_(std::move(previous)) {
  DCHECK(local_state_);
}

// static
ArcDataSnapshotdManager* ArcDataSnapshotdManager::Get() {
  return g_arc_data_snapshotd_manager;
}

ArcDataSnapshotdManager::ArcDataSnapshotdManager(
    PrefService* local_state,
    base::OnceClosure attempt_user_exit_callback)
    : snapshot_{local_state},
      attempt_user_exit_callback_(std::move(attempt_user_exit_callback)) {
  DCHECK(!g_arc_data_snapshotd_manager);
  DCHECK(local_state);
  g_arc_data_snapshotd_manager = this;

  snapshot_.Parse();

  if (IsRestoredSession()) {
    state_ = State::kRestored;
  } else {
    if (snapshot_.is_blocked_ui_mode() && IsSnapshotEnabled()) {
      state_ = State::kBlockedUi;
      EnableHeadlessMode();
    }
  }
  // Ensure the snapshot's info is up-to-date.
  DoClearSnapshots();
}

ArcDataSnapshotdManager::~ArcDataSnapshotdManager() {
  DCHECK(g_arc_data_snapshotd_manager);
  g_arc_data_snapshotd_manager = nullptr;

  session_manager::SessionManager::Get()->RemoveObserver(this);

  snapshot_.Sync();
  EnsureDaemonStopped(base::DoNothing());
}

void ArcDataSnapshotdManager::EnsureDaemonStarted(base::OnceClosure callback) {
  if (bridge_) {
    std::move(callback).Run();
    return;
  }
  VLOG(1) << "Starting arc-data-snapshotd";
  daemon_weak_ptr_factory_.InvalidateWeakPtrs();
  chromeos::UpstartClient::Get()->StartArcDataSnapshotd(base::BindOnce(
      &ArcDataSnapshotdManager::OnDaemonStarted,
      daemon_weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcDataSnapshotdManager::EnsureDaemonStopped(base::OnceClosure callback) {
  if (!bridge_) {
    std::move(callback).Run();
    return;
  }
  StopDaemon(std::move(callback));
}

bool ArcDataSnapshotdManager::IsAutoLoginConfigured() {
  switch (state_) {
    case ArcDataSnapshotdManager::State::kBlockedUi:
    case ArcDataSnapshotdManager::State::kMgsToLaunch:
    case ArcDataSnapshotdManager::State::kMgsLaunched:
      return true;
    case ArcDataSnapshotdManager::State::kNone:
    case ArcDataSnapshotdManager::State::kRestored:
      return false;
  }
}

bool ArcDataSnapshotdManager::IsAutoLoginAllowed() {
  switch (state_) {
    case ArcDataSnapshotdManager::State::kBlockedUi:
      return false;
    case ArcDataSnapshotdManager::State::kNone:
    case ArcDataSnapshotdManager::State::kRestored:
    case ArcDataSnapshotdManager::State::kMgsLaunched:
    case ArcDataSnapshotdManager::State::kMgsToLaunch:
      return true;
  }
}

void ArcDataSnapshotdManager::OnSessionStateChanged() {
  if (state_ != State::kMgsToLaunch)
    return;
  if (user_manager::UserManager::Get()->IsLoggedInAsPublicAccount())
    state_ = State::kMgsLaunched;
}

void ArcDataSnapshotdManager::StopDaemon(base::OnceClosure callback) {
  VLOG(1) << "Stopping arc-data-snapshotd";
  daemon_weak_ptr_factory_.InvalidateWeakPtrs();
  chromeos::UpstartClient::Get()->StopArcDataSnapshotd(base::BindOnce(
      &ArcDataSnapshotdManager::OnDaemonStopped,
      daemon_weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void ArcDataSnapshotdManager::DoClearSnapshots() {
  DoClearSnapshot(
      snapshot_.previous(),
      base::BindOnce(
          &ArcDataSnapshotdManager::DoClearSnapshot,
          weak_ptr_factory_.GetWeakPtr(), snapshot_.last(),
          base::BindOnce(&ArcDataSnapshotdManager::OnSnapshotsCleared,
                         weak_ptr_factory_.GetWeakPtr())),
      true /* success */);
}

void ArcDataSnapshotdManager::DoClearSnapshot(
    SnapshotInfo* snapshot,
    base::OnceCallback<void(bool)> callback,
    bool success) {
  if (!success)
    LOG(ERROR) << "Failed to clear snapshot";
  if (snapshot && (!IsSnapshotEnabled() || snapshot->IsExpired() ||
                   snapshot->IsOsVersionUpdated())) {
    EnsureDaemonStarted(base::BindOnce(
        &ArcDataSnapshotdManager::ClearSnapshot, weak_ptr_factory_.GetWeakPtr(),
        snapshot->is_last(), std::move(callback)));
    snapshot_.ClearSnapshot(snapshot->is_last());
  } else {
    std::move(callback).Run(success);
  }
}

void ArcDataSnapshotdManager::GenerateKeyPair() {
  DCHECK(bridge_);
  bridge_->GenerateKeyPair(
      base::BindOnce(&ArcDataSnapshotdManager::OnKeyPairGenerated,
                     weak_ptr_factory_.GetWeakPtr()));
}

void ArcDataSnapshotdManager::ClearSnapshot(
    bool last,
    base::OnceCallback<void(bool)> callback) {
  DCHECK(bridge_);
  bridge_->ClearSnapshot(last, std::move(callback));
}

void ArcDataSnapshotdManager::OnSnapshotsCleared(bool success) {
  switch (state_) {
    case State::kBlockedUi:
      EnsureDaemonStarted(
          base::BindOnce(&ArcDataSnapshotdManager::GenerateKeyPair,
                         weak_ptr_factory_.GetWeakPtr()));
      return;
    case State::kNone:
    case State::kRestored:
      StopDaemon(base::DoNothing());
      return;
    case State::kMgsToLaunch:
    case State::kMgsLaunched:
      LOG(WARNING) << "Snapshots are cleared while in incorrect state";
      return;
  }
}

void ArcDataSnapshotdManager::OnKeyPairGenerated(bool success) {
  if (success) {
    VLOG(1) << "Managed Guest Session is ready to be started with blocked UI.";
    state_ = State::kMgsToLaunch;
    session_manager::SessionManager::Get()->AddObserver(this);
    // Move last to previous snapshot:
    snapshot_.StartNewSnapshot();

    if (!reset_autologin_callback_.is_null())
      std::move(reset_autologin_callback_).Run();
  } else {
    LOG(ERROR) << "Key pair generation failed. Abort snapshot creation.";

    snapshot_.set_blocked_ui_mode(false);
    snapshot_.Sync();

    DCHECK(!attempt_user_exit_callback_.is_null());
    EnsureDaemonStopped(std::move(attempt_user_exit_callback_));
  }
}

void ArcDataSnapshotdManager::OnDaemonStarted(base::OnceClosure callback,
                                              bool success) {
  if (!success) {
    DLOG(ERROR) << "Failed to start arc-data-snapshotd, it might be already "
                << "running";
  } else {
    VLOG(1) << "arc-data-snapshotd started";
  }

  // The bridge has to be created regardless of a |success| value. When
  // arc-data-snapshotd is already running, it responds with an error on
  // attempt to start it.
  if (!bridge_) {
    bridge_ = std::make_unique<ArcDataSnapshotdBridge>(std::move(callback));
    DCHECK(bridge_);
  } else {
    std::move(callback).Run();
  }
}

void ArcDataSnapshotdManager::OnDaemonStopped(base::OnceClosure callback,
                                              bool success) {
  if (!success) {
    DLOG(ERROR) << "Failed to stop arc-data-snapshotd, it might be already "
                << "stopped";
  } else {
    VLOG(1) << "arc-data-snapshotd stopped";
  }
  bridge_.reset();
  std::move(callback).Run();
}

}  // namespace data_snapshotd
}  // namespace arc
