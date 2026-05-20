// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/scoped_device_attestation_service_factory.h"

#include <utility>

#include "base/check.h"
#include "components/enterprise/device_attestation/mock_device_attestation_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace enterprise::test {

const char kFakeAttestationBlob[] = "FakeBlobForTesting";

const int kFakeContentBindingVersion = 3;

ScopedDeviceAttestationServiceFactory::ScopedDeviceAttestationServiceFactory() {
  DeviceAttestationServiceFactory::SetInstanceForTesting(this);
}

ScopedDeviceAttestationServiceFactory::
    ~ScopedDeviceAttestationServiceFactory() {
  DeviceAttestationServiceFactory::ClearInstanceForTesting();
}

std::string
ScopedDeviceAttestationServiceFactory::GetExpectedAttestationBlob() {
  return kFakeAttestationBlob;
}

int ScopedDeviceAttestationServiceFactory::GetExpectedContentBindingVersion() {
  return kFakeContentBindingVersion;
}

std::unique_ptr<DeviceAttestationService>
ScopedDeviceAttestationServiceFactory::CreateDeviceAttestationService() {
  auto mocked_service = std::make_unique<MockDeviceAttestationService>();
  ON_CALL(*mocked_service.get(), GetAttestationResponse)
      .WillByDefault(
          [](std::string_view,
             const enterprise_management::ChromeProfileReportRequest&,
             std::string_view, std::string_view, std::string_view,
             DeviceAttestationService::DeviceAttestationCallback callback) {
            std::move(callback).Run(
                AttestationResult{{kFakeAttestationBlob, std::string()},
                                  kFakeContentBindingVersion});
          });
  return mocked_service;
}

}  // namespace enterprise::test
