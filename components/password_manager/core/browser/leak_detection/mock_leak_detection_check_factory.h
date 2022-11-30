// Copyright 2020 The Chromium Authors
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
  MOCK_METHOD(std::unique_ptr<LeakDetectionCheck>,
              TryCreateLeakCheck,
              (LeakDetectionDelegateInterface*,
               signin::IdentityManager*,
               scoped_refptr<network::SharedURLLoaderFactory>,
               version_info::Channel),
              (const, override));

  MOCK_METHOD(std::unique_ptr<BulkLeakCheck>,
              TryCreateBulkLeakCheck,
              (BulkLeakCheckDelegateInterface*,
               signin::IdentityManager*,
               scoped_refptr<network::SharedURLLoaderFactory>),
              (const, override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_LEAK_DETECTION_CHECK_FACTORY_H_
