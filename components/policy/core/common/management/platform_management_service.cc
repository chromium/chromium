// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/platform_management_service.h"

#include "base/no_destructor.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_MAC)
#include "components/policy/core/common/management/platform_management_status_provider_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "components/policy/core/common/management/platform_management_status_provider_win.h"
#elif BUILDFLAG(IS_IOS)
#include "components/policy/core/common/management/platform_management_status_provider_ios.h"
#endif

namespace policy {

namespace {
std::vector<std::unique_ptr<ManagementStatusProvider>>
GetPlatformManagementSatusProviders() {
  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  providers.push_back(std::make_unique<DomainEnrollmentStatusProvider>());
  providers.push_back(
      std::make_unique<EnterpriseMDMManagementStatusProvider>());
#endif
#if BUILDFLAG(IS_WIN)
  providers.push_back(std::make_unique<AzureActiveDirectoryStatusProvider>());
#endif
#if BUILDFLAG(IS_IOS)
  providers.push_back(std::make_unique<DeviceManagementStatusProvider>());
#endif
  return providers;
}

}  // namespace

// static
PlatformManagementService* PlatformManagementService::GetInstance() {
  static base::NoDestructor<PlatformManagementService> instance;
  return instance.get();
}

PlatformManagementService::PlatformManagementService()
    : ManagementService(GetPlatformManagementSatusProviders()) {}

PlatformManagementService::~PlatformManagementService() = default;

void PlatformManagementService::AddLocalBrowserManagementStatusProvider(
    std::unique_ptr<ManagementStatusProvider> provider) {
  AddManagementStatusProvider(std::move(provider));
  has_local_browser_managment_status_provider_ = true;
}

#if BUILDFLAG(IS_CHROMEOS)
void PlatformManagementService::AddChromeOsStatusProvider(
    std::unique_ptr<ManagementStatusProvider> provider) {
  AddManagementStatusProvider(std::move(provider));
  has_cros_status_provider_ = true;
}
#endif

#if BUILDFLAG(IS_ANDROID)
void PlatformManagementService::AddAndroidStatusProvider(
    std::unique_ptr<ManagementStatusProvider> provider) {
  AddManagementStatusProvider(std::move(provider));
  has_android_status_provider_ = true;
}
#endif

}  // namespace policy
