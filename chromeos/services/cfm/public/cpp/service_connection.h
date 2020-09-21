// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_CFM_PUBLIC_CPP_SERVICE_CONNECTION_H_
#define CHROMEOS_SERVICES_CFM_PUBLIC_CPP_SERVICE_CONNECTION_H_

#include <string>
#include "base/bind.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

#include "chromeos/services/cfm/public/mojom/cfm_service_manager.mojom.h"

namespace chromeos {
namespace cfm {

// Encapsulates a connection to the CfM Mojo Broker Service daemon via its Mojo
// interface.
//
// Sequencing: Must be used on a single sequence (may be created on another).
class ServiceConnection {
 public:
  static ServiceConnection* GetInstance();

  // Overrides the result of GetInstance() for use in tests.
  // Does not take ownership of |fake_service_connection|.
  static void UseFakeServiceConnectionForTesting(
      ServiceConnection* fake_service_connection);

  // Bind to the CfM Service Context Daemon
  virtual void BindServiceContext(
      mojo::PendingReceiver<mojom::CfmServiceContext> receiver) = 0;

 protected:
  ServiceConnection() = default;
  virtual ~ServiceConnection() = default;
};

}  // namespace cfm
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_CFM_PUBLIC_CPP_SERVICE_CONNECTION_H_
