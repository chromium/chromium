// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_SERVICE_FACTORY_H_
#define COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_SERVICE_FACTORY_H_

#include <memory>

#include "base/functional/callback.h"
#include "build/build_config.h"

namespace enterprise {

class DeviceAttestationService;

#if BUILDFLAG(IS_IOS)
class AttestationServiceIOS;
#endif

class DeviceAttestationServiceFactory {
 public:
  virtual ~DeviceAttestationServiceFactory() = default;

  // Returns the singleton factory instance.
  static DeviceAttestationServiceFactory* GetInstance();

  // Returns a new DeviceAttestationService instance.
  virtual std::unique_ptr<DeviceAttestationService>
  CreateDeviceAttestationService();

#if BUILDFLAG(IS_IOS)
  using AttestationServiceIOSProvider =
      base::RepeatingCallback<std::unique_ptr<AttestationServiceIOS>()>;
  // Sets the provider used to create `AttestationServiceIOS` instances.
  static void SetAttestationServiceIOSProvider(
      AttestationServiceIOSProvider provider);
  static void ClearAttestationServiceIOSProvider();
#endif

 protected:
  static void SetInstanceForTesting(DeviceAttestationServiceFactory* factory);
  static void ClearInstanceForTesting();
};

}  // namespace enterprise

#endif  // COMPONENTS_ENTERPRISE_DEVICE_ATTESTATION_DEVICE_ATTESTATION_SERVICE_FACTORY_H_
