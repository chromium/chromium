// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_IN_PROCESS_INSTANCES_H_
#define CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_IN_PROCESS_INSTANCES_H_

#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-forward.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

// This file contains functions to bind mojo clients for the auth factor config
// related services to server implementations. The server implementations are
// singletons defined in the .cc file.

namespace ash::auth {

void BindToAuthFactorConfig(
    mojo::PendingReceiver<mojom::AuthFactorConfig> receiver);

void BindToRecoveryFactorEditor(
    mojo::PendingReceiver<mojom::RecoveryFactorEditor> receiver);

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_IN_PROCESS_INSTANCES_H_
