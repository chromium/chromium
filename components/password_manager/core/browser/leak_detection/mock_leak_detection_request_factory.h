// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_LEAK_DETECTION_REQUEST_FACTORY_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_LEAK_DETECTION_REQUEST_FACTORY_H_

#include "components/password_manager/core/browser/leak_detection/leak_detection_request_factory.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_request_utils.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockLeakDetectionRequest : public LeakDetectionRequestInterface {
 public:
  MockLeakDetectionRequest();
  ~MockLeakDetectionRequest() override;

  // LeakDetectionRequestInterface:
  MOCK_METHOD(void,
              LookupSingleLeak,
              (network::mojom::URLLoaderFactory*,
               const std::optional<std::string>&,
               const std::optional<std::string>&,
               LookupSingleLeakPayload,
               LookupSingleLeakCallback),
              (override));
};

class MockLeakDetectionRequestFactory : public LeakDetectionRequestFactory {
 public:
  MockLeakDetectionRequestFactory();
  ~MockLeakDetectionRequestFactory() override;

  // LeakDetectionRequestFactory:
  MOCK_METHOD(std::unique_ptr<LeakDetectionRequestInterface>,
              CreateNetworkRequest,
              (),
              (const override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_LEAK_DETECTION_MOCK_LEAK_DETECTION_REQUEST_FACTORY_H_
