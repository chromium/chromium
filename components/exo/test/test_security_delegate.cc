// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/exo/test/test_security_delegate.h"

#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/window.h"

namespace exo::test {

std::string TestSecurityDelegate::GetSecurityContext() const {
  return "test";
}

bool TestSecurityDelegate::CanLockPointer(aura::Window* toplevel) const {
  return toplevel->GetProperty(chromeos::kUseOverviewToExitPointerLock);
}

}  // namespace exo::test
