// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/device_attestation_service_factory.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/enterprise/device_attestation/device_attestation_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/enterprise/device_attestation/android/device_attestation_service_android.h"
#endif

namespace enterprise {

namespace {

std::optional<DeviceAttestationServiceFactory*>& GetTestInstanceStorage() {
  static std::optional<DeviceAttestationServiceFactory*> storage;
  return storage;
}

}  // namespace

// static
DeviceAttestationServiceFactory*
DeviceAttestationServiceFactory::GetInstance() {
  std::optional<DeviceAttestationServiceFactory*>& test_instance =
      GetTestInstanceStorage();
  if (test_instance.has_value() && test_instance.value()) {
    return test_instance.value();
  }
  static base::NoDestructor<DeviceAttestationServiceFactory> instance;
  return instance.get();
}

std::unique_ptr<DeviceAttestationService>
DeviceAttestationServiceFactory::CreateDeviceAttestationService() {
#if BUILDFLAG(IS_ANDROID)
  return std::make_unique<DeviceAttestationServiceAndroid>();
#else
  return std::make_unique<DeviceAttestationService>();
#endif
}

// static
void DeviceAttestationServiceFactory::SetInstanceForTesting(
    DeviceAttestationServiceFactory* factory) {
  CHECK(factory);
  GetTestInstanceStorage().emplace(factory);
}

// static
void DeviceAttestationServiceFactory::ClearInstanceForTesting() {
  GetTestInstanceStorage().reset();
}

}  // namespace enterprise
