// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/password_cross_domain_confirmation_popup_controller_impl.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/passwords/password_cross_domain_confirmation_popup_view.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/ui/popup_open_enums.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "ui/base/l10n/l10n_util.h"

PasswordCrossDomainConfirmationPopupControllerImpl::
    PasswordCrossDomainConfirmationPopupControllerImpl(
        content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

PasswordCrossDomainConfirmationPopupControllerImpl::
    ~PasswordCrossDomainConfirmationPopupControllerImpl() {
  HideImpl(CrossDomainPasswordFillingConfirmation::kIgnored);
}

void PasswordCrossDomainConfirmationPopupControllerImpl::Show(
    const gfx::RectF& element_bounds,
    base::i18n::TextDirection text_direction,
    const GURL& domain,
    std::u16string password_hostname,
    base::OnceClosure confirmation_callback,
    bool show_warning_text) {
  if (!web_contents()) {
    return;
  }

  if (view_) {
    HideImpl(CrossDomainPasswordFillingConfirmation::kIgnored);
  }

  element_bounds_ = element_bounds;
  text_direction_ = text_direction;
  domain_ = domain;
  password_hostname_ = std::move(password_hostname);
  show_warning_text_ = show_warning_text;
  confirmation_callback_ = std::move(confirmation_callback);

  auto on_view_confirm = base::BindOnce(
      &PasswordCrossDomainConfirmationPopupControllerImpl::OnConfirm,
      weak_ptr_factory_.GetWeakPtr());
  auto on_view_cancel = base::BindOnce(
      &PasswordCrossDomainConfirmationPopupControllerImpl::OnCancel,
      weak_ptr_factory_.GetWeakPtr());
  view_ = view_factory_for_testing_
              ? view_factory_for_testing_.Run(
                    weak_ptr_factory_.GetWeakPtr(), domain, password_hostname_,
                    std::move(on_view_confirm), std::move(on_view_cancel))
              : PasswordCrossDomainConfirmationPopupView::Show(
                    weak_ptr_factory_.GetWeakPtr(), domain, password_hostname_,
                    std::move(on_view_confirm), std::move(on_view_cancel));

  content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
  popup_hide_helper_.emplace(
      web_contents(), rfh->GetGlobalId(),
      autofill::AutofillPopupHideHelper::HidingParams{
          .hide_on_web_contents_lost_focus = false,
      },
      /*hiding_callback=*/
      base::BindRepeating(
          &PasswordCrossDomainConfirmationPopupControllerImpl::Hide,
          base::Unretained(this)),
      /*pip_detection_callback=*/
      base::BindRepeating(&PasswordCrossDomainConfirmationPopupControllerImpl::
                              OverlapsWithPictureInPictureWindow,
                          base::Unretained(this)));
}

void PasswordCrossDomainConfirmationPopupControllerImpl::Hide(
    autofill::SuggestionHidingReason) {
  HideImpl(CrossDomainPasswordFillingConfirmation::kIgnored);
}

void PasswordCrossDomainConfirmationPopupControllerImpl::ViewDestroyed() {
  HideImpl(CrossDomainPasswordFillingConfirmation::kIgnored);
}

gfx::NativeView
PasswordCrossDomainConfirmationPopupControllerImpl::container_view() const {
  return web_contents() ? web_contents()->GetContentNativeView()
                        : gfx::NativeView();
}

content::WebContents*
PasswordCrossDomainConfirmationPopupControllerImpl::GetWebContents() const {
  return web_contents();
}

const gfx::RectF&
PasswordCrossDomainConfirmationPopupControllerImpl::element_bounds() const {
  return element_bounds_;
}

autofill::PopupAnchorType
PasswordCrossDomainConfirmationPopupControllerImpl::anchor_type() const {
  return autofill::PopupAnchorType::kField;
}

base::i18n::TextDirection
PasswordCrossDomainConfirmationPopupControllerImpl::GetElementTextDirection()
    const {
  return text_direction_;
}

std::u16string PasswordCrossDomainConfirmationPopupControllerImpl::GetBodyText()
    const {
  return show_warning_text_
             ? l10n_util::GetStringFUTF16(
                   IDS_PASSWORD_CROSS_DOMAIN_FILLING_WARNING_DESCRIPTION,
                   password_hostname_, base::UTF8ToUTF16(domain_.host()))
             : l10n_util::GetStringFUTF16(
                   IDS_PASSWORD_CROSS_DOMAIN_FILLING_CONFIRMATION_DESCRIPTION,
                   password_hostname_, base::UTF8ToUTF16(domain_.host()));
}

std::u16string
PasswordCrossDomainConfirmationPopupControllerImpl::GetTitleText() const {
  return l10n_util::GetStringFUTF16(
      IDS_PASSWORD_CROSS_DOMAIN_FILLING_CONFIRMATION_TITLE, password_hostname_);
}

void PasswordCrossDomainConfirmationPopupControllerImpl::DidGetUserInteraction(
    const blink::WebInputEvent&) {
  HideImpl(CrossDomainPasswordFillingConfirmation::kIgnored);
}

void PasswordCrossDomainConfirmationPopupControllerImpl::HideImpl(
    CrossDomainPasswordFillingConfirmation result) {
  if (view_) {
    view_->Hide();
    view_ = nullptr;
  }
  popup_hide_helper_.reset();

  base::UmaHistogramEnumeration(
      "PasswordManager.ManualFallback.CrossDomainPasswordFilling."
      "ConfirmationBubbleResult",
      result);
}

bool PasswordCrossDomainConfirmationPopupControllerImpl::
    OverlapsWithPictureInPictureWindow() const {
  return view_ && view_->OverlapsWithPictureInPictureWindow();
}

void PasswordCrossDomainConfirmationPopupControllerImpl::OnConfirm() {
  HideImpl(CrossDomainPasswordFillingConfirmation::kConfirmed);

  std::move(confirmation_callback_).Run();
}

void PasswordCrossDomainConfirmationPopupControllerImpl::OnCancel() {
  HideImpl(CrossDomainPasswordFillingConfirmation::kCanceled);
}
