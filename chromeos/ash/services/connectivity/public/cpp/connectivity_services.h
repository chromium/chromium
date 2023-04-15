// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_CONNECTIVITY_PUBLIC_CPP_CONNECTIVITY_SERVICES_H_
#define CHROMEOS_ASH_SERVICES_CONNECTIVITY_PUBLIC_CPP_CONNECTIVITY_SERVICES_H_

#include "chromeos/ash/services/connectivity/public/mojom/passpoint.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"

namespace ash::connectivity {

void BindToPasspointService(
    mojo::PendingReceiver<chromeos::connectivity::mojom::PasspointService>
        receiver);

}  // namespace ash::connectivity

#endif  // CHROMEOS_ASH_SERVICES_CONNECTIVITY_PUBLIC_CPP_CONNECTIVITY_SERVICES_H_
