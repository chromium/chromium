// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/signin/identity_manager_provider.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"

namespace ash {
namespace {
IdentityManagerProvider* g_instance = nullptr;
}  // namespace

IdentityManagerProvider::IdentityManagerProvider() {
  CHECK(!g_instance);
  g_instance = this;
}

IdentityManagerProvider::~IdentityManagerProvider() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
IdentityManagerProvider& IdentityManagerProvider::Get() {
  return CHECK_DEREF(g_instance);
}

}  // namespace ash
