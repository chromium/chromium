// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"

#include <utility>

#include "base/no_destructor.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "chromeos/ash/services/auth_factor_config/password_factor_editor.h"
#include "chromeos/ash/services/auth_factor_config/pin_factor_editor.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-test-utils.h"
#include "chromeos/ash/services/auth_factor_config/recovery_factor_editor.h"

namespace ash::auth {

namespace {

AuthFactorConfig& GetAuthFactorConfigImpl(QuickUnlockStorageDelegate& delegate,
                                          PrefService* local_state) {
  static base::NoDestructor<AuthFactorConfig> auth_factor_config{&delegate,
                                                                 local_state};
  return *auth_factor_config;
}

RecoveryFactorEditor& GetRecoveryFactorEditorImpl(
    QuickUnlockStorageDelegate& delegate,
    PrefService* local_state) {
  static base::NoDestructor<RecoveryFactorEditor> recovery_factor_editor(
      &GetAuthFactorConfigImpl(delegate, local_state), &delegate);
  return *recovery_factor_editor;
}

PinFactorEditor& GetPinFactorEditorImpl(QuickUnlockStorageDelegate& storage,
                                        PrefService* local_state,
                                        PinBackendDelegate& pin_backend) {
  static base::NoDestructor<PinFactorEditor> pin_factor_editor(
      &GetAuthFactorConfigImpl(storage, local_state), &pin_backend, &storage);
  return *pin_factor_editor;
}

PasswordFactorEditor& GetPasswordFactorEditorImpl(
    QuickUnlockStorageDelegate& storage,
    PrefService* local_state) {
  static base::NoDestructor<PasswordFactorEditor> password_factor_editor(
      &GetAuthFactorConfigImpl(storage, local_state), &storage);
  return *password_factor_editor;
}

}  // namespace

void BindToAuthFactorConfig(
    mojo::PendingReceiver<mojom::AuthFactorConfig> receiver,
    QuickUnlockStorageDelegate& delegate,
    PrefService* local_state) {
  GetAuthFactorConfigImpl(delegate, local_state)
      .BindReceiver(std::move(receiver));
}

mojom::AuthFactorConfig& GetAuthFactorConfig(
    QuickUnlockStorageDelegate& delegate,
    PrefService* local_state) {
  return GetAuthFactorConfigImpl(delegate, local_state);
}

void BindToRecoveryFactorEditor(
    mojo::PendingReceiver<mojom::RecoveryFactorEditor> receiver,
    QuickUnlockStorageDelegate& delegate,
    PrefService* local_state) {
  GetRecoveryFactorEditorImpl(delegate, local_state)
      .BindReceiver(std::move(receiver));
}

mojom::RecoveryFactorEditor& GetRecoveryFactorEditor(
    QuickUnlockStorageDelegate& delegate,
    PrefService* local_state) {
  return GetRecoveryFactorEditorImpl(delegate, local_state);
}

void BindToPinFactorEditor(
    mojo::PendingReceiver<mojom::PinFactorEditor> receiver,
    QuickUnlockStorageDelegate& storage,
    PrefService* local_state,
    PinBackendDelegate& pin_backend) {
  GetPinFactorEditorImpl(storage, local_state, pin_backend)
      .BindReceiver(std::move(receiver));
}

void BindToPasswordFactorEditor(
    mojo::PendingReceiver<mojom::PasswordFactorEditor> receiver,
    QuickUnlockStorageDelegate& storage,
    PrefService* local_state) {
  GetPasswordFactorEditorImpl(storage, local_state)
      .BindReceiver(std::move(receiver));
}

mojom::PasswordFactorEditor& GetPasswordFactorEditor(
    QuickUnlockStorageDelegate& delegate,
    PrefService* local_state) {
  return GetPasswordFactorEditorImpl(delegate, local_state);
}

}  // namespace ash::auth
