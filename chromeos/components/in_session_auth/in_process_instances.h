// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_IN_SESSION_AUTH_IN_PROCESS_INSTANCES_H_
#define CHROMEOS_COMPONENTS_IN_SESSION_AUTH_IN_PROCESS_INSTANCES_H_

#include "chromeos/components/in_session_auth/in_session_auth.h"
#include "chromeos/components/in_session_auth/mojom/in_session_auth.mojom.h"

namespace chromeos::auth {

// Binds the pending receiver to the implementation of the InSessionAuth
// service. The InSessionAuth service is a global singleton that persists
// in memory after being created.
void BindToInSessionAuthService(
    mojo::PendingReceiver<mojom::InSessionAuth> receiver);

}  // namespace chromeos::auth

#endif  // CHROMEOS_COMPONENTS_IN_SESSION_AUTH_IN_PROCESS_INSTANCES_H_
