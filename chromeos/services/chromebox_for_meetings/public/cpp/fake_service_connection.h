// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_
#define CHROMEOS_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_

#include "base/functional/bind.h"
#include "chromeos/services/chromebox_for_meetings/public/cpp/service_connection.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace cfm {

// Fake implementation of cfm::ServiceConnection.
// For use with ServiceConnection::UseFakeServiceConnectionForTesting().
class FakeServiceConnectionImpl
    : public ServiceConnection {
 public:
  using FakeBootstrapCallback = base::OnceCallback<void(
      mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext>,
      bool)>;

  FakeServiceConnectionImpl();
  FakeServiceConnectionImpl(const FakeServiceConnectionImpl&) = delete;
  FakeServiceConnectionImpl& operator=(const FakeServiceConnectionImpl&) =
      delete;
  ~FakeServiceConnectionImpl() override;

  void BindServiceContext(
      mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext>
          pending_receiver) override;

  void SetCallback(FakeBootstrapCallback callback);

 private:
  void CfMContextServiceStarted(
      mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext>
          pending_receiver,
      bool is_available);

  FakeBootstrapCallback callback_;
};

}  // namespace cfm
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_FAKE_SERVICE_CONNECTION_H_
