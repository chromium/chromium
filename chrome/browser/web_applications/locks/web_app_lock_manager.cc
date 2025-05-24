// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/types/pass_key.h"
#include "base/values.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/partitioned_lock_id.h"
#include "chrome/browser/web_applications/locks/partitioned_lock_manager.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/webapps/common/web_app_id.h"

namespace web_app {

namespace {

// The WebAppLockManager uses the following breakdown to define its locks with
// the PartitionedLockManager:
// - NoopLock:
//   - No locks.
// - SharedWebContentsLock:
//   - Exclusive {kStatic, kBackgroundWebContents}
// - AllAppsLock:
//   - Exclusive {kStatic, kAllApps}
// - AppLock:
//   - Shared {kStatic, kAllApps}
//   - Exclusive {kApp, <app_id>}
//   - <repeat for each app id requested>
// - SharedWebContentsWithAppLock
//   - Exclusive {kStatic, kBackgroundWebContents}
//   - Shared {kStatic, kAllApps}
//   - Exclusive {kApp, <app_id>}
//   - <repeat for each app id requested>
//
// We use strict lock ordering to ensure no deadlocks occur, and to allow
// 'upgrading'.  Locks must be in this order to facilitate the upgrade
// functionality provided by the system and not deadlock
// - kBackgroundWebContents (static partition)
// - kAllApps (static partition)
// - kApp (app partition)
//
// For example, the lock used for the SharedWebContentsLock  has to be
// 'before' the locks used for the AppLock, as this allows the lock upgrade
// method `UpgradeAndAcquireLock` below where a SharedWebContentsLock can be
// upgraded to a SharedWebContentsWithAppLock.

// These values are not persisted to disk or logs.
enum class LockPartition {
  kStatic = 0,
  kApp = 1,
  kMaxValue = kApp,
};

// These values are not persisted to disk or logs.
enum KeysOnStaticPartition {
  kBackgroundWebContents = 0,
  kAllApps = 1,
  kMaxValue = kAllApps,
};

static_assert(LockPartition::kStatic < LockPartition::kApp);
static_assert(KeysOnStaticPartition::kBackgroundWebContents <
              KeysOnStaticPartition::kAllApps);
// Since we use `base::NumberToString` for the static partition data below, and
// the PartitionedLockId uses alphabetical sorting for the data field, the value
// must stay below 10 until there is a different sorting / encoding scheme.
static_assert(KeysOnStaticPartition::kMaxValue < 10);

const char* KeysOnStaticPartitionToString(KeysOnStaticPartition key) {
  switch (key) {
    case KeysOnStaticPartition::kAllApps:
      return "AllApps";
    case KeysOnStaticPartition::kBackgroundWebContents:
      return "BackgroundWebContents";
  }
}

PartitionedLockManager::PartitionedLockRequest GetAllAppsLock(
    PartitionedLockManager::LockType type) {
  return PartitionedLockManager::PartitionedLockRequest(
      PartitionedLockId(
          {static_cast<int>(LockPartition::kStatic),
           base::NumberToString(KeysOnStaticPartition::kAllApps)}),
      type);
}

PartitionedLockManager::PartitionedLockRequest GetSharedWebContentsLock() {
  return PartitionedLockManager::PartitionedLockRequest(
      PartitionedLockId({static_cast<int>(LockPartition::kStatic),
                         base::NumberToString(
                             KeysOnStaticPartition::kBackgroundWebContents)}),
      PartitionedLockManager::LockType::kExclusive);
}

std::vector<PartitionedLockManager::PartitionedLockRequest>
GetExclusiveAppIdLocks(const base::flat_set<webapps::AppId>& app_ids) {
  std::vector<PartitionedLockManager::PartitionedLockRequest> lock_requests;
  for (const webapps::AppId& app_id : app_ids) {
    lock_requests.emplace_back(
        PartitionedLockId({static_cast<int>(LockPartition::kApp), app_id}),
        PartitionedLockManager::LockType::kExclusive);
  }
  return lock_requests;
}

std::vector<PartitionedLockManager::PartitionedLockRequest>
GetLockRequestsForLock(const LockDescription& lock) {
  std::vector<PartitionedLockManager::PartitionedLockRequest> requests;
  switch (lock.type()) {
    case LockDescription::Type::kNoOp:
      return {};
    case LockDescription::Type::kApp:
      requests = GetExclusiveAppIdLocks(lock.app_ids());
      requests.push_back(
          GetAllAppsLock(PartitionedLockManager::LockType::kShared));
      return requests;
    case LockDescription::Type::kAllAppsLock:
      return {GetAllAppsLock(PartitionedLockManager::LockType::kExclusive)};
    case LockDescription::Type::kAppAndWebContents:
      requests = GetExclusiveAppIdLocks(lock.app_ids());
      requests.push_back(
          GetAllAppsLock(PartitionedLockManager::LockType::kShared));
      requests.push_back(GetSharedWebContentsLock());
      return requests;
    case LockDescription::Type::kBackgroundWebContents:
      return {GetSharedWebContentsLock()};
  }
}

#if DCHECK_IS_ON()
void LogLockRequest(
    const LockDescription& description,
    const base::Location& location,
    const std::vector<PartitionedLockManager::PartitionedLockRequest>& requests,
    const PartitionedLockManager& lock_manager) {
  DVLOG(1) << "Requesting or upgrading to lock " << base::ToString(description)
           << " for location " << location.ToString();
  std::vector<base::Location> locations =
      lock_manager.GetHeldAndQueuedLockLocations(requests);
  if (!locations.empty()) {
    DVLOG(1) << "Lock currently held (or queued to be held) by:";
    for (const auto& held_location : locations) {
      DVLOG(1) << " " << held_location.ToString();
    }
  }
}
#endif

}  // namespace

template <class LockType>
void WebAppLockManager::GrantLock(base::WeakPtr<LockType> lock) {
  // This callback is never called if the lock holder is destroyed, see
  // invariant on PartitionedLockManager::AcquireLocks
  CHECK(lock);
  lock->GrantLock(*this);
}

template void WebAppLockManager::GrantLock<NoopLock>(
    base::WeakPtr<NoopLock> lock);
template void WebAppLockManager::GrantLock<AppLock>(
    base::WeakPtr<AppLock> lock);

template void WebAppLockManager::GrantLock<AllAppsLock>(
    base::WeakPtr<AllAppsLock> lock);

template <>
void WebAppLockManager::GrantLock(base::WeakPtr<SharedWebContentsLock> lock) {
  // This callback is never called if the lock holder is destroyed, see
  // invariant on PartitionedLockManager::AcquireLocks
  CHECK(lock);
  lock->GrantLock(
      *this, *provider_->command_manager().EnsureWebContentsCreated(PassKey()));
}

template <>
void WebAppLockManager::GrantLock(
    base::WeakPtr<SharedWebContentsWithAppLock> lock) {
  // This callback is never called if the lock holder is destroyed, see
  // invariant on PartitionedLockManager::AcquireLocks
  CHECK(lock);
  lock->GrantLock(
      *this, *provider_->command_manager().EnsureWebContentsCreated(PassKey()));
}

WebAppLockManager::WebAppLockManager() = default;
WebAppLockManager::~WebAppLockManager() = default;

void WebAppLockManager::SetProvider(base::PassKey<WebAppCommandManager>,
                                    WebAppProvider& provider) {
  provider_ = &provider;
}

bool WebAppLockManager::IsSharedWebContentsLockFree() {
  return lock_manager_.TestLock(GetSharedWebContentsLock()) ==
         PartitionedLockManager::TestLockResult::kFree;
}

template <class LockType>
void WebAppLockManager::AcquireLock(
    const LockType::LockDescription& lock_description,
    LockType& lock,
    base::OnceClosure on_lock_acquired,
    const base::Location& location) {
  CHECK(!lock.IsGranted());
  AcquireLockImpl(lock.GetLockHolder(PassKey()), lock_description,
                  base::BindOnce(&WebAppLockManager::GrantLock<LockType>,
                                 GetWeakPtr(), lock.AsWeakPtr())
                      .Then(std::move(on_lock_acquired)),
                  location);
}

template void WebAppLockManager::AcquireLock<NoopLock>(
    const NoopLockDescription& lock_description,
    NoopLock& lock,
    base::OnceClosure on_lock_acquired,
    const base::Location& location);
template void WebAppLockManager::AcquireLock<SharedWebContentsLock>(
    const SharedWebContentsLockDescription& lock_description,
    SharedWebContentsLock& lock,
    base::OnceClosure on_lock_acquired,
    const base::Location& location);
template void WebAppLockManager::AcquireLock<AllAppsLock>(
    const AllAppsLockDescription& lock_description,
    AllAppsLock& lock,
    base::OnceClosure on_lock_acquired,
    const base::Location& location);
template void WebAppLockManager::AcquireLock<AppLock>(
    const AppLockDescription& lock_description,
    AppLock& lock,
    base::OnceClosure on_lock_acquired,
    const base::Location& location);
template void WebAppLockManager::AcquireLock<SharedWebContentsWithAppLock>(
    const SharedWebContentsWithAppLockDescription& lock_description,
    SharedWebContentsWithAppLock& lock,
    base::OnceClosure on_lock_acquired,
    const base::Location& location);

std::unique_ptr<SharedWebContentsWithAppLockDescription>
WebAppLockManager::UpgradeAndAcquireLock(
    std::unique_ptr<SharedWebContentsLock> old_lock,
    SharedWebContentsWithAppLock& new_lock,
    const base::flat_set<webapps::AppId>& app_ids,
    base::OnceClosure on_lock_acquired,
    const base::Location& location) {
  CHECK(!new_lock.IsGranted());
  std::unique_ptr<SharedWebContentsWithAppLockDescription>
      result_lock_description =
          std::make_unique<SharedWebContentsWithAppLockDescription>(app_ids);

  // Upgrading requires the new lock to still hold all of the old locks.
  std::swap(new_lock.GetLockHolder(PassKey()).locks,
            old_lock->GetLockHolder(PassKey()).locks);

  // Note that the description given to `AcquireLock` is the
  // `AppLockDescription` and not the `SharedWebContentsWithAppLock`. This is
  // because `SharedWebContentsLock` already has the web contents lock granted,
  // and we only need the extra app locks.

  AcquireLockImpl(
      new_lock.GetLockHolder(PassKey()), AppLockDescription(app_ids),
      base::BindOnce(
          &WebAppLockManager::GrantLock<SharedWebContentsWithAppLock>,
          GetWeakPtr(), new_lock.AsWeakPtr())
          .Then(std::move(on_lock_acquired)),
      location);
  return result_lock_description;
}

std::unique_ptr<AppLockDescription> WebAppLockManager::UpgradeAndAcquireLock(
    std::unique_ptr<NoopLock> old_lock,
    AppLock& new_lock,
    const base::flat_set<webapps::AppId>& app_ids,
    base::OnceClosure on_lock_acquired,
    const base::Location& location) {
  CHECK(!new_lock.IsGranted());
  std::unique_ptr<AppLockDescription> result_lock_description =
      std::make_unique<AppLockDescription>(app_ids);

  // Upgrading requires the new lock to still hold all of the old locks.
  std::swap(new_lock.GetLockHolder(PassKey()).locks,
            old_lock->GetLockHolder(PassKey()).locks);

  AcquireLockImpl(new_lock.GetLockHolder(PassKey()), *result_lock_description,
                  base::BindOnce(&WebAppLockManager::GrantLock<AppLock>,
                                 GetWeakPtr(), new_lock.AsWeakPtr())
                      .Then(std::move(on_lock_acquired)),
                  location);
  return result_lock_description;
}

base::Value WebAppLockManager::ToDebugValue() const {
  return lock_manager_.ToDebugValue([](const PartitionedLockId& lock)
                                        -> std::string {
    // Out of bounds things should NOT happen here, but given this is being
    // called from debug UI, it's better to return something reasonable
    // instead of doing undefined behavior.
    DCHECK_GE(lock.partition, 0);
    DCHECK_LE(lock.partition, static_cast<int>(LockPartition::kMaxValue));
    if (lock.partition < 0 ||
        lock.partition > static_cast<int>(LockPartition::kMaxValue)) {
      return base::StringPrintf("Invalid lock partition: %i", lock.partition);
    }
    LockPartition partition = static_cast<LockPartition>(lock.partition);
    switch (partition) {
      case LockPartition::kApp:
        return base::StrCat({"App, ", lock.key});
      case LockPartition::kStatic: {
        int lock_key = -1;
        if (!base::StringToInt(lock.key, &lock_key)) {
          return base::StringPrintf("Static, invalid number '%s'",
                                    lock.key.c_str());
        }
        DCHECK_GE(lock_key, 0);
        DCHECK_LE(lock_key, static_cast<int>(KeysOnStaticPartition::kMaxValue));
        if (lock_key < 0 ||
            lock_key > static_cast<int>(KeysOnStaticPartition::kMaxValue)) {
          return base::StringPrintf("Static, invalid key number: %i", lock_key);
        }
        KeysOnStaticPartition key =
            static_cast<KeysOnStaticPartition>(lock_key);
        return base::StringPrintf("Static, '%s'",
                                  KeysOnStaticPartitionToString(key));
      }
    }
  });
}

base::WeakPtr<WebAppLockManager> WebAppLockManager::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void WebAppLockManager::AcquireLockImpl(PartitionedLockHolder& holder,
                                        const LockDescription& lock_description,
                                        base::OnceClosure on_lock_acquired,
                                        const base::Location& location) {
  std::vector<PartitionedLockManager::PartitionedLockRequest> requests =
      GetLockRequestsForLock(lock_description);
#if DCHECK_IS_ON()
  LogLockRequest(lock_description, location, requests, lock_manager_);
#endif
  lock_manager_.AcquireLocks(std::move(requests), holder,
                             std::move(on_lock_acquired), location);
}

}  // namespace web_app
