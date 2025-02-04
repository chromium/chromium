// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/session_manager/core/session_manager.h"

#include "base/check.h"
#include "base/containers/contains.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"

namespace session_manager {

// static
SessionManager* SessionManager::instance = nullptr;

SessionManager::SessionManager() {
  DCHECK(!SessionManager::Get());
  SessionManager::SetInstance(this);
}

SessionManager::~SessionManager() {
  DCHECK_EQ(instance, this);
  SessionManager::SetInstance(nullptr);
}

// static
SessionManager* SessionManager::Get() {
  return SessionManager::instance;
}

void SessionManager::SetSessionState(SessionState state) {
  if (session_state_ == state)
    return;

  VLOG(1) << "Changing session state to: " << static_cast<int>(state);

  session_state_ = state;
  for (auto& observer : observers_)
    observer.OnSessionStateChanged();
}

void SessionManager::CreateSession(const AccountId& user_account_id,
                                   const std::string& user_id_hash,
                                   bool is_child) {
  CreateSessionInternal(user_account_id, user_id_hash,
                        false /* browser_restart */, is_child);
}

void SessionManager::CreateSessionForRestart(const AccountId& user_account_id,
                                             const std::string& user_id_hash) {
  CHECK(user_manager_);
  const user_manager::User* user = user_manager_->FindUser(user_account_id);
  // Tests do not always create users.
  const bool is_child =
      user && user->GetType() == user_manager::UserType::kChild;
  CreateSessionInternal(user_account_id, user_id_hash,
                        true /* browser_restart */, is_child);
}

void SessionManager::OnUserManagerCreated(
    user_manager::UserManager* user_manager) {
  user_manager_ = user_manager;
  user_manager_observation_.Observe(user_manager_);
}

bool SessionManager::IsSessionStarted() const {
  return session_started_;
}

bool SessionManager::IsUserSessionStartUpTaskCompleted() const {
  return user_session_start_up_task_completed_;
}

void SessionManager::SessionStarted() {
  TRACE_EVENT0("login", "SessionManager::SessionStarted");
  session_started_ = true;

  bool is_primary = sessions_.size() == 1;
  for (auto& observer : observers_)
    observer.OnUserSessionStarted(is_primary);
}

bool SessionManager::HasSessionForAccountId(
    const AccountId& user_account_id) const {
  return base::Contains(sessions_, user_account_id, [](const auto& session) {
    return session->account_id();
  });
}

bool SessionManager::IsInSecondaryLoginScreen() const {
  return session_state_ == SessionState::LOGIN_SECONDARY;
}

bool SessionManager::IsScreenLocked() const {
  return session_state_ == SessionState::LOCKED;
}

bool SessionManager::IsUserSessionBlocked() const {
  return session_state_ != SessionState::ACTIVE;
}

void SessionManager::AddObserver(SessionManagerObserver* observer) {
  observers_.AddObserver(observer);
}

void SessionManager::RemoveObserver(SessionManagerObserver* observer) {
  observers_.RemoveObserver(observer);
}

void SessionManager::NotifyUserProfileLoaded(const AccountId& account_id) {
  for (auto& observer : observers_)
    observer.OnUserProfileLoaded(account_id);
}

void SessionManager::NotifyLoginOrLockScreenVisible() {
  login_or_lock_screen_shown_for_test_ = true;
  for (auto& observer : observers_)
    observer.OnLoginOrLockScreenVisible();
}

void SessionManager::NotifyUnlockAttempt(const bool success,
                                         const UnlockType unlock_type) {
  for (auto& observer : observers_)
    observer.OnUnlockScreenAttempt(success, unlock_type);
}

void SessionManager::NotifyUserLoggedIn(const AccountId& user_account_id,
                                        const std::string& user_id_hash,
                                        bool browser_restart,
                                        bool is_child) {
  CHECK(user_manager_);
  user_manager_->UserLoggedIn(user_account_id, user_id_hash, browser_restart,
                              is_child);
}

void SessionManager::HandleUserSessionStartUpTaskCompleted() {
  // This method must not be called twice.
  CHECK(!user_session_start_up_task_completed_);
  user_session_start_up_task_completed_ = true;
  for (auto& observer : observers_) {
    observer.OnUserSessionStartUpTaskCompleted();
  }
}

// static
void SessionManager::SetInstance(SessionManager* session_manager) {
  SessionManager::instance = session_manager;
}

void SessionManager::CreateSessionInternal(const AccountId& user_account_id,
                                           const std::string& user_id_hash,
                                           bool browser_restart,
                                           bool is_child) {
  DCHECK(!HasSessionForAccountId(user_account_id));
  sessions_.push_back(std::make_unique<Session>(next_id_++, user_account_id));
  NotifyUserLoggedIn(user_account_id, user_id_hash, browser_restart, is_child);
}

}  // namespace session_manager
