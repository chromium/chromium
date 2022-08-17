// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "components/services/storage/indexed_db/locks/leveled_lock_manager.h"
#include "components/services/storage/indexed_db/locks/leveled_lock_range.h"

namespace web_app {

namespace {
// Creates a `LeveledLockRange` that only includes the provided string `key`
content::LeveledLockRange StringToLockRange(std::string key) {
  return content::LeveledLockRange{key, key + static_cast<char>(0)};
}

enum class LockLevel {
  kStatic = 0,
  kApp = 1,
  kMaxValue = kApp,
};

enum KeysOnStaticLevel {
  kFullSystem = 0,
  kBackgroundWebContents = 1,
  kNoOp = 2,
};

content::LeveledLockManager::LeveledLockRequest GetSystemLock(
    content::LeveledLockManager::LockType type) {
  return content::LeveledLockManager::LeveledLockRequest(
      static_cast<int>(LockLevel::kStatic),
      StringToLockRange(base::NumberToString(KeysOnStaticLevel::kFullSystem)),
      type);
}

content::LeveledLockManager::LeveledLockRequest GetSharedWebContentsLock() {
  return content::LeveledLockManager::LeveledLockRequest(
      {static_cast<int>(LockLevel::kStatic),
       StringToLockRange(
           base::NumberToString(KeysOnStaticLevel::kBackgroundWebContents)),
       content::LeveledLockManager::LockType::kExclusive});
}

std::vector<content::LeveledLockManager::LeveledLockRequest> GetAppIdLocks(
    const base::flat_set<AppId>& app_ids) {
  std::vector<content::LeveledLockManager::LeveledLockRequest> lock_requests;
  for (const AppId& app_id : app_ids) {
    lock_requests.emplace_back(
        static_cast<int>(LockLevel::kApp), StringToLockRange(app_id),
        content::LeveledLockManager::LockType::kExclusive);
  }
  return lock_requests;
}

std::vector<content::LeveledLockManager::LeveledLockRequest>
GetLockRequestsForLock(const Lock& lock) {
  std::vector<content::LeveledLockManager::LeveledLockRequest> requests;
  switch (lock.type()) {
    case Lock::Type::kNoOp:
      requests = {
          GetSystemLock(content::LeveledLockManager::LockType::kShared)};
      break;
    case Lock::Type::kApp:
      requests = GetAppIdLocks(lock.app_ids());
      requests.push_back(
          GetSystemLock(content::LeveledLockManager::LockType::kShared));
      break;
    case Lock::Type::kAppAndWebContents:
      requests = GetAppIdLocks(lock.app_ids());
      ABSL_FALLTHROUGH_INTENDED;
    case Lock::Type::kBackgroundWebContents:
      requests.push_back(
          GetSystemLock(content::LeveledLockManager::LockType::kShared));
      requests.push_back(GetSharedWebContentsLock());
      break;
    case Lock::Type::kFullSystem:
      requests = {
          GetSystemLock(content::LeveledLockManager::LockType::kExclusive)};
      break;
  }
  return requests;
}

}  // namespace

WebAppLockManager::WebAppLockManager() = default;
WebAppLockManager::~WebAppLockManager() = default;

bool WebAppLockManager::IsSharedWebContentsLockFree() {
  return lock_manager_.TestLock(GetSharedWebContentsLock()) ==
         content::DisjointRangeLockManager::TestLockResult::kFree;
}

void WebAppLockManager::AcquireLock(Lock& lock,
                                    base::OnceClosure on_lock_acquired) {
  CHECK(!lock.HasLockBeenRequested()) << "Cannot acquire a lock twice.";
  std::vector<content::LeveledLockManager::LeveledLockRequest> requests =
      GetLockRequestsForLock(lock);
  lock.holder_ = std::make_unique<content::LeveledLockHolder>();
  bool success =
      lock_manager_.AcquireLocks(std::move(requests), lock.holder_->AsWeakPtr(),
                                 std::move(on_lock_acquired));
  DCHECK(success);
}

std::unique_ptr<SharedWebContentsWithAppLock>
WebAppLockManager::UpgradeAndAcquireLock(
    std::unique_ptr<SharedWebContentsLock> lock,
    const base::flat_set<AppId>& app_ids,
    base::OnceClosure on_lock_acquired) {
  CHECK(lock->HasLockBeenRequested());
  std::unique_ptr<SharedWebContentsWithAppLock> result_lock =
      std::make_unique<SharedWebContentsWithAppLock>(app_ids);
  result_lock->holder_ = std::move(lock->holder_);
  bool success = lock_manager_.AcquireLocks(GetAppIdLocks(app_ids),
                                            result_lock->holder_->AsWeakPtr(),
                                            std::move(on_lock_acquired));
  DCHECK(success);
  return result_lock;
}

std::unique_ptr<AppLock> WebAppLockManager::UpgradeAndAcquireLock(
    std::unique_ptr<NoopLock> lock,
    const base::flat_set<AppId>& app_ids,
    base::OnceClosure on_lock_acquired) {
  CHECK(lock->HasLockBeenRequested());
  std::unique_ptr<AppLock> result_lock = std::make_unique<AppLock>(app_ids);
  result_lock->holder_ = std::move(lock->holder_);
  bool success = lock_manager_.AcquireLocks(GetAppIdLocks(app_ids),
                                            result_lock->holder_->AsWeakPtr(),
                                            std::move(on_lock_acquired));
  DCHECK(success);
  return result_lock;
}

}  // namespace web_app
