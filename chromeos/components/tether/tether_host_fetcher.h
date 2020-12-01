// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_TETHER_HOST_FETCHER_H_
#define CHROMEOS_COMPONENTS_TETHER_TETHER_HOST_FETCHER_H_

#include <memory>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "chromeos/components/multidevice/remote_device_ref.h"

namespace chromeos {

namespace tether {

// Fetches RemoteDevice objects corresponding to tether hosts which have been
// synced via CryptAuth.
// TODO(khorimoto): Update functions to return RemoteDevice objects directly
//     instead of via a callback. This pattern is an artifact from when these
//     objects were fetched asynchronously. This refactor should wait until the
//     CrOS MultiDevice APIs are complete (see crbug.com/752273).
class TetherHostFetcher {
 public:
  class Observer {
   public:
    virtual void OnTetherHostsUpdated() = 0;
  };

  TetherHostFetcher();
  virtual ~TetherHostFetcher();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  virtual bool HasSyncedTetherHosts() = 0;

  // Fetches all tether hosts.
  using TetherHostListCallback =
      base::OnceCallback<void(const multidevice::RemoteDeviceRefList&)>;
  virtual void FetchAllTetherHosts(TetherHostListCallback callback) = 0;

  // Fetches the tether host with the ID |device_id|.
  using TetherHostCallback =
      base::OnceCallback<void(base::Optional<multidevice::RemoteDeviceRef>)>;
  virtual void FetchTetherHost(const std::string& device_id,
                               TetherHostCallback callback) = 0;

 protected:
  void ProcessFetchAllTetherHostsRequest(
      const multidevice::RemoteDeviceRefList& remote_device_list,
      TetherHostListCallback callback);
  void ProcessFetchSingleTetherHostRequest(
      const std::string& device_id,
      const multidevice::RemoteDeviceRefList& remote_device_list,
      TetherHostCallback callback);

  void NotifyTetherHostsUpdated();

 private:
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(TetherHostFetcher);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_TETHER_HOST_FETCHER_H_
