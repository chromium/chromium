// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/profile_sync_test_util.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/sync/history_model_worker.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync/base/sync_prefs.h"
#include "components/sync/engine/passive_model_worker.h"
#include "components/sync/engine/sequenced_model_worker.h"
#include "components/sync/engine/ui_model_worker.h"
#include "components/sync/model/model_type_store_test_util.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_network_connection_tracker.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace browser_sync {

namespace {

class BundleSyncClient : public syncer::FakeSyncClient {
 public:
  BundleSyncClient(syncer::SyncApiComponentFactory* factory,
                   PrefService* pref_service,
                   syncer::ModelTypeStoreService* model_type_store_service,
                   autofill::PersonalDataManager* personal_data_manager,
                   const base::Callback<base::WeakPtr<syncer::SyncableService>(
                       syncer::ModelType type)>& get_syncable_service_callback,
                   const base::Callback<syncer::SyncService*(void)>&
                       get_sync_service_callback,
                   const base::Callback<bookmarks::BookmarkModel*(void)>&
                       get_bookmark_model_callback,
                   scoped_refptr<base::SequencedTaskRunner> db_thread,
                   scoped_refptr<base::SequencedTaskRunner> file_thread,
                   history::HistoryService* history_service);

  ~BundleSyncClient() override;

  PrefService* GetPrefService() override;
  autofill::PersonalDataManager* GetPersonalDataManager() override;
  base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) override;
  syncer::SyncService* GetSyncService() override;
  syncer::ModelTypeStoreService* GetModelTypeStoreService() override;
  scoped_refptr<syncer::ModelSafeWorker> CreateModelWorkerForGroup(
      syncer::ModelSafeGroup group) override;
  history::HistoryService* GetHistoryService() override;
  bookmarks::BookmarkModel* GetBookmarkModel() override;

 private:
  PrefService* const pref_service_;
  syncer::ModelTypeStoreService* const model_type_store_service_;
  autofill::PersonalDataManager* const personal_data_manager_;
  const base::Callback<base::WeakPtr<syncer::SyncableService>(
      syncer::ModelType type)>
      get_syncable_service_callback_;
  const base::Callback<syncer::SyncService*(void)> get_sync_service_callback_;
  const base::Callback<bookmarks::BookmarkModel*(void)>
      get_bookmark_model_callback_;
  // These task runners, if not null, are used in CreateModelWorkerForGroup.
  const scoped_refptr<base::SequencedTaskRunner> db_thread_;
  const scoped_refptr<base::SequencedTaskRunner> file_thread_;
  history::HistoryService* history_service_;
};

BundleSyncClient::BundleSyncClient(
    syncer::SyncApiComponentFactory* factory,
    PrefService* pref_service,
    syncer::ModelTypeStoreService* model_type_store_service,
    autofill::PersonalDataManager* personal_data_manager,
    const base::Callback<base::WeakPtr<syncer::SyncableService>(
        syncer::ModelType type)>& get_syncable_service_callback,
    const base::Callback<syncer::SyncService*(void)>& get_sync_service_callback,
    const base::Callback<bookmarks::BookmarkModel*(void)>&
        get_bookmark_model_callback,
    scoped_refptr<base::SequencedTaskRunner> db_thread,
    scoped_refptr<base::SequencedTaskRunner> file_thread,
    history::HistoryService* history_service)
    : syncer::FakeSyncClient(factory),
      pref_service_(pref_service),
      model_type_store_service_(model_type_store_service),
      personal_data_manager_(personal_data_manager),
      get_syncable_service_callback_(get_syncable_service_callback),
      get_sync_service_callback_(get_sync_service_callback),
      get_bookmark_model_callback_(get_bookmark_model_callback),
      db_thread_(db_thread),
      file_thread_(file_thread),
      history_service_(history_service) {
  EXPECT_EQ(!!db_thread_, !!file_thread_);
}

BundleSyncClient::~BundleSyncClient() = default;

PrefService* BundleSyncClient::GetPrefService() {
  return pref_service_;
}

autofill::PersonalDataManager* BundleSyncClient::GetPersonalDataManager() {
  return personal_data_manager_;
}

base::WeakPtr<syncer::SyncableService>
BundleSyncClient::GetSyncableServiceForType(syncer::ModelType type) {
  if (get_syncable_service_callback_.is_null())
    return syncer::FakeSyncClient::GetSyncableServiceForType(type);
  return get_syncable_service_callback_.Run(type);
}

syncer::SyncService* BundleSyncClient::GetSyncService() {
  if (get_sync_service_callback_.is_null())
    return syncer::FakeSyncClient::GetSyncService();
  return get_sync_service_callback_.Run();
}

scoped_refptr<syncer::ModelSafeWorker>
BundleSyncClient::CreateModelWorkerForGroup(syncer::ModelSafeGroup group) {
  if (!db_thread_)
    return FakeSyncClient::CreateModelWorkerForGroup(group);
  EXPECT_TRUE(file_thread_)
      << "DB thread was specified but FILE thread was not.";
  switch (group) {
    case syncer::GROUP_DB:
      return new syncer::SequencedModelWorker(db_thread_, syncer::GROUP_DB);
    case syncer::GROUP_FILE:
      return new syncer::SequencedModelWorker(file_thread_, syncer::GROUP_FILE);
    case syncer::GROUP_UI:
      return new syncer::UIModelWorker(base::ThreadTaskRunnerHandle::Get());
    case syncer::GROUP_PASSIVE:
      return new syncer::PassiveModelWorker();
    case syncer::GROUP_HISTORY: {
      history::HistoryService* history_service = GetHistoryService();
      if (!history_service)
        return nullptr;
      return new HistoryModelWorker(history_service->AsWeakPtr(),
                                    base::ThreadTaskRunnerHandle::Get());
    }
    default:
      return nullptr;
  }
}

syncer::ModelTypeStoreService* BundleSyncClient::GetModelTypeStoreService() {
  return model_type_store_service_;
}

history::HistoryService* BundleSyncClient::GetHistoryService() {
  if (history_service_)
    return history_service_;
  return FakeSyncClient::GetHistoryService();
}

bookmarks::BookmarkModel* BundleSyncClient::GetBookmarkModel() {
  if (get_bookmark_model_callback_.is_null())
    return FakeSyncClient::GetBookmarkModel();
  return get_bookmark_model_callback_.Run();
}

}  // namespace

void RegisterPrefsForProfileSyncService(
    user_prefs::PrefRegistrySyncable* registry) {
  syncer::SyncPrefs::RegisterProfilePrefs(registry);
  AccountTrackerService::RegisterPrefs(registry);
  ProfileOAuth2TokenService::RegisterProfilePrefs(registry);
  SigninManagerBase::RegisterProfilePrefs(registry);
  SigninManagerBase::RegisterPrefs(registry);
}

ProfileSyncServiceBundle::SyncClientBuilder::~SyncClientBuilder() = default;

ProfileSyncServiceBundle::SyncClientBuilder::SyncClientBuilder(
    ProfileSyncServiceBundle* bundle)
    : bundle_(bundle) {}

void ProfileSyncServiceBundle::SyncClientBuilder::SetPersonalDataManager(
    autofill::PersonalDataManager* personal_data_manager) {
  personal_data_manager_ = personal_data_manager;
}

// The client will call this callback to produce the service.
void ProfileSyncServiceBundle::SyncClientBuilder::SetSyncableServiceCallback(
    const base::RepeatingCallback<base::WeakPtr<syncer::SyncableService>(
        syncer::ModelType type)>& get_syncable_service_callback) {
  get_syncable_service_callback_ = get_syncable_service_callback;
}

// The client will call this callback to produce the service.
void ProfileSyncServiceBundle::SyncClientBuilder::SetSyncServiceCallback(
    const base::Callback<syncer::SyncService*(void)>&
        get_sync_service_callback) {
  get_sync_service_callback_ = get_sync_service_callback;
}

void ProfileSyncServiceBundle::SyncClientBuilder::SetHistoryService(
    history::HistoryService* history_service) {
  history_service_ = history_service;
}

void ProfileSyncServiceBundle::SyncClientBuilder::SetBookmarkModelCallback(
    const base::Callback<bookmarks::BookmarkModel*(void)>&
        get_bookmark_model_callback) {
  get_bookmark_model_callback_ = get_bookmark_model_callback;
}

std::unique_ptr<syncer::FakeSyncClient>
ProfileSyncServiceBundle::SyncClientBuilder::Build() {
  return std::make_unique<BundleSyncClient>(
      bundle_->component_factory(), bundle_->pref_service(),
      &bundle_->model_type_store_service_, personal_data_manager_,
      get_syncable_service_callback_, get_sync_service_callback_,
      get_bookmark_model_callback_,
      activate_model_creation_ ? bundle_->db_thread() : nullptr,
      activate_model_creation_ ? base::SequencedTaskRunnerHandle::Get()
                               : nullptr,
      history_service_);
}

ProfileSyncServiceBundle::ProfileSyncServiceBundle()
    : db_thread_(base::SequencedTaskRunnerHandle::Get()),
      signin_client_(&pref_service_),
#if defined(OS_CHROMEOS)
      signin_manager_(&signin_client_, &account_tracker_),
#else
      signin_manager_(&signin_client_,
                      &auth_service_,
                      &account_tracker_,
                      nullptr),
#endif
      auth_service_(&pref_service_),
      gaia_cookie_manager_service_(&auth_service_,
                                   "profile_sync_service_bundle",
                                   &signin_client_),
      identity_test_env_(&account_tracker_,
                         &auth_service_,
                         &signin_manager_,
                         &gaia_cookie_manager_service_) {
  RegisterPrefsForProfileSyncService(pref_service_.registry());
  auth_service_.set_auto_post_fetch_response_on_message_loop(true);
  account_tracker_.Initialize(&pref_service_, base::FilePath());
  signin_manager_.Initialize(&pref_service_);
  identity_provider_ = std::make_unique<invalidation::ProfileIdentityProvider>(
      identity_manager());
}

ProfileSyncServiceBundle::~ProfileSyncServiceBundle() {}

ProfileSyncService::InitParams ProfileSyncServiceBundle::CreateBasicInitParams(
    ProfileSyncService::StartBehavior start_behavior,
    std::unique_ptr<syncer::SyncClient> sync_client) {
  ProfileSyncService::InitParams init_params;

  init_params.start_behavior = start_behavior;
  init_params.sync_client = std::move(sync_client);
  init_params.identity_manager = identity_manager();
  init_params.signin_scoped_device_id_callback =
      base::BindRepeating([]() { return std::string(); });
  init_params.invalidations_identity_providers.push_back(
      identity_provider_.get());
  init_params.network_time_update_callback = base::DoNothing();
  init_params.url_loader_factory =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  init_params.network_connection_tracker =
      network::TestNetworkConnectionTracker::GetInstance();
  init_params.debug_identifier = "dummyDebugName";
  init_params.channel = version_info::Channel::UNKNOWN;

  return init_params;
}

}  // namespace browser_sync
