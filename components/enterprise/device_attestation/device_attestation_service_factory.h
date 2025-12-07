// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_SERVICE_FACTORY_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_SERVICE_FACTORY_H_

#include <memory>

namespace enterprise {

class DeviceAttestationService;

class DeviceAttestationServiceFactory {
 public:
  virtual ~DeviceAttestationServiceFactory() = default;

  // Returns the singleton factory instance.
  static DeviceAttestationServiceFactory* GetInstance();

  // Returns a new DeviceAttestationService instance.
  virtual std::unique_ptr<DeviceAttestationService>
  CreateDeviceAttestationService();

 protected:
  static void SetInstanceForTesting(DeviceAttestationServiceFactory* factory);
  static void ClearInstanceForTesting();
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_SERVICE_FACTORY_H_
