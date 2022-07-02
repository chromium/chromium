// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"
#include <utility>
#include "base/no_destructor.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "chromeos/ash/services/auth_factor_config/recovery_factor_editor.h"

namespace ash::auth {

namespace {

// TODO(crbug.com/1327627): We probably want to initialize these later: Not on
// chrome startup but when a user logs in. Optionally we could delay
// initialization to first use (static member of a getter) or even create new
// instances of the services for each webui that consumes them.
base::NoDestructor<AuthFactorConfig> auth_factor_config;
base::NoDestructor<RecoveryFactorEditor> recovery_factor_editor(
    auth_factor_config.get());

}  // namespace

void BindToAuthFactorConfig(
    mojo::PendingReceiver<mojom::AuthFactorConfig> receiver) {
  auth_factor_config->BindReceiver(std::move(receiver));
}

void BindToRecoveryFactorEditor(
    mojo::PendingReceiver<mojom::RecoveryFactorEditor> receiver) {
  recovery_factor_editor->BindReceiver(std::move(receiver));
}

}  // namespace ash::auth
