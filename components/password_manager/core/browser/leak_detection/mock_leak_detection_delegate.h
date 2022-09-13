// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_LEAK_DETECTION_DELEGATE_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_LEAK_DETECTION_DELEGATE_H_

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_delegate_interface.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockLeakDetectionDelegateInterface
    : public LeakDetectionDelegateInterface {
 public:
  MockLeakDetectionDelegateInterface();
  ~MockLeakDetectionDelegateInterface() override;

  // LeakDetectionDelegateInterface:
  MOCK_METHOD4(OnLeakDetectionDone,
               void(bool, GURL, std::u16string, std::u16string));
  MOCK_METHOD1(OnError, void(LeakDetectionError));
};

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

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_LEAK_DETECTION_DELEGATE_H_
