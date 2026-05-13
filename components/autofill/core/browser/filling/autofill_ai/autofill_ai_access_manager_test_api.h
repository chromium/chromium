// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_AUTOFILL_AI_AUTOFILL_AI_ACCESS_MANAGER_TEST_API_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_AUTOFILL_AI_AUTOFILL_AI_ACCESS_MANAGER_TEST_API_H_

#include <memory>

#include "base/memory/raw_ref.h"
#include "components/autofill/core/browser/filling/autofill_ai/autofill_ai_access_manager.h"
#include "components/device_reauth/device_authenticator.h"

namespace autofill {

class AutofillAiAccessManagerTestApi {
 public:
  explicit AutofillAiAccessManagerTestApi(
      AutofillAiAccessManager* access_manager)
      : access_manager_(*access_manager) {}

  void SetDeviceAuthenticator(
      std::unique_ptr<device_reauth::DeviceAuthenticator> authenticator) {
    access_manager_->authenticator_ = std::move(authenticator);
  }

 private:
  raw_ref<AutofillAiAccessManager> access_manager_;
};

inline AutofillAiAccessManagerTestApi test_api(
    AutofillAiAccessManager& access_manager) {
  return AutofillAiAccessManagerTestApi(&access_manager);
}

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_FILLING_AUTOFILL_AI_AUTOFILL_AI_ACCESS_MANAGER_TEST_API_H_
