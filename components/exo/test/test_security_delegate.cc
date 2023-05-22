// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/test_security_delegate.h"

#include "chromeos/ui/base/window_properties.h"
#include "components/exo/security_delegate.h"
#include "ui/aura/window.h"

namespace exo::test {

bool TestSecurityDelegate::CanLockPointer(aura::Window* toplevel) const {
  return toplevel->GetProperty(chromeos::kUseOverviewToExitPointerLock);
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
