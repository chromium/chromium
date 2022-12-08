// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/services/auth_factor_config/in_process_instances.h"

#include <utility>

#include "base/no_destructor.h"
#include "chromeos/ash/services/auth_factor_config/auth_factor_config.h"
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom-test-utils.h"
#include "chromeos/ash/services/auth_factor_config/recovery_factor_editor.h"

namespace ash::auth {

namespace {

AuthFactorConfig& GetAuthFactorConfigImpl(
    QuickUnlockStorageDelegate& delegate) {
  static base::NoDestructor<AuthFactorConfig> auth_factor_config{&delegate};
  return *auth_factor_config;
}

RecoveryFactorEditor& GetRecoveryFactorEditorImpl(
    QuickUnlockStorageDelegate& delegate) {
  static base::NoDestructor<RecoveryFactorEditor> recovery_factor_editor(
      &GetAuthFactorConfigImpl(delegate), &delegate);
  return *recovery_factor_editor;
}

}  // namespace

void BindToAuthFactorConfig(
    mojo::PendingReceiver<mojom::AuthFactorConfig> receiver,
    QuickUnlockStorageDelegate& delegate) {
  GetAuthFactorConfigImpl(delegate).BindReceiver(std::move(receiver));
}

mojom::AuthFactorConfig& GetAuthFactorConfig(
    QuickUnlockStorageDelegate& delegate) {
  return GetAuthFactorConfigImpl(delegate);
}

void BindToRecoveryFactorEditor(
    mojo::PendingReceiver<mojom::RecoveryFactorEditor> receiver,
    QuickUnlockStorageDelegate& delegate) {
  GetRecoveryFactorEditorImpl(delegate).BindReceiver(std::move(receiver));
}

mojom::RecoveryFactorEditor& GetRecoveryFactorEditor(
    QuickUnlockStorageDelegate& delegate) {
  return GetRecoveryFactorEditorImpl(delegate);
}

}  // namespace ash::auth
