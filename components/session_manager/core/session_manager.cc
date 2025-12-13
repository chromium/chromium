// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/session_manager/core/session_manager.h"

#include <algorithm>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/logging.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "components/session_manager/core/session.h"
#include "components/session_manager/core/session_manager_delegate.h"
#include "components/session_manager/core/session_manager_observer.h"
#include "components/user_manager/user_manager.h"

namespace session_manager {

// static
SessionManager* SessionManager::instance = nullptr;

SessionManager::SessionManager(
    std::unique_ptr<session_manager::SessionManagerDelegate> delegate)
    : delegate_(std::move(delegate)) {
  CHECK(delegate_);
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
                                   const std::string& username_hash,
                                   bool new_user,
                                   bool has_active_session) {
  // For secondary user log-in in common cases, we switch the active
  // session after user Profile is created, so data needed for UI update,
  // such as wallpaper, can be ready on activation.
  // We do not switch if this is for recovering multi-sign-in user sessions
  // because in the case we do not switch every user on starting, but
  // only for the last active user after all sessions are created.
  if (!has_active_session && !sessions_.empty()) {
    pending_active_account_id_ = user_account_id;
  }
  CreateSessionInternal(user_account_id, username_hash, new_user,
                        /*browser_restart=*/false);
}

void SessionManager::CreateSessionForRestart(const AccountId& user_account_id,
                                             const std::string& username_hash,
                                             bool new_user) {
  CreateSessionInternal(user_account_id, username_hash, new_user,
                        /*browser_restart=*/true);
}

void SessionManager::SwitchActiveSession(const AccountId& account_id) {
  CHECK(user_manager_);
  CHECK(HasSessionForAccountId(account_id));
  user_manager_->SwitchActiveUser(account_id);
}

void SessionManager::RequestSignOut() {
  delegate_->RequestSignOut();
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

bool SessionManager::HasSessionForAccountId(const AccountId& account_id) const {
  return FindSession(account_id) != nullptr;
}

const Session* SessionManager::FindSession(const AccountId& account_id) const {
  auto it = std::ranges::find(sessions_, account_id, [](const auto& session) {
    return session->account_id();
  });
  return it == sessions_.end() ? nullptr : it->get();
}

const Session* SessionManager::GetActiveSession() const {
  CHECK(user_manager_);
  const auto* active_user = user_manager_->GetActiveUser();
  if (!active_user) {
    return nullptr;
  }
  return FindSession(active_user->GetAccountId());
}

const Session* SessionManager::GetPrimarySession() const {
  CHECK(user_manager_);
  const auto* primary_user = user_manager_->GetPrimaryUser();
  CHECK_EQ(!!primary_user, !sessions_.empty());
  if (sessions_.empty()) {
    return nullptr;
  }
  const Session* primary_session = sessions_[0].get();
  CHECK_EQ(primary_user->GetAccountId(), primary_session->account_id());
  return primary_session;
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

void SessionManager::HandleUserSessionStartUpTaskCompleted() {
  // This method must not be called twice.
  CHECK(!user_session_start_up_task_completed_);
  user_session_start_up_task_completed_ = true;
  for (auto& observer : observers_) {
    observer.OnUserSessionStartUpTaskCompleted();
  }
}

// Note: there're *two* types of timing that are considered "profile created".
// 1) ProfileManager has responsibility to create a Profile
// then it invokes ProfileManagerObserver::OnProfileAdded().
// UserManager::OnUserProfileCreated() is called at the timing.
// 2) After finishing Profile creation, there are several more tasks to
// run for ash-chrome's Profile initialization. After all the tasks
// UserSessionManager (indirectly) calls NotifyUserProfileLoaded(). This
// practically happens after 1).
// The gap of the timing looks source of the confusion. We probably want to
// revisit here to unify the timing of "profile creation completion".
void SessionManager::OnUserProfileCreated(const user_manager::User& user) {
  if (pending_active_account_id_.is_valid()) {
    user_manager_->SwitchActiveUser(
        std::exchange(pending_active_account_id_, EmptyAccountId()));
  }
}

// static
void SessionManager::SetInstance(SessionManager* session_manager) {
  SessionManager::instance = session_manager;
}

void SessionManager::CreateSessionInternal(const AccountId& user_account_id,
                                           const std::string& username_hash,
                                           bool new_user,
                                           bool browser_restart) {
  CHECK(user_manager_);
  DCHECK(!HasSessionForAccountId(user_account_id));

  const auto& user = CHECK_DEREF(user_manager_->FindUser(user_account_id));

  // TODO(crbug.com/278643115): This attribute looks like the one for Session
  // rather than UserManager. Move the field.
  // Note: For KioskApp user, this may be updated later in UserSessionManager.
  user_manager_->SetIsCurrentUserNew(
      (new_user && user.HasGaiaAccount()) ||
      user.GetType() == user_manager::UserType::kPublicAccount);

  observers_.Notify(&SessionManagerObserver::OnSessionCreationStarted,
                    user_account_id);
  sessions_.push_back(std::make_unique<Session>(next_id_++, user_account_id));
  user_manager_->UserLoggedIn(user_account_id, username_hash);
  // The created sessions and logged-in users in UserManager should be the
  // same list.
  const auto& logged_in_users = user_manager_->GetLoggedInUsers();
  CHECK_EQ(sessions_.size(), logged_in_users.size());
  for (size_t i = 0; i < sessions_.size(); ++i) {
    CHECK_EQ(sessions_[i]->account_id(), logged_in_users[i]->GetAccountId());
  }
  OnSessionCreated(browser_restart);
  observers_.Notify(&SessionManagerObserver::OnSessionCreated, user_account_id);
}

}  // namespace session_manager
