// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_api_component_factory_impl.h"

#include <optional>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/browser_sync/active_devices_provider_impl.h"
#include "components/browser_sync/browser_sync_client.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync/base/features.h"
#include "components/sync/base/legacy_directory_deletion.h"
#include "components/sync/base/model_type.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync/service/data_type_manager_impl.h"
#include "components/sync/service/glue/sync_engine_impl.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/service/model_type_controller.h"
#include "components/sync/service/syncable_service_based_model_type_controller.h"
#include "components/sync_device_info/device_info_sync_service.h"

namespace browser_sync {

using syncer::DataTypeManager;
using syncer::DataTypeManagerImpl;
using syncer::DataTypeManagerObserver;

SyncApiComponentFactoryImpl::SyncApiComponentFactoryImpl(
    browser_sync::BrowserSyncClient* sync_client,
    version_info::Channel channel,
    const scoped_refptr<base::SequencedTaskRunner>& ui_thread,
    const scoped_refptr<base::SequencedTaskRunner>& db_thread,
    const scoped_refptr<autofill::AutofillWebDataService>&
        web_data_service_on_disk,
    const scoped_refptr<autofill::AutofillWebDataService>&
        web_data_service_in_memory,
    const scoped_refptr<password_manager::PasswordStoreInterface>&
        profile_password_store,
    const scoped_refptr<password_manager::PasswordStoreInterface>&
        account_password_store,
    sync_bookmarks::BookmarkSyncService*
        local_or_syncable_bookmark_sync_service,
    sync_bookmarks::BookmarkSyncService* account_bookmark_sync_service,
    power_bookmarks::PowerBookmarkService* power_bookmark_service,
    supervised_user::SupervisedUserSettingsService*
        supervised_user_settings_service,
    plus_addresses::PlusAddressSettingService* plus_address_setting_service,
    const scoped_refptr<plus_addresses::PlusAddressWebDataService>&
        plus_address_webdata_service,
    commerce::ProductSpecificationsService* product_specifications_service,
    data_sharing::DataSharingService* data_sharing_service)
    : sync_client_(sync_client),
      channel_(channel),
      engines_and_directory_deletion_thread_(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN})) {
  DCHECK(sync_client_);

  controller_builder_.SetAutofillWebDataService(ui_thread, db_thread,
                                                web_data_service_on_disk,
                                                web_data_service_in_memory);
  controller_builder_.SetBookmarkSyncService(
      local_or_syncable_bookmark_sync_service, account_bookmark_sync_service);
  controller_builder_.SetDataSharingService(data_sharing_service);
  controller_builder_.SetPasswordStore(profile_password_store,
                                       account_password_store);
  controller_builder_.SetPowerBookmarkService(power_bookmark_service);
  controller_builder_.SetPlusAddressServices(plus_address_setting_service,
                                             plus_address_webdata_service);
  controller_builder_.SetProductSpecificationsService(
      product_specifications_service);

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  controller_builder_.SetSupervisedUserSettingsService(
      supervised_user_settings_service);
#else   // BUILDFLAG(ENABLE_SUPERVISED_USERS)
  CHECK(!supervised_user_settings_service);
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)
}

SyncApiComponentFactoryImpl::~SyncApiComponentFactoryImpl() = default;

syncer::ModelTypeController::TypeVector
SyncApiComponentFactoryImpl::CreateCommonModelTypeControllers(
    syncer::ModelTypeSet disabled_types,
    syncer::SyncService* sync_service) {
  controller_builder_.SetIdentityManager(sync_client_->GetIdentityManager());
  controller_builder_.SetConsentAuditor(sync_client_->GetConsentAuditor());
  controller_builder_.SetDeviceInfoSyncService(
      sync_client_->GetDeviceInfoSyncService());
  controller_builder_.SetFaviconService(sync_client_->GetFaviconService());
  controller_builder_.SetHistoryService(sync_client_->GetHistoryService());
  controller_builder_.SetModelTypeStoreService(
      sync_client_->GetModelTypeStoreService());
#if !BUILDFLAG(IS_ANDROID)
  controller_builder_.SetPasskeyModel(
      base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials)
          ? sync_client_->GetPasskeyModel()
          : nullptr);
#endif  // !BUILDFLAG(IS_ANDROID)
  controller_builder_.SetPasswordReceiverService(
      sync_client_->GetPasswordReceiverService());
  controller_builder_.SetPasswordSenderService(
      sync_client_->GetPasswordSenderService());
  controller_builder_.SetPrefService(sync_client_->GetPrefService());
  controller_builder_.SetPrefServiceSyncable(
      sync_client_->GetPrefServiceSyncable());
  controller_builder_.SetSessionSyncService(
      sync_client_->GetSessionSyncService());
  controller_builder_.SetDualReadingListModel(
      sync_client_->GetDualReadingListModel());
  controller_builder_.SetSendTabToSelfSyncService(
      sync_client_->GetSendTabToSelfSyncService());
  controller_builder_.SetUserEventService(sync_client_->GetUserEventService());

  return controller_builder_.Build(disabled_types, sync_service, channel_);
}

std::unique_ptr<DataTypeManager>
SyncApiComponentFactoryImpl::CreateDataTypeManager(
    const syncer::ModelTypeController::TypeMap* controllers,
    const syncer::DataTypeEncryptionHandler* encryption_handler,
    DataTypeManagerObserver* observer) {
  return std::make_unique<DataTypeManagerImpl>(controllers, encryption_handler,
                                               observer);
}

std::unique_ptr<syncer::SyncEngine>
SyncApiComponentFactoryImpl::CreateSyncEngine(
    const std::string& name,
    const signin::GaiaIdHash& gaia_id_hash,
    syncer::SyncInvalidationsService* sync_invalidation_service) {
  return std::make_unique<syncer::SyncEngineImpl>(
      name, sync_invalidation_service,
      std::make_unique<browser_sync::ActiveDevicesProviderImpl>(
          sync_client_->GetDeviceInfoSyncService()->GetDeviceInfoTracker(),
          base::DefaultClock::GetInstance()),
      std::make_unique<syncer::SyncTransportDataPrefs>(
          sync_client_->GetPrefService(), gaia_id_hash),
      sync_client_->GetModelTypeStoreService()->GetSyncDataPath(),
      engines_and_directory_deletion_thread_);
}

bool SyncApiComponentFactoryImpl::HasTransportDataIncludingFirstSync(
    const signin::GaiaIdHash& gaia_id_hash) {
  syncer::SyncTransportDataPrefs sync_transport_data_prefs(
      sync_client_->GetPrefService(), gaia_id_hash);
  // NOTE: Keep this logic consistent with how SyncEngineImpl reports
  // is-first-sync.
  return !sync_transport_data_prefs.GetLastSyncedTime().is_null();
}

void SyncApiComponentFactoryImpl::CleanupOnDisableSync() {
  PrefService* pref_service = sync_client_->GetPrefService();
  // Clearing the Directory via DeleteLegacyDirectoryFilesAndNigoriStorage()
  // means there's IO involved which may be considerable overhead if
  // triggered consistently upon browser startup (which is the case for
  // certain codepaths such as the user being signed out). To avoid that, prefs
  // are used to determine whether it's worth it.
  if (syncer::SyncTransportDataPrefs::HasCurrentSyncingGaiaId(pref_service)) {
    syncer::SyncTransportDataPrefs::ClearCurrentSyncingGaiaId(pref_service);
    engines_and_directory_deletion_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &syncer::DeleteLegacyDirectoryFilesAndNigoriStorage,
            sync_client_->GetModelTypeStoreService()->GetSyncDataPath()));
  }

  syncer::SyncTransportDataPrefs::ClearAllLegacy(pref_service);
}

void SyncApiComponentFactoryImpl::ClearTransportDataForAccount(
    const signin::GaiaIdHash& gaia_id_hash) {
  syncer::SyncTransportDataPrefs prefs(sync_client_->GetPrefService(),
                                       gaia_id_hash);
  prefs.ClearForCurrentAccount();
}

}  // namespace browser_sync
