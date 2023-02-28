// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EXO_TEST_MOCK_SECURITY_DELEGATE_H_
#define COMPONENTS_EXO_TEST_MOCK_SECURITY_DELEGATE_H_

#include "components/exo/security_delegate.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace exo::test {

class MockSecurityDelegate : public SecurityDelegate {
 public:
  MockSecurityDelegate();
  ~MockSecurityDelegate() override;

  MOCK_METHOD(std::string, GetSecurityContext, (), (const, override));
  MOCK_METHOD(bool, CanSelfActivate, (aura::Window*), (const, override));
  MOCK_METHOD(bool, CanLockPointer, (aura::Window*), (const, override));
  MOCK_METHOD(bool,
              CanSetBoundsWithServerSideDecoration,
              (aura::Window*),
              (const, override));
};

}  // namespace exo::test

#endif  // COMPONENTS_EXO_TEST_MOCK_SECURITY_DELEGATE_H_
