// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verification_controller_desktop.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_controller.h"
#include "chrome/browser/ui/autofill/email_verifier/email_verified_toast_menu_model.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/toasts/api/toast_id.h"
#include "chrome/browser/ui/toasts/toast_controller.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "ui/gfx/geometry/rect.h"

namespace autofill {

EmailVerificationControllerDesktop::EmailVerificationControllerDesktop(
    content::WebContents* web_contents)
    : EmailVerificationController(web_contents),
      popup_controller_(web_contents) {}

EmailVerificationControllerDesktop::~EmailVerificationControllerDesktop() =
    default;

void EmailVerificationControllerDesktop::ShowPopupImpl(
    const gfx::RectF& element_bounds,
    const url::Origin& issuer_origin,
    const std::u16string& email,
    base::OnceCallback<void(
        AutofillClient::EmailVerificationPermissionUiStatus)> decision_callback,
    base::OnceClosure on_dismissed) {
  const gfx::Rect client_area = web_contents().GetContainerBounds();
  const gfx::RectF bounds_in_screen_space =
      element_bounds + client_area.OffsetFromOrigin();
  popup_controller_.Show(bounds_in_screen_space, issuer_origin, email,
                         std::move(decision_callback), std::move(on_dismissed));
}

void EmailVerificationControllerDesktop::HidePopupImpl() {
  const bool was_loading = popup_controller_.is_loading();
  popup_controller_.Dismiss();
  if (was_loading && IsWebContentsVisible()) {
    // The popup is activatable and took focus when the user clicked "Verify".
    // Once it closes, give focus back to the page so it returns to the form
    // field, rather than letting the platform hand activation to the
    // completion toast (whose first focusable view is its menu button).
    web_contents().Focus();
  }
}

void EmailVerificationControllerDesktop::ShowLoadingToastImpl(
    base::OnceClosure /*on_cancelled*/) {
  if (ToastController* toast_controller = GetToastController()) {
    toast_controller->MaybeShowToast(
        ToastParams(ToastId::kEmailVerificationLoading));
  }
}

void EmailVerificationControllerDesktop::ShowErrorToastImpl() {
  if (ToastController* toast_controller = GetToastController()) {
    toast_controller->MaybeShowToast(
        ToastParams(ToastId::kEmailVerificationError));
  }
}

void EmailVerificationControllerDesktop::ShowVerifiedToastImpl(
    const GURL& url) {
  ToastController* toast_controller = GetToastController();
  if (!toast_controller) {
    return;
  }
  ToastParams params(ToastId::kEmailVerified);
  params.body_string_replacement_params.push_back(
      base::UTF8ToUTF16(url.host()));
  params.menu_model = std::make_unique<EmailVerifiedToastMenuModel>(
      GetBrowserWindowInterface());
  toast_controller->MaybeShowToast(std::move(params));
}

base::WeakPtr<EmailVerificationController>
EmailVerificationControllerDesktop::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

BrowserWindowInterface*
EmailVerificationControllerDesktop::GetBrowserWindowInterface() const {
  tabs::TabInterface* tab_interface =
      tabs::TabInterface::MaybeGetFromContents(&web_contents());
  return tab_interface ? tab_interface->GetBrowserWindowInterface() : nullptr;
}

ToastController* EmailVerificationControllerDesktop::GetToastController()
    const {
  if (!IsWebContentsVisible()) {
    return nullptr;
  }
  BrowserWindowInterface* window_interface = GetBrowserWindowInterface();
  return window_interface ? ToastController::From(window_interface) : nullptr;
}

}  // namespace autofill
