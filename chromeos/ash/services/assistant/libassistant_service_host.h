// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_ASSISTANT_LIBASSISTANT_SERVICE_HOST_H_
#define CHROMEOS_ASH_SERVICES_ASSISTANT_LIBASSISTANT_SERVICE_HOST_H_

#include "chromeos/ash/services/libassistant/public/mojom/service.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

namespace ash::assistant {

// Interface which can be implemented to control launching and the lifetime of
// the Libassistant service. The API is losely inspired by
// ServiceProcessHost::Launch(), to make it easier to migrate to a real mojom
// service running in its own process.
class LibassistantServiceHost {
 public:
  virtual ~LibassistantServiceHost() = default;

  // Launch the mojom service. Barring crashes, the service will remain running
  // as long as both the receiver and this host class remain alive, or until
  // |Stop| is called.
  virtual void Launch(
      mojo::PendingReceiver<libassistant::mojom::LibassistantService>
          receiver) = 0;

  // Stop the mojom service.
  virtual void Stop() = 0;
};

}  // namespace ash::assistant

#endif  // CHROMEOS_ASH_SERVICES_ASSISTANT_LIBASSISTANT_SERVICE_HOST_H_
