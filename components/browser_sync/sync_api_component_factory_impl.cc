// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/sync_api_component_factory_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/time/default_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/autofill/core/browser/payments/autofill_wallet_model_type_controller.h"
#include "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_credential_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_offer_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_usage_data_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/autofill/core/browser/webdata/contact_info_model_type_controller.h"
#include "components/autofill/core/browser/webdata/contact_info_sync_bridge.h"
#include "components/browser_sync/active_devices_provider_impl.h"
#include "components/browser_sync/browser_sync_client.h"
#include "components/history/core/browser/sync/history_delete_directives_model_type_controller.h"
#include "components/history/core/browser/sync/history_model_type_controller.h"
#include "components/password_manager/core/browser/password_store_interface.h"
#include "components/password_manager/core/browser/sharing/incoming_password_sharing_invitation_model_type_controller.h"
#include "components/password_manager/core/browser/sharing/outgoing_password_sharing_invitation_model_type_controller.h"
#include "components/password_manager/core/browser/sharing/password_receiver_service.h"
#include "components/password_manager/core/browser/sharing/password_sender_service.h"
#include "components/password_manager/core/browser/sync/credential_model_type_controller.h"
#include "components/power_bookmarks/core/power_bookmark_features.h"
#include "components/power_bookmarks/core/power_bookmark_service.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/core/reading_list_model.h"
#include "components/send_tab_to_self/send_tab_to_self_model_type_controller.h"
#include "components/send_tab_to_self/send_tab_to_self_sync_service.h"
#include "components/supervised_user/core/common/buildflags.h"
#include "components/sync/base/features.h"
#include "components/sync/base/legacy_directory_deletion.h"
#include "components/sync/base/model_type.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/invalidations/sync_invalidations_service.h"
#include "components/sync/model/forwarding_model_type_controller_delegate.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "components/sync/service/data_type_manager_impl.h"
#include "components/sync/service/glue/sync_engine_impl.h"
#include "components/sync/service/glue/sync_transport_data_prefs.h"
#include "components/sync/service/model_type_controller.h"
#include "components/sync/service/syncable_service_based_model_type_controller.h"
#include "components/sync_bookmarks/bookmark_model_type_controller.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "components/sync_device_info/device_info_sync_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/sync_sessions/session_model_type_controller.h"
#include "components/sync_sessions/session_sync_service.h"
#include "components/sync_user_events/user_event_model_type_controller.h"

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
#include "components/supervised_user/core/browser/supervised_user_settings_model_type_controller.h"
#include "components/supervised_user/core/browser/supervised_user_settings_service.h"
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USER)

using syncer::DataTypeController;
using syncer::DataTypeManager;
using syncer::DataTypeManagerImpl;
using syncer::DataTypeManagerObserver;
using syncer::ModelTypeController;
using syncer::SyncableServiceBasedModelTypeController;

namespace browser_sync {

namespace {

// These helper functions only wrap the factory functions of the bridges. This
// way, it simplifies life for the compiler which cannot directly cast
// "WeakPtr<ModelTypeSyncBridge> (AutofillWebDataService*)" to
// "WeakPtr<ModelTypeControllerDelegate> (AutofillWebDataService*)".
base::WeakPtr<syncer::ModelTypeControllerDelegate>
AutocompleteDelegateFromDataService(autofill::AutofillWebDataService* service) {
  return autofill::AutocompleteSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
AutofillProfileDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillProfileSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
AutofillWalletCredentialDataDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillWalletCredentialSyncBridge::FromWebDataService(
             service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
AutofillWalletDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillWalletSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
AutofillWalletMetadataDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillWalletMetadataSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
AutofillWalletOfferDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillWalletOfferSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
AutofillWalletUsageDataDelegateFromDataService(
    autofill::AutofillWebDataService* service) {
  return autofill::AutofillWalletUsageDataSyncBridge::FromWebDataService(
             service)
      ->change_processor()
      ->GetControllerDelegate();
}

base::WeakPtr<syncer::ModelTypeControllerDelegate>
ContactInfoDelegateFromDataService(autofill::AutofillWebDataService* service) {
  return autofill::ContactInfoSyncBridge::FromWebDataService(service)
      ->change_processor()
      ->GetControllerDelegate();
}

// Helper function that deals will null (e.g. tests, iOS webview).
base::WeakPtr<syncer::SyncableService> SyncableServiceForPrefs(
    sync_preferences::PrefServiceSyncable* prefs_service,
    syncer::ModelType type) {
  return prefs_service ? prefs_service->GetSyncableService(type)->AsWeakPtr()
                       : nullptr;
}

}  // namespace

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
        supervised_user_settings_service)
    : sync_client_(sync_client),
      channel_(channel),
      ui_thread_(ui_thread),
      db_thread_(db_thread),
      engines_and_directory_deletion_thread_(
          base::ThreadPool::CreateSequencedTaskRunner(
              {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
               base::TaskShutdownBehavior::BLOCK_SHUTDOWN})),
      web_data_service_on_disk_(web_data_service_on_disk),
      web_data_service_in_memory_(web_data_service_in_memory),
      profile_password_store_(profile_password_store),
      account_password_store_(account_password_store),
      local_or_syncable_bookmark_sync_service_(
          local_or_syncable_bookmark_sync_service),
      account_bookmark_sync_service_(account_bookmark_sync_service),
      power_bookmark_service_(power_bookmark_service),
      supervised_user_settings_service_(supervised_user_settings_service) {
  DCHECK(sync_client_);
}

SyncApiComponentFactoryImpl::~SyncApiComponentFactoryImpl() = default;

syncer::DataTypeController::TypeVector
SyncApiComponentFactoryImpl::CreateCommonDataTypeControllers(
    syncer::ModelTypeSet disabled_types,
    syncer::SyncService* sync_service) {
  syncer::DataTypeController::TypeVector controllers;

  const base::RepeatingClosure dump_stack =
      base::BindRepeating(&syncer::ReportUnrecoverableError, channel_);

  // Same delegate for full-sync or transport mode.
  controllers.push_back(std::make_unique<ModelTypeController>(
      syncer::DEVICE_INFO,
      /*delegate_for_full_sync_mode=*/
      std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
          sync_client_->GetDeviceInfoSyncService()
              ->GetControllerDelegate()
              .get()),
      /*delegate_for_transport_mode=*/
      std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
          sync_client_->GetDeviceInfoSyncService()
              ->GetControllerDelegate()
              .get())));

  // These features are enabled only if there's a DB thread to post tasks to.
  if (db_thread_) {
    if (!disabled_types.Has(syncer::AUTOFILL)) {
      // Note: Transport mode is not and will not be supported.
      controllers.push_back(std::make_unique<ModelTypeController>(
          syncer::AUTOFILL,
          std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
              db_thread_, base::BindRepeating(
                              &AutocompleteDelegateFromDataService,
                              base::RetainedRef(web_data_service_on_disk_))),
          /*delegate_for_transport_mode=*/nullptr));
    }

    if (!disabled_types.Has(syncer::AUTOFILL_PROFILE)) {
      // Note: Transport mode is not and will not be supported - support is
      // coming via CONTACT_INFO instead.
      controllers.push_back(std::make_unique<syncer::ModelTypeController>(
          syncer::AUTOFILL_PROFILE,
          std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
              db_thread_, base::BindRepeating(
                              &AutofillProfileDelegateFromDataService,
                              base::RetainedRef(web_data_service_on_disk_))),
          /*delegate_for_transport_mode=*/nullptr));
    }

    if (!disabled_types.Has(syncer::CONTACT_INFO)) {
      // The same delegate is used for full sync and transport mode.
      controllers.push_back(
          std::make_unique<autofill::ContactInfoModelTypeController>(
              /*delegate_for_full_sync_mode=*/
              std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
                  db_thread_,
                  base::BindRepeating(
                      &ContactInfoDelegateFromDataService,
                      base::RetainedRef(web_data_service_on_disk_))),
              /*delegate_for_transport_mode=*/
              std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
                  db_thread_,
                  base::BindRepeating(
                      &ContactInfoDelegateFromDataService,
                      base::RetainedRef(web_data_service_on_disk_))),
              sync_service, sync_client_->GetIdentityManager()));
    }

    if (!disabled_types.Has(syncer::AUTOFILL_WALLET_DATA)) {
      controllers.push_back(CreateWalletModelTypeController(
          syncer::AUTOFILL_WALLET_DATA,
          base::BindRepeating(&AutofillWalletDelegateFromDataService),
          sync_service, /*with_transport_mode_support=*/true));
    }

    // Wallet metadata sync depends on Wallet data sync.
    if (!disabled_types.Has(syncer::AUTOFILL_WALLET_DATA) &&
        !disabled_types.Has(syncer::AUTOFILL_WALLET_METADATA)) {
      controllers.push_back(CreateWalletModelTypeController(
          syncer::AUTOFILL_WALLET_METADATA,
          base::BindRepeating(&AutofillWalletMetadataDelegateFromDataService),
          sync_service, /*with_transport_mode_support=*/
          base::FeatureList::IsEnabled(
              syncer::kSyncEnableWalletMetadataInTransportMode)));
    }

    // Wallet offer sync depends on Wallet data sync.
    if (!disabled_types.Has(syncer::AUTOFILL_WALLET_DATA) &&
        !disabled_types.Has(syncer::AUTOFILL_WALLET_OFFER)) {
      controllers.push_back(CreateWalletModelTypeController(
          syncer::AUTOFILL_WALLET_OFFER,
          base::BindRepeating(&AutofillWalletOfferDelegateFromDataService),
          sync_service, /*with_transport_mode_support=*/
          base::FeatureList::IsEnabled(
              syncer::kSyncEnableWalletOfferInTransportMode)));
    }

    // Wallet usage data sync depends on Wallet data sync.
    if (base::FeatureList::IsEnabled(syncer::kSyncAutofillWalletUsageData) &&
        !disabled_types.Has(syncer::AUTOFILL_WALLET_DATA) &&
        !disabled_types.Has(syncer::AUTOFILL_WALLET_USAGE)) {
      controllers.push_back(CreateWalletModelTypeController(
          syncer::AUTOFILL_WALLET_USAGE,
          base::BindRepeating(&AutofillWalletUsageDataDelegateFromDataService),
          sync_service, /*with_transport_mode_support=*/true));
    }

    // Wallet credential data sync depends on Wallet data sync.
    if (base::FeatureList::IsEnabled(
            syncer::kSyncAutofillWalletCredentialData) &&
        !disabled_types.Has(syncer::AUTOFILL_WALLET_DATA) &&
        !disabled_types.Has(syncer::AUTOFILL_WALLET_CREDENTIAL)) {
      controllers.push_back(CreateWalletModelTypeController(
          syncer::AUTOFILL_WALLET_CREDENTIAL,
          base::BindRepeating(
              &AutofillWalletCredentialDataDelegateFromDataService),
          sync_service, /*with_transport_mode_support=*/true));
    }
  }

  if (!disabled_types.Has(syncer::BOOKMARKS)) {
    favicon::FaviconService* favicon_service =
        sync_client_->GetFaviconService();
    // Services can be null in tests.
    if (local_or_syncable_bookmark_sync_service_ && favicon_service) {
      auto full_mode_delegate =
          std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
              local_or_syncable_bookmark_sync_service_
                  ->GetBookmarkSyncControllerDelegate(favicon_service)
                  .get());
      auto transport_mode_delegate =
          account_bookmark_sync_service_
              ? std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
                    account_bookmark_sync_service_
                        ->GetBookmarkSyncControllerDelegate(favicon_service)
                        .get())
              : nullptr;
      controllers.push_back(
          std::make_unique<sync_bookmarks::BookmarkModelTypeController>(
              std::move(full_mode_delegate),
              std::move(transport_mode_delegate)));
    }

    if (!disabled_types.Has(syncer::POWER_BOOKMARK) &&
        power_bookmark_service_ &&
        base::FeatureList::IsEnabled(power_bookmarks::kPowerBookmarkBackend)) {
      // TODO(crbug.com/1426496): Support transport mode for POWER_BOOKMARK.
      controllers.push_back(std::make_unique<ModelTypeController>(
          syncer::POWER_BOOKMARK,
          power_bookmark_service_->CreateSyncControllerDelegate(),
          /*delegate_for_transport_mode=*/nullptr));
    }
  }

  if (!disabled_types.Has(syncer::HISTORY)) {
    controllers.push_back(std::make_unique<history::HistoryModelTypeController>(
        sync_service, sync_client_->GetIdentityManager(),
        sync_client_->GetHistoryService(), sync_client_->GetPrefService()));
  }

  if (!disabled_types.Has(syncer::HISTORY_DELETE_DIRECTIVES)) {
    controllers.push_back(
        std::make_unique<history::HistoryDeleteDirectivesModelTypeController>(
            dump_stack, sync_service, sync_client_->GetModelTypeStoreService(),
            sync_client_->GetHistoryService(), sync_client_->GetPrefService()));
  }

  if (!disabled_types.Has(syncer::SESSIONS)) {
    syncer::ModelTypeControllerDelegate* delegate =
        sync_client_->GetSessionSyncService()->GetControllerDelegate().get();
    auto full_sync_mode_delegate =
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            delegate);
    auto transport_mode_delegate =
        base::FeatureList::IsEnabled(syncer::kReplaceSyncPromosWithSignInPromos)
            ? std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
                  delegate)
            : nullptr;
    controllers.push_back(
        std::make_unique<sync_sessions::SessionModelTypeController>(
            sync_service, sync_client_->GetPrefService(),
            std::move(full_sync_mode_delegate),
            std::move(transport_mode_delegate)));
  }

  if (!disabled_types.Has(syncer::PASSWORDS)) {
    if (profile_password_store_) {
      // |profile_password_store_| can be null in tests.
      controllers.push_back(
          std::make_unique<password_manager::CredentialModelTypeController>(
              syncer::PASSWORDS,
              profile_password_store_->CreateSyncControllerDelegate(),
              account_password_store_
                  ? account_password_store_->CreateSyncControllerDelegate()
                  : nullptr,
              sync_client_->GetPrefService(),
              sync_client_->GetIdentityManager(), sync_service));

      // Couple password sharing invitations with password data type.
      if (!disabled_types.Has(syncer::INCOMING_PASSWORD_SHARING_INVITATION) &&
          sync_client_->GetPasswordReceiverService()) {
        controllers.push_back(
            std::make_unique<
                password_manager::
                    IncomingPasswordSharingInvitationModelTypeController>(
                sync_service, sync_client_->GetPasswordReceiverService(),
                sync_client_->GetPrefService()));
      }

      if (!disabled_types.Has(syncer::OUTGOING_PASSWORD_SHARING_INVITATION) &&
          sync_client_->GetPasswordSenderService()) {
        controllers.push_back(
            std::make_unique<
                password_manager::
                    OutgoingPasswordSharingInvitationModelTypeController>(
                sync_service, sync_client_->GetPasswordSenderService(),
                sync_client_->GetPrefService()));
      }
    }
  }

  if (!disabled_types.Has(syncer::PREFERENCES)) {
    bool allow_transport_mode =
        base::FeatureList::IsEnabled(
            syncer::kReplaceSyncPromosWithSignInPromos) &&
        base::FeatureList::IsEnabled(syncer::kEnablePreferencesAccountStorage);
    controllers.push_back(
        std::make_unique<SyncableServiceBasedModelTypeController>(
            syncer::PREFERENCES,
            sync_client_->GetModelTypeStoreService()->GetStoreFactory(),
            SyncableServiceForPrefs(sync_client_->GetPrefServiceSyncable(),
                                    syncer::PREFERENCES),
            dump_stack,
            allow_transport_mode
                ? SyncableServiceBasedModelTypeController::DelegateMode::
                      kTransportModeWithSingleModel
                : SyncableServiceBasedModelTypeController::DelegateMode::
                      kLegacyFullSyncModeOnly));
  }

  if (!disabled_types.Has(syncer::PRIORITY_PREFERENCES)) {
    bool allow_transport_mode =
        base::FeatureList::IsEnabled(
            syncer::kReplaceSyncPromosWithSignInPromos) &&
        base::FeatureList::IsEnabled(syncer::kEnablePreferencesAccountStorage);
    controllers.push_back(
        std::make_unique<SyncableServiceBasedModelTypeController>(
            syncer::PRIORITY_PREFERENCES,
            sync_client_->GetModelTypeStoreService()->GetStoreFactory(),
            SyncableServiceForPrefs(sync_client_->GetPrefServiceSyncable(),
                                    syncer::PRIORITY_PREFERENCES),
            dump_stack,
            allow_transport_mode
                ? SyncableServiceBasedModelTypeController::DelegateMode::
                      kTransportModeWithSingleModel
                : SyncableServiceBasedModelTypeController::DelegateMode::
                      kLegacyFullSyncModeOnly));
  }

  if (!disabled_types.Has(syncer::READING_LIST)) {
    // The transport-mode delegate may or may not be null depending on
    // platform and feature toggle state.
    syncer::ModelTypeControllerDelegate* delegate_for_transport_mode =
        sync_client_->GetReadingListModel()
            ->GetSyncControllerDelegateForTransportMode()
            .get();

    controllers.push_back(std::make_unique<ModelTypeController>(
        syncer::READING_LIST,
        /*delegate_for_full_sync_mode=*/
        std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
            sync_client_->GetReadingListModel()
                ->GetSyncControllerDelegate()
                .get()),
        /*delegate_for_transport_mode=*/
        delegate_for_transport_mode
            ? std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
                  delegate_for_transport_mode)
            : nullptr));
  }

  if (!disabled_types.Has(syncer::USER_EVENTS)) {
    controllers.push_back(
        std::make_unique<syncer::UserEventModelTypeController>(
            sync_service,
            /*delegate_for_full_sync_mode=*/
            CreateForwardingControllerDelegate(syncer::USER_EVENTS),
            /*delegate_for_transport_mode=*/
            base::FeatureList::IsEnabled(
                syncer::kReplaceSyncPromosWithSignInPromos)
                ? CreateForwardingControllerDelegate(syncer::USER_EVENTS)
                : nullptr));
  }

  if (!disabled_types.Has(syncer::SEND_TAB_TO_SELF)) {
    syncer::ModelTypeControllerDelegate* delegate =
        sync_client_->GetSendTabToSelfSyncService()
            ->GetControllerDelegate()
            .get();
    controllers.push_back(
        std::make_unique<send_tab_to_self::SendTabToSelfModelTypeController>(
            /*delegate_for_full_sync_mode=*/
            std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
                delegate),
            /*delegate_for_transport_mode=*/
            std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
                delegate)));
  }

  if (!disabled_types.Has(syncer::USER_CONSENTS)) {
    // Forward both full-sync and transport-only modes to the same delegate,
    // since behavior for USER_CONSENTS does not differ (they are always
    // persisted).
    controllers.push_back(std::make_unique<ModelTypeController>(
        syncer::USER_CONSENTS,
        /*delegate_for_full_sync_mode=*/
        CreateForwardingControllerDelegate(syncer::USER_CONSENTS),
        /*delegate_for_transport_mode=*/
        CreateForwardingControllerDelegate(syncer::USER_CONSENTS)));
  }

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_IOS)
  if (base::FeatureList::IsEnabled(syncer::kSyncWebauthnCredentials) &&
      !disabled_types.Has(syncer::WEBAUTHN_CREDENTIAL)) {
    controllers.push_back(
        std::make_unique<password_manager::CredentialModelTypeController>(
            syncer::WEBAUTHN_CREDENTIAL,
            /*delegate_for_full_sync_mode=*/
            CreateForwardingControllerDelegate(syncer::WEBAUTHN_CREDENTIAL),
            /*delegate_for_transport_mode=*/
            CreateForwardingControllerDelegate(syncer::WEBAUTHN_CREDENTIAL),
            sync_client_->GetPrefService(), sync_client_->GetIdentityManager(),
            sync_service));
  }
#endif

#if BUILDFLAG(ENABLE_SUPERVISED_USERS)
  if (supervised_user_settings_service_) {
    controllers.push_back(
        std::make_unique<SupervisedUserSettingsModelTypeController>(
            dump_stack,
            sync_client_->GetModelTypeStoreService()->GetStoreFactory(),
            supervised_user_settings_service_->AsWeakPtr(),
            sync_client_->GetPrefService()));
  }
#endif  // BUILDFLAG(ENABLE_SUPERVISED_USERS)

  return controllers;
}

std::unique_ptr<DataTypeManager>
SyncApiComponentFactoryImpl::CreateDataTypeManager(
    const DataTypeController::TypeMap* controllers,
    const syncer::DataTypeEncryptionHandler* encryption_handler,
    syncer::ModelTypeConfigurer* configurer,
    DataTypeManagerObserver* observer) {
  return std::make_unique<DataTypeManagerImpl>(controllers, encryption_handler,
                                               configurer, observer);
}

std::unique_ptr<syncer::SyncEngine>
SyncApiComponentFactoryImpl::CreateSyncEngine(
    const std::string& name,
    syncer::SyncInvalidationsService* sync_invalidation_service) {
  return std::make_unique<syncer::SyncEngineImpl>(
      name, sync_invalidation_service,
      std::make_unique<browser_sync::ActiveDevicesProviderImpl>(
          sync_client_->GetDeviceInfoSyncService()->GetDeviceInfoTracker(),
          base::DefaultClock::GetInstance()),
      std::make_unique<syncer::SyncTransportDataPrefs>(
          sync_client_->GetPrefService()),
      sync_client_->GetModelTypeStoreService()->GetSyncDataPath(),
      engines_and_directory_deletion_thread_,
      base::BindRepeating(&syncer::SyncClient::OnLocalSyncTransportDataCleared,
                          base::Unretained(sync_client_)));
}

bool SyncApiComponentFactoryImpl::HasTransportDataIncludingFirstSync() {
  syncer::SyncTransportDataPrefs sync_transport_data_prefs(
      sync_client_->GetPrefService());
  // NOTE: Keep this logic consistent with how SyncEngineImpl reports
  // is-first-sync.
  return !sync_transport_data_prefs.GetLastSyncedTime().is_null();
}

void SyncApiComponentFactoryImpl::ClearAllTransportData() {
  syncer::SyncTransportDataPrefs sync_transport_data_prefs(
      sync_client_->GetPrefService());

  // Clearing the Directory via DeleteLegacyDirectoryFilesAndNigoriStorage()
  // means there's IO involved which may be considerable overhead if
  // triggered consistently upon browser startup (which is the case for
  // certain codepaths such as the user being signed out). To avoid that, prefs
  // are used to determine whether it's worth it.
  if (!sync_transport_data_prefs.GetCacheGuid().empty()) {
    engines_and_directory_deletion_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &syncer::DeleteLegacyDirectoryFilesAndNigoriStorage,
            sync_client_->GetModelTypeStoreService()->GetSyncDataPath()));
  }

  sync_transport_data_prefs.ClearAll();
  sync_client_->OnLocalSyncTransportDataCleared();
}

std::unique_ptr<syncer::ModelTypeControllerDelegate>
SyncApiComponentFactoryImpl::CreateForwardingControllerDelegate(
    syncer::ModelType type) {
  return std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
      sync_client_->GetControllerDelegateForModelType(type).get());
}

std::unique_ptr<ModelTypeController>
SyncApiComponentFactoryImpl::CreateWalletModelTypeController(
    syncer::ModelType type,
    const base::RepeatingCallback<
        base::WeakPtr<syncer::ModelTypeControllerDelegate>(
            autofill::AutofillWebDataService*)>& delegate_from_web_data,
    syncer::SyncService* sync_service,
    bool with_transport_mode_support) {
  // Transport mode should be supported, except for METADATA and OFFER where
  // support is still work in progress, see crbug.com/1448894 and
  // crbug.com/1448895.
  CHECK(with_transport_mode_support ||
        type == syncer::AUTOFILL_WALLET_METADATA ||
        type == syncer::AUTOFILL_WALLET_OFFER);
  auto delegate_for_full_sync_mode =
      std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
          db_thread_,
          base::BindRepeating(delegate_from_web_data,
                              base::RetainedRef(web_data_service_on_disk_)));
  auto delegate_for_transport_mode =
      with_transport_mode_support
          ? std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
                db_thread_, base::BindRepeating(
                                delegate_from_web_data,
                                base::RetainedRef(web_data_service_in_memory_)))
          : nullptr;
  return std::make_unique<AutofillWalletModelTypeController>(
      type, std::move(delegate_for_full_sync_mode),
      std::move(delegate_for_transport_mode), sync_client_->GetPrefService(),
      sync_service);
}

}  // namespace browser_sync
