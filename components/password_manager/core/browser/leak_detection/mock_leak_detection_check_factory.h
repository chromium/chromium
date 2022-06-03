// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_LEAK_DETECTION_CHECK_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_LEAK_DETECTION_CHECK_FACTORY_H_

#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_factory.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockLeakDetectionCheckFactory : public LeakDetectionCheckFactory {
 public:
  MockLeakDetectionCheckFactory();
  ~MockLeakDetectionCheckFactory() override;

  // LeakDetectionCheckFactory:
  MOCK_CONST_METHOD3(TryCreateLeakCheck,
                     std::unique_ptr<LeakDetectionCheck>(
                         LeakDetectionDelegateInterface*,
                         signin::IdentityManager*,
                         scoped_refptr<network::SharedURLLoaderFactory>));

  MOCK_CONST_METHOD3(TryCreateBulkLeakCheck,
                     std::unique_ptr<BulkLeakCheck>(
                         BulkLeakCheckDelegateInterface*,
                         signin::IdentityManager*,
                         scoped_refptr<network::SharedURLLoaderFactory>));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_LEAK_DETECTION_CHECK_FACTORY_H_
