// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_IMPL_H_
#define COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_IMPL_H_

#include <memory>
#include <string>

#include "components/sync/invalidations/subscribed_data_types_manager.h"
#include "components/sync/invalidations/sync_invalidations_service.h"

namespace gcm {
class GCMDriver;
}

namespace instance_id {
class InstanceIDDriver;
}

namespace syncer {
class FCMHandler;
class InvalidationsListener;

// The non-test implementation of SyncInvalidationsService.
class SyncInvalidationsServiceImpl : public SyncInvalidationsService {
 public:
  SyncInvalidationsServiceImpl(
      gcm::GCMDriver* gcm_driver,
      instance_id::InstanceIDDriver* instance_id_driver,
      const std::string& sender_id,
      const std::string& app_id);
  ~SyncInvalidationsServiceImpl() override;

  // SyncInvalidationsService implementation.
  void AddListener(InvalidationsListener* listener) override;
  void RemoveListener(InvalidationsListener* listener) override;
  void AddTokenObserver(FCMRegistrationTokenObserver* observer) override;
  void RemoveTokenObserver(FCMRegistrationTokenObserver* observer) override;
  const std::string& GetFCMRegistrationToken() const override;
  void AddSubscribedDataTypesObserver(
      SubscribedDataTypesObserver* observer) override;
  void RemoveSubscribedDataTypesObserver(
      SubscribedDataTypesObserver* observer) override;
  const ModelTypeSet& GetSubscribedDataTypes() const override;
  void SetSubscribedDataTypes(const ModelTypeSet& data_types) override;

  // KeyedService overrides.
  void Shutdown() override;

 private:
  std::unique_ptr<FCMHandler> fcm_handler_;
  SubscribedDataTypesManager data_types_manager_;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_IMPL_H_
