// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_H_

#include <memory>
#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "net/base/schemeful_site.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class ToastController;

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class EmailVerificationPopupController;

// Coordinates the Email Verification Protocol (EVP) UI flows on Desktop,
// including the first-run permission prompt popup, the subsequent-run loading
// toast, completion toasts (verified / error), and minimum display duration
// anti-flicker timing.
class EmailVerificationController {
 public:
  // Minimum duration the loading state (popup spinner or loading toast) should
  // remain visible to avoid visual flickering on fast network responses.
  static constexpr base::TimeDelta kMinimumLoadingDuration =
      base::Milliseconds(800);

  explicit EmailVerificationController(content::WebContents* web_contents);
  EmailVerificationController(const EmailVerificationController&) = delete;
  EmailVerificationController& operator=(const EmailVerificationController&) =
      delete;
  virtual ~EmailVerificationController();

  // Shows the permission prompt popup anchored to the element bounds.
  void ShowPopup(
      const gfx::RectF& element_bounds_in_screen_space,
      const net::SchemefulSite& issuer_site,
      const std::u16string& email,
      base::OnceCallback<
          void(AutofillClient::EmailVerificationPermissionUiStatus)> callback);

  // Dismisses the permission popup, deferring closure if necessary to satisfy
  // the minimum loading display duration.
  void HidePopup();

  // Displays the "Verifying email" loading toast at the top of the browser
  // for subsequent-run verifications.
  void ShowLoadingToast();

  // Displays the "Couldn't verify your email" error toast, deferring display
  // until any active loading UI has satisfied the minimum display duration.
  void ShowErrorToast();

  // Displays the "{issuer} verified your email" success toast, deferring
  // display until any active loading UI has satisfied the minimum display
  // duration.
  void ShowVerifiedToast(const GURL& url);

 private:
  friend class EmailVerificationControllerTestApi;

  BrowserWindowInterface* GetBrowserWindowInterface();
  ToastController* GetToastController();

  // Defers executing `show_toast_callback` if the active loading state has not
  // yet satisfied `kMinimumLoadingDuration` (800ms). Returns true if deferred
  // via `toast_timer_`, or false if no remaining delay was needed (in which
  // case `loading_start_time_` is reset).
  bool DeferToast(base::OnceClosure show_toast_callback);

  // Handles the user's decision on the first-run permission prompt popup.
  // If granted (`kAllowed`), starts tracking the loading duration so that the
  // in-button spinner remains visible for at least `kMinimumLoadingDuration`
  // before the popup is dismissed or any completion toast is shown.
  // Then forwards the decision to `callback` so the delegate can initiate
  // token retrieval.
  void OnPopupPermissionDecision(
      base::OnceCallback<
          void(AutofillClient::EmailVerificationPermissionUiStatus)> callback,
      AutofillClient::EmailVerificationPermissionUiStatus status);

  // Computes the remaining duration needed to satisfy the minimum loading
  // display duration (800ms) across any active loading UI (first-run popup or
  // subsequent-run loading toast) before tearing down loading UI or showing
  // completion toasts.
  base::TimeDelta GetRemainingLoadingDuration() const;

  const raw_ref<content::WebContents> web_contents_;

  // Controller for the first-run permission popup.
  std::unique_ptr<EmailVerificationPopupController> popup_controller_;

  // Start timestamp when the loading state was initiated (either when the user
  // confirmed the first-run prompt or when the subsequent-run loading toast was
  // shown), used to enforce kMinimumLoadingDuration.
  std::optional<base::TimeTicks> loading_start_time_;

  base::OneShotTimer hide_popup_timer_;
  base::OneShotTimer toast_timer_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<EmailVerificationController> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_H_
