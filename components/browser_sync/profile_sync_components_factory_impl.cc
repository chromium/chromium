// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/browser_sync/profile_sync_components_factory_impl.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/single_thread_task_runner.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_wallet_data_type_controller.h"
#include "components/autofill/core/browser/autofill_wallet_model_type_controller.h"
#include "components/autofill/core/browser/webdata/autocomplete_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_profile_data_type_controller.h"
#include "components/autofill/core/browser/webdata/autofill_profile_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_metadata_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_wallet_sync_bridge.h"
#include "components/autofill/core/browser/webdata/autofill_webdata_service.h"
#include "components/browser_sync/browser_sync_switches.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/history/core/browser/sync/history_delete_directives_data_type_controller.h"
#include "components/history/core/browser/sync/history_delete_directives_model_type_controller.h"
#include "components/history/core/browser/sync/typed_url_model_type_controller.h"
#include "components/password_manager/core/browser/password_data_type_controller.h"
#include "components/password_manager/core/browser/password_model_type_controller.h"
#include "components/password_manager/core/browser/password_store.h"
#include "components/prefs/pref_service.h"
#include "components/reading_list/features/reading_list_switches.h"
#include "components/sync/base/report_unrecoverable_error.h"
#include "components/sync/device_info/local_device_info_provider_impl.h"
#include "components/sync/driver/async_directory_type_controller.h"
#include "components/sync/driver/data_type_manager_impl.h"
#include "components/sync/driver/glue/sync_backend_host_impl.h"
#include "components/sync/driver/model_type_controller.h"
#include "components/sync/driver/proxy_data_type_controller.h"
#include "components/sync/driver/sync_client.h"
#include "components/sync/driver/sync_driver_switches.h"
#include "components/sync/driver/syncable_service_based_model_type_controller.h"
#include "components/sync/engine/sync_engine.h"
#include "components/sync/model/model_type_store_service.h"
#include "components/sync/model_impl/forwarding_model_type_controller_delegate.h"
#include "components/sync/model_impl/proxy_model_type_controller_delegate.h"
#include "components/sync_bookmarks/bookmark_change_processor.h"
#include "components/sync_bookmarks/bookmark_data_type_controller.h"
#include "components/sync_bookmarks/bookmark_model_associator.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "components/sync_sessions/session_data_type_controller.h"
#include "components/sync_sessions/session_model_type_controller.h"
#include "components/sync_sessions/session_sync_service.h"

using base::FeatureList;
using bookmarks::BookmarkModel;
using sync_bookmarks::BookmarkChangeProcessor;
using sync_bookmarks::BookmarkDataTypeController;
using sync_bookmarks::BookmarkModelAssociator;
using sync_sessions::SessionDataTypeController;
using syncer::AsyncDirectoryTypeController;
using syncer::DataTypeController;
using syncer::DataTypeManager;
using syncer::DataTypeManagerImpl;
using syncer::DataTypeManagerObserver;
using syncer::ModelTypeController;
using syncer::ProxyDataTypeController;
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

}  // namespace

ProfileSyncComponentsFactoryImpl::ProfileSyncComponentsFactoryImpl(
    syncer::SyncClient* sync_client,
    version_info::Channel channel,
    const std::string& version,
    bool is_tablet,
    const char* history_disabled_pref,
    const scoped_refptr<base::SingleThreadTaskRunner>& ui_thread,
    const scoped_refptr<base::SingleThreadTaskRunner>& db_thread,
    const scoped_refptr<autofill::AutofillWebDataService>&
        web_data_service_on_disk,
    const scoped_refptr<autofill::AutofillWebDataService>&
        web_data_service_in_memory,
    const scoped_refptr<password_manager::PasswordStore>& password_store,
    sync_bookmarks::BookmarkSyncService* bookmark_sync_service)
    : sync_client_(sync_client),
      channel_(channel),
      version_(version),
      is_tablet_(is_tablet),
      history_disabled_pref_(history_disabled_pref),
      ui_thread_(ui_thread),
      db_thread_(db_thread),
      web_data_service_on_disk_(web_data_service_on_disk),
      web_data_service_in_memory_(web_data_service_in_memory),
      password_store_(password_store),
      bookmark_sync_service_(bookmark_sync_service) {}

ProfileSyncComponentsFactoryImpl::~ProfileSyncComponentsFactoryImpl() {}

syncer::DataTypeController::TypeVector
ProfileSyncComponentsFactoryImpl::CreateCommonDataTypeControllers(
    syncer::ModelTypeSet disabled_types,
    syncer::LocalDeviceInfoProvider* local_device_info_provider) {
  syncer::DataTypeController::TypeVector controllers;
  const base::RepeatingClosure dump_stack =
      base::BindRepeating(&syncer::ReportUnrecoverableError, channel_);

  // TODO(stanisc): can DEVICE_INFO be one of disabled datatypes?
  // Use an error callback that always uploads a stacktrace if it can to help
  // get USS as stable as possible.
  controllers.push_back(
      CreateModelTypeControllerForModelRunningOnUIThread(syncer::DEVICE_INFO));
  // These features are enabled only if there's a DB thread to post tasks to.
  if (db_thread_) {
    // Autocomplete sync is enabled by default.  Register unless explicitly
    // disabled.
    if (!disabled_types.Has(syncer::AUTOFILL)) {
      controllers.push_back(CreateWebDataModelTypeController(
          syncer::AUTOFILL,
          base::BindRepeating(&AutocompleteDelegateFromDataService)));
    }

    // Autofill sync is enabled by default.  Register unless explicitly
    // disabled.
    if (!disabled_types.Has(syncer::AUTOFILL_PROFILE)) {
      if (FeatureList::IsEnabled(switches::kSyncUSSAutofillProfile)) {
        controllers.push_back(CreateWebDataModelTypeController(
            syncer::AUTOFILL_PROFILE,
            base::BindRepeating(&AutofillProfileDelegateFromDataService)));
      } else {
        controllers.push_back(
            std::make_unique<AutofillProfileDataTypeController>(
                db_thread_, dump_stack, sync_client_,
                web_data_service_on_disk_));
      }
    }

    // Wallet data sync is enabled by default, but behind a syncer experiment
    // enforced by the datatype controller. Register unless explicitly disabled.
    bool wallet_disabled = disabled_types.Has(syncer::AUTOFILL_WALLET_DATA);
    if (!wallet_disabled) {
      if (base::FeatureList::IsEnabled(switches::kSyncUSSAutofillWalletData)) {
        controllers.push_back(
            CreateWalletModelTypeControllerWithInMemorySupport(
                syncer::AUTOFILL_WALLET_DATA,
                base::BindRepeating(&AutofillWalletDelegateFromDataService)));
      } else {
        controllers.push_back(
            std::make_unique<AutofillWalletDataTypeController>(
                syncer::AUTOFILL_WALLET_DATA, db_thread_, dump_stack,
                sync_client_, web_data_service_on_disk_));
      }
    }

    // Wallet metadata sync depends on Wallet data sync. Register if Wallet data
    // is syncing and metadata sync is not explicitly disabled.
    if (!wallet_disabled &&
        !disabled_types.Has(syncer::AUTOFILL_WALLET_METADATA)) {
      if (base::FeatureList::IsEnabled(
              switches::kSyncUSSAutofillWalletMetadata)) {
        controllers.push_back(CreateWalletModelTypeController(
            syncer::AUTOFILL_WALLET_METADATA,
            base::BindRepeating(
                &AutofillWalletMetadataDelegateFromDataService)));
      } else {
        controllers.push_back(
            std::make_unique<AutofillWalletDataTypeController>(
                syncer::AUTOFILL_WALLET_METADATA, db_thread_, dump_stack,
                sync_client_, web_data_service_on_disk_));
      }
    }
  }

  // Bookmark sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::BOOKMARKS)) {
    if (FeatureList::IsEnabled(switches::kSyncUSSBookmarks)) {
      controllers.push_back(std::make_unique<ModelTypeController>(
          syncer::BOOKMARKS,
          std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
              ui_thread_,
              base::BindRepeating(&sync_bookmarks::BookmarkSyncService::
                                      GetBookmarkSyncControllerDelegate,
                                  base::Unretained(bookmark_sync_service_),
                                  sync_client_->GetFaviconService()))));
    } else {
      controllers.push_back(std::make_unique<BookmarkDataTypeController>(
          dump_stack, sync_client_));
    }
  }

  // These features are enabled only if history is not disabled.
  if (!sync_client_->GetPrefService()->GetBoolean(history_disabled_pref_)) {
    // TypedUrl sync is enabled by default.  Register unless explicitly
    // disabled.
    if (!disabled_types.Has(syncer::TYPED_URLS)) {
      // TypedURLModelTypeController uses a proxy delegate internally, as
      // provided by HistoryService.
      controllers.push_back(
          std::make_unique<history::TypedURLModelTypeController>(
              sync_client_->GetHistoryService(), sync_client_->GetPrefService(),
              history_disabled_pref_));
    }

    // Delete directive sync is enabled by default.
    if (!disabled_types.Has(syncer::HISTORY_DELETE_DIRECTIVES)) {
      if (base::FeatureList::IsEnabled(
              switches::kSyncPseudoUSSHistoryDeleteDirectives)) {
        controllers.push_back(
            std::make_unique<HistoryDeleteDirectivesModelTypeController>(
                dump_stack, sync_client_));

      } else {
        controllers.push_back(
            std::make_unique<HistoryDeleteDirectivesDataTypeController>(
                dump_stack, sync_client_));
      }
    }

    // Session sync is enabled by default.  This is disabled if history is
    // disabled because the tab sync data is added to the web history on the
    // server.
    if (!disabled_types.Has(syncer::PROXY_TABS)) {
      controllers.push_back(
          std::make_unique<ProxyDataTypeController>(syncer::PROXY_TABS));
      if (FeatureList::IsEnabled(switches::kSyncUSSSessions)) {
        controllers.push_back(
            std::make_unique<sync_sessions::SessionModelTypeController>(
                sync_client_->GetPrefService(),
                std::make_unique<syncer::ForwardingModelTypeControllerDelegate>(
                    sync_client_->GetSessionSyncService()
                        ->GetControllerDelegate()
                        .get()),
                history_disabled_pref_));
      } else {
        controllers.push_back(std::make_unique<SessionDataTypeController>(
            dump_stack, sync_client_, local_device_info_provider,
            history_disabled_pref_));
      }
    }

    // Favicon sync is enabled by default. Register unless explicitly disabled.
    if (!disabled_types.Has(syncer::FAVICON_IMAGES) &&
        !disabled_types.Has(syncer::FAVICON_TRACKING)) {
      if (base::FeatureList::IsEnabled(switches::kSyncPseudoUSSFavicons)) {
        controllers.push_back(
            std::make_unique<SyncableServiceBasedModelTypeController>(
                syncer::FAVICON_IMAGES,
                sync_client_->GetModelTypeStoreService()->GetStoreFactory(),
                base::BindOnce(&syncer::SyncClient::GetSyncableServiceForType,
                               base::Unretained(sync_client_),
                               syncer::FAVICON_IMAGES),
                dump_stack));
        controllers.push_back(
            std::make_unique<SyncableServiceBasedModelTypeController>(
                syncer::FAVICON_TRACKING,
                sync_client_->GetModelTypeStoreService()->GetStoreFactory(),
                base::BindOnce(&syncer::SyncClient::GetSyncableServiceForType,
                               base::Unretained(sync_client_),
                               syncer::FAVICON_TRACKING),
                dump_stack));
      } else {
        controllers.push_back(std::make_unique<AsyncDirectoryTypeController>(
            syncer::FAVICON_IMAGES, base::RepeatingClosure(), sync_client_,
            syncer::GROUP_UI, ui_thread_));
        controllers.push_back(std::make_unique<AsyncDirectoryTypeController>(
            syncer::FAVICON_TRACKING, base::RepeatingClosure(), sync_client_,
            syncer::GROUP_UI, ui_thread_));
      }
    }
  }

  // Password sync is enabled by default.  Register unless explicitly
  // disabled.
  if (!disabled_types.Has(syncer::PASSWORDS)) {
    if (base::FeatureList::IsEnabled(switches::kSyncPseudoUSSPasswords)) {
      controllers.push_back(
          std::make_unique<password_manager::PasswordModelTypeController>(
              sync_client_->GetModelTypeStoreService()->GetStoreFactory(),
              dump_stack, password_store_, sync_client_));
    } else {
      controllers.push_back(std::make_unique<PasswordDataTypeController>(
          dump_stack, sync_client_,
          sync_client_->GetPasswordStateChangedCallback(), password_store_));
    }
  }

  if (!disabled_types.Has(syncer::PREFERENCES)) {
    if (override_prefs_controller_to_uss_for_test_) {
      controllers.push_back(CreateModelTypeControllerForModelRunningOnUIThread(
          syncer::PREFERENCES));
    } else if (base::FeatureList::IsEnabled(
                   switches::kSyncPseudoUSSPreferences)) {
      controllers.push_back(
          std::make_unique<SyncableServiceBasedModelTypeController>(
              syncer::PREFERENCES,
              sync_client_->GetModelTypeStoreService()->GetStoreFactory(),
              base::BindOnce(&syncer::SyncClient::GetSyncableServiceForType,
                             base::Unretained(sync_client_),
                             syncer::PREFERENCES),
              dump_stack));
    } else {
      controllers.push_back(std::make_unique<AsyncDirectoryTypeController>(
          syncer::PREFERENCES, dump_stack, sync_client_, syncer::GROUP_UI,
          ui_thread_));
    }
  }

  if (!disabled_types.Has(syncer::PRIORITY_PREFERENCES)) {
    if (base::FeatureList::IsEnabled(
            switches::kSyncPseudoUSSPriorityPreferences)) {
      controllers.push_back(
          std::make_unique<SyncableServiceBasedModelTypeController>(
              syncer::PRIORITY_PREFERENCES,
              sync_client_->GetModelTypeStoreService()->GetStoreFactory(),
              base::BindOnce(&syncer::SyncClient::GetSyncableServiceForType,
                             base::Unretained(sync_client_),
                             syncer::PRIORITY_PREFERENCES),
              dump_stack));
    } else {
      controllers.push_back(std::make_unique<AsyncDirectoryTypeController>(
          syncer::PRIORITY_PREFERENCES, dump_stack, sync_client_,
          syncer::GROUP_UI, ui_thread_));
    }
  }

#if defined(OS_CHROMEOS)
  if (!disabled_types.Has(syncer::PRINTERS)) {
    controllers.push_back(
        CreateModelTypeControllerForModelRunningOnUIThread(syncer::PRINTERS));
  }
#endif

  // Reading list sync is enabled by default only on iOS. Register unless
  // Reading List or Reading List Sync is explicitly disabled.
  if (!disabled_types.Has(syncer::READING_LIST) &&
      reading_list::switches::IsReadingListEnabled()) {
    controllers.push_back(CreateModelTypeControllerForModelRunningOnUIThread(
        syncer::READING_LIST));
  }

  if (!disabled_types.Has(syncer::USER_EVENTS) &&
      FeatureList::IsEnabled(switches::kSyncUserEvents)) {
    controllers.push_back(CreateModelTypeControllerForModelRunningOnUIThread(
        syncer::USER_EVENTS));
  }

  if (base::FeatureList::IsEnabled(switches::kSyncUserConsentSeparateType)) {
    // Forward both on-disk and in-memory storage modes to the same delegate,
    // since behavior for USER_CONSENTS does not differ (they are always
    // persisted).
    // TODO(crbug.com/867801): Replace the proxy delegates below with a simpler
    // forwarding delegate that involves no posting of tasks.
    controllers.push_back(std::make_unique<ModelTypeController>(
        syncer::USER_CONSENTS,
        /*delegate_on_disk=*/
        std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
            ui_thread_,
            base::BindRepeating(
                &syncer::SyncClient::GetControllerDelegateForModelType,
                base::Unretained(sync_client_), syncer::USER_CONSENTS)),
        /*delegate_in_memory=*/
        std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
            ui_thread_,
            base::BindRepeating(
                &syncer::SyncClient::GetControllerDelegateForModelType,
                base::Unretained(sync_client_), syncer::USER_CONSENTS))));
  }

  return controllers;
}

std::unique_ptr<DataTypeManager>
ProfileSyncComponentsFactoryImpl::CreateDataTypeManager(
    syncer::ModelTypeSet initial_types,
    const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
        debug_info_listener,
    const DataTypeController::TypeMap* controllers,
    const syncer::DataTypeEncryptionHandler* encryption_handler,
    syncer::ModelTypeConfigurer* configurer,
    DataTypeManagerObserver* observer) {
  return std::make_unique<DataTypeManagerImpl>(
      sync_client_, initial_types, debug_info_listener, controllers,
      encryption_handler, configurer, observer);
}

std::unique_ptr<syncer::SyncEngine>
ProfileSyncComponentsFactoryImpl::CreateSyncEngine(
    const std::string& name,
    invalidation::InvalidationService* invalidator,
    const base::WeakPtr<syncer::SyncPrefs>& sync_prefs,
    const base::FilePath& sync_data_folder) {
  return std::make_unique<syncer::SyncBackendHostImpl>(
      name, sync_client_, invalidator, sync_prefs, sync_data_folder);
}

std::unique_ptr<syncer::LocalDeviceInfoProvider>
ProfileSyncComponentsFactoryImpl::CreateLocalDeviceInfoProvider() {
  return std::make_unique<syncer::LocalDeviceInfoProviderImpl>(
      channel_, version_, is_tablet_);
}

syncer::SyncApiComponentFactory::SyncComponents
ProfileSyncComponentsFactoryImpl::CreateBookmarkSyncComponents(
    std::unique_ptr<syncer::DataTypeErrorHandler> error_handler) {
  BookmarkModel* bookmark_model = sync_client_->GetBookmarkModel();
  syncer::UserShare* user_share =
      sync_client_->GetSyncService()->GetUserShare();
// TODO(akalin): We may want to propagate this switch up eventually.
#if defined(OS_ANDROID) || defined(OS_IOS)
  const bool kExpectMobileBookmarksFolder = true;
#else
  const bool kExpectMobileBookmarksFolder = false;
#endif

  auto model_associator = std::make_unique<BookmarkModelAssociator>(
      bookmark_model, sync_client_, user_share, error_handler->Copy(),
      kExpectMobileBookmarksFolder);

  SyncComponents components;
  components.change_processor = std::make_unique<BookmarkChangeProcessor>(
      sync_client_, model_associator.get(), std::move(error_handler));
  components.model_associator = std::move(model_associator);
  return components;
}

// static
void ProfileSyncComponentsFactoryImpl::OverridePrefsForUssTest(bool use_uss) {
  override_prefs_controller_to_uss_for_test_ = use_uss;
}

bool ProfileSyncComponentsFactoryImpl::
    override_prefs_controller_to_uss_for_test_ = false;

std::unique_ptr<ModelTypeController> ProfileSyncComponentsFactoryImpl::
    CreateModelTypeControllerForModelRunningOnUIThread(syncer::ModelType type) {
  // TODO(crbug.com/867801): Replace the proxy delegate below with a simpler
  // forwarding delegate that involves no posting of tasks.
  return std::make_unique<ModelTypeController>(
      type, std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
                ui_thread_,
                base::BindRepeating(
                    &syncer::SyncClient::GetControllerDelegateForModelType,
                    base::Unretained(sync_client_), type)));
}

std::unique_ptr<ModelTypeController>
ProfileSyncComponentsFactoryImpl::CreateWebDataModelTypeController(
    syncer::ModelType type,
    const base::RepeatingCallback<
        base::WeakPtr<syncer::ModelTypeControllerDelegate>(
            autofill::AutofillWebDataService*)>& delegate_from_web_data) {
  return std::make_unique<ModelTypeController>(
      type, std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
                db_thread_, base::BindRepeating(
                                delegate_from_web_data,
                                base::RetainedRef(web_data_service_on_disk_))));
}

std::unique_ptr<ModelTypeController>
ProfileSyncComponentsFactoryImpl::CreateWalletModelTypeController(
    syncer::ModelType type,
    const base::RepeatingCallback<
        base::WeakPtr<syncer::ModelTypeControllerDelegate>(
            autofill::AutofillWebDataService*)>& delegate_from_web_data) {
  return std::make_unique<AutofillWalletModelTypeController>(
      type,
      std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
          db_thread_,
          base::BindRepeating(delegate_from_web_data,
                              base::RetainedRef(web_data_service_on_disk_))),
      sync_client_);
}

std::unique_ptr<ModelTypeController> ProfileSyncComponentsFactoryImpl::
    CreateWalletModelTypeControllerWithInMemorySupport(
        syncer::ModelType type,
        const base::RepeatingCallback<
            base::WeakPtr<syncer::ModelTypeControllerDelegate>(
                autofill::AutofillWebDataService*)>& delegate_from_web_data) {
  return std::make_unique<AutofillWalletModelTypeController>(
      type, /*delegate_on_disk=*/
      std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
          db_thread_,
          base::BindRepeating(delegate_from_web_data,
                              base::RetainedRef(web_data_service_on_disk_))),
      /*delegate_in_memory=*/
      std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
          db_thread_,
          base::BindRepeating(delegate_from_web_data,
                              base::RetainedRef(web_data_service_in_memory_))),
      sync_client_);
}

}  // namespace browser_sync
