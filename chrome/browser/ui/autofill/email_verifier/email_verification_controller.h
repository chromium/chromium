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
  void ShowPopup(const gfx::RectF& element_bounds,
                 const url::Origin& issuer_origin,
                 const std::u16string& email,
                 base::OnceCallback<
                     void(AutofillClient::EmailVerificationPermissionUiStatus)>
                     decision_callback);

  // Dismisses the permission popup, deferring closure if necessary to satisfy
  // the minimum loading display duration.
  void HidePopup();

  // Displays the "Verifying email" loading notification (Desktop toast or
  // Android Message banner) for subsequent-run verifications, dismissing any
  // open first-run permission prompt first. `cancel_callback` is invoked if
  // the user cancels verification from the loading UI.
  void ShowLoadingToast(base::OnceClosure cancel_callback);

  // Displays the "Couldn't verify your email" error toast, deferring display
  // until any active loading UI has satisfied the minimum display duration.
  void ShowErrorToast();

  // Displays the "{issuer} verified your email" success toast, deferring
  // display until any active loading UI has satisfied the minimum display
  // duration.
  void ShowVerifiedToast(const GURL& url);

 protected:
  // Lifecycle state of the first-run permission prompt:
  //
  // clang-format off
  //                            ┌───────────────────┐
  //                            │      kClosed      │◄─────────────────────────┐
  //                            └─────────┬─────────┘                          │
  //                                      │  ▲                                 │
  //                          ShowPopup() │  │ OnPromptDecision(!kAllowed)     │
  //                                      │  │   or HidePopup()                │
  //                                      ▼  │                                 │
  //                            ┌───────────────────┐                          │
  //         ┌─────────────────►│     kShowing      │                          │
  //         │                  └─────────┬─────────┘                          │
  //         │                            │                                    │
  //         │ ShowPopup()                │ OnPromptDecision(kAllowed)         │
  //         │ [timer running]            │                                    │
  //         │                            ▼                                    │
  //         │                  ┌───────────────────┐                          │
  //         └──────────────────┤     kLoading      ├──────────────────────────┘
  //                            └───────────────────┘  HidePopup() [>= 800ms]
  //                              │               ▲      or ShowLoadingToast()
  //                              └───────────────┘
  //                             HidePopup() [< 800ms]
  //                               or ShowPopup() [in flight]
  // clang-format on
  enum class PromptState {
    kClosed,
    kShowing,
    kLoading,
  };

  // Handles the user's decision from the first-run permission prompt.
  void OnPromptDecision(
      AutofillClient::EmailVerificationPermissionUiStatus status);

  PromptState prompt_state() const { return prompt_state_; }

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
  virtual void ShowErrorToastImpl() = 0;
  virtual void ShowVerifiedToastImpl(const GURL& url) = 0;

  // Subclasses override this to provide a weak-pointer to this class. This is
  // not done in the parent class as the base::WeakPtrFactory member must be the
  // last member in the object, which is not possible to ensure for
  // superclasses.
  virtual base::WeakPtr<EmailVerificationController> GetWeakPtr() = 0;

 private:
  friend class EmailVerificationControllerTestApi;

  // Invoked when the first-run permission prompt is dismissed externally while
  // in the loading state.
  void OnPromptDismissed();

  // Invoked when the user cancels or dismisses the "Verifying email" loading
  // notification UI.
  void OnLoadingCancelled();

  // Resets loading state (`prompt_state_`, `hide_popup_timer_`, and
  // `loading_start_time_`) and flushes any pending `toast_timer_` if the
  // WebContents is visible.
  void ResetLoadingStateAndFlushToast();

  bool IsFirstRunRequestInFlight() const;
  bool IsLoadingToastActive() const;

  // Records `loading_start_time_` so that subsequent transitions out of loading
  // (`HidePopup()`, `ShowVerifiedToast()`, or `ShowErrorToast()`) enforce
  // `kMinimumLoadingDuration`.
  void StartLoadingTimer();

  // Defers executing `show_toast_callback` if the active loading state has not
  // yet satisfied `kMinimumLoadingDuration` (800ms). Returns true if deferred
  // via `toast_timer_`, or false if no remaining delay was needed (in which
  // case `loading_start_time_` is reset).
  bool DeferToast(base::OnceClosure show_toast_callback);

  void OnToastTimerFired(base::OnceClosure show_toast_callback);

  // Computes the remaining duration needed to satisfy the minimum loading
  // display duration (800ms) across any active loading UI (first-run popup or
  // subsequent-run loading toast) before tearing down loading UI or showing
  // completion toasts.
  base::TimeDelta GetRemainingLoadingDuration() const;

  const raw_ref<content::WebContents> web_contents_;

  // Tracks the lifecycle state of the first-run permission prompt. Needed
  // alongside `loading_start_time_` because `loading_start_time_` is set for
  // both the first-run prompt (`kLoading`) and the subsequent-run loading
  // toast, whereas `HidePopup()` is called unconditionally by
  // `EmailVerifierDelegate` on every verification response and must be a no-op
  // when no prompt is open (`kClosed`).
  PromptState prompt_state_ = PromptState::kClosed;

  // Start timestamp when the loading state was initiated (either when the user
  // confirmed the first-run prompt or when the subsequent-run loading toast was
  // shown), used to enforce kMinimumLoadingDuration.
  std::optional<base::TimeTicks> loading_start_time_;

  // Callback invoked to abort the in-flight verification request if the user
  // cancels or dismisses the subsequent-run "Verifying email" loading
  // notification before the verification response arrives.
  base::OnceClosure loading_cancel_callback_;

  base::OneShotTimer hide_popup_timer_;
  base::OneShotTimer toast_timer_;

  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_H_
