// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CFM_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_
#define CHROMEOS_SERVICES_CFM_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_

#include <string>
#include "base/bind.h"
#include "chromeos/dbus/cfm/cfm_hotline_client.h"
#include "chromeos/services/cfm/public/cpp/service_connection.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

#include "chromeos/services/cfm/public/mojom/cfm_service_manager.mojom.h"

namespace chromeos {
namespace cfm {

// Fake implementation of chromeos::cfm::ServiceConnection.
// For use with ServiceConnection::UseFakeServiceConnectionForTesting().
class FakeServiceConnectionImpl : public ServiceConnection {
 public:
  using FakeBootstrapCallback =
      CfmHotlineClient::BootstrapMojoConnectionCallback;

  FakeServiceConnectionImpl();
  FakeServiceConnectionImpl(const FakeServiceConnectionImpl&) = delete;
  FakeServiceConnectionImpl& operator=(const FakeServiceConnectionImpl&) =
      delete;
  ~FakeServiceConnectionImpl() override;

  void BindServiceContext(
      mojo::PendingReceiver<mojom::CfmServiceContext> receiver) override;

  void SetCallback(FakeBootstrapCallback callback);

 private:
  void CfMContextServiceStarted(
      mojo::PendingReceiver<mojom::CfmServiceContext> receiver,
      bool is_available);

  FakeBootstrapCallback callback_;
};

}  // namespace cfm
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CFM_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_
