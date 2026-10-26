// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_ANDROID_H_
#define CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_ANDROID_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller.h"
#include "components/autofill/core/browser/foundations/autofill_client.h"
#include "ui/gfx/geometry/rect_f.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {
class WebContents;
}  // namespace content

namespace autofill {

class AutofillMessageController;
class EmailVerificationBottomSheetBridge;

// Android implementation of `EmailVerificationController` using the first-run
// bottom sheet (`EmailVerificationBottomSheetBridge`) and Message banners
// (`AutofillMessageController`).
class EmailVerificationControllerAndroid final
    : public EmailVerificationController {
 public:
  explicit EmailVerificationControllerAndroid(
      content::WebContents* web_contents);
  EmailVerificationControllerAndroid(
      const EmailVerificationControllerAndroid&) = delete;
  EmailVerificationControllerAndroid& operator=(
      const EmailVerificationControllerAndroid&) = delete;
  ~EmailVerificationControllerAndroid() override;

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
  void HideLoadingToastImpl() override;
  void ShowErrorToastImpl() override;
  void ShowVerifiedToastImpl(const GURL& url) override;
  base::WeakPtr<EmailVerificationController> GetWeakPtr() override;

 private:
  friend class EmailVerificationControllerTestApi;

  AutofillMessageController& GetMessageController();

  raw_ptr<AutofillMessageController> message_controller_for_testing_ = nullptr;
  base::RepeatingClosure open_settings_callback_;
  // Bridge for the first-run permission bottom sheet on Android.
  std::unique_ptr<EmailVerificationBottomSheetBridge> bottom_sheet_bridge_;

  // Weak pointer factory for this instance. Must be the last member in the
  // class.
  base::WeakPtrFactory<EmailVerificationControllerAndroid> weak_ptr_factory_{
      this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_EMAIL_VERIFIER_EMAIL_VERIFICATION_CONTROLLER_ANDROID_H_
