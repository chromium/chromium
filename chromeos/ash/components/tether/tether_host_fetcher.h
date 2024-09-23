// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_HOST_FETCHER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_HOST_FETCHER_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"

namespace ash::tether {

// Fetches RemoteDevice objects corresponding to tether hosts which have been
// synced via CryptAuth.
class TetherHostFetcher {
 public:
  class Observer {
   public:
    virtual void OnTetherHostUpdated() = 0;
  };

  TetherHostFetcher();

  TetherHostFetcher(const TetherHostFetcher&) = delete;
  TetherHostFetcher& operator=(const TetherHostFetcher&) = delete;

  virtual ~TetherHostFetcher();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  std::optional<multidevice::RemoteDeviceRef> GetTetherHost();

 protected:
  void NotifyTetherHostUpdated();

  std::optional<multidevice::RemoteDeviceRef> tether_host_;

 private:
  base::ObserverList<Observer>::Unchecked observers_;
};

}  // namespace ash::tether

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_HOST_FETCHER_H_
