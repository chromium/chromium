// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_DATA_SHARING_CHANGE_NOTIFIER_H_
#define COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_DATA_SHARING_CHANGE_NOTIFIER_H_

#include "base/functional/callback.h"
#include "components/data_sharing/public/data_sharing_service.h"
#include "google_apis/gaia/gaia_id.h"

namespace base {
class Time;
}  // namespace base

namespace collaboration::messaging {

// The `DataSharingChangeNotifier` is an interface that observes the
// `DataSharingService` and translates its events into a simplified,
// delta-based format for the messaging backend.
//
// This class is responsible for:
// - Observing group and member changes from the `DataSharingService`.
// - Computing deltas (e.g., added, removed) to represent these changes.
// - Notifying its observers of these deltas asynchronously.
//
// The observers of this class are expected to handle the simplified change
// events to update their own state and the UI accordingly.
class DataSharingChangeNotifier
    : public data_sharing::DataSharingService::Observer {
 public:
  using FlushCallback = base::OnceClosure;

  // A delta observer that is invoked asynchronously based on updates from the
  // DataSharingService.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the DataSharingServiceChangeNofitier has been initialized.
    // Use DataSharingChangeNotifier::IsInitialized() to check whether it has
    // already been initialized. If the service has already been initialized
    // when an observer is added, this is invoked asynchronously.
    // No other observer calls happen until the FlushCallback returned from
    // Initialize has been invoked, which flushes all events up until then.
    virtual void OnDataSharingChangeNotifierInitialized() {}

    // User either created a new group or has joined an existing one.
    // GroupData is provided as long as it is still available.
    virtual void OnGroupAdded(
        const data_sharing::GroupId& group_id,
        const std::optional<data_sharing::GroupData>& group_data,
        const base::Time& event_time) {}
    // Either group has been deleted or user has been removed from the group.
    // GroupData is provided as long as it is still available.
    virtual void OnGroupRemoved(
        const data_sharing::GroupId& group_id,
        const std::optional<data_sharing::GroupData>& group_data,
        const base::Time& event_time) {}

    // Called when a new member has been added to the group.
    virtual void OnGroupMemberAdded(const data_sharing::GroupData& group_data,
                                    const GaiaId& member_gaia_id,
                                    const base::Time& event_time) {}
    // Called when a member has been removed from the group.
    virtual void OnGroupMemberRemoved(const data_sharing::GroupData& group_data,
                                      const GaiaId& member_gaia_id,
                                      const base::Time& event_time) {}
  };

  ~DataSharingChangeNotifier() override;

  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Kicks off the initialization of this component. Returns a callback for when
  // the service can start flushing events since startup. The flushing of events
  // happen synchronously.
  virtual FlushCallback Initialize() = 0;

  // Whether this instance has finished initialization.
  virtual bool IsInitialized() = 0;
};

}  // namespace collaboration::messaging

#endif  // COMPONENTS_COLLABORATION_INTERNAL_MESSAGING_DATA_SHARING_CHANGE_NOTIFIER_H_
