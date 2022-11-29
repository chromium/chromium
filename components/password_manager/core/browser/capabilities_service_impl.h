// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CAPABILITIES_SERVICE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CAPABILITIES_SERVICE_IMPL_H_

#include "components/password_manager/core/browser/capabilities_service.h"

#include "base/callback.h"
#include "url/origin.h"

class CapabilitiesServiceImpl : public password_manager::CapabilitiesService {
 public:
  CapabilitiesServiceImpl();
  CapabilitiesServiceImpl(const CapabilitiesServiceImpl&) = delete;
  CapabilitiesServiceImpl& operator=(const CapabilitiesServiceImpl&) = delete;

  ~CapabilitiesServiceImpl() override;

  // CapabilitiesService:
  void QueryPasswordChangeScriptAvailability(
      const std::vector<url::Origin>& origins,
      ResponseCallback callback) override;
};

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CAPABILITIES_SERVICE_IMPL_H_
