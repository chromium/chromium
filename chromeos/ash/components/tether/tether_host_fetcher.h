// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_HOST_FETCHER_H_
#define CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_HOST_FETCHER_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/observer_list.h"
#include "chromeos/ash/components/multidevice/remote_device_ref.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

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

  TetherHostFetcher(const TetherHostFetcher&) = delete;
  TetherHostFetcher& operator=(const TetherHostFetcher&) = delete;

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
      base::OnceCallback<void(absl::optional<multidevice::RemoteDeviceRef>)>;
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
};

}  // namespace tether

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_TETHER_TETHER_HOST_FETCHER_H_
