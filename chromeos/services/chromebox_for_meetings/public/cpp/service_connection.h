// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_SERVICE_CONNECTION_H_
#define CHROMEOS_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_SERVICE_CONNECTION_H_

#include "base/functional/bind.h"
#include "chromeos/services/chromebox_for_meetings/public/mojom/cfm_service_manager.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace chromeos {
namespace cfm {

// Encapsulates a connection to the CfM Mojo Broker Service daemon via its Mojo
// interface.
//
// Sequencing: Must be used on a single sequence (may be created on another).
class COMPONENT_EXPORT(CHROMEOS_CFMSERVICE) ServiceConnection {
 public:
  // Gets the ServiceConnection singleton, or a test fake if one has been
  // specified
  static ServiceConnection* GetInstance();

  // Overrides the result of GetInstance() for use in tests.
  // Does not take ownership of |fake_service_connection|.
  static void UseFakeServiceConnectionForTesting(
      ServiceConnection* fake_service_connection);

  // Binds a |CfMServiceContext| receiver to this implementation in order to
  // forward requests to the underlying daemon.
  // Note: A mojo::Remote<mojom::CfMServiceContext> bound using this method does
  // not directly affect the lifetime of the underlying daemon, and so it is
  // safe to release the remote when no longer in use.
  virtual void BindServiceContext(
      mojo::PendingReceiver<chromeos::cfm::mojom::CfmServiceContext>
          receiver) = 0;

 protected:
  ServiceConnection() = default;
  virtual ~ServiceConnection() = default;

 private:
  // Gets the ServiceConnection singleton, used by the platform specific
  // implementation.
  static ServiceConnection* GetInstanceForCurrentPlatform();
};

}  // namespace cfm
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CHROMEBOX_FOR_MEETINGS_PUBLIC_CPP_SERVICE_CONNECTION_H_
