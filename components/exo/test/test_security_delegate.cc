// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/test_security_delegate.h"

#include "chromeos/ui/base/window_properties.h"
#include "components/exo/security_delegate.h"
#include "components/exo/shell_surface_util.h"
#include "ui/aura/window.h"

namespace exo::test {

TestSecurityDelegate::TestSecurityDelegate() = default;

TestSecurityDelegate::~TestSecurityDelegate() = default;

bool TestSecurityDelegate::CanSelfActivate(aura::Window* window) const {
  return HasPermissionToActivate(window);
}

bool TestSecurityDelegate::CanLockPointer(aura::Window* window) const {
  return window->GetProperty(chromeos::kUseOverviewToExitPointerLock);
}

exo::SecurityDelegate::SetBoundsPolicy TestSecurityDelegate::CanSetBounds(
    aura::Window* window) const {
  return policy_;
}

void TestSecurityDelegate::SetCanSetBounds(
    exo::SecurityDelegate::SetBoundsPolicy policy) {
  policy_ = policy;
}

}  // namespace exo::test
