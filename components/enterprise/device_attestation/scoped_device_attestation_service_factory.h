// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_SCOPED_DEVICE_ATTESTATION_SERVICE_FACTORY_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_SCOPED_DEVICE_ATTESTATION_SERVICE_FACTORY_H_

#include <memory>

#include "components/enterprise/device_attestation/device_attestation_service.h"
#include "components/enterprise/device_attestation/device_attestation_service_factory.h"

namespace enterprise::test {

// Class used in tests to mock the process of device attestation. Creating
// an instance of this will prevent tests from accessing actual device
// attestation services.
class ScopedDeviceAttestationServiceFactory
    : public DeviceAttestationServiceFactory {
 public:
  ScopedDeviceAttestationServiceFactory();
  ~ScopedDeviceAttestationServiceFactory() override;

  // Get the expected generated attestation blob that's hardcoded in the scoped
  // factory.
  static std::string GetExpectedAttestationBlob();

  // DeviceAttestationServiceFactory:
  std::unique_ptr<DeviceAttestationService> CreateDeviceAttestationService()
      override;
};

}  // namespace enterprise::test

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_SCOPED_DEVICE_ATTESTATION_SERVICE_FACTORY_H_
