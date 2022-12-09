// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/locks/web_app_lock_manager.h"
#include <memory>

#include "base/bind.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
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

void WebAppLockManager::AcquireLock(
    base::WeakPtr<content::PartitionedLockHolder> holder,
    LockDescription& lock_description,
    base::OnceClosure on_lock_acquired) {
  std::vector<content::PartitionedLockManager::PartitionedLockRequest>
      requests = GetLockRequestsForLock(lock_description);
  content::PartitionedLockManager::AcquireOptions options;
  options.ensure_async = true;
  lock_manager_.AcquireLocks(std::move(requests), holder,
                             std::move(on_lock_acquired), options);
}

template <>
void WebAppLockManager::AcquireLock(
    LockDescription& lock_description,
    base::OnceCallback<void(std::unique_ptr<NoopLock>)> on_lock_acquired) {
  CHECK(lock_description.type() == LockDescription::Type::kNoOp);

  auto lock = std::make_unique<NoopLock>(
      std::make_unique<content::PartitionedLockHolder>());
  base::WeakPtr<content::PartitionedLockHolder> holder =
      lock->holder_->AsWeakPtr();
  AcquireLock(holder, lock_description,
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
      std::make_unique<content::PartitionedLockHolder>(),
      *provider_->command_manager().EnsureWebContentsCreated(PassKey()));

  base::WeakPtr<content::PartitionedLockHolder> holder =
      lock->holder_->AsWeakPtr();
  AcquireLock(holder, lock_description,
              base::BindOnce(std::move(on_lock_acquired), std::move(lock)));
}

template <>
void WebAppLockManager::AcquireLock(
    LockDescription& lock_description,
    base::OnceCallback<void(std::unique_ptr<AppLock>)> on_lock_acquired) {
  CHECK(lock_description.type() == LockDescription::Type::kApp);

  auto lock = std::make_unique<AppLock>(
      std::make_unique<content::PartitionedLockHolder>(),
      provider_->registrar_unsafe(), provider_->sync_bridge(),
      provider_->install_finalizer(), provider_->os_integration_manager(),
      provider_->install_manager(), provider_->icon_manager(),
      provider_->translation_manager(), provider_->ui_manager());

  base::WeakPtr<content::PartitionedLockHolder> holder =
      lock->holder_->AsWeakPtr();
  AcquireLock(holder, lock_description,
              base::BindOnce(std::move(on_lock_acquired), std::move(lock)));
}

template <>
void WebAppLockManager::AcquireLock(
    LockDescription& lock_description,
    base::OnceCallback<void(std::unique_ptr<SharedWebContentsWithAppLock>)>
        on_lock_acquired) {
  CHECK(lock_description.type() == LockDescription::Type::kAppAndWebContents);

  auto lock = std::make_unique<SharedWebContentsWithAppLock>(
      std::make_unique<content::PartitionedLockHolder>(),
      *provider_->command_manager().EnsureWebContentsCreated(PassKey()),
      provider_->registrar_unsafe(), provider_->sync_bridge(),
      provider_->install_finalizer(), provider_->os_integration_manager(),
      provider_->install_manager(), provider_->icon_manager(),
      provider_->translation_manager(), provider_->ui_manager());

  base::WeakPtr<content::PartitionedLockHolder> holder =
      lock->holder_->AsWeakPtr();
  AcquireLock(holder, lock_description,
              base::BindOnce(std::move(on_lock_acquired), std::move(lock)));
}

template <>
void WebAppLockManager::AcquireLock(
    LockDescription& lock_description,
    base::OnceCallback<void(std::unique_ptr<FullSystemLock>)>
        on_lock_acquired) {
  CHECK(lock_description.type() == LockDescription::Type::kFullSystem);

  auto lock = std::make_unique<FullSystemLock>(
      std::make_unique<content::PartitionedLockHolder>(),
      provider_->registrar_unsafe(), provider_->sync_bridge(),
      provider_->install_finalizer(), provider_->os_integration_manager(),
      provider_->install_manager(), provider_->icon_manager(),
      provider_->translation_manager(), provider_->ui_manager());
  base::WeakPtr<content::PartitionedLockHolder> holder =
      lock->holder_->AsWeakPtr();
  AcquireLock(holder, lock_description,
              base::BindOnce(std::move(on_lock_acquired), std::move(lock)));
}

std::unique_ptr<SharedWebContentsWithAppLockDescription>
WebAppLockManager::UpgradeAndAcquireLock(
    std::unique_ptr<SharedWebContentsLock> lock,
    const base::flat_set<AppId>& app_ids,
    base::OnceCallback<void(std::unique_ptr<SharedWebContentsWithAppLock>)>
        on_lock_acquired) {
  std::unique_ptr<SharedWebContentsWithAppLockDescription>
      result_lock_description =
          std::make_unique<SharedWebContentsWithAppLockDescription>(app_ids);
  auto result_lock = std::make_unique<SharedWebContentsWithAppLock>(
      std::move(lock->holder_),
      *provider_->command_manager().EnsureWebContentsCreated(PassKey()),
      provider_->registrar_unsafe(), provider_->sync_bridge(),
      provider_->install_finalizer(), provider_->os_integration_manager(),
      provider_->install_manager(), provider_->icon_manager(),
      provider_->translation_manager(), provider_->ui_manager());
  base::WeakPtr<content::PartitionedLockHolder> holder =
      result_lock->holder_->AsWeakPtr();

  content::PartitionedLockManager::AcquireOptions options;
  options.ensure_async = true;
  lock_manager_.AcquireLocks(
      GetAppIdLocks(app_ids), holder,
      base::BindOnce(std::move(on_lock_acquired), std::move(result_lock)),
      options);

  return result_lock_description;
}

std::unique_ptr<AppLockDescription> WebAppLockManager::UpgradeAndAcquireLock(
    std::unique_ptr<NoopLock> lock,
    const base::flat_set<AppId>& app_ids,
    base::OnceCallback<void(std::unique_ptr<AppLock>)> on_lock_acquired) {
  std::unique_ptr<AppLockDescription> result_lock_description =
      std::make_unique<AppLockDescription>(app_ids);

  auto result_lock = std::make_unique<AppLock>(
      std::move(lock->holder_), provider_->registrar_unsafe(),
      provider_->sync_bridge(), provider_->install_finalizer(),
      provider_->os_integration_manager(), provider_->install_manager(),
      provider_->icon_manager(), provider_->translation_manager(),
      provider_->ui_manager());
  base::WeakPtr<content::PartitionedLockHolder> holder =
      result_lock->holder_->AsWeakPtr();

  // TODO(dmurph): Create option for lock acquisition callbacks to always be
  // posted async. https://crbug.com/1354312
  auto posted_callback = base::BindOnce(
      base::IgnoreResult(&base::TaskRunner::PostTask),
      base::SequencedTaskRunner::GetCurrentDefault(), FROM_HERE,
      base::BindOnce(std::move(on_lock_acquired), std::move(result_lock)));
  lock_manager_.AcquireLocks(GetAppIdLocks(app_ids), holder,
                             std::move(posted_callback));
  return result_lock_description;
}

}  // namespace web_app
