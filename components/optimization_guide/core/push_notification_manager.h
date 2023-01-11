// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_OPTIMIZATION_GUIDE_CORE_PUSH_NOTIFICATION_MANAGER_H_
#define COMPONENTS_OPTIMIZATION_GUIDE_CORE_PUSH_NOTIFICATION_MANAGER_H_

#include <string>

#include "base/containers/flat_set.h"
#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "components/optimization_guide/proto/hints.pb.h"
#include "components/optimization_guide/proto/push_notification.pb.h"

namespace optimization_guide {

// An interface for classes that implement the handling of push hint
// notifications. The primary task of this interface is to remove any hints from
// persistent storage that are invalidated by a more recent hint being pushed.
class PushNotificationManager {
 public:
  // Receives actions from the PushNotificationManager to execute when a new
  // pushed hint is being processed. The Delegate should be implemented by the
  // owner of the PushNotificationManager.
  class Delegate {
   public:
    // Corresponds to a batch of hints being processed, typically from a cache.
    virtual void RemoveFetchedEntriesByHintKeys(
        base::OnceClosure on_success,
        proto::KeyRepresentation key_representation,
        const base::flat_set<std::string>& hint_keys) = 0;
  };

  // Observer interface to process HintNotificationPayload.payload. Subclasses
  // should parse the optimization_guide::proto::Any to a specific proto type.
  class Observer : public base::CheckedObserver {
   public:
    // Processes the HintNotificationPayload.payload.
    virtual void OnNotificationPayload(
        proto::OptimizationType optimization_type,
        const proto::Any& payload) = 0;
  };

  PushNotificationManager(const PushNotificationManager&) = delete;
  PushNotificationManager& operator=(const PushNotificationManager&) = delete;
  PushNotificationManager();
  virtual ~PushNotificationManager();

  // Sets |this|'s delegate.
  void SetDelegate(Delegate* delegate);

  // Informs |this| that the delegate is ready to process pushed hints.
  virtual void OnDelegateReady();

  // Called when a new push notification arrives.
  virtual void OnNewPushNotification(
      const proto::HintNotificationPayload& notification);

  // Adds an observer to handle payload in HintNotificationPayload.payload.
  virtual void AddObserver(Observer* observer);

  // Removes an observer that handles HintNotificationPayload.payload.
  virtual void RemoveObserver(Observer* observer);

 protected:
  // Owns |this|
  raw_ptr<PushNotificationManager::Delegate> delegate_ = nullptr;

 private:
  friend class PushNotificationManagerUnitTest;
  void DispatchPayload(const proto::HintNotificationPayload& notification);

  // Observers to handle the custom payload.
  base::ObserverList<PushNotificationManager::Observer> observers_;
};

}  // namespace optimization_guide

#endif  // COMPONENTS_OPTIMIZATION_GUIDE_CORE_PUSH_NOTIFICATION_MANAGER_H_
