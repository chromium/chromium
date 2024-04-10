// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_cross_domain_confirmation_popup_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/passwords/password_cross_domain_confirmation_popup_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

PasswordCrossDomainConfirmationPopupController::
    PasswordCrossDomainConfirmationPopupController(
        content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

PasswordCrossDomainConfirmationPopupController::
    ~PasswordCrossDomainConfirmationPopupController() {
  HideImpl();
}

void PasswordCrossDomainConfirmationPopupController::Show(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const GURL& domain,
    const std::u16string& password_origin,
    base::OnceClosure confirmation_callback) {
  if (!web_contents()) {
    return;
  }

  HideImpl();

  element_bounds_ = element_bounds;
  text_direction_ = text_direction;
  confirmation_callback_ = std::move(confirmation_callback);

  view_ = PasswordCrossDomainConfirmationPopupView::Show(
      weak_ptr_factory_.GetWeakPtr(), domain, password_origin,
      base::BindOnce(&PasswordCrossDomainConfirmationPopupController::OnConfirm,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&PasswordCrossDomainConfirmationPopupController::OnCancel,
                     weak_ptr_factory_.GetWeakPtr()));

  content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
  popup_hide_helper_.emplace(
      web_contents(), rfh->GetGlobalId(),
      autofill::AutofillPopupHideHelper::HidingParams{},
      /*hiding_callback=*/
      base::BindRepeating(&PasswordCrossDomainConfirmationPopupController::Hide,
                          base::Unretained(this)),
      /*pip_detection_callback=*/
      base::BindRepeating(&PasswordCrossDomainConfirmationPopupController::
                              OverlapsWithPictureInPictureWindow,
                          base::Unretained(this)));
}

void PasswordCrossDomainConfirmationPopupController::Hide(
    autofill::PopupHidingReason) {
  HideImpl();
}

void PasswordCrossDomainConfirmationPopupController::ViewDestroyed() {
  HideImpl();
}

gfx::NativeView PasswordCrossDomainConfirmationPopupController::container_view()
    const {
  return web_contents() ? web_contents()->GetContentNativeView()
                        : gfx::NativeView();
}

content::WebContents*
PasswordCrossDomainConfirmationPopupController::GetWebContents() const {
  return web_contents();
}

const gfx::RectF&
PasswordCrossDomainConfirmationPopupController::element_bounds() const {
  return element_bounds_;
}

base::i18n::TextDirection
PasswordCrossDomainConfirmationPopupController::GetElementTextDirection()
    const {
  return text_direction_;
}

void PasswordCrossDomainConfirmationPopupController::HideImpl() {
  if (view_) {
    view_->Hide();
  }
  popup_hide_helper_.reset();
}

bool PasswordCrossDomainConfirmationPopupController::
    OverlapsWithPictureInPictureWindow() const {
  return view_ && view_->OverlapsWithPictureInPictureWindow();
}

void PasswordCrossDomainConfirmationPopupController::OnConfirm() {
  HideImpl();

  std::move(confirmation_callback_).Run();
}

void PasswordCrossDomainConfirmationPopupController::OnCancel() {
  HideImpl();
}
