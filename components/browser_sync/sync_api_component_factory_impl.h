// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_BROWSER_SYNC_SYNC_API_COMPONENT_FACTORY_IMPL_H_
#define COMPONENTS_BROWSER_SYNC_SYNC_API_COMPONENT_FACTORY_IMPL_H_

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/sync/base/model_type.h"
#include "components/sync/service/sync_api_component_factory.h"
#include "components/version_info/channel.h"

namespace syncer {
class ModelTypeController;
class ModelTypeControllerDelegate;
class SyncInvalidationsService;
class SyncService;
}  // namespace syncer

namespace autofill {
class AutofillWebDataService;
}

namespace password_manager {
class PasswordStoreInterface;
}

namespace sync_bookmarks {
class BookmarkSyncService;
}

namespace power_bookmarks {
class PowerBookmarkService;
}
namespace supervised_user {
class SupervisedUserSettingsService;
}  // namespace supervised_user

namespace browser_sync {

class BrowserSyncClient;

class SyncApiComponentFactoryImpl : public syncer::SyncApiComponentFactory {
 public:
  SyncApiComponentFactoryImpl(
      BrowserSyncClient* sync_client,
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
          supervised_user_settings_service);
  SyncApiComponentFactoryImpl(const SyncApiComponentFactoryImpl&) = delete;
  SyncApiComponentFactoryImpl& operator=(const SyncApiComponentFactoryImpl&) =
      delete;
  ~SyncApiComponentFactoryImpl() override;

  // Creates and returns enabled datatypes and their controllers.
  // `disabled_types` allows callers to prevent certain types from being
  // created.
  syncer::DataTypeController::TypeVector CreateCommonDataTypeControllers(
      syncer::ModelTypeSet disabled_types,
      syncer::SyncService* sync_service);

  // SyncApiComponentFactory implementation:
  std::unique_ptr<syncer::DataTypeManager> CreateDataTypeManager(
      const syncer::DataTypeController::TypeMap* controllers,
      const syncer::DataTypeEncryptionHandler* encryption_handler,
      syncer::ModelTypeConfigurer* configurer,
      syncer::DataTypeManagerObserver* observer) override;
  std::unique_ptr<syncer::SyncEngine> CreateSyncEngine(
      const std::string& name,
      syncer::SyncInvalidationsService* sync_invalidation_service) override;
  void ClearAllTransportData() override;

 private:
  // Factory function for ModelTypeControllerDelegate instances for models
  // living in `ui_thread_` that have their delegate accessible via SyncClient.
  std::unique_ptr<syncer::ModelTypeControllerDelegate>
  CreateForwardingControllerDelegate(syncer::ModelType type);

  // Factory function for ModelTypeController instances for wallet-related
  // datatypes, which live in `db_thread_` and have a delegate accessible via
  // AutofillWebDataService.
  // If `with_transport_mode_support` is true, the controller will support
  // transport mode, implemented via an independent AutofillWebDataService,
  // namely `web_data_service_in_memory_`.
  std::unique_ptr<syncer::ModelTypeController> CreateWalletModelTypeController(
      syncer::ModelType type,
      const base::RepeatingCallback<
          base::WeakPtr<syncer::ModelTypeControllerDelegate>(
              autofill::AutofillWebDataService*)>& delegate_from_web_data,
      syncer::SyncService* sync_service,
      bool with_transport_mode_support);

  // Client/platform specific members.
  const raw_ptr<BrowserSyncClient> sync_client_;
  const version_info::Channel channel_;
  const scoped_refptr<base::SequencedTaskRunner> ui_thread_;
  const scoped_refptr<base::SequencedTaskRunner> db_thread_;
  const scoped_refptr<base::SequencedTaskRunner>
      engines_and_directory_deletion_thread_;
  const scoped_refptr<autofill::AutofillWebDataService>
      web_data_service_on_disk_;
  const scoped_refptr<autofill::AutofillWebDataService>
      web_data_service_in_memory_;
  const scoped_refptr<password_manager::PasswordStoreInterface>
      profile_password_store_;
  const scoped_refptr<password_manager::PasswordStoreInterface>
      account_password_store_;
  const raw_ptr<sync_bookmarks::BookmarkSyncService>
      local_or_syncable_bookmark_sync_service_;
  const raw_ptr<sync_bookmarks::BookmarkSyncService>
      account_bookmark_sync_service_;
  const raw_ptr<power_bookmarks::PowerBookmarkService> power_bookmark_service_;
  const raw_ptr<supervised_user::SupervisedUserSettingsService>
      supervised_user_settings_service_;
};

}  // namespace browser_sync

#endif  // COMPONENTS_BROWSER_SYNC_SYNC_API_COMPONENT_FACTORY_IMPL_H_
