// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/session/test_session_controller.h"

#include <utility>

#include "ash/public/cpp/session/session_observer.h"
#include "base/functional/callback.h"

TestSessionController::TestSessionController() = default;
TestSessionController::~TestSessionController() = default;

void TestSessionController::SetScreenLocked(bool locked) {
  is_screen_locked_ = locked;
  for (auto& observer : observers_)
    observer.OnLockStateChanged(locked);
}

void TestSessionController::SetClient(ash::SessionControllerClient* client) {}

void TestSessionController::SetSessionInfo(const ash::SessionInfo& info) {
  last_session_info_ = info;
}

void TestSessionController::UpdateUserSession(
    const ash::UserSession& user_session) {
  last_user_session_ = user_session;
  ++update_user_session_count_;
}

void TestSessionController::SetUserSessionOrder(
    const std::vector<uint32_t>& user_session_order) {
  ++set_user_session_order_count_;
}

void TestSessionController::PrepareForLock(PrepareForLockCallback callback) {
  std::move(callback).Run();
}

void TestSessionController::StartLock(StartLockCallback callback) {
  std::move(callback).Run(true);
}

void TestSessionController::NotifyChromeLockAnimationsComplete() {
  lock_animation_complete_call_count_++;
}

void TestSessionController::RunUnlockAnimation(
    RunUnlockAnimationCallback callback) {
  std::move(callback).Run(false);
}

void TestSessionController::NotifyChromeTerminating() {}

void TestSessionController::SetSessionLengthLimit(base::TimeDelta length_limit,
                                                  base::Time start_time) {
  last_session_length_limit_ = length_limit;
  last_session_start_time_ = start_time;
}

void TestSessionController::CanSwitchActiveUser(
    CanSwitchActiveUserCallback callback) {
  std::move(callback).Run(true);
}

void TestSessionController::ShowMultiprofilesIntroDialog(
    ShowMultiprofilesIntroDialogCallback callback) {
  std::move(callback).Run(true, false);
}

void TestSessionController::ShowTeleportWarningDialog(
    ShowTeleportWarningDialogCallback callback) {
  std::move(callback).Run(true, false);
}

void TestSessionController::ShowMultiprofilesSessionAbortedDialog(
    const std::string& user_email) {}

void TestSessionController::AddSessionActivationObserverForAccountId(
    const AccountId& account_id,
    ash::SessionActivationObserver* observer) {}

void TestSessionController::RemoveSessionActivationObserverForAccountId(
    const AccountId& account_id,
    ash::SessionActivationObserver* observer) {}

void TestSessionController::AddObserver(ash::SessionObserver* observer) {
  observers_.AddObserver(observer);
}

void TestSessionController::RemoveObserver(ash::SessionObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool TestSessionController::IsScreenLocked() const {
  return is_screen_locked_;
}

std::optional<int> TestSessionController::GetExistingUsersCount() const {
  return existing_users_count_;
}

void TestSessionController::NotifyFirstSessionReady() {
  ++first_session_ready_count_;
}

void TestSessionController::NotifyUserToBeRemoved(const AccountId& account_id) {
}
