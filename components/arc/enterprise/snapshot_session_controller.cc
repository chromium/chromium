// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/arc/enterprise/snapshot_session_controller.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/user_manager.h"

namespace arc {
namespace data_snapshotd {

namespace {

// The maximum duration of all required apps being installed.
const base::TimeDelta kDuration = base::TimeDelta::FromMinutes(5);

// This class tracks a user session lifetime and notifies its observers about
// the appropriate session state changes.
class SnapshotSessionControllerImpl final
    : public SnapshotSessionController,
      public session_manager::SessionManagerObserver {
 public:
  explicit SnapshotSessionControllerImpl(ArcAppsTracker* apps_tracker);
  SnapshotSessionControllerImpl(const SnapshotSessionControllerImpl&) = delete;
  SnapshotSessionControllerImpl& operator=(
      const SnapshotSessionControllerImpl&) = delete;
  ~SnapshotSessionControllerImpl() override;

  // SnapshotSessionController overrides:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;

  const base::OneShotTimer* get_timer_for_testing() const override {
    return &duration_timer_;
  }

  // session_manager::SessionManagerObserver overrides:
  void OnSessionStateChanged() override;

 private:
  // Calls StartSession() is MGS is active.
  bool MaybeStartSession();

  void StartSession();
  void StopSession();

  // Callback to be passed to |apps_tracker_|.
  void OnAppInstalled(int percent);
  // Called back once the session duration exceeds the maximum duration.
  void OnTimerFired();

  void NotifySnapshotSessionStarted();
  void NotifySnapshotSessionStopped();
  void NotifySnapshotSessionFailed();
  void NotifySnapshotAppInstalled(int percent);

  // Owned by ArcDataSnapshotdManager.
  ArcAppsTracker* apps_tracker_;

  base::OneShotTimer duration_timer_;
  base::ObserverList<Observer> observers_;

  // True, if |apps_tracker_| notified 100% of required apps installed.
  // Note: the value never flips back to false.
  bool all_apps_installed_ = false;

  base::WeakPtrFactory<SnapshotSessionControllerImpl> weak_ptr_factory_{this};
};

}  // namespace

// static
std::unique_ptr<SnapshotSessionController> SnapshotSessionController::Create(
    ArcAppsTracker* apps_tracker) {
  return std::make_unique<SnapshotSessionControllerImpl>(apps_tracker);
}

const base::OneShotTimer* SnapshotSessionController::get_timer_for_testing()
    const {
  return nullptr;
}

SnapshotSessionController::~SnapshotSessionController() = default;

SnapshotSessionControllerImpl::SnapshotSessionControllerImpl(
    ArcAppsTracker* apps_tracker)
    : apps_tracker_(apps_tracker) {
  session_manager::SessionManager::Get()->AddObserver(this);
  // Start tracking apps for active MGS.
  ignore_result(MaybeStartSession());
}

SnapshotSessionControllerImpl::~SnapshotSessionControllerImpl() {
  session_manager::SessionManager::Get()->RemoveObserver(this);
}

void SnapshotSessionControllerImpl::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void SnapshotSessionControllerImpl::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void SnapshotSessionControllerImpl::OnSessionStateChanged() {
  if (!MaybeStartSession())
    StopSession();
}

bool SnapshotSessionControllerImpl::MaybeStartSession() {
  if (user_manager::UserManager::Get() &&
      user_manager::UserManager::Get()->IsLoggedInAsPublicAccount()) {
    StartSession();
    return true;
  }
  return false;
}
void SnapshotSessionControllerImpl::StartSession() {
  DCHECK(!duration_timer_.IsRunning());

  duration_timer_.Start(
      FROM_HERE, kDuration,
      base::BindOnce(&SnapshotSessionControllerImpl::OnTimerFired,
                     weak_ptr_factory_.GetWeakPtr()));
  apps_tracker_->StartTracking(
      base::BindRepeating(&SnapshotSessionControllerImpl::OnAppInstalled,
                          weak_ptr_factory_.GetWeakPtr()));

  NotifySnapshotSessionStarted();
}

void SnapshotSessionControllerImpl::StopSession() {
  if (all_apps_installed_) {
    NotifySnapshotSessionStopped();
  } else {
    DCHECK(duration_timer_.IsRunning());
    duration_timer_.Stop();

    apps_tracker_->StopTracking();
    NotifySnapshotSessionFailed();
  }
}

void SnapshotSessionControllerImpl::OnAppInstalled(int percent) {
  if (percent == 100) {
    all_apps_installed_ = true;
    apps_tracker_->StopTracking();
    duration_timer_.Stop();
  }
  NotifySnapshotAppInstalled(percent);
}

void SnapshotSessionControllerImpl::OnTimerFired() {
  DCHECK(!all_apps_installed_);

  apps_tracker_->StopTracking();
  NotifySnapshotSessionFailed();
}

void SnapshotSessionControllerImpl::NotifySnapshotSessionStarted() {
  for (auto& observer : observers_)
    observer.OnSnapshotSessionStarted();
}

void SnapshotSessionControllerImpl::NotifySnapshotSessionStopped() {
  for (auto& observer : observers_)
    observer.OnSnapshotSessionStopped();
}

void SnapshotSessionControllerImpl::NotifySnapshotSessionFailed() {
  for (auto& observer : observers_)
    observer.OnSnapshotSessionFailed();
}

void SnapshotSessionControllerImpl::NotifySnapshotAppInstalled(int percent) {
  for (auto& observer : observers_)
    observer.OnSnapshotAppInstalled(percent);
}

}  // namespace data_snapshotd
}  // namespace arc
