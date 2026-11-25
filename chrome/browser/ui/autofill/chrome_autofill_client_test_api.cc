// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/chrome_autofill_client_test_api.h"

#include <memory>
#include <utility>

#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/android/autofill/autofill_ai_attribution_verification_sheet_bridge.h"
#include "chrome/browser/ui/android/autofill/autofill_snackbar_controller_impl.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace autofill {

ChromeAutofillClientTestApi::ScopedDeviceAuthAvailabilityCheckerFactory::
    ScopedDeviceAuthAvailabilityCheckerFactory(
        DeviceAuthAvailabilityCheckerFactory factory)
    : factory_(std::move(factory)),
      reset_(&ChromeAutofillClient::
                 device_auth_availability_checker_factory_for_testing_,
             factory_ ? &factory_ : nullptr) {}

ChromeAutofillClientTestApi::ScopedDeviceAuthAvailabilityCheckerFactory::
    ~ScopedDeviceAuthAvailabilityCheckerFactory() = default;

#if BUILDFLAG(IS_ANDROID)
void ChromeAutofillClientTestApi::
    set_autofill_ai_attribution_verification_sheet_bridge(
        std::unique_ptr<AutofillAiAttributionVerificationSheetBridge> bridge) {
  client_->autofill_ai_attribution_verification_sheet_bridge_ =
      std::move(bridge);
}

void ChromeAutofillClientTestApi::set_autofill_snackbar_controller(
    std::unique_ptr<AutofillSnackbarControllerImpl> controller) {
  client_->autofill_snackbar_controller_impl_ = std::move(controller);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace autofill
