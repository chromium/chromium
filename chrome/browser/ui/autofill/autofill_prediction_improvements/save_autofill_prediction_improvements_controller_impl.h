// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_AUTOFILL_PREDICTION_IMPROVEMENTS_CONTROLLER_IMPL_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_AUTOFILL_PREDICTION_IMPROVEMENTS_CONTROLLER_IMPL_H_

#include <vector>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_bubble_controller_base.h"
#include "chrome/browser/ui/autofill/autofill_prediction_improvements/save_autofill_prediction_improvements_controller.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_user_data.h"

namespace autofill {

// Implementation of per-tab class to control the save prediction improvements
// bubble.
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
  void OfferSave(
      std::vector<PredictionImprovement> prediction_improvements) override;
  void OnSaveButtonClicked() override;
  const std::vector<PredictionImprovement>& GetPredictionImprovements()
      const override;
  void OnBubbleClosed(

      PredictionImprovementsBubbleClosedReason closed_reason) override;
  base::WeakPtr<SaveAutofillPredictionImprovementsController> GetWeakPtr()
      override;

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
  std::vector<PredictionImprovement> prediction_improvements_;

  // Weak pointer factory for this save prediction improvements bubble
  // controller.
  base::WeakPtrFactory<SaveAutofillPredictionImprovementsControllerImpl>
      weak_ptr_factory_{this};

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_PREDICTION_IMPROVEMENTS_SAVE_AUTOFILL_PREDICTION_IMPROVEMENTS_CONTROLLER_IMPL_H_
