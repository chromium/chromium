// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_H_

#include <optional>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

// Coordinates the Email Verification Protocol (EVP) UI flows across Desktop and
// Android, including the first-run permission prompt, the subsequent-run
// loading notification, completion notifications (verified / error), and
// minimum display duration anti-flicker timing.
class EmailVerificationController {
 public:
  // Minimum duration the loading state (popup spinner or loading toast) should
  // remain visible to avoid visual flickering on fast network responses.
  static constexpr base::TimeDelta kMinimumLoadingDuration =
      base::Milliseconds(800);

  // Outcome of an email verification attempt when hiding the active UI.
  enum class CompletionStatus {
    // Verification was aborted (e.g. inactive frame); hide active UI without
    // showing a completion notification.
    kAborted,
    // Verification failed; hide active UI and show the error notification.
    kError,
    // Verification succeeded; hide active UI and show the verified notification
    // for `verified_issuer_url`.
    kVerified,
  };

  explicit EmailVerificationController(content::WebContents* web_contents);
  EmailVerificationController(const EmailVerificationController&) = delete;
  EmailVerificationController& operator=(const EmailVerificationController&) =
      delete;
  virtual ~EmailVerificationController();

  // Shows the permission prompt popup anchored to the element bounds.
  void ShowPopup(const gfx::RectF& element_bounds,
                 const url::Origin& issuer_origin,
                 const std::u16string& email,
                 base::OnceCallback<
                     void(AutofillClient::EmailVerificationPermissionUiStatus)>
                     decision_callback);

  // Dismisses any active permission popup or subsequent-run loading
  // notification, deferring dismissal if necessary to satisfy the minimum
  // loading display duration, and then displays the completion notification
  // corresponding to `status` (if any).
  void HidePopup(CompletionStatus status = CompletionStatus::kAborted,
                 const GURL& verified_issuer_url = GURL());

  // Displays the "Verifying email" loading notification (Desktop toast or
  // Android Message banner) for subsequent-run verifications, dismissing any
  // open first-run permission prompt first. `cancel_callback` is invoked if
  // the user cancels verification from the loading UI.
  void ShowLoadingToast(base::OnceClosure cancel_callback);

 protected:
  // Handles the user's decision from the first-run permission prompt.
  void OnPromptDecision(
      AutofillClient::EmailVerificationPermissionUiStatus status);

  content::WebContents& web_contents() const { return *web_contents_; }
  bool IsWebContentsVisible() const;

  // Platform-specific UI hooks implemented by subclasses.
  virtual void ShowPopupImpl(
      const gfx::RectF& element_bounds,
      const url::Origin& issuer_origin,
      const std::u16string& email,
      base::OnceCallback<
          void(AutofillClient::EmailVerificationPermissionUiStatus)>
          decision_callback,
      base::OnceClosure on_dismissed) = 0;
  virtual void HidePopupImpl() = 0;
  virtual void ShowLoadingToastImpl(base::OnceClosure on_cancelled) = 0;
  // Dismisses the subsequent-run loading notification. Defaults to a no-op
  // because on Desktop, toasts manage their own lifecycle and preemption
  // automatically, whereas Android overrides this to dismiss the active loading
  // Message banner.
  virtual void HideLoadingToastImpl() {}
  virtual void ShowErrorToastImpl() = 0;
  virtual void ShowVerifiedToastImpl(const GURL& url) = 0;

  // Subclasses override this to provide a weak-pointer to this class. This is
  // not done in the parent class as the base::WeakPtrFactory member must be the
  // last member in the object, which is not possible to ensure for
  // superclasses.
  virtual base::WeakPtr<EmailVerificationController> GetWeakPtr() = 0;

 private:
  friend class EmailVerificationControllerTestApi;

  // Immediately stops `hide_popup_timer_`, resets `loading_start_time_`, and
  // dismisses any active permission prompt or subsequent-run loading
  // notification.
  void DismissActiveUi();

  // Invoked when the first-run permission prompt is dismissed externally while
  // in the loading state.
  void OnPromptDismissed();

  // Invoked when the user cancels or dismisses the "Verifying email" loading
  // notification UI.
  void OnLoadingCancelled();

  // Resets loading state (`hide_active_ui_callback_`,
  // `loading_cancel_callback_`, and `loading_start_time_`) and flushes any
  // pending `hide_popup_timer_` callback if the WebContents is visible.
  void ResetLoadingStateAndFlushToast();

  bool IsFirstRunRequestInFlight() const;
  bool IsLoadingToastActive() const;

  // Records `loading_start_time_` so that subsequent transitions out of loading
  // via `HidePopup()` enforce `kMinimumLoadingDuration`.
  void StartLoadingTimer();

  // Computes the remaining duration needed to satisfy the minimum loading
  // display duration (800ms) across any active loading UI (first-run popup or
  // subsequent-run loading toast) before tearing down loading UI or showing
  // completion notifications.
  base::TimeDelta GetRemainingLoadingDuration() const;

  const raw_ref<content::WebContents> web_contents_;

  // Callback that tears down whichever UI surface is currently active
  // (`HidePopupImpl()` for a first-run permission prompt, or
  // `HideLoadingToastImpl()` for a subsequent-run loading notification). Null
  // when no prompt or loading notification is open.
  base::OnceClosure hide_active_ui_callback_;

  // Start timestamp when the loading state was initiated (either when the user
  // confirmed the first-run prompt or when the subsequent-run loading toast was
  // shown), used to enforce kMinimumLoadingDuration.
  std::optional<base::TimeTicks> loading_start_time_;

  // Callback invoked to abort the in-flight verification request if the user
  // cancels or dismisses the subsequent-run "Verifying email" loading
  // notification before the verification response arrives.
  base::OnceClosure loading_cancel_callback_;

  base::OneShotTimer hide_popup_timer_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_H_
