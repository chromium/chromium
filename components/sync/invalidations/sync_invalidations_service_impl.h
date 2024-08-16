// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_IMPL_H_

#include <memory>
#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "base/sequence_checker.h"
#include "components/sync/base/data_type.h"
#include "components/sync/invalidations/sync_invalidations_service.h"

namespace gcm {
class GCMDriver;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace syncer {
class FCMHandler;
class InterestedDataTypesHandler;
class InvalidationsListener;

// The non-test implementation of SyncInvalidationsService.
class SyncInvalidationsServiceImpl : public SyncInvalidationsService {
 public:
  SyncInvalidationsServiceImpl(
      gcm::GCMDriver* gcm_driver,
      instance_id::InstanceIDDriver* instance_id_driver);
  ~SyncInvalidationsServiceImpl() override;

  // SyncInvalidationsService implementation.
  void AddListener(InvalidationsListener* listener) override;
  bool HasListener(InvalidationsListener* listener) override;
  void RemoveListener(InvalidationsListener* listener) override;
  void StartListening() override;
  void StopListening() override;
  void StopListeningPermanently() override;
  void AddTokenObserver(FCMRegistrationTokenObserver* observer) override;
  void RemoveTokenObserver(FCMRegistrationTokenObserver* observer) override;
  std::optional<std::string> GetFCMRegistrationToken() const override;
  void SetInterestedDataTypesHandler(
      InterestedDataTypesHandler* handler) override;
  std::optional<DataTypeSet> GetInterestedDataTypes() const override;
  void SetInterestedDataTypes(const DataTypeSet& data_types) override;
  void SetCommittedAdditionalInterestedDataTypesCallback(
      InterestedDataTypesAppliedCallback callback) override;

  // KeyedService overrides.
  void Shutdown() override;

  // Used in integration tests.
  FCMHandler* GetFCMHandlerForTesting();

 private:
  SEQUENCE_CHECKER(sequence_checker_);

  std::unique_ptr<FCMHandler> fcm_handler_;
  raw_ptr<InterestedDataTypesHandler> interested_data_types_handler_ = nullptr;
  std::optional<DataTypeSet> interested_data_types_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_IMPL_H_
