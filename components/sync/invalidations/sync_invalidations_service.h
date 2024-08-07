// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_H_
#define COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sync/base/data_type.h"

namespace syncer {
class FCMRegistrationTokenObserver;
class InvalidationsListener;
class InterestedDataTypesHandler;

// Service which is used to register with FCM. It is used to obtain an FCM token
// which is used to send invalidations from the server. The service also
// provides incoming invalidations handling and an interface to subscribe to
// invalidations. To subscribe for invalidations a new InvalidationsListener
// should be added.
class SyncInvalidationsService : public KeyedService {
 public:
  // Data types which are newly marked as interesting will be passed to the
  // callback.
  using InterestedDataTypesAppliedCallback =
      base::RepeatingCallback<void(const DataTypeSet&)>;

  // Starts handling incoming invalidations and obtains an FCM token if it
  // doesn't have one.
  virtual void StartListening() = 0;

  // Stop listening to invalidations and doesn't remove FCM registration token.
  virtual void StopListening() = 0;

  // Stop listening to invalidations and removes FCM registration token.
  virtual void StopListeningPermanently() = 0;

  // Add a new |listener| which will be notified on each new incoming
  // invalidation. |listener| must not be nullptr. Does nothing if the
  // |listener| has already been added before. When a new |listener| is added,
  // previously received messages will be immediately replayed.
  virtual void AddListener(InvalidationsListener* listener) = 0;

  // Returns whether `listener` was added.
  virtual bool HasListener(InvalidationsListener* listener) = 0;

  // Removes |listener|, does nothing if it wasn't added before. |listener| must
  // not be nullptr.
  virtual void RemoveListener(InvalidationsListener* listener) = 0;

  // Add or remove an FCM token change observer. |observer| must not be nullptr.
  virtual void AddTokenObserver(FCMRegistrationTokenObserver* observer) = 0;
  virtual void RemoveTokenObserver(FCMRegistrationTokenObserver* observer) = 0;

  // Used to get an obtained FCM token. std::nullopt is returned if the token
  // has been requested but hasn't been received yet. Returns an empty string if
  // the device is not listening to invalidations.
  virtual std::optional<std::string> GetFCMRegistrationToken() const = 0;

  // Set the interested data types change handler. |handler| can be nullptr to
  // unregister any existing handler. There can be at most one handler.
  virtual void SetInterestedDataTypesHandler(
      InterestedDataTypesHandler* handler) = 0;

  // Get or set for which data types should the device receive invalidations.
  // GetInterestedDataTypes() will return base::nullptr until
  // SetInterestedDataTypes() has been called at least once.
  virtual std::optional<DataTypeSet> GetInterestedDataTypes() const = 0;
  virtual void SetInterestedDataTypes(const DataTypeSet& data_types) = 0;
  virtual void SetCommittedAdditionalInterestedDataTypesCallback(
      InterestedDataTypesAppliedCallback callback) = 0;
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_INVALIDATIONS_SYNC_INVALIDATIONS_SERVICE_H_
