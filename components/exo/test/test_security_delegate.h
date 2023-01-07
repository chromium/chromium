// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_TEST_SECURITY_DELEGATE_H_
#define COMPONENTS_EXO_TEST_TEST_SECURITY_DELEGATE_H_

#include <string>

#include "components/exo/security_delegate.h"

namespace aura {
class Window;
}

namespace exo::test {

class TestSecurityDelegate : public SecurityDelegate {
 public:
  std::string GetSecurityContext() const override;
  bool CanLockPointer(aura::Window* toplevel) const override;
};

}  // namespace exo::test

#endif  // COMPONENTS_EXO_TEST_TEST_SECURITY_DELEGATE_H_
