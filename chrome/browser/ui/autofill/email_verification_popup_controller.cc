// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verification_popup_controller.h"

#include "base/functional/bind.h"
#include "chrome/browser/ui/views/autofill/email_verification_popup_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

namespace autofill {

EmailVerificationPopupController::EmailVerificationPopupController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

EmailVerificationPopupController::~EmailVerificationPopupController() {
  HideImpl(/*confirmed=*/false);
}

void EmailVerificationPopupController::Show(
    const gfx::RectF& element_bounds,
    const net::SchemefulSite& issuer_site,
    const std::u16string& email,
    base::OnceCallback<void(bool)> callback) {
  if (!web_contents()) {
    std::move(callback).Run(false);
    return;
  }

  if (view_) {
    HideImpl(/*confirmed=*/false);
  }

  element_bounds_ = element_bounds;
  callback_ = std::move(callback);

  auto on_view_decision = base::BindOnce(
      [](base::WeakPtr<EmailVerificationPopupController> self, bool confirmed) {
        if (!self) {
          return;
        }
        if (confirmed) {
          self->OnConfirm();
        } else {
          self->OnCancel();
        }
      },
      GetWeakPtr());

  views::Widget* parent_widget =
      views::Widget::GetTopLevelWidgetForNativeView(container_view());

  view_ = view_factory_for_testing_
              ? view_factory_for_testing_.Run(GetWeakPtr(), parent_widget,
                                              issuer_site, email,
                                              std::move(on_view_decision))
              : EmailVerificationPopupView::Show(GetWeakPtr(), parent_widget,
                                                 issuer_site, email,
                                                 std::move(on_view_decision));

  if (!view_) {
    std::move(callback_).Run(false);
    return;
  }

  content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
  popup_hide_helper_.emplace(
      web_contents(),
      rfh ? rfh->GetGlobalId() : content::GlobalRenderFrameHostId(),
      autofill::AutofillPopupHideHelper::HidingParams{
          .hide_on_web_contents_lost_focus = false,
      },
      /*hiding_callback=*/
      base::BindRepeating(&EmailVerificationPopupController::Hide,
                          base::Unretained(this)),
      /*pip_detection_callback=*/
      base::BindRepeating(
          &EmailVerificationPopupController::OverlapsWithPictureInPictureWindow,
          base::Unretained(this)));
}

void EmailVerificationPopupController::Hide(
    autofill::SuggestionHidingReason reason) {
  HideImpl(/*confirmed=*/false);
}

void EmailVerificationPopupController::ViewDestroyed() {
  view_ = nullptr;
  HideImpl(/*confirmed=*/false);
}

gfx::NativeView EmailVerificationPopupController::container_view() const {
  return web_contents() ? web_contents()->GetContentNativeView()
                        : gfx::NativeView();
}

content::WebContents* EmailVerificationPopupController::GetWebContents() const {
  return web_contents();
}

const gfx::RectF& EmailVerificationPopupController::element_bounds() const {
  return element_bounds_;
}

autofill::PopupAnchorType EmailVerificationPopupController::anchor_type()
    const {
  return autofill::PopupAnchorType::kField;
}

base::i18n::TextDirection
EmailVerificationPopupController::GetElementTextDirection() const {
  return base::i18n::TextDirection::UNKNOWN_DIRECTION;
}

void EmailVerificationPopupController::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  HideImpl(/*confirmed=*/false);
}

void EmailVerificationPopupController::HideImpl(bool confirmed) {
  if (view_) {
    view_->Hide();
    view_ = nullptr;
  }
  popup_hide_helper_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (callback_) {
    std::move(callback_).Run(confirmed);
  }
}

bool EmailVerificationPopupController::OverlapsWithPictureInPictureWindow()
    const {
  return view_ && view_->OverlapsWithPictureInPictureWindow();
}

void EmailVerificationPopupController::OnConfirm() {
  HideImpl(/*confirmed=*/true);
}

void EmailVerificationPopupController::OnCancel() {
  HideImpl(/*confirmed=*/false);
}

}  // namespace autofill
