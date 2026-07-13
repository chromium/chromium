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

EmailVerificationPopupController::EvpPermissionUiStatus MapReasonToStatus(
    SuggestionHidingReason reason) {
  switch (reason) {
    case SuggestionHidingReason::kUserAborted:
    case SuggestionHidingReason::kFocusChanged:
    case SuggestionHidingReason::kEndEditing:
      return EmailVerificationPopupController::EvpPermissionUiStatus::
          kUserAborted;
    case SuggestionHidingReason::kTabGone:
      return EmailVerificationPopupController::EvpPermissionUiStatus::kTabGone;
    case SuggestionHidingReason::kWidgetChanged:
      return EmailVerificationPopupController::EvpPermissionUiStatus::
          kWidgetChanged;
    case SuggestionHidingReason::kOverlappingWithAnotherPrompt:
    case SuggestionHidingReason::kOverlappingWithPictureInPictureWindow:
    case SuggestionHidingReason::kOverlappingWithPasswordGenerationPopup:
    case SuggestionHidingReason::kOverlappingWithTouchToFillSurface:
    case SuggestionHidingReason::kOverlappingWithAutofillContextMenu:
    case SuggestionHidingReason::kContextMenuOpened:
      return EmailVerificationPopupController::EvpPermissionUiStatus::
          kOverlappingPrompt;
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
      return EmailVerificationPopupController::EvpPermissionUiStatus::kOther;
  }
}

}  // namespace

EmailVerificationPopupController::EmailVerificationPopupController(
    content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {}

EmailVerificationPopupController::~EmailVerificationPopupController() {
  HideImpl(AutofillClient::EmailVerificationPermissionUiResult::kIgnored,
           EvpPermissionUiStatus::kOther);
}

void EmailVerificationPopupController::Show(
    const gfx::RectF& element_bounds,
    const net::SchemefulSite& issuer,
    const std::u16string& email,
    base::OnceCallback<
        void(AutofillClient::EmailVerificationPermissionUiResult)> callback) {
  if (!web_contents()) {
    std::move(callback).Run(
        AutofillClient::EmailVerificationPermissionUiResult::kIgnored);
    return;
  }

  if (view_) {
    HideImpl(AutofillClient::EmailVerificationPermissionUiResult::kIgnored,
             EvpPermissionUiStatus::kOther);
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
    std::move(callback_).Run(
        AutofillClient::EmailVerificationPermissionUiResult::kIgnored);
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
  HideImpl(AutofillClient::EmailVerificationPermissionUiResult::kIgnored,
           MapReasonToStatus(reason));
}

void EmailVerificationPopupController::ViewDestroyed() {
  view_ = nullptr;
  // If the view is destroyed directly without `Hide()` being called first (e.g.
  // under rare platform-specific native close flows), log it separately.
  HideImpl(AutofillClient::EmailVerificationPermissionUiResult::kIgnored,
           EvpPermissionUiStatus::kViewDestroyedDirectly);
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
  HideImpl(AutofillClient::EmailVerificationPermissionUiResult::kIgnored,
           EvpPermissionUiStatus::kUserAborted);
}

void EmailVerificationPopupController::HideImpl(
    AutofillClient::EmailVerificationPermissionUiResult result,
    EvpPermissionUiStatus status) {
  if (view_) {
    view_->Hide();
    view_ = nullptr;
  }
  popup_hide_helper_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();

  if (callback_) {
    base::UmaHistogramEnumeration("Blink.Evp.PermissionUi.Status", status);
    std::move(callback_).Run(result);
  }
}

bool EmailVerificationPopupController::OverlapsWithPictureInPictureWindow()
    const {
  return view_ && view_->OverlapsWithPictureInPictureWindow();
}

void EmailVerificationPopupController::OnConfirm() {
  HideImpl(AutofillClient::EmailVerificationPermissionUiResult::kAccepted,
           EvpPermissionUiStatus::kAllowed);
}

void EmailVerificationPopupController::OnCancel() {
  HideImpl(AutofillClient::EmailVerificationPermissionUiResult::kDeclined,
           EvpPermissionUiStatus::kDeclined);
}

}  // namespace autofill
