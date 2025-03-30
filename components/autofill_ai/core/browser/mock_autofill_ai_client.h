// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_MOCK_AUTOFILL_AI_CLIENT_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_MOCK_AUTOFILL_AI_CLIENT_H_

#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill_ai/core/browser/autofill_ai_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_ai {

class MockAutofillAiClient : public AutofillAiClient {
 public:
  MockAutofillAiClient();
  MockAutofillAiClient(const MockAutofillAiClient&) = delete;
  MockAutofillAiClient& operator=(const MockAutofillAiClient&) = delete;
  ~MockAutofillAiClient() override;

  MOCK_METHOD(autofill::AutofillClient&, GetAutofillClient, (), (override));
  MOCK_METHOD(AutofillAiManager&, GetManager, (), (override));
  MOCK_METHOD(autofill::EntityDataManager*,
              GetEntityDataManager,
              (),
              (override));
  MOCK_METHOD(autofill::FormStructure*,
              GetCachedFormStructure,
              (const autofill::FormGlobalId& form_id),
              (override));
  MOCK_METHOD(optimization_guide::ModelQualityLogsUploaderService*,
              GetMqlsUploadService,
              (),
              (override));
  MOCK_METHOD(void,
              ShowSaveOrUpdateBubble,
              (autofill::EntityInstance entity,
               std::optional<autofill::EntityInstance> old_entity,
               SaveOrUpdatePromptResultCallback prompt_acceptance_callback),
              (override));
};

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_MOCK_AUTOFILL_AI_CLIENT_H_
