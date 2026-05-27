// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_SERVICE_H_
#define COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_SERVICE_H_

#include "base/no_destructor.h"

#include "components/policy/core/common/management/management_service.h"
#include "components/policy/policy_export.h"

namespace policy {

// This class gives information related to the OS and device's management
// state.
// For more information please read
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

#if BUILDFLAG(IS_CHROMEOS)
  void AddChromeOsStatusProvider(
      std::unique_ptr<ManagementStatusProvider> provider);
  bool has_cros_status_provider() const { return has_cros_status_provider_; }
#endif

#if BUILDFLAG(IS_ANDROID)
  void AddAndroidStatusProvider(
      std::unique_ptr<ManagementStatusProvider> provider);
  bool has_android_status_provider() const {
    return has_android_status_provider_;
  }
#endif

 private:
  friend class base::NoDestructor<PlatformManagementService>;

  PlatformManagementService();
  ~PlatformManagementService() override;

  bool has_local_browser_managment_status_provider_ = false;
#if BUILDFLAG(IS_CHROMEOS)
  bool has_cros_status_provider_ = false;
#endif
#if BUILDFLAG(IS_ANDROID)
  bool has_android_status_provider_ = false;
#endif
};

}  // namespace policy

#endif  // COMPONENTS_POLICY_CORE_COMMON_MANAGEMENT_PLATFORM_MANAGEMENT_SERVICE_H_
