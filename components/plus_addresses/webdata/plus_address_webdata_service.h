// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_WEBDATA_SERVICE_H_
#define COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_WEBDATA_SERVICE_H_

#include <memory>

#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/memory/scoped_refptr.h"
#include "base/task/sequenced_task_runner.h"
#include "components/plus_addresses/plus_address_types.h"
#include "components/webdata/common/web_data_service_base.h"
#include "components/webdata/common/web_data_service_consumer.h"

class WebDatabaseService;

namespace syncer {
class ModelTypeControllerDelegate;
}

namespace plus_addresses {

class PlusAddressSyncBridge;

// `PlusAddressWebDataService` acts as the bridge between code on the UI
// sequence (`PlusAddressService`) and code on the DB sequence (
// `PlusAddressTable` and `PlusAddressSyncBridge`). It should only be called
// from the UI sequence.
//
// It mirrors `PlusAddressTable`'s API and is responsible for posting tasks from
// the UI sequence to the DB sequence, invoking the relevant function on
// `PlusAddressTable`. For read operations, results are returned to a
// `WebDataServiceConsumer`, who must live on the UI sequence.
//
// Owned by `WebDataServiceWrapper`.
class PlusAddressWebDataService : public WebDataServiceBase {
 public:
  PlusAddressWebDataService(
      scoped_refptr<WebDatabaseService> wdbs,
      scoped_refptr<base::SequencedTaskRunner> ui_task_runner,
      scoped_refptr<base::SequencedTaskRunner> db_task_runner);

  // `PlusAddressTable`'s API.
  void GetPlusProfiles(WebDataServiceConsumer* consumer);
  void AddPlusProfile(const PlusProfile& profile);
  void ClearPlusProfiles();

  // Returns a controller delegate for the `sync_bridge` owned this service.
  std::unique_ptr<syncer::ModelTypeControllerDelegate>
  GetSyncControllerDelegate();

 protected:
  ~PlusAddressWebDataService() override;

 private:
  scoped_refptr<base::SequencedTaskRunner> ui_task_runner_;
  scoped_refptr<base::SequencedTaskRunner> db_task_runner_;

  // `PlusAddressWebDataService` owns the `PlusAddressSyncBridge`. However, the
  // bridge itself lives on the `db_task_runner_`. `SyncBridgeDBSequenceWrapper`
  // is a wrapper around the bridge, to ensure destruction happens on
  // `db_task_runner_`.
  struct SyncBridgeDBSequenceWrapper
      : public base::RefCountedDeleteOnSequence<SyncBridgeDBSequenceWrapper> {
    explicit SyncBridgeDBSequenceWrapper(
        scoped_refptr<base::SequencedTaskRunner> db_task_runner);

    // Should only be accessed through the `db_task_runner_`.
    std::unique_ptr<PlusAddressSyncBridge> sync_bridge;

   private:
    friend class base::RefCountedDeleteOnSequence<SyncBridgeDBSequenceWrapper>;
    friend class base::DeleteHelper<SyncBridgeDBSequenceWrapper>;

    ~SyncBridgeDBSequenceWrapper();
  };
  // `scoped_refptr<>`, because the destruction order of
  // `PlusAddressWebDataService` and `db_task_runner_` is unclear.
  // `PlusAddressWebDataService` is the primary owner.
  scoped_refptr<SyncBridgeDBSequenceWrapper> sync_bridge_wrapper_;
};

}  // namespace plus_addresses

#endif  // COMPONENTS_PLUS_ADDRESSES_WEBDATA_PLUS_ADDRESS_WEBDATA_SERVICE_H_
