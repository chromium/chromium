// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_TEST_SECURITY_DELEGATE_H_
#define COMPONENTS_EXO_TEST_TEST_SECURITY_DELEGATE_H_

#include "components/exo/security_delegate.h"

namespace aura {
class Window;
}

namespace exo::test {

class TestSecurityDelegate : public SecurityDelegate {
 public:
  bool CanLockPointer(aura::Window* toplevel) const override;
  SetBoundsPolicy CanSetBounds(aura::Window* window) const override;

  // Choose the return value of |CanSetBounds()|.
  void SetCanSetBounds(SetBoundsPolicy policy);

 protected:
  SetBoundsPolicy policy_ = SetBoundsPolicy::IGNORE;
};

}  // namespace exo::test

#endif  // COMPONENTS_EXO_TEST_TEST_SECURITY_DELEGATE_H_
