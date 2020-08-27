// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_H_
#define COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_H_

#include <string>

#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/model_type.h"

namespace syncer {
class FCMRegistrationTokenObserver;
class InvalidationsListener;
class SubscribedDataTypesObserver;

// Service which is used to register with FCM. It is used to obtain an FCM token
// which is used to send invalidations from the server. The service also
// provides incoming invalidations handling and an interface to subscribe to
// invalidations. To subscribe for invalidations a new InvalidationsListener
// should be added.
class SyncInvalidationsService : public KeyedService {
 public:
  // Add or remove a new listener which will be notified on each new incoming
  // invalidation. |listener| must not be nullptr. If there is no such
  // |listener| then RemoveListener will do nothing.
  virtual void AddListener(InvalidationsListener* listener) = 0;
  virtual void RemoveListener(InvalidationsListener* listener) = 0;

  // Add or remove an FCM token change observer. |observer| must not be nullptr.
  virtual void AddTokenObserver(FCMRegistrationTokenObserver* observer) = 0;
  virtual void RemoveTokenObserver(FCMRegistrationTokenObserver* observer) = 0;

  // Used to get an obtained FCM token. Returns empty string if it hasn't been
  // received yet.
  virtual const std::string& GetFCMRegistrationToken() const = 0;

  // Add or remove a subscribed data types change observer. |observer| must not
  // be nullptr.
  virtual void AddSubscribedDataTypesObserver(
      SubscribedDataTypesObserver* observer) = 0;
  virtual void RemoveSubscribedDataTypesObserver(
      SubscribedDataTypesObserver* observer) = 0;

  // Get or set for which data types should the device receive invalidations.
  virtual const ModelTypeSet& GetSubscribedDataTypes() const = 0;
  virtual void SetSubscribedDataTypes(const ModelTypeSet& data_types) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_H_
