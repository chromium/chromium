// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_SERVICE_H_

#include "base/containers/flat_map.h"
#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"

#include "components/policy/core/common/management/management_service.h"
#include "components/policy/policy_export.h"

namespace policy {

// This class gives information related to the OS and device's management
// state.
// For more imformation please read
// //components/policy/core/common/management/management_service.md
class POLICY_EXPORT PlatformManagementService : public ManagementService {
 public:
  // Returns the singleton instance of PlatformManagementService.
  static PlatformManagementService* GetInstance();

  void AddLocalBrowserManagementStatusProvider(
      std::unique_ptr<ManagementStatusProvider> provider);
  bool has_local_browser_managment_status_provider() const {
    return has_local_browser_managment_status_provider_;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void AddChromeOsStatusProvider(
      std::unique_ptr<ManagementStatusProvider> provider);
  bool has_cros_status_provider() const { return has_cros_status_provider_; }
#endif

  void RefreshCache(CacheRefreshCallback callback) override;

 private:
  friend class base::NoDestructor<PlatformManagementService>;

  // Returns a map of the status providers to their non-cached management
  // authority. This is used to update their cache. This may have some I/O
  // calls, therefore must never be called on the main thread.
  base::flat_map<ManagementStatusProvider*, EnterpriseManagementAuthority>
  GetCacheUpdate();

  // Updates the cached values of the status providers with the appropriate
  // value and call `callback` with the previous and new management authority
  // trustworthiness.
  void UpdateCache(CacheRefreshCallback callback,
                   base::flat_map<ManagementStatusProvider*,
                                  EnterpriseManagementAuthority> cache_update);

  PlatformManagementService();
  ~PlatformManagementService() override;

  bool has_local_browser_managment_status_provider_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool has_cros_status_provider_;
#endif
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_SERVICE_H_
