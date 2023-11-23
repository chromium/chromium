// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_IN_PROCESS_INSTANCES_H_
#define CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_IN_PROCESS_INSTANCES_H_

#include "chromeos/ash/services/auth_factor_config/chrome_browser_delegates.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-forward.h"
#include "chromeos/ash/services/auth_factor_config/recovery_factor_editor.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"

// This file contains functions to bind mojo clients for the auth factor config
// related services to server implementations. The server implementations are
// singletons defined in the .cc file.
//
// Until the QuickUnlockPrivate API is removed, the mojo services provided here
// need to interact nicely with QuickUnlockPrivate internals. Because of DEPS
// issues, a delegate to these internals must be provided whenever one of the
// global mojo service instances is accessed.

namespace ash::auth {

void BindToAuthFactorConfig(
    mojo::PendingReceiver<mojom::AuthFactorConfig> receiver,
    QuickUnlockStorageDelegate&,
    PrefService* local_state);
mojom::AuthFactorConfig& GetAuthFactorConfig(QuickUnlockStorageDelegate&,
                                             PrefService* local_state);
AuthFactorConfig& GetAuthFactorConfigForTesting(QuickUnlockStorageDelegate&,
                                                PrefService* local_state);

void BindToRecoveryFactorEditor(
    mojo::PendingReceiver<mojom::RecoveryFactorEditor> receiver,
    QuickUnlockStorageDelegate&,
    PrefService* local_state);
mojom::RecoveryFactorEditor& GetRecoveryFactorEditor(
    QuickUnlockStorageDelegate&,
    PrefService* local_state);

void BindToPinFactorEditor(
    mojo::PendingReceiver<mojom::PinFactorEditor> receiver,
    QuickUnlockStorageDelegate&,
    PrefService* local_state,
    PinBackendDelegate&);

void BindToPasswordFactorEditor(
    mojo::PendingReceiver<mojom::PasswordFactorEditor> receiver,
    QuickUnlockStorageDelegate&,
    PrefService* local_state);

mojom::PasswordFactorEditor& GetPasswordFactorEditor(
    QuickUnlockStorageDelegate& delegate,
    PrefService* local_state);

}  // namespace ash::auth

#endif  // CHROMEOS_ASH_SERVICES_AUTH_FACTOR_CONFIG_IN_PROCESS_INSTANCES_H_
