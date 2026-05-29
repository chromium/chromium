// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/email_verification_popup_controller.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ui/views/autofill/email_verification_popup_view.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/widget/widget.h"

namespace {

autofill::EmailVerificationPopupController::EvpPermissionUiStatus
MapReasonToStatus(autofill::SuggestionHidingReason reason) {
  switch (reason) {
    case autofill::SuggestionHidingReason::kUserAborted:
    case autofill::SuggestionHidingReason::kFocusChanged:
    case autofill::SuggestionHidingReason::kEndEditing:
      return autofill::EmailVerificationPopupController::EvpPermissionUiStatus::
          kUserAborted;
    case autofill::SuggestionHidingReason::kNavigation:
      return autofill::EmailVerificationPopupController::EvpPermissionUiStatus::
          kNavigation;
    case autofill::SuggestionHidingReason::kTabGone:
      return autofill::EmailVerificationPopupController::EvpPermissionUiStatus::
          kTabGone;
    case autofill::SuggestionHidingReason::kWidgetChanged:
      return autofill::EmailVerificationPopupController::EvpPermissionUiStatus::
          kWidgetChanged;
    case autofill::SuggestionHidingReason::kOverlappingWithAnotherPrompt:
    case autofill::SuggestionHidingReason::
        kOverlappingWithPictureInPictureWindow:
    case autofill::SuggestionHidingReason::
        kOverlappingWithPasswordGenerationPopup:
    case autofill::SuggestionHidingReason::kOverlappingWithTouchToFillSurface:
    case autofill::SuggestionHidingReason::kOverlappingWithAutofillContextMenu:
    case autofill::SuggestionHidingReason::kContextMenuOpened:
      return autofill::EmailVerificationPopupController::EvpPermissionUiStatus::
          kOverlappingPrompt;
    case autofill::SuggestionHidingReason::kAcceptSuggestion:
    case autofill::SuggestionHidingReason::kAttachInterstitialPage:
    case autofill::SuggestionHidingReason::kContentAreaMoved:
    case autofill::SuggestionHidingReason::kNoSuggestions:
    case autofill::SuggestionHidingReason::kRendererEvent:
    case autofill::SuggestionHidingReason::kStaleData:
    case autofill::SuggestionHidingReason::kViewDestroyed:
    case autofill::SuggestionHidingReason::kInsufficientSpace:
    case autofill::SuggestionHidingReason::kElementOutsideOfContentArea:
    case autofill::SuggestionHidingReason::kMouseLocked:
    case autofill::SuggestionHidingReason::kNoFrameHasFocus:
    case autofill::SuggestionHidingReason::kExpandedSuggestionCollapsedSubPopup:
    case autofill::SuggestionHidingReason::kFieldValueChanged:
    case autofill::SuggestionHidingReason::kFadeTimerExpired:
    case autofill::SuggestionHidingReason::kSearchBarFocusLost:
    case autofill::SuggestionHidingReason::kHiddenByCaller:
      return autofill::EmailVerificationPopupController::EvpPermissionUiStatus::
          kOther;
  }
}

}  // namespace

namespace autofill {

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
