// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_MOCK_SAVE_OR_UPDATE_AI_DATA_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_MOCK_SAVE_OR_UPDATE_AI_DATA_CONTROLLER_H_

#include "chrome/browser/ui/autofill/autofill_ai/save_or_update_autofill_ai_data_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {
class EntityInstance;
}

namespace autofill_ai {

class MockSaveOrUpdateAutofillAiDataController
    : public SaveOrUpdateAutofillAiDataController {
 public:
  MockSaveOrUpdateAutofillAiDataController();
  ~MockSaveOrUpdateAutofillAiDataController() override;

  MOCK_METHOD(void,
              ShowPrompt,
              (autofill::EntityInstance,
               std::optional<autofill::EntityInstance>,
               AutofillAiClient::SaveOrUpdatePromptResultCallback),
              (override));
  MOCK_METHOD(base::optional_ref<const autofill::EntityInstance>,
              GetAutofillAiData,
              (),
              (const override));
  MOCK_METHOD(void, OnSaveButtonClicked, (), (override));
  MOCK_METHOD(std::u16string, GetDialogTitle, (), (const override));
  MOCK_METHOD(std::vector<EntityAttributeUpdateDetails>,
              GetUpdatedAttributesDetails,
              (),
              (const override));
  MOCK_METHOD(bool, IsSavePrompt, (), (const override));
  MOCK_METHOD((std::pair<int, int>),
              GetTitleImagesResourceId,
              (),
              (const override));
  MOCK_METHOD(void, OnBubbleClosed, (AutofillAiBubbleClosedReason), (override));
  base::WeakPtr<SaveOrUpdateAutofillAiDataController> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<SaveOrUpdateAutofillAiDataController> weak_ptr_factory_{
      this};
};

}  // namespace autofill_ai

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_MOCK_SAVE_OR_UPDATE_AI_DATA_CONTROLLER_H_
