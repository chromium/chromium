// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_DESKTOP_H_
#define CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_DESKTOP_H_

#include <string>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_controller.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"
#include "url/origin.h"

class BrowserWindowInterface;
class ToastController;

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

// Desktop implementation of `EmailVerificationController` using the Autofill
// permission popup (`EmailVerificationPopupController`) and browser window
// toasts (`ToastController`).
class EmailVerificationControllerDesktop final
    : public EmailVerificationController {
 public:
  explicit EmailVerificationControllerDesktop(
      content::WebContents* web_contents);
  EmailVerificationControllerDesktop(
      const EmailVerificationControllerDesktop&) = delete;
  EmailVerificationControllerDesktop& operator=(
      const EmailVerificationControllerDesktop&) = delete;
  ~EmailVerificationControllerDesktop() override;

 protected:
  // EmailVerificationController:
  void ShowPopupImpl(const gfx::RectF& element_bounds,
                     const url::Origin& issuer_origin,
                     const std::u16string& email,
                     base::OnceCallback<void(
                         AutofillClient::EmailVerificationPermissionUiStatus)>
                         decision_callback,
                     base::OnceClosure on_dismissed) override;
  void HidePopupImpl() override;
  void ShowLoadingToastImpl(base::OnceClosure on_cancelled) override;
  void ShowErrorToastImpl() override;
  void ShowVerifiedToastImpl(const GURL& url) override;
  base::WeakPtr<EmailVerificationController> GetWeakPtr() override;

 private:
  friend class EmailVerificationControllerTestApi;

  BrowserWindowInterface* GetBrowserWindowInterface() const;
  ToastController* GetToastController() const;

  // Controller for the first-run permission popup on Desktop.
  EmailVerificationPopupController popup_controller_;

  // Weak pointer factory for this instance. Must be the last member in the
  // class.
  base::WeakPtrFactory<EmailVerificationControllerDesktop> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_DESKTOP_H_
