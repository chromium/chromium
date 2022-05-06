// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/commands/web_app_command.h"

#include "base/atomic_sequence_num.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "components/services/storage/indexed_db/locks/leveled_lock_range.h"

namespace web_app {

namespace {
// Creates a `LeveledLockRange` that only includes the provided string `key`
content::LeveledLockRange StringToLockRange(std::string key) {
  return content::LeveledLockRange{key, key + static_cast<char>(0)};
}

enum KeysOnStaticLevel {
  kFullSystem = 0,
  kBackgroundWebContents = 1,
  kNoOp = 2,
};

}  // namespace

WebAppCommandLock::WebAppCommandLock(base::flat_set<AppId> app_ids,
                                     LockType lock_type,
                                     LockRequestSet lock_requests)
    : app_ids_(std::move(app_ids)),
      lock_type_(lock_type),
      lock_requests_(std::move(lock_requests)) {}

WebAppCommandLock::WebAppCommandLock(WebAppCommandLock&&) = default;

WebAppCommandLock::~WebAppCommandLock() = default;

// static
WebAppCommandLock WebAppCommandLock::CreateForFullSystemLock() {
  LockRequestSet lock_requests;
  lock_requests.emplace(
      static_cast<int>(LockLevel::kStatic),
      StringToLockRange(base::NumberToString(KeysOnStaticLevel::kFullSystem)),
      content::LeveledLockManager::LockType::kExclusive);
  return WebAppCommandLock({}, LockType::kFullSystem, std::move(lock_requests));
}

// static
WebAppCommandLock WebAppCommandLock::CreateForBackgroundWebContentsLock() {
  LockRequestSet lock_requests;
  lock_requests.emplace(static_cast<int>(LockLevel::kStatic),
                        StringToLockRange(base::NumberToString(
                            KeysOnStaticLevel::kBackgroundWebContents)),
                        content::LeveledLockManager::LockType::kExclusive);
  lock_requests.emplace(
      static_cast<int>(LockLevel::kStatic),
      StringToLockRange(base::NumberToString(KeysOnStaticLevel::kFullSystem)),
      content::LeveledLockManager::LockType::kShared);
  return WebAppCommandLock({}, LockType::kBackgroundWebContents,
                           std::move(lock_requests));
}

// static
WebAppCommandLock WebAppCommandLock::CreateForAppLock(
    base::flat_set<AppId> app_ids) {
  LockRequestSet lock_requests;
  lock_requests.emplace(
      static_cast<int>(LockLevel::kStatic),
      StringToLockRange(base::NumberToString(KeysOnStaticLevel::kFullSystem)),
      content::LeveledLockManager::LockType::kShared);

  for (const auto& app_id : app_ids) {
    lock_requests.emplace(static_cast<int>(LockLevel::kApp),
                          StringToLockRange(app_id),
                          content::LeveledLockManager::LockType::kExclusive);
  }
  return WebAppCommandLock(app_ids, LockType::kApp, std::move(lock_requests));
}

// static
WebAppCommandLock WebAppCommandLock::CreateForAppAndWebContentsLock(
    base::flat_set<AppId> app_ids) {
  LockRequestSet lock_requests;
  lock_requests.emplace(
      static_cast<int>(LockLevel::kStatic),
      StringToLockRange(base::NumberToString(KeysOnStaticLevel::kFullSystem)),
      content::LeveledLockManager::LockType::kShared);

  lock_requests.emplace(static_cast<int>(LockLevel::kStatic),
                        StringToLockRange(base::NumberToString(
                            KeysOnStaticLevel::kBackgroundWebContents)),
                        content::LeveledLockManager::LockType::kExclusive);
  for (const auto& app_id : app_ids) {
    lock_requests.emplace(static_cast<int>(LockLevel::kApp),
                          StringToLockRange(app_id),
                          content::LeveledLockManager::LockType::kExclusive);
  }
  return WebAppCommandLock(app_ids, LockType::kAppAndWebContents,
                           std::move(lock_requests));
}

// static
WebAppCommandLock WebAppCommandLock::CreateForNoOpLock() {
  LockRequestSet lock_requests;
  lock_requests.emplace(
      static_cast<int>(LockLevel::kStatic),
      StringToLockRange(base::NumberToString(KeysOnStaticLevel::kNoOp)),
      content::LeveledLockManager::LockType::kShared);
  return WebAppCommandLock({}, LockType::kNoOp, std::move(lock_requests));
}

bool WebAppCommandLock::IsAppLocked(const AppId& app_id) const {
  switch (lock_type_) {
    case LockType::kFullSystem:
      return true;
    case LockType::kBackgroundWebContents:
      return false;
    case LockType::kAppAndWebContents:
      return app_ids_.contains(app_id);
    case LockType::kApp:
      return app_ids_.contains(app_id);
    case LockType::kNoOp:
      return false;
  }
}

bool WebAppCommandLock::IncludesSharedWebContents() const {
  switch (lock_type_) {
    case LockType::kBackgroundWebContents:
    case LockType::kAppAndWebContents:
      return true;
    case LockType::kFullSystem:
    case LockType::kApp:
    case LockType::kNoOp:
      return false;
  }
}

WebAppCommand::WebAppCommand(WebAppCommandLock command_lock)
    : command_lock_(std::move(command_lock)) {
  DETACH_FROM_SEQUENCE(command_sequence_checker_);
  // We don't have an easy way to enforce construction of class on the
  // WebAppProvider sequence without requiring a UI thread in unittests, so just
  // allow this construction to happen from any thread.
  static base::AtomicSequenceNumber g_incrementing_id_;
  id_ = g_incrementing_id_.GetNext();
}
WebAppCommand::~WebAppCommand() = default;

content::WebContents* WebAppCommand::GetInstallingWebContents() {
  return nullptr;
}

void WebAppCommand::SignalCompletionAndSelfDestruct(
    CommandResult result,
    base::OnceClosure completion_callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(command_sequence_checker_);
  // Surround the check in an if-statement to avoid evaluating the debug value
  // every time.
  if (!command_manager_) {
    CHECK(command_manager_)
        << "Command was never started: " << ToDebugValue().DebugString();
  }
  command_manager_->OnCommandComplete(this, result,
                                      std::move(completion_callback));
}

void WebAppCommand::Start(WebAppCommandManager* command_manager) {
  command_manager_ = command_manager;
  Start();
}

base::WeakPtr<WebAppCommand> WebAppCommand::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace web_app
