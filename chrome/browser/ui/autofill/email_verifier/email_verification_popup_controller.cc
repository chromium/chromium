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

// `Hide(reason)` is invoked by `AutofillPopupHideHelper` when window, focus,
// or web contents events occur.
// While actively waiting for the email verification response (`is_loading_`):
// - Transient page and window interactions (such as clicking outside, field
//   focus changes, text editing ending, scrolling, or window resizes) are
//   suppressed so that the in-button spinner remains visible while token
//   retrieval is in flight. The popup will be explicitly dismissed via
//   `Dismiss()` by `EmailVerificationController` once token retrieval
//   completes and the minimum loading display duration has elapsed.
// - Fatal events where the tab or web contents is torn down or navigated
//   (`kTabGone`, `kAttachInterstitialPage`) are NOT suppressed and proceed
//   immediately to `HideImpl()` to tear down the popup.
void EmailVerificationPopupController::Hide(SuggestionHidingReason reason) {
  if (is_loading_) {
    switch (reason) {
      // Fatal events: proceed to teardown the popup immediately.
      case SuggestionHidingReason::kTabGone:
      case SuggestionHidingReason::kAttachInterstitialPage:
        break;

      // Transient or non-fatal events: suppressed so that the in-button spinner
      // remains visible while token retrieval is in flight.
      case SuggestionHidingReason::kAcceptSuggestion:
      case SuggestionHidingReason::kContextMenuOpened:
      case SuggestionHidingReason::kContentAreaMoved:
      case SuggestionHidingReason::kElementOutsideOfContentArea:
      case SuggestionHidingReason::kEndEditing:
      case SuggestionHidingReason::kExpandedSuggestionCollapsedSubPopup:
      case SuggestionHidingReason::kFadeTimerExpired:
      case SuggestionHidingReason::kFieldValueChanged:
      case SuggestionHidingReason::kFocusChanged:
      case SuggestionHidingReason::kHiddenByCaller:
      case SuggestionHidingReason::kInsufficientSpace:
      case SuggestionHidingReason::kMouseLocked:
      case SuggestionHidingReason::kNoFrameHasFocus:
      case SuggestionHidingReason::kNoSuggestions:
      case SuggestionHidingReason::kOverlappingWithAnotherPrompt:
      case SuggestionHidingReason::kOverlappingWithAutofillContextMenu:
      case SuggestionHidingReason::kOverlappingWithPasswordGenerationPopup:
      case SuggestionHidingReason::kOverlappingWithPictureInPictureWindow:
      case SuggestionHidingReason::kOverlappingWithTouchToFillSurface:
      case SuggestionHidingReason::kRendererEvent:
      case SuggestionHidingReason::kSearchBarFocusLost:
      case SuggestionHidingReason::kStaleData:
      case SuggestionHidingReason::kUserAborted:
      case SuggestionHidingReason::kViewDestroyed:
      case SuggestionHidingReason::kWidgetChanged:
        return;
    }
  }
  HideImpl(MapReasonToStatus(reason));
}

void EmailVerificationPopupController::Dismiss() {
  is_loading_ = false;
  if (view_) {
    view_->Hide();
    view_ = nullptr;
  }
  popup_hide_helper_.reset();
  weak_ptr_factory_.InvalidateWeakPtrs();
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
  if (is_loading_) {
    return;
  }
  HideImpl(EmailVerificationPermissionUiStatus::kUserAborted);
}

void EmailVerificationPopupController::HideImpl(
    AutofillClient::EmailVerificationPermissionUiStatus status) {
  Dismiss();

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
  // Prevent re-entrancy if the verify button is pressed multiple times.
  if (is_loading_) {
    return;
  }
  is_loading_ = true;

  // Transition the popup view to the loading state, displaying the circular
  // animated spinner in the verify button and disabling the cancel button.
  if (view_) {
    view_->ShowLoadingState();
  }

  // Notify the delegate/controller immediately that the user granted permission
  // so it can initiate background token retrieval and track the loading
  // duration in parallel with the loading animation.
  if (callback_) {
    base::UmaHistogramEnumeration(
        "Blink.Evp.PermissionUi.Status",
        EmailVerificationPermissionUiStatus::kAllowed);
    std::move(callback_).Run(EmailVerificationPermissionUiStatus::kAllowed);
  }
}

void EmailVerificationPopupController::OnCancel() {
  HideImpl(EmailVerificationPermissionUiStatus::kDeclined);
}

}  // namespace autofill
