// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/plus_addresses/webdata/plus_address_webdata_service.h"

#include <memory>

#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/bind_post_task.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/plus_addresses/webdata/plus_address_sync_bridge.h"
#include "components/plus_addresses/webdata/plus_address_sync_util.h"
#include "components/plus_addresses/webdata/plus_address_table.h"
#include "components/sync/base/features.h"
#include "components/sync/base/model_type.h"
#include "components/sync/model/client_tag_based_model_type_processor.h"
#include "components/sync/model/proxy_model_type_controller_delegate.h"
#include "components/webdata/common/web_data_results.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_database.h"
#include "components/webdata/common/web_database_backend.h"
#include "components/webdata/common/web_database_service.h"

namespace plus_addresses {

PlusAddressWebDataService::PlusAddressWebDataService(
    scoped_refptr<WebDatabaseService> wdbs,
    scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
    scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : WebDataServiceBase(wdbs, ui_task_runner),
      ui_task_runner_(std::move(ui_task_runner)),
      db_task_runner_(std::move(db_task_runner)) {
  if (base::FeatureList::IsEnabled(syncer::kSyncPlusAddress)) {
    sync_bridge_wrapper_ =
        base::MakeRefCounted<SyncBridgeDBSequenceWrapper>(db_task_runner_);

    // When sync changes `PlusAddressTable`, observers on the `ui_task_runner_`
    // are notified. To avoid round trips to the `db_task_runner_`, this
    // notification includes the set of addition and removal operations
    // committed to the database from the sync bridge.
    PlusAddressSyncBridge::DataChangedBySyncCallback notify_sync_observers =
        base::BindPostTask(
            ui_task_runner_,
            base::BindRepeating(
                &PlusAddressWebDataService::NotifyOnWebDataChangedBySync,
                weak_factory_.GetWeakPtr()));

    // The `state->sync_bridge` can only be used on the sequence that it
    // was constructed on. Ensure it is created on the `db_task_runner_`.
    db_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<WebDatabaseBackend> db_backend,
               PlusAddressSyncBridge::DataChangedBySyncCallback
                   notify_observers,
               SyncBridgeDBSequenceWrapper* wrapper) {
              wrapper->sync_bridge = std::make_unique<PlusAddressSyncBridge>(
                  std::make_unique<syncer::ClientTagBasedModelTypeProcessor>(
                      syncer::PLUS_ADDRESS,
                      /*dump_stack=*/base::DoNothing()),
                  std::move(db_backend), std::move(notify_observers));
            },
            wdbs_->GetBackend(), std::move(notify_sync_observers),
            base::RetainedRef(sync_bridge_wrapper_)));
  }
}

PlusAddressWebDataService::~PlusAddressWebDataService() = default;

PlusAddressWebDataService::SyncBridgeDBSequenceWrapper::
    SyncBridgeDBSequenceWrapper(
        scoped_refptr<base::SequencedTaskRunner> db_task_runner)
    : base::RefCountedDeleteOnSequence<SyncBridgeDBSequenceWrapper>(
          db_task_runner) {}

PlusAddressWebDataService::SyncBridgeDBSequenceWrapper::
    ~SyncBridgeDBSequenceWrapper() = default;

void PlusAddressWebDataService::GetPlusProfiles(
    WebDataServiceConsumer* consumer) {
  CHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  wdbs_->ScheduleDBTaskWithResult(
      FROM_HERE,
      base::BindOnce([](WebDatabase* db) -> std::unique_ptr<WDTypedResult> {
        return std::make_unique<WDResult<std::vector<PlusProfile>>>(
            PLUS_ADDRESS_RESULT,
            PlusAddressTable::FromWebDatabase(db)->GetPlusProfiles());
      }),
      consumer);
}

void PlusAddressWebDataService::AddOrUpdatePlusProfile(
    const PlusProfile& profile) {
  CHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  auto db_task = base::BindOnce(
      [](const PlusProfile& profile, WebDatabase* db) {
        return PlusAddressTable::FromWebDatabase(db)->AddOrUpdatePlusProfile(
                   profile)
                   ? WebDatabase::COMMIT_NEEDED
                   : WebDatabase::COMMIT_NOT_NEEDED;
      },
      profile);
  wdbs_->ScheduleDBTask(FROM_HERE, std::move(db_task));
}

void PlusAddressWebDataService::ClearPlusProfiles() {
  CHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  wdbs_->ScheduleDBTask(
      FROM_HERE, base::BindOnce([](WebDatabase* db) {
        return PlusAddressTable::FromWebDatabase(db)->ClearPlusProfiles()
                   ? WebDatabase::COMMIT_NEEDED
                   : WebDatabase::COMMIT_NOT_NEEDED;
      }));
}

std::unique_ptr<syncer::ModelTypeControllerDelegate>
PlusAddressWebDataService::GetSyncControllerDelegate() {
  CHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  CHECK(base::FeatureList::IsEnabled(syncer::kSyncPlusAddress));
  // `sync_bridge` operates on the `db_task_runner_` - use a
  // `ProxyModelTypeControllerDelegate` to forward calls to that sequence.
  // Because `db_task_runner_` is a `SequencedTaskRunner`, the `sync_bridge`
  // will already be initialized in the callback by the task posted in the
  // constructor.
  return std::make_unique<syncer::ProxyModelTypeControllerDelegate>(
      db_task_runner_, base::BindRepeating(
                           [](SyncBridgeDBSequenceWrapper* wrapper) {
                             return wrapper->sync_bridge->change_processor()
                                 ->GetControllerDelegate();
                           },
                           base::RetainedRef(sync_bridge_wrapper_)));
}

void PlusAddressWebDataService::NotifyOnWebDataChangedBySync(
    std::vector<PlusAddressSyncDataChange> changes) {
  CHECK(ui_task_runner_->RunsTasksInCurrentSequence());
  for (Observer& o : observers_) {
    for (const PlusAddressSyncDataChange& change : changes) {
      o.OnWebDataChangedBySync(change);
    }
  }
}

bool IsSyncingPlusAddresses() {
  return base::FeatureList::IsEnabled(syncer::kSyncPlusAddress);
}

}  // namespace plus_addresses
