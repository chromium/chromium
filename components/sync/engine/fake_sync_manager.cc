// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/engine/fake_sync_manager.h"

#include <cstddef>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/sync/base/weak_handle.h"
#include "components/sync/engine/engine_components_factory.h"
#include "components/sync/engine/fake_model_type_connector.h"
#include "components/sync/engine/net/http_post_provider_factory.h"
#include "components/sync/syncable/directory.h"

class GURL;

namespace syncer {

FakeSyncManager::FakeSyncManager(ModelTypeSet initial_sync_ended_types,
                                 ModelTypeSet progress_marker_types,
                                 ModelTypeSet configure_fail_types,
                                 bool should_fail_on_init)
    : should_fail_on_init_(should_fail_on_init),
      initial_sync_ended_types_(initial_sync_ended_types),
      progress_marker_types_(progress_marker_types),
      configure_fail_types_(configure_fail_types),
      last_configure_reason_(CONFIGURE_REASON_UNKNOWN),
      num_invalidations_received_(0) {}

FakeSyncManager::~FakeSyncManager() {}

ModelTypeSet FakeSyncManager::GetAndResetPurgedTypes() {
  ModelTypeSet purged_types = purged_types_;
  purged_types_.Clear();
  return purged_types;
}

ModelTypeSet FakeSyncManager::GetAndResetUnappliedTypes() {
  ModelTypeSet unapplied_types = unapplied_types_;
  unapplied_types_.Clear();
  return unapplied_types;
}

ModelTypeSet FakeSyncManager::GetAndResetDownloadedTypes() {
  ModelTypeSet downloaded_types = downloaded_types_;
  downloaded_types_.Clear();
  return downloaded_types;
}

ConfigureReason FakeSyncManager::GetAndResetConfigureReason() {
  ConfigureReason reason = last_configure_reason_;
  last_configure_reason_ = CONFIGURE_REASON_UNKNOWN;
  return reason;
}

int FakeSyncManager::GetInvalidationCount() const {
  return num_invalidations_received_;
}

void FakeSyncManager::WaitForSyncThread() {
  // Post a task to |sync_task_runner_| and block until it runs.
  base::RunLoop run_loop;
  if (!sync_task_runner_->PostTaskAndReply(FROM_HERE, base::DoNothing(),
                                           run_loop.QuitClosure())) {
    NOTREACHED();
  }
  run_loop.Run();
}

void FakeSyncManager::Init(InitArgs* args) {
  sync_task_runner_ = base::SequencedTaskRunnerHandle::Get();
  PurgePartiallySyncedTypes();

  test_user_share_.SetUp();
  UserShare* share = test_user_share_.user_share();
  for (ModelType type : initial_sync_ended_types_) {
    TestUserShare::CreateRoot(type, share);
  }

  for (auto& observer : observers_) {
    observer.OnInitializationComplete(WeakHandle<JsBackend>(),
                                      WeakHandle<DataTypeDebugInfoListener>(),
                                      !should_fail_on_init_);
  }
}

ModelTypeSet FakeSyncManager::InitialSyncEndedTypes() {
  return initial_sync_ended_types_;
}

ModelTypeSet FakeSyncManager::GetTypesWithEmptyProgressMarkerToken(
    ModelTypeSet types) {
  ModelTypeSet empty_types = types;
  empty_types.RemoveAll(progress_marker_types_);
  return empty_types;
}

void FakeSyncManager::PurgePartiallySyncedTypes() {
  ModelTypeSet partial_types;
  for (ModelType type : progress_marker_types_) {
    if (!initial_sync_ended_types_.Has(type))
      partial_types.Put(type);
  }
  progress_marker_types_.RemoveAll(partial_types);
  purged_types_.PutAll(partial_types);
}

void FakeSyncManager::PurgeDisabledTypes(ModelTypeSet to_purge,
                                         ModelTypeSet to_journal,
                                         ModelTypeSet to_unapply) {
  // Simulate cleaning up disabled types.
  // TODO(sync): consider only cleaning those types that were recently disabled,
  // if this isn't the first cleanup, which more accurately reflects the
  // behavior of the real cleanup logic.
  GetUserShare()->directory->PurgeEntriesWithTypeIn(to_purge, to_journal,
                                                    to_unapply);
  purged_types_.PutAll(to_purge);
  unapplied_types_.PutAll(to_unapply);
  // Types from |to_unapply| should retain their server data and progress
  // markers.
  initial_sync_ended_types_.RemoveAll(Difference(to_purge, to_unapply));
  progress_marker_types_.RemoveAll(Difference(to_purge, to_unapply));
}

void FakeSyncManager::UpdateCredentials(const SyncCredentials& credentials) {
  NOTIMPLEMENTED();
}

void FakeSyncManager::InvalidateCredentials() {
  NOTIMPLEMENTED();
}

void FakeSyncManager::StartSyncingNormally(base::Time last_poll_time) {
  // Do nothing.
}

void FakeSyncManager::StartConfiguration() {
  // Do nothing.
}

void FakeSyncManager::ConfigureSyncer(ConfigureReason reason,
                                      ModelTypeSet to_download,
                                      SyncFeatureState sync_feature_state,
                                      const base::Closure& ready_task) {
  last_configure_reason_ = reason;
  ModelTypeSet success_types = to_download;
  success_types.RemoveAll(configure_fail_types_);

  DVLOG(1) << "Faking configuration. Downloading: "
           << ModelTypeSetToString(success_types);

  // Update our fake directory by clearing and fake-downloading as necessary.
  UserShare* share = GetUserShare();
  for (ModelType type : success_types) {
    // We must be careful to not create the same root node twice.
    if (!initial_sync_ended_types_.Has(type)) {
      TestUserShare::CreateRoot(type, share);
    }
  }

  // Now simulate the actual configuration for those types that successfully
  // download + apply.
  progress_marker_types_.PutAll(success_types);
  initial_sync_ended_types_.PutAll(success_types);
  downloaded_types_.PutAll(success_types);

  ready_task.Run();
}

void FakeSyncManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FakeSyncManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

SyncStatus FakeSyncManager::GetDetailedStatus() const {
  NOTIMPLEMENTED();
  return SyncStatus();
}

void FakeSyncManager::SaveChanges() {
  // Do nothing.
}

void FakeSyncManager::ShutdownOnSyncThread() {
  DCHECK(sync_task_runner_->RunsTasksInCurrentSequence());
  test_user_share_.TearDown();
}

UserShare* FakeSyncManager::GetUserShare() {
  return test_user_share_.user_share();
}

ModelTypeConnector* FakeSyncManager::GetModelTypeConnector() {
  return &fake_model_type_connector_;
}

std::unique_ptr<ModelTypeConnector>
FakeSyncManager::GetModelTypeConnectorProxy() {
  return std::make_unique<FakeModelTypeConnector>();
}

std::string FakeSyncManager::cache_guid() {
  return test_user_share_.user_share()->directory->cache_guid();
}

std::string FakeSyncManager::birthday() {
  NOTIMPLEMENTED();
  return std::string();
}

std::string FakeSyncManager::bag_of_chips() {
  NOTIMPLEMENTED();
  return std::string();
}

bool FakeSyncManager::HasUnsyncedItemsForTest() {
  NOTIMPLEMENTED();
  return false;
}

SyncEncryptionHandler* FakeSyncManager::GetEncryptionHandler() {
  return &fake_encryption_handler_;
}

std::vector<std::unique_ptr<ProtocolEvent>>
FakeSyncManager::GetBufferedProtocolEvents() {
  return std::vector<std::unique_ptr<ProtocolEvent>>();
}

void FakeSyncManager::RefreshTypes(ModelTypeSet types) {
  last_refresh_request_types_ = types;
}

void FakeSyncManager::RegisterDirectoryTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {}

void FakeSyncManager::UnregisterDirectoryTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {}

bool FakeSyncManager::HasDirectoryTypeDebugInfoObserver(
    TypeDebugInfoObserver* observer) {
  return false;
}

void FakeSyncManager::RequestEmitDebugInfo() {}

void FakeSyncManager::OnIncomingInvalidation(
    ModelType type,
    std::unique_ptr<InvalidationInterface> invalidation) {
  num_invalidations_received_++;
}

ModelTypeSet FakeSyncManager::GetLastRefreshRequestTypes() {
  return last_refresh_request_types_;
}

void FakeSyncManager::SetInvalidatorEnabled(bool invalidator_enabled) {
  // Do nothing.
}

void FakeSyncManager::OnCookieJarChanged(bool account_mismatch,
                                         bool empty_jar) {}

void FakeSyncManager::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd) {
  NOTIMPLEMENTED();
}

void FakeSyncManager::UpdateInvalidationClientId(const std::string&) {
  NOTIMPLEMENTED();
}

}  // namespace syncer
