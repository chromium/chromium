// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_AUTOFILL_PREDICTION_IMPROVEMENTS_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_AUTOFILL_PREDICTION_IMPROVEMENTS_CONTROLLER_IMPL_H_

#include <vector>

#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/autofill_prediction_improvements/save_autofill_prediction_improvements_controller.h"
#include "components/autofill/core/browser/autofill_prediction_improvements_delegate.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

// Implementation of per-tab class to control the save prediction improvements
// bubble.
// TODO(crbug.com/361434879): Introduce tests when this class has more than
// simple forwarding method.
class SaveAutofillPredictionImprovementsControllerImpl
    : public AutofillBubbleControllerBase,
      public SaveAutofillPredictionImprovementsController,
      public content::WebContentsUserData<
          SaveAutofillPredictionImprovementsControllerImpl> {
 public:
  SaveAutofillPredictionImprovementsControllerImpl(
      const SaveAutofillPredictionImprovementsControllerImpl&) = delete;
  SaveAutofillPredictionImprovementsControllerImpl& operator=(
      const SaveAutofillPredictionImprovementsControllerImpl&) = delete;
  ~SaveAutofillPredictionImprovementsControllerImpl() override;

  // SaveAutofillPredictionImprovementsController:
  void OfferSave(std::vector<optimization_guide::proto::UserAnnotationsEntry>
                     prediction_improvements,
                 PromptAcceptanceCallback prompt_acceptance_callback,
                 LearnMoreClickedCallback learn_more_clicked_callback,
                 UserFeedbackCallback user_feedback_callback) override;
  void OnSaveButtonClicked() override;
  const std::vector<optimization_guide::proto::UserAnnotationsEntry>&
  GetPredictionImprovements() const override;
  void OnBubbleClosed(
      PredictionImprovementsBubbleClosedReason closed_reason) override;
  base::WeakPtr<SaveAutofillPredictionImprovementsController> GetWeakPtr()
      override;
  void OnThumbsUpClicked() override;
  void OnThumbsDownClicked() override;
  void OnLearnMoreClicked() override;

 protected:
  explicit SaveAutofillPredictionImprovementsControllerImpl(
      content::WebContents* web_contents);

  // AutofillBubbleControllerBase::
  PageActionIconType GetPageActionIconType() override;
  void DoShowBubble() override;

 private:
  friend class content::WebContentsUserData<
      SaveAutofillPredictionImprovementsControllerImpl>;
  friend class SaveAutofillPredictionImprovementsControllerImplTest;

  void ShowBubble();

  // A list of prediction improvements keys and values that the user can accept
  // to save.
  std::vector<optimization_guide::proto::UserAnnotationsEntry>
      prediction_improvements_;

  // Callback to notify the data provider about the user decision for the save
  // prompt.
  PromptAcceptanceCallback prompt_acceptance_callback_ = base::NullCallback();

  // Callback to notify that the user clicked the button to learn more about the
  // feature.
  LearnMoreClickedCallback learn_more_clicked_callback_ = base::NullCallback();

  // Callback to notify that the user has given feedback about Autofill with AI.
  UserFeedbackCallback user_feedback_callback_ = base::NullCallback();
  // Weak pointer factory for this save prediction improvements bubble
  // controller.
  base::WeakPtrFactory<SaveAutofillPredictionImprovementsControllerImpl>
      weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_AUTOFILL_PREDICTION_IMPROVEMENTS_CONTROLLER_IMPL_H_
