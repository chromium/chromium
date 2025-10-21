// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_MOCK_AUTOFILL_AI_IMPORT_DATA_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_MOCK_AUTOFILL_AI_IMPORT_DATA_CONTROLLER_H_

#include "chrome/browser/ui/autofill/autofill_ai/autofill_ai_import_data_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class EntityInstance;

class MockAutofillAiImportDataController
    : public AutofillAiImportDataController {
 public:
  MockAutofillAiImportDataController();
  ~MockAutofillAiImportDataController() override;

  MOCK_METHOD(void,
              ShowPrompt,
              (EntityInstance,
               std::optional<EntityInstance>,
               AutofillClient::EntityImportPromptResultCallback),
              (override));
  MOCK_METHOD(base::optional_ref<const EntityInstance>,
              GetAutofillAiData,
              (),
              (const override));
  MOCK_METHOD(void, OnSaveButtonClicked, (), (override));
  MOCK_METHOD(std::u16string, GetDialogTitle, (), (const override));
  MOCK_METHOD(std::u16string, GetPrimaryAccountEmail, (), (const override));
  MOCK_METHOD(std::vector<EntityAttributeUpdateDetails>,
              GetUpdatedAttributesDetails,
              (),
              (const override));
  MOCK_METHOD(bool, IsWalletableEntity, (), (const override));
  MOCK_METHOD(bool, IsSavePrompt, (), (const override));
  MOCK_METHOD(void, OnGoToWalletLinkClicked, (), (override));
  MOCK_METHOD((int), GetTitleImagesResourceId, (), (const override));
  MOCK_METHOD(void,
              OnBubbleClosed,
              (AutofillClient::AutofillAiBubbleClosedReason),
              (override));
  base::WeakPtr<AutofillAiImportDataController> GetWeakPtr() override {
    return weak_ptr_factory_.GetWeakPtr();
  }

 private:
  base::WeakPtrFactory<AutofillAiImportDataController> weak_ptr_factory_{this};
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_AI_MOCK_AUTOFILL_AI_IMPORT_DATA_CONTROLLER_H_
