// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_DBUS_CUPS_PROXY_FAKE_CUPS_PROXY_CLIENT_H_
#define CHROMEOS_DBUS_CUPS_PROXY_FAKE_CUPS_PROXY_CLIENT_H_

#include "base/callback_forward.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "chromeos/dbus/cups_proxy/cups_proxy_client.h"
#include "dbus/object_proxy.h"

namespace chromeos {

// Fake implementation of CupsProxyClient. This is currently a no-op fake.
class FakeCupsProxyClient : public CupsProxyClient {
 public:
  FakeCupsProxyClient();
  ~FakeCupsProxyClient() override;

  // CupsProxyClient:
  void WaitForServiceToBeAvailable(
      dbus::ObjectProxy::WaitForServiceToBeAvailableCallback callback) override;
  void BootstrapMojoConnection(
      base::ScopedFD fd,
      base::OnceCallback<void(bool success)> result_callback) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeCupsProxyClient);
};

}  // namespace chromeos

#endif  // CHROMEOS_DBUS_CUPS_PROXY_FAKE_CUPS_PROXY_CLIENT_H_
