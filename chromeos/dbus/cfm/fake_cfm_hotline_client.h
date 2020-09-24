// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CFM_FAKE_CFM_HOTLINE_CLIENT_H_
#define CHROMEOS_DBUS_CFM_FAKE_CFM_HOTLINE_CLIENT_H_

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"

#include "chromeos/dbus/cfm/cfm_hotline_client.h"

namespace chromeos {

// Fake implementation of CfmHotlineClient. This is currently a no-op fake.
class FakeCfmHotlineClient : public CfmHotlineClient {
 public:
  FakeCfmHotlineClient();
  FakeCfmHotlineClient(const FakeCfmHotlineClient&) = delete;
  FakeCfmHotlineClient& operator=(const FakeCfmHotlineClient&) = delete;
  ~FakeCfmHotlineClient() override;

  // CfmHotlineClient:
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;
  void BootstrapMojoConnection(
      base::ScopedFD fd,
      BootstrapMojoConnectionCallback result_callback) override;
  void AddObserver(cfm::CfmObserver* observer) override;
  void RemoveObserver(cfm::CfmObserver* observer) override;

 private:
  // A list of observers that are listening on state changes, etc.
  cfm::CfmObserverList observer_list_;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CFM_FAKE_CFM_HOTLINE_CLIENT_H_
