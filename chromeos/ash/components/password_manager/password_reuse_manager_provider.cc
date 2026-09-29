// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/ash/components/password_manager/password_reuse_manager_provider.h"

#include "base/check.h"
#include "base/check_deref.h"
#include "base/check_op.h"

namespace ash {
namespace {
PasswordReuseManagerProvider* g_instance = nullptr;
}  // namespace

PasswordReuseManagerProvider::PasswordReuseManagerProvider() {
  CHECK(!g_instance);
  g_instance = this;
}

PasswordReuseManagerProvider::~PasswordReuseManagerProvider() {
  CHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
PasswordReuseManagerProvider& PasswordReuseManagerProvider::Get() {
  return CHECK_DEREF(g_instance);
}

}  // namespace ash
