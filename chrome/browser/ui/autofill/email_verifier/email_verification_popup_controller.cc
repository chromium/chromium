// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verifier/email_verification_popup_controller.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/views/autofill/popup/email_verifier/email_verification_popup_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

namespace autofill {

namespace {

using EmailVerificationPermissionUiStatus =
    AutofillClient::EmailVerificationPermissionUiStatus;

EmailVerificationPermissionUiStatus MapReasonToStatus(
    SuggestionHidingReason reason) {
  switch (reason) {
    case SuggestionHidingReason::kUserAborted:
    case SuggestionHidingReason::kFocusChanged:
    case SuggestionHidingReason::kEndEditing:
      return EmailVerificationPermissionUiStatus::kUserAborted;
    case SuggestionHidingReason::kTabGone:
      return EmailVerificationPermissionUiStatus::kTabGone;
    case SuggestionHidingReason::kWidgetChanged:
      return EmailVerificationPermissionUiStatus::kWidgetChanged;
    case SuggestionHidingReason::kOverlappingWithAnotherPrompt:
    case SuggestionHidingReason::kOverlappingWithPictureInPictureWindow:
    case SuggestionHidingReason::kOverlappingWithPasswordGenerationPopup:
    case SuggestionHidingReason::kOverlappingWithTouchToFillSurface:
    case SuggestionHidingReason::kOverlappingWithAutofillContextMenu:
    case SuggestionHidingReason::kContextMenuOpened:
      return EmailVerificationPermissionUiStatus::kOverlappingPrompt;
    case SuggestionHidingReason::kAcceptSuggestion:
    case SuggestionHidingReason::kAttachInterstitialPage:
    case SuggestionHidingReason::kContentAreaMoved:
    case SuggestionHidingReason::kNoSuggestions:
    case SuggestionHidingReason::kRendererEvent:
    case SuggestionHidingReason::kStaleData:
    case SuggestionHidingReason::kViewDestroyed:
    case SuggestionHidingReason::kInsufficientSpace:
    case SuggestionHidingReason::kElementOutsideOfContentArea:
    case SuggestionHidingReason::kMouseLocked:
    case SuggestionHidingReason::kNoFrameHasFocus:
    case SuggestionHidingReason::kExpandedSuggestionCollapsedSubPopup:
    case SuggestionHidingReason::kFieldValueChanged:
    case SuggestionHidingReason::kFadeTimerExpired:
    case SuggestionHidingReason::kSearchBarFocusLost:
    case SuggestionHidingReason::kHiddenByCaller:
      return EmailVerificationPermissionUiStatus::kOther;
  }
}

}  // namespace

EmailVerificationPopupController::EmailVerificationPopupController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

EmailVerificationPopupController::~EmailVerificationPopupController() {
  HideImpl(EmailVerificationPermissionUiStatus::kOther);
}

void EmailVerificationPopupController::Show(
    const gfx::RectF& element_bounds,
    const net::SchemefulSite& issuer,
    const std::u16string& email,
    base::OnceCallback<
        void(AutofillClient::EmailVerificationPermissionUiStatus)> callback) {
  if (!web_contents()) {
    std::move(callback).Run(EmailVerificationPermissionUiStatus::kOther);
    return;
  }

  if (view_) {
    HideImpl(EmailVerificationPermissionUiStatus::kOther);
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

  view_ =
      view_factory_for_testing_
          ? view_factory_for_testing_.Run(GetWeakPtr(), parent_widget, issuer,
                                          email, std::move(on_view_decision))
          : EmailVerificationPopupView::Show(GetWeakPtr(), parent_widget,
                                             issuer, email,
                                             std::move(on_view_decision));

  if (!view_) {
    HideImpl(EmailVerificationPermissionUiStatus::kOther);
    return;
  }

  content::RenderFrameHost* rfh = web_contents()->GetFocusedFrame();
  popup_hide_helper_.emplace(
      web_contents(),
      rfh ? rfh->GetGlobalId() : content::GlobalRenderFrameHostId(),
      AutofillPopupHideHelper::HidingParams{
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

void EmailVerificationPopupController::Hide(SuggestionHidingReason reason) {
  HideImpl(MapReasonToStatus(reason));
}

void EmailVerificationPopupController::ViewDestroyed() {
  view_ = nullptr;
  // If the view is destroyed directly without `Hide()` being called first (e.g.
  // under rare platform-specific native close flows), log it separately.
  HideImpl(EmailVerificationPermissionUiStatus::kViewDestroyedDirectly);
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

PopupAnchorType EmailVerificationPopupController::anchor_type() const {
  return PopupAnchorType::kField;
}

base::i18n::TextDirection
EmailVerificationPopupController::GetElementTextDirection() const {
  return base::i18n::TextDirection::UNKNOWN_DIRECTION;
}

void EmailVerificationPopupController::DidGetUserInteraction(
    const blink::WebInputEvent& event) {
  HideImpl(EmailVerificationPermissionUiStatus::kUserAborted);
}

void EmailVerificationPopupController::HideImpl(
    AutofillClient::EmailVerificationPermissionUiStatus status) {
  if (view_) {
    view_->Hide();
    view_ = nullptr;
  }
  popup_hide_helper_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (callback_) {
    base::UmaHistogramEnumeration("Blink.Evp.PermissionUi.Status", status);
    std::move(callback_).Run(status);
  }
}

bool EmailVerificationPopupController::OverlapsWithPictureInPictureWindow()
    const {
  return view_ && view_->OverlapsWithPictureInPictureWindow();
}

void EmailVerificationPopupController::OnConfirm() {
  HideImpl(EmailVerificationPermissionUiStatus::kAllowed);
}

void EmailVerificationPopupController::OnCancel() {
  HideImpl(EmailVerificationPermissionUiStatus::kDeclined);
}

}  // namespace autofill
