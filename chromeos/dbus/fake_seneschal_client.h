// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_FAKE_SENESCHAL_CLIENT_H_
#define CHROMEOS_DBUS_FAKE_SENESCHAL_CLIENT_H_

#include "base/observer_list.h"
#include "chromeos/dbus/seneschal_client.h"

namespace chromeos {

// FakeSeneschalClient is a stub implementation of SeneschalClient used for
// testing.
class CHROMEOS_EXPORT FakeSeneschalClient : public SeneschalClient {
 public:
  FakeSeneschalClient();
  ~FakeSeneschalClient() override;

  // SeneschalClient:
  void SharePath(const vm_tools::seneschal::SharePathRequest& request,
                 DBusMethodCallback<vm_tools::seneschal::SharePathResponse>
                     callback) override;

  bool share_path_called() const { return share_path_called_; }

  void set_share_path_response(
      const vm_tools::seneschal::SharePathResponse& share_path_response) {
    share_path_response_ = share_path_response;
  }

  const vm_tools::seneschal::SharePathRequest& last_request() const {
    return last_request_;
  }

 protected:
  void Init(dbus::Bus* bus) override {}

 private:
  void InitializeProtoResponses();

  bool share_path_called_ = false;

  vm_tools::seneschal::SharePathRequest last_request_;
  vm_tools::seneschal::SharePathResponse share_path_response_;

  DISALLOW_COPY_AND_ASSIGN(FakeSeneschalClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_FAKE_SENESCHAL_CLIENT_H_
