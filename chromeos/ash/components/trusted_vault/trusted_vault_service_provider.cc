// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/trusted_vault/trusted_vault_service_provider.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"

namespace ash {
namespace {
TrustedVaultServiceProvider* g_instance = nullptr;
}  // namespace

TrustedVaultServiceProvider::TrustedVaultServiceProvider() {
  CHECK(!g_instance);
  g_instance = this;
}

TrustedVaultServiceProvider::~TrustedVaultServiceProvider() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
TrustedVaultServiceProvider& TrustedVaultServiceProvider::Get() {
  return CHECK_DEREF(g_instance);
}

}  // namespace ash
