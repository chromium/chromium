// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_SENESCHAL_FAKE_SENESCHAL_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_SENESCHAL_FAKE_SENESCHAL_CLIENT_H_

#include "base/observer_list.h"
#include "chromeos/ash/components/dbus/seneschal/seneschal_client.h"

namespace ash {

// FakeSeneschalClient is a stub implementation of SeneschalClient used for
// testing.
class COMPONENT_EXPORT(SENESCHAL) FakeSeneschalClient : public SeneschalClient {
 public:
  // Returns the fake global instance if initialized. May return null.
  static FakeSeneschalClient* Get();

  FakeSeneschalClient(const FakeSeneschalClient&) = delete;
  FakeSeneschalClient& operator=(const FakeSeneschalClient&) = delete;

  // SeneschalClient:
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;
  void SharePath(
      const vm_tools::seneschal::SharePathRequest& request,
      chromeos::DBusMethodCallback<vm_tools::seneschal::SharePathResponse>
          callback) override;
  void UnsharePath(
      const vm_tools::seneschal::UnsharePathRequest& request,
      chromeos::DBusMethodCallback<vm_tools::seneschal::UnsharePathResponse>
          callback) override;

  bool share_path_called() const { return share_path_called_; }
  bool unshare_path_called() const { return unshare_path_called_; }

  void set_share_path_response(
      const vm_tools::seneschal::SharePathResponse& share_path_response) {
    share_path_response_ = share_path_response;
  }

  void set_unshare_path_response(
      const vm_tools::seneschal::UnsharePathResponse& unshare_path_response) {
    unshare_path_response_ = unshare_path_response;
  }

  const vm_tools::seneschal::SharePathRequest& last_share_path_request() const {
    return last_share_path_request_;
  }

  const vm_tools::seneschal::UnsharePathRequest& last_unshare_path_request()
      const {
    return last_unshare_path_request_;
  }

  void NotifySeneschalStopped();
  void NotifySeneschalStarted();

 protected:
  friend class SeneschalClient;

  FakeSeneschalClient();
  ~FakeSeneschalClient() override;

  void Init(dbus::Bus* bus) override {}

 private:
  void InitializeProtoResponses();

  base::ObserverList<Observer> observer_list_;

  bool share_path_called_ = false;
  bool unshare_path_called_ = false;

  vm_tools::seneschal::SharePathRequest last_share_path_request_;
  vm_tools::seneschal::UnsharePathRequest last_unshare_path_request_;
  vm_tools::seneschal::SharePathResponse share_path_response_;
  vm_tools::seneschal::UnsharePathResponse unshare_path_response_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_SENESCHAL_FAKE_SENESCHAL_CLIENT_H_
