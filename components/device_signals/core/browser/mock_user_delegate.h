// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_USER_DELEGATE_H_
#define COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_USER_DELEGATE_H_

#include "components/device_signals/core/browser/user_delegate.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace device_signals {

class MockUserDelegate : public UserDelegate {
 public:
  MockUserDelegate();
  ~MockUserDelegate() override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_METHOD(bool, IsSigninContext, (), (const, override));
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  MOCK_METHOD(bool, IsAffiliated, (), (const, override));
  MOCK_METHOD(bool, IsManagedUser, (), (const, override));
  MOCK_METHOD(bool, IsSameUser, (const std::string&), (const, override));
  MOCK_METHOD(std::set<policy::PolicyScope>,
              GetPolicyScopesNeedingSignals,
              (),
              (const, override));
};

}  // namespace device_signals

#endif  // COMPONENTS_DEVICE_SIGNALS_CORE_BROWSER_MOCK_USER_DELEGATE_H_
