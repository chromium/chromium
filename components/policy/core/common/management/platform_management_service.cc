// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/platform_management_service.h"

#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#if BUILDFLAG(IS_MAC)
#include "components/policy/core/common/management/platform_management_status_provider_mac.h"
#elif BUILDFLAG(IS_WIN)
#include "components/policy/core/common/management/platform_management_status_provider_win.h"
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
#include "build/chromeos_buildflags.h"
#include "components/policy/core/common/management/platform_management_status_provider_lacros.h"
#endif

namespace policy {

namespace {
std::vector<std::unique_ptr<ManagementStatusProvider>>
GetPlatformManagementSatusProviders() {
  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  providers.emplace_back(std::make_unique<DomainEnrollmentStatusProvider>());
  providers.emplace_back(
      std::make_unique<EnterpriseMDMManagementStatusProvider>());
#endif
#if BUILDFLAG(IS_WIN)
  providers.emplace_back(
      std::make_unique<AzureActiveDirectoryStatusProvider>());
#endif
#if BUILDFLAG(IS_CHROMEOS_LACROS)
  providers.emplace_back(
      std::make_unique<DeviceEnterpriseManagedStatusProvider>());
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
void PlatformManagementService::AddChromeOsStatusProvider(
    std::unique_ptr<ManagementStatusProvider> provider) {
  AddManagementStatusProvider(std::move(provider));
  has_cros_status_provider_ = true;
}
#endif

void PlatformManagementService::RefreshCache(CacheRefreshCallback callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock()},
      // Unretained here since this class should never be destroyed.
      base::BindOnce(&PlatformManagementService::GetCacheUpdate,
                     base::Unretained(this)),
      base::BindOnce(&PlatformManagementService::UpdateCache,
                     base::Unretained(this), std::move(callback)));
}

base::flat_map<ManagementStatusProvider*, EnterpriseManagementAuthority>
PlatformManagementService::GetCacheUpdate() {
  base::flat_map<ManagementStatusProvider*, EnterpriseManagementAuthority>
      result;
  for (const auto& provider : management_status_providers()) {
    if (provider->RequiresCache())
      result.insert({provider.get(), provider->FetchAuthority()});
  }
  return result;
}

void PlatformManagementService::UpdateCache(
    CacheRefreshCallback callback,
    base::flat_map<ManagementStatusProvider*, EnterpriseManagementAuthority>
        cache_update) {
  ManagementAuthorityTrustworthiness previous =
      GetManagementAuthorityTrustworthiness();
  for (const auto& it : cache_update) {
    it.first->UpdateCache(it.second);
  }
  ManagementAuthorityTrustworthiness next =
      GetManagementAuthorityTrustworthiness();
  if (callback)
    std::move(callback).Run(previous, next);
}

}  // namespace policy
