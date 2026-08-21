// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/enterprise/device_attestation/device_attestation_service_factory.h"

#include <memory>

#include "base/no_destructor.h"
#include "build/build_config.h"
#include "components/enterprise/device_attestation/device_attestation_service.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/enterprise/device_attestation/android/android_attestation_client.h"
#include "components/enterprise/device_attestation/android/device_attestation_service_android.h"
#elif BUILDFLAG(IS_IOS)
#include "components/enterprise/device_attestation/ios/attestation_service_ios.h"
#include "components/enterprise/device_attestation/ios/device_attestation_service_ios.h"
#endif

namespace enterprise {

namespace {

std::optional<DeviceAttestationServiceFactory*>& GetTestInstanceStorage() {
  static std::optional<DeviceAttestationServiceFactory*> storage;
  return storage;
}

#if BUILDFLAG(IS_IOS)
std::optional<DeviceAttestationServiceFactory::AttestationServiceIOSProvider>&
GetAttestationServiceIOSProviderStorage() {
  static base::NoDestructor<
      std::optional<DeviceAttestationServiceFactory::AttestationServiceIOSProvider>>
      storage;
  return *storage;
}
#endif

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
  return std::make_unique<DeviceAttestationServiceAndroid>(
      std::make_unique<AndroidAttestationClient>());
#elif BUILDFLAG(IS_IOS)
  auto& provider = GetAttestationServiceIOSProviderStorage();
  std::unique_ptr<AttestationServiceIOS> attestation_service_ios =
      provider.has_value() && provider.value() ? provider.value().Run()
                                               : nullptr;
  return std::make_unique<DeviceAttestationServiceIOS>(
      std::move(attestation_service_ios));
#else
  return std::make_unique<DeviceAttestationService>();
#endif
}

#if BUILDFLAG(IS_IOS)
// static
void DeviceAttestationServiceFactory::SetAttestationServiceIOSProvider(
    AttestationServiceIOSProvider provider) {
  GetAttestationServiceIOSProviderStorage().emplace(std::move(provider));
}

// static
void DeviceAttestationServiceFactory::ClearAttestationServiceIOSProvider() {
  GetAttestationServiceIOSProviderStorage().reset();
}
#endif

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
