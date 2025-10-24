// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_LEAK_DETECTION_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_LEAK_DETECTION_DELEGATE_H_

#include "build/build_config.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "components/password_manager/core/browser/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"

#if !BUILDFLAG(IS_ANDROID)
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#endif  // !BUILDFLAG(IS_ANDROID)

namespace password_manager {

class MockLeakDetectionDelegateInterface
    : public LeakDetectionDelegateInterface {
 public:
  MockLeakDetectionDelegateInterface();
  ~MockLeakDetectionDelegateInterface() override;

  // LeakDetectionDelegateInterface:
  MOCK_METHOD(void, OnLeakDetectionDone, (bool, PasswordForm), (override));
  MOCK_METHOD(void, OnError, (LeakDetectionError), (override));
};

#if !BUILDFLAG(IS_ANDROID)
class MockBulkLeakCheckDelegateInterface
    : public BulkLeakCheckDelegateInterface {
 public:
  MockBulkLeakCheckDelegateInterface();
  ~MockBulkLeakCheckDelegateInterface() override;

  // BulkLeakCheckDelegateInterface:
  MOCK_METHOD2(OnFinishedCredential,
               void(LeakCheckCredential credential, IsLeaked is_leaked));
  MOCK_METHOD1(OnError, void(LeakDetectionError));
};
#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_LEAK_DETECTION_DELEGATE_H_
