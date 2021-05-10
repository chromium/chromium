// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/policy/core/common/management/platform_management_service.h"

#include "build/build_config.h"
#if defined(OS_WIN)
#include "components/policy/core/common/management/platform_management_status_provider_win.h"
#endif

namespace policy {

PlatformManagementService::PlatformManagementService()
    : ManagementService(ManagementTarget::PLATFORM) {
  InitManagementStatusProviders();
}

PlatformManagementService::~PlatformManagementService() = default;

// static
PlatformManagementService& PlatformManagementService::GetInstance() {
  static base::NoDestructor<PlatformManagementService> instance;
  return *instance;
}

void PlatformManagementService::InitManagementStatusProviders() {
  std::vector<std::unique_ptr<ManagementStatusProvider>> providers;
#if defined(OS_WIN)
  providers.emplace_back(std::make_unique<DomainEnrollmentStatusProvider>());
  providers.emplace_back(
      std::make_unique<EnterpriseMDMManagementStatusProvider>());
#endif
  SetManagementStatusProvider(std::move(providers));
}

}  // namespace policy
