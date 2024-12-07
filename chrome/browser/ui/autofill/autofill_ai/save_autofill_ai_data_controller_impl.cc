// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_ai/save_autofill_ai_data_controller_impl.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_ai/save_autofill_ai_data_controller.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/browser/autofill_ai_delegate.h"
#include "content/public/browser/navigation_handle.h"

namespace autofill_ai {

namespace {

using autofill::AutofillAiDelegate;

// Returns whether user interacted with the bubble, based on its closed reason.
bool GetUserInteractionFromAutofillAiBubbleClosedReason(
    SaveAutofillAiDataController::AutofillAiBubbleClosedReason closed_reason) {
  using enum SaveAutofillAiDataController::AutofillAiBubbleClosedReason;
  switch (closed_reason) {
    case kAccepted:
    case kCancelled:
    case kClosed:
      return true;
    case kUnknown:
    case kNotInteracted:
    case kLostFocus:
      return false;
  }
}

}  // namespace

SaveAutofillAiDataControllerImpl::SaveAutofillAiDataControllerImpl(
    content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<SaveAutofillAiDataControllerImpl>(
          *web_contents) {}

SaveAutofillAiDataControllerImpl::~SaveAutofillAiDataControllerImpl() = default;

// static
SaveAutofillAiDataController* SaveAutofillAiDataController::GetOrCreate(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  SaveAutofillAiDataControllerImpl::CreateForWebContents(web_contents);
  return SaveAutofillAiDataControllerImpl::FromWebContents(web_contents);
}

void SaveAutofillAiDataControllerImpl::OfferSave(
    std::vector<optimization_guide::proto::UserAnnotationsEntry>
        autofill_ai_data,
    user_annotations::PromptAcceptanceCallback prompt_acceptance_callback,
    LearnMoreClickedCallback learn_more_clicked_callback,
    UserFeedbackCallback user_feedback_callback) {
  // Don't show the bubble if it's already visible.
  if (bubble_view()) {
    return;
  }
  autofill_ai_data_ = std::move(autofill_ai_data);
  prompt_acceptance_callback_ = std::move(prompt_acceptance_callback);
  learn_more_clicked_callback_ = std::move(learn_more_clicked_callback);
  user_feedback_callback_ = std::move(user_feedback_callback);
  DoShowBubble();
}

void SaveAutofillAiDataControllerImpl::OnSaveButtonClicked() {
  OnBubbleClosed(AutofillAiBubbleClosedReason::kAccepted);
}

void SaveAutofillAiDataControllerImpl::OnBubbleClosed(
    SaveAutofillAiDataController::AutofillAiBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);
  UpdatePageActionIcon();
  if (!prompt_acceptance_callback_.is_null()) {
    std::move(prompt_acceptance_callback_)
        .Run({/*prompt_was_accepted=*/closed_reason ==
                  AutofillAiBubbleClosedReason::kAccepted,
              /*did_user_interact=*/
              GetUserInteractionFromAutofillAiBubbleClosedReason(closed_reason),
              did_trigger_thumbs_up_, did_trigger_thumbs_down_});
  }
}

void SaveAutofillAiDataControllerImpl::OnThumbsUpClicked() {
  if (!user_feedback_callback_.is_null()) {
    std::move(user_feedback_callback_)
        .Run(AutofillAiDelegate::UserFeedback::kThumbsUp);
  }
  did_trigger_thumbs_up_ = true;
}

void SaveAutofillAiDataControllerImpl::OnThumbsDownClicked() {
  if (!user_feedback_callback_.is_null()) {
    std::move(user_feedback_callback_)
        .Run(AutofillAiDelegate::UserFeedback::kThumbsDown);
  }
  did_trigger_thumbs_down_ = true;
}

void SaveAutofillAiDataControllerImpl::OnLearnMoreClicked() {
  if (!learn_more_clicked_callback_.is_null()) {
    std::move(learn_more_clicked_callback_).Run();
  }
}

PageActionIconType SaveAutofillAiDataControllerImpl::GetPageActionIconType() {
  // TODO(crbug.com/362227379): Update icon.
  return PageActionIconType::kAutofillAddress;
}

void SaveAutofillAiDataControllerImpl::DoShowBubble() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  set_bubble_view(browser->window()
                      ->GetAutofillBubbleHandler()
                      ->ShowSaveAutofillAiDataBubble(web_contents(), this));
  CHECK(bubble_view());
}

base::WeakPtr<SaveAutofillAiDataController>
SaveAutofillAiDataControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

const std::vector<optimization_guide::proto::UserAnnotationsEntry>&
SaveAutofillAiDataControllerImpl::GetAutofillAiData() const {
  return autofill_ai_data_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(SaveAutofillAiDataControllerImpl);

}  // namespace autofill_ai
