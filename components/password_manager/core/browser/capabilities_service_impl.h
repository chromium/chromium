// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CAPABILITIES_SERVICE_IMPL_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CAPABILITIES_SERVICE_IMPL_H_

#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "components/password_manager/core/browser/capabilities_service.h"

class CapabilitiesServiceImpl : public password_manager::CapabilitiesService {
 public:
  explicit CapabilitiesServiceImpl(
      std::unique_ptr<autofill_assistant::AutofillAssistant>
          autofill_assistant);
  CapabilitiesServiceImpl(const CapabilitiesServiceImpl&) = delete;
  CapabilitiesServiceImpl& operator=(const CapabilitiesServiceImpl&) = delete;

  ~CapabilitiesServiceImpl() override;

  // CapabilitiesService:
  void QueryPasswordChangeScriptAvailability(
      const std::vector<url::Origin>& origins,
      ResponseCallback callback) override;

 private:
  using CapabilitiesInfo =
      autofill_assistant::AutofillAssistant::CapabilitiesInfo;

  void OnGetCapabilitiesResult(
      const std::vector<url::Origin>& origins,
      ResponseCallback callback,
      int http_status,
      const std::vector<
          autofill_assistant::AutofillAssistant::CapabilitiesInfo>& infos);

  // Used for capabilities requests.
  std::unique_ptr<autofill_assistant::AutofillAssistant> autofill_assistant_;
};

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_CAPABILITIES_SERVICE_IMPL_H_
