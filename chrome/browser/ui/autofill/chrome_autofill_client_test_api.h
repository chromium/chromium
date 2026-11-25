// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_TEST_API_H_
#define CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_TEST_API_H_

#include <memory>
#include <utility>

#include "base/auto_reset.h"
#include "base/check_deref.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/ui/autofill/autofill_suggestion_controller.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "content/public/browser/web_contents_observer.h"

#if BUILDFLAG(IS_ANDROID)
#include "components/autofill/core/browser/ui/autofill_tapjacking_protector.h"
#else
#include "chrome/browser/ui/autofill/autofill_field_promo_controller.h"
#endif  // BUILDFLAG(IS_ANDROID)

namespace content {
class WebContents;
}

namespace autofill {

#if BUILDFLAG(IS_ANDROID)
class AutofillAiAttributionVerificationSheetBridge;
class AutofillSnackbarControllerImpl;
#endif  // BUILDFLAG(IS_ANDROID)
class EmailVerificationController;

// Exposes some testing operations for `ChromeAutofillClient`.
class ChromeAutofillClientTestApi {
 public:
  // Generally, code should use `ContentAutofillClient::FromWebContents()` if
  // possible. This is because many tests inject clients that do not inherit
  // from `ChromeAutofillClient`.
  static ChromeAutofillClient* FromWebContents(
      content::WebContents* web_contents) {
    return static_cast<ChromeAutofillClient*>(
        ContentAutofillClient::FromWebContents(web_contents));
  }

  using DeviceAuthAvailabilityCheckerFactory =
      ChromeAutofillClient::DeviceAuthAvailabilityCheckerFactoryForTesting;

  // Test override factory invoked on each `GetDeviceAuthAvailabilityChecker()`
  // call instead of querying the OS. Keep this object alive for the duration of
  // the test. Passing a null callback resets the override to fall back to the
  // OS checker.
  class [[maybe_unused, nodiscard]] ScopedDeviceAuthAvailabilityCheckerFactory {
   public:
    [[nodiscard]] explicit ScopedDeviceAuthAvailabilityCheckerFactory(
        DeviceAuthAvailabilityCheckerFactory factory);
    ScopedDeviceAuthAvailabilityCheckerFactory(
        const ScopedDeviceAuthAvailabilityCheckerFactory&) = delete;
    ScopedDeviceAuthAvailabilityCheckerFactory& operator=(
        const ScopedDeviceAuthAvailabilityCheckerFactory&) = delete;
    ~ScopedDeviceAuthAvailabilityCheckerFactory();

   private:
    // `reset_` must be destroyed before `factory_` so the static pointer never
    // dangles.
    const DeviceAuthAvailabilityCheckerFactory factory_;
    base::AutoReset<const DeviceAuthAvailabilityCheckerFactory*> reset_;
  };

  explicit ChromeAutofillClientTestApi(ChromeAutofillClient* client)
      : client_(CHECK_DEREF(client)) {}
  ChromeAutofillClientTestApi(const ChromeAutofillClientTestApi&) = delete;
  ChromeAutofillClientTestApi& operator=(const ChromeAutofillClientTestApi&) =
      delete;
  ~ChromeAutofillClientTestApi() = default;

  content::WebContentsObserver& at_memory_copy_paste_observer() {
    return client_->at_memory_copy_paste_observer_;
  }

  EmailVerificationController* email_verification_controller() {
    return client_->email_verification_controller_.get();
  }

  base::WeakPtr<AutofillSuggestionController> suggestion_controller() {
    return client_->suggestion_controller_;
  }

  void set_suggestion_controller(
      base::WeakPtr<AutofillSuggestionController> controller) {
    client_->suggestion_controller_ = std::move(controller);
  }

  void SetKeepPopupOpen(bool keep_popup_open) {
    client_->keep_popup_open_for_testing_ = keep_popup_open;
    if (client_->suggestion_controller_) {
      client_->suggestion_controller_->SetKeepPopupOpenForTesting(
          keep_popup_open);
    }
  }

#if BUILDFLAG(IS_ANDROID)
  void set_autofill_ai_attribution_verification_sheet_bridge(
      std::unique_ptr<AutofillAiAttributionVerificationSheetBridge> bridge);

  void set_autofill_snackbar_controller(
      std::unique_ptr<AutofillSnackbarControllerImpl> controller);

  void set_autofill_tapjacking_protector(
      std::unique_ptr<AutofillTapjackingProtector> protector) {
    client_->autofill_tapjacking_protector_impl_ = std::move(protector);
  }
#else
  void set_autofill_field_promo_controller(
      std::unique_ptr<AutofillFieldPromoController> controller) {
    client_->autofill_field_promo_controller_ = std::move(controller);
  }
#endif  // BUILDFLAG(IS_ANDROID)

 private:
  const raw_ref<ChromeAutofillClient> client_;
};

inline ChromeAutofillClientTestApi test_api(ChromeAutofillClient& client) {
  return ChromeAutofillClientTestApi(&client);
}

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_CHROME_AUTOFILL_CLIENT_TEST_API_H_
