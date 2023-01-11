// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_COMPONENTS_DBUS_CHROMEBOX_FOR_MEETINGS_FAKE_CFM_HOTLINE_CLIENT_H_
#define CHROMEOS_ASH_COMPONENTS_DBUS_CHROMEBOX_FOR_MEETINGS_FAKE_CFM_HOTLINE_CLIENT_H_

#include "base/files/scoped_file.h"
#include "base/functional/callback_forward.h"

#include "chromeos/ash/components/dbus/chromebox_for_meetings/cfm_hotline_client.h"

namespace ash {

// Fake implementation of CfmHotlineClient. This is currently a no-op fake.
class COMPONENT_EXPORT(CFM_HOTLINE_CLIENT) FakeCfmHotlineClient
    : public CfmHotlineClient {
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

  // Fake a ::cfm::broker::kMojoServiceRequestedSignal signal event
  bool FakeEmitSignal(const std::string& interface_name);

 private:
  // A list of observers that are listening on state changes, etc.
  cfm::CfmObserverList observer_list_;
};

}  // namespace ash

#endif  // CHROMEOS_ASH_COMPONENTS_DBUS_CHROMEBOX_FOR_MEETINGS_FAKE_CFM_HOTLINE_CLIENT_H_
