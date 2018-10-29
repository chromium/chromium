// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__
#define COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "components/sync/base/model_type.h"
#include "components/sync/driver/sync_api_component_factory.h"
#include "components/version_info/version_info.h"

namespace syncer {
class ModelTypeController;
class ModelTypeControllerDelegate;
class SyncClient;
}

namespace autofill {
class AutofillWebDataService;
}

namespace password_manager {
class PasswordStore;
}

namespace sync_bookmarks {
class BookmarkSyncService;
}

namespace browser_sync {

class ProfileSyncComponentsFactoryImpl
    : public syncer::SyncApiComponentFactory {
 public:
  ProfileSyncComponentsFactoryImpl(
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
      sync_bookmarks::BookmarkSyncService* bookmark_sync_service);
  ~ProfileSyncComponentsFactoryImpl() override;

  // SyncApiComponentFactory implementation:
  syncer::DataTypeController::TypeVector CreateCommonDataTypeControllers(
      syncer::ModelTypeSet disabled_types,
      syncer::LocalDeviceInfoProvider* local_device_info_provider) override;
  std::unique_ptr<syncer::DataTypeManager> CreateDataTypeManager(
      syncer::ModelTypeSet initial_types,
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      const syncer::DataTypeController::TypeMap* controllers,
      const syncer::DataTypeEncryptionHandler* encryption_handler,
      syncer::ModelTypeConfigurer* configurer,
      syncer::DataTypeManagerObserver* observer) override;
  std::unique_ptr<syncer::SyncEngine> CreateSyncEngine(
      const std::string& name,
      invalidation::InvalidationService* invalidator,
      const base::WeakPtr<syncer::SyncPrefs>& sync_prefs,
      const base::FilePath& sync_data_folder) override;
  std::unique_ptr<syncer::LocalDeviceInfoProvider>
  CreateLocalDeviceInfoProvider() override;
  syncer::SyncApiComponentFactory::SyncComponents CreateBookmarkSyncComponents(
      std::unique_ptr<syncer::DataTypeErrorHandler> error_handler) override;

  // Sets a bit that determines whether PREFERENCES should be registered with a
  // ModelTypeController for testing purposes.
  static void OverridePrefsForUssTest(bool use_uss);

 private:
  // Factory function for ModelTypeController instances for models living on
  // |ui_thread_|.
  std::unique_ptr<syncer::ModelTypeController>
  CreateModelTypeControllerForModelRunningOnUIThread(syncer::ModelType type);

  // Factory function for ModelTypeController instances for autofill-related
  // datatypes, which live in |db_thread_| and have a delegate accesible via
  // AutofillWebDataService.
  std::unique_ptr<syncer::ModelTypeController> CreateWebDataModelTypeController(
      syncer::ModelType type,
      const base::RepeatingCallback<
          base::WeakPtr<syncer::ModelTypeControllerDelegate>(
              autofill::AutofillWebDataService*)>& delegate_from_web_data);
  // Same as above, but for AUTOFILL_WALLET_* datatypes.
  std::unique_ptr<syncer::ModelTypeController> CreateWalletModelTypeController(
      syncer::ModelType type,
      const base::RepeatingCallback<
          base::WeakPtr<syncer::ModelTypeControllerDelegate>(
              autofill::AutofillWebDataService*)>& delegate_from_web_data);
  // Same as above, but datatypes supporting STORAGE_IN_MEMORY implemented
  // as an independent AutofillWebDataService, namely
  // |web_data_service_in_memory_|.
  std::unique_ptr<syncer::ModelTypeController>
  CreateWalletModelTypeControllerWithInMemorySupport(
      syncer::ModelType type,
      const base::RepeatingCallback<
          base::WeakPtr<syncer::ModelTypeControllerDelegate>(
              autofill::AutofillWebDataService*)>& delegate_from_web_data);

  // Client/platform specific members.
  syncer::SyncClient* const sync_client_;
  const version_info::Channel channel_;
  const std::string version_;
  const bool is_tablet_;
  const char* history_disabled_pref_;
  const scoped_refptr<base::SingleThreadTaskRunner> ui_thread_;
  const scoped_refptr<base::SingleThreadTaskRunner> db_thread_;
  const scoped_refptr<autofill::AutofillWebDataService>
      web_data_service_on_disk_;
  const scoped_refptr<autofill::AutofillWebDataService>
      web_data_service_in_memory_;
  const scoped_refptr<password_manager::PasswordStore> password_store_;
  sync_bookmarks::BookmarkSyncService* const bookmark_sync_service_;

  // Whether to override PREFERENCES to use USS.
  static bool override_prefs_controller_to_uss_for_test_;

  DISALLOW_COPY_AND_ASSIGN(ProfileSyncComponentsFactoryImpl);
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_PROFILE_SYNC_COMPONENTS_FACTORY_IMPL_H__
