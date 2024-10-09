// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_prediction_improvements/save_autofill_prediction_improvements_controller_impl.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/autofill_bubble_handler.h"
#include "chrome/browser/ui/autofill/autofill_prediction_improvements/save_autofill_prediction_improvements_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/autofill/core/browser/autofill_prediction_improvements_delegate.h"
#include "content/public/browser/navigation_handle.h"

namespace autofill {

SaveAutofillPredictionImprovementsControllerImpl::
    SaveAutofillPredictionImprovementsControllerImpl(
        content::WebContents* web_contents)
    : AutofillBubbleControllerBase(web_contents),
      content::WebContentsUserData<
          SaveAutofillPredictionImprovementsControllerImpl>(*web_contents) {}

SaveAutofillPredictionImprovementsControllerImpl::
    ~SaveAutofillPredictionImprovementsControllerImpl() = default;

// static
SaveAutofillPredictionImprovementsController*
SaveAutofillPredictionImprovementsController::GetOrCreate(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }

  SaveAutofillPredictionImprovementsControllerImpl::CreateForWebContents(
      web_contents);
  return SaveAutofillPredictionImprovementsControllerImpl::FromWebContents(
      web_contents);
}

void SaveAutofillPredictionImprovementsControllerImpl::OfferSave(
    std::vector<optimization_guide::proto::UserAnnotationsEntry>
        new_prediction_improvements,
    PromptAcceptanceCallback prompt_acceptance_callback,
    LearnMoreClickedCallback learn_more_clicked_callback,
    UserFeedbackCallback user_feedback_callback) {
  // Don't show the bubble if it's already visible.
  if (bubble_view()) {
    return;
  }
  prediction_improvements_ = std::move(new_prediction_improvements);
  prompt_acceptance_callback_ = std::move(prompt_acceptance_callback);
  learn_more_clicked_callback_ = std::move(learn_more_clicked_callback);
  user_feedback_callback_ = std::move(user_feedback_callback);
  DoShowBubble();
}

void SaveAutofillPredictionImprovementsControllerImpl::OnSaveButtonClicked() {
  OnBubbleClosed(PredictionImprovementsBubbleClosedReason::kAccepted);
}

void SaveAutofillPredictionImprovementsControllerImpl::OnBubbleClosed(
    SaveAutofillPredictionImprovementsController::
        PredictionImprovementsBubbleClosedReason closed_reason) {
  set_bubble_view(nullptr);
  UpdatePageActionIcon();
  if (!prompt_acceptance_callback_.is_null()) {
    std::move(prompt_acceptance_callback_)
        .Run(/*prompt_was_accepted=*/closed_reason ==
             PredictionImprovementsBubbleClosedReason::kAccepted);
  }
}

void SaveAutofillPredictionImprovementsControllerImpl::OnThumbsUpClicked() {
  if (!user_feedback_callback_.is_null()) {
    std::move(user_feedback_callback_)
        .Run(AutofillPredictionImprovementsDelegate::UserFeedback::kThumbsUp);
  }
}
void SaveAutofillPredictionImprovementsControllerImpl::OnThumbsDownClicked() {
  if (!user_feedback_callback_.is_null()) {
    std::move(user_feedback_callback_)
        .Run(AutofillPredictionImprovementsDelegate::UserFeedback::kThumbsDown);
  }
}
void SaveAutofillPredictionImprovementsControllerImpl::OnLearnMoreClicked() {
  if (!learn_more_clicked_callback_.is_null()) {
    std::move(learn_more_clicked_callback_).Run();
  }
}
PageActionIconType
SaveAutofillPredictionImprovementsControllerImpl::GetPageActionIconType() {
  // TODO(crbug.com/362227379): Update icon.
  return PageActionIconType::kAutofillAddress;
}

void SaveAutofillPredictionImprovementsControllerImpl::DoShowBubble() {
  Browser* browser = chrome::FindBrowserWithTab(web_contents());
  set_bubble_view(
      browser->window()
          ->GetAutofillBubbleHandler()
          ->ShowSaveAutofillPredictionImprovementsBubble(web_contents(), this));
  CHECK(bubble_view());
}

base::WeakPtr<SaveAutofillPredictionImprovementsController>
SaveAutofillPredictionImprovementsControllerImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

const std::vector<optimization_guide::proto::UserAnnotationsEntry>&
SaveAutofillPredictionImprovementsControllerImpl::GetPredictionImprovements()
    const {
  return prediction_improvements_;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(
    SaveAutofillPredictionImprovementsControllerImpl);

}  // namespace autofill
