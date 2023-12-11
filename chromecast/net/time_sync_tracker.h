// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_TIME_SYNC_TRACKER_H_
#define CHROMECAST_NET_TIME_SYNC_TRACKER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace chromecast {

// Tracks whether or not time has synced on the device.
//
// In cases where general network connectivity does not include whether or not
// time has synced, this class provides that information.
class TimeSyncTracker {
 public:
  class Observer : public base::CheckedObserver {
   public:
    virtual void OnTimeSynced() = 0;

   protected:
    Observer() {}
    ~Observer() override = default;
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Returns if the time has been synced.
  virtual bool IsTimeSynced() const = 0;

 protected:
  TimeSyncTracker();
  virtual ~TimeSyncTracker();

  // Notifies observer that time has been synced.
  void Notify();

 private:
  base::ObserverList<Observer> observer_list_;
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_TIME_SYNC_TRACKER_H_
