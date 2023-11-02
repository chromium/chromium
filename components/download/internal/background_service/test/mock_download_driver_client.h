// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_MOCK_DOWNLOAD_DRIVER_CLIENT_H_
#define COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_MOCK_DOWNLOAD_DRIVER_CLIENT_H_

#include "components/download/internal/background_service/download_driver.h"

#include "testing/gmock/include/gmock/gmock.h"

namespace download {

class MockDriverClient : public DownloadDriver::Client {
 public:
  MockDriverClient();
  ~MockDriverClient();

  MOCK_METHOD1(OnDriverReady, void(bool));
  MOCK_METHOD1(OnDriverHardRecoverComplete, void(bool));
  MOCK_METHOD1(OnDownloadCreated, void(const DriverEntry&));
  MOCK_METHOD2(OnDownloadFailed, void(const DriverEntry&, FailureType));
  MOCK_METHOD1(OnDownloadSucceeded, void(const DriverEntry&));
  MOCK_METHOD1(OnDownloadUpdated, void(const DriverEntry&));
  MOCK_CONST_METHOD1(IsTrackingDownload, bool(const std::string&));
  MOCK_CONST_METHOD2(OnUploadProgress, void(const std::string&, uint64_t));
};

}  // namespace download

#endif  // COMPONENTS_DOWNLOAD_INTERNAL_BACKGROUND_SERVICE_TEST_MOCK_DOWNLOAD_DRIVER_CLIENT_H_
