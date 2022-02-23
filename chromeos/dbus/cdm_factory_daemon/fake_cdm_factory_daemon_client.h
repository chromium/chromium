// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CDM_FACTORY_DAEMON_FAKE_CDM_FACTORY_DAEMON_CLIENT_H_
#define CHROMEOS_DBUS_CDM_FACTORY_DAEMON_FAKE_CDM_FACTORY_DAEMON_CLIENT_H_

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "chromeos/dbus/cdm_factory_daemon/cdm_factory_daemon_client.h"
#include "dbus/object_proxy.h"

namespace chromeos {

// Fake implementation of CdmFactoryDaemonClient. This is currently a no-op
// fake.
class FakeCdmFactoryDaemonClient : public CdmFactoryDaemonClient {
 public:
  FakeCdmFactoryDaemonClient();

  FakeCdmFactoryDaemonClient(const FakeCdmFactoryDaemonClient&) = delete;
  FakeCdmFactoryDaemonClient& operator=(const FakeCdmFactoryDaemonClient&) =
      delete;

  ~FakeCdmFactoryDaemonClient() override;

  // CdmFactoryDaemonClient:
  void BootstrapMojoConnection(
      base::ScopedFD fd,
      base::OnceCallback<void(bool success)> callback) override;
  void WaitForServiceToBeAvailable(
      WaitForServiceToBeAvailableCallback callback) override;
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CDM_FACTORY_DAEMON_FAKE_CDM_FACTORY_DAEMON_CLIENT_H_
