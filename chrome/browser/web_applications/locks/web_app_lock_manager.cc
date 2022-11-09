// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include <memory>

#include "base/bind.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/locks/full_system_lock.h"
#include "chrome/browser/web_applications/locks/lock.h"
#include "chrome/browser/web_applications/locks/noop_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_lock.h"
#include "chrome/browser/web_applications/locks/shared_web_contents_with_app_lock.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_id.h"
#include "components/services/storage/indexed_db/locks/partitioned_lock_manager.h"

namespace web_app {

namespace {

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

content::PartitionedLockManager::PartitionedLockRequest GetSystemLock(
    content::PartitionedLockManager::LockType type) {
  return content::PartitionedLockManager::PartitionedLockRequest(
      content::PartitionedLockId(
          {static_cast<int>(LockLevel::kStatic),
           base::NumberToString(KeysOnStaticLevel::kFullSystem)}),
      type);
}

content::PartitionedLockManager::PartitionedLockRequest
GetSharedWebContentsLock() {
  return content::PartitionedLockManager::PartitionedLockRequest(
      content::PartitionedLockId(
          {static_cast<int>(LockLevel::kStatic),
           base::NumberToString(KeysOnStaticLevel::kBackgroundWebContents)}),
      content::PartitionedLockManager::LockType::kExclusive);
}

std::vector<content::PartitionedLockManager::PartitionedLockRequest>
GetAppIdLocks(const base::flat_set<AppId>& app_ids) {
  std::vector<content::PartitionedLockManager::PartitionedLockRequest>
      lock_requests;
  for (const AppId& app_id : app_ids) {
    lock_requests.emplace_back(
        content::PartitionedLockId({static_cast<int>(LockLevel::kApp), app_id}),
        content::PartitionedLockManager::LockType::kExclusive);
  }
  return lock_requests;
}

std::vector<content::PartitionedLockManager::PartitionedLockRequest>
GetLockRequestsForLock(const LockDescription& lock) {
  std::vector<content::PartitionedLockManager::PartitionedLockRequest> requests;
  switch (lock.type()) {
    case LockDescription::Type::kNoOp:
      requests = {
          GetSystemLock(content::PartitionedLockManager::LockType::kShared)};
      break;
    case LockDescription::Type::kApp:
      requests = GetAppIdLocks(lock.app_ids());
      requests.push_back(
          GetSystemLock(content::PartitionedLockManager::LockType::kShared));
      break;
    case LockDescription::Type::kAppAndWebContents:
      requests = GetAppIdLocks(lock.app_ids());
      ABSL_FALLTHROUGH_INTENDED;
    case LockDescription::Type::kBackgroundWebContents:
      requests.push_back(
          GetSystemLock(content::PartitionedLockManager::LockType::kShared));
      requests.push_back(GetSharedWebContentsLock());
      break;
    case LockDescription::Type::kFullSystem:
      requests = {
          GetSystemLock(content::PartitionedLockManager::LockType::kExclusive)};
      break;
  }
  return requests;
}

}  // namespace

WebAppLockManager::WebAppLockManager(WebAppProvider& provider)
    : provider_(provider) {}
WebAppLockManager::~WebAppLockManager() = default;

bool WebAppLockManager::IsSharedWebContentsLockFree() {
  return lock_manager_.TestLock(GetSharedWebContentsLock()) ==
         content::PartitionedLockManager::TestLockResult::kFree;
}

void WebAppLockManager::AcquireLock(LockDescription& lock_description,
                                    base::OnceClosure on_lock_acquired) {
  CHECK(!lock_description.HasLockBeenRequested())
      << "Cannot acquire a lock twice.";
  std::vector<content::PartitionedLockManager::PartitionedLockRequest>
      requests = GetLockRequestsForLock(lock_description);
  content::PartitionedLockManager::AcquireOptions options;
  options.ensure_async = true;
  lock_description.holder_ = std::make_unique<content::PartitionedLockHolder>();
  lock_manager_.AcquireLocks(std::move(requests),
                             lock_description.holder_->AsWeakPtr(),
                             std::move(on_lock_acquired), options);
}

template <>
void WebAppLockManager::AcquireLock(
    LockDescription& lock_description,
    base::OnceCallback<void(std::unique_ptr<NoopLock>)> on_lock_acquired) {
  CHECK(lock_description.type() == LockDescription::Type::kNoOp);

  auto lock = std::make_unique<NoopLock>();

  AcquireLock(lock_description,
              base::BindOnce(std::move(on_lock_acquired), std::move(lock)));
}

template <>
void WebAppLockManager::AcquireLock(
    LockDescription& lock_description,
    base::OnceCallback<void(std::unique_ptr<SharedWebContentsLock>)>
        on_lock_acquired) {
  CHECK(lock_description.type() ==
        LockDescription::Type::kBackgroundWebContents);

  auto lock = std::make_unique<SharedWebContentsLock>(
      *provider_->command_manager().EnsureWebContentsCreated(PassKey()));

  AcquireLock(lock_description,
              base::BindOnce(std::move(on_lock_acquired), std::move(lock)));
}

template <>
void WebAppLockManager::AcquireLock(
    LockDescription& lock_description,
    base::OnceCallback<void(std::unique_ptr<AppLock>)> on_lock_acquired) {
  CHECK(lock_description.type() == LockDescription::Type::kApp);

  auto lock = std::make_unique<AppLock>(
      provider_->registrar(), provider_->sync_bridge(),
      provider_->install_finalizer(), provider_->os_integration_manager());

  AcquireLock(lock_description,
              base::BindOnce(std::move(on_lock_acquired), std::move(lock)));
}

template <>
void WebAppLockManager::AcquireLock(
    LockDescription& lock_description,
    base::OnceCallback<void(std::unique_ptr<SharedWebContentsWithAppLock>)>
        on_lock_acquired) {
  CHECK(lock_description.type() == LockDescription::Type::kAppAndWebContents);

  auto lock = std::make_unique<SharedWebContentsWithAppLock>(
      *provider_->command_manager().EnsureWebContentsCreated(PassKey()),
      provider_->registrar(), provider_->sync_bridge(),
      provider_->install_finalizer(), provider_->os_integration_manager());

  AcquireLock(lock_description,
              base::BindOnce(std::move(on_lock_acquired), std::move(lock)));
}

template <>
void WebAppLockManager::AcquireLock(
    LockDescription& lock_description,
    base::OnceCallback<void(std::unique_ptr<FullSystemLock>)>
        on_lock_acquired) {
  CHECK(lock_description.type() == LockDescription::Type::kFullSystem);

  auto lock = std::make_unique<FullSystemLock>(
      provider_->registrar(), provider_->sync_bridge(),
      provider_->install_finalizer(), provider_->os_integration_manager());

  AcquireLock(lock_description,
              base::BindOnce(std::move(on_lock_acquired), std::move(lock)));
}

std::unique_ptr<SharedWebContentsWithAppLockDescription>
WebAppLockManager::UpgradeAndAcquireLock(
    std::unique_ptr<SharedWebContentsLockDescription> lock_description,
    const base::flat_set<AppId>& app_ids,
    base::OnceClosure on_lock_acquired) {
  CHECK(lock_description->HasLockBeenRequested());
  std::unique_ptr<SharedWebContentsWithAppLockDescription> result_lock =
      std::make_unique<SharedWebContentsWithAppLockDescription>(app_ids);
  result_lock->holder_ = std::move(lock_description->holder_);
  content::PartitionedLockManager::AcquireOptions options;
  options.ensure_async = true;
  lock_manager_.AcquireLocks(GetAppIdLocks(app_ids),
                             result_lock->holder_->AsWeakPtr(),
                             std::move(on_lock_acquired), options);

  return result_lock;
}

std::unique_ptr<AppLockDescription> WebAppLockManager::UpgradeAndAcquireLock(
    std::unique_ptr<NoopLockDescription> lock_description,
    std::unique_ptr<NoopLock> lock,
    const base::flat_set<AppId>& app_ids,
    base::OnceCallback<void(std::unique_ptr<AppLock>)> on_lock_acquired) {
  CHECK(lock_description->HasLockBeenRequested());
  std::unique_ptr<AppLockDescription> result_lock_description =
      std::make_unique<AppLockDescription>(app_ids);
  // TODO(https://crbug.com/1375870): move `holder_` from lock description to
  // lock after all commands are migrated to use command template.
  result_lock_description->holder_ = std::move(lock_description->holder_);

  auto result_lock = std::make_unique<AppLock>(
      provider_->registrar(), provider_->sync_bridge(),
      provider_->install_finalizer(), provider_->os_integration_manager());
  // TODO(dmurph): Create option for lock acquisition callbacks to always be
  // posted async. https://crbug.com/1354312
  auto posted_callback = base::BindOnce(
      base::IgnoreResult(&base::TaskRunner::PostTask),
      base::SequencedTaskRunnerHandle::Get(), FROM_HERE,
      base::BindOnce(std::move(on_lock_acquired), std::move(result_lock)));
  lock_manager_.AcquireLocks(GetAppIdLocks(app_ids),
                             result_lock_description->holder_->AsWeakPtr(),
                             std::move(posted_callback));
  return result_lock_description;
}

}  // namespace web_app
