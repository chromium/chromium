// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_AI_CORE_BROWSER_MOCK_AUTOFILL_AI_CLIENT_H_
#define COMPONENTS_AUTOFILL_AI_CORE_BROWSER_MOCK_AUTOFILL_AI_CLIENT_H_

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
  MOCK_METHOD(void,
              GetAXTree,
              (AutofillAiClient::AXTreeCallback callback),
              (override));
  MOCK_METHOD(AutofillAiManager&, GetManager, (), (override));
  MOCK_METHOD(AutofillAiModelExecutor*, GetModelExecutor, (), (override));
  MOCK_METHOD(const GURL&, GetLastCommittedURL, (), (override));
  MOCK_METHOD(const url::Origin&, GetLastCommittedOrigin, (), (override));
  MOCK_METHOD(std::string, GetTitle, (), (override));
  MOCK_METHOD(user_annotations::UserAnnotationsService*,
              GetUserAnnotationsService,
              (),
              (override));
  MOCK_METHOD(bool, IsAutofillAiEnabledPref, (), (const override));
  MOCK_METHOD(void,
              TryToOpenFeedbackPage,
              (const std::string& feedback_id),
              (override));
  MOCK_METHOD(void, OpenPredictionImprovementsSettings, (), (override));
  MOCK_METHOD(bool, IsUserEligible, (), (override));
  MOCK_METHOD(autofill::FormStructure*,
              GetCachedFormStructure,
              (const autofill::FormData& form_data),
              (override));
  MOCK_METHOD(std::u16string,
              GetAutofillNameFillingValue,
              (const std::string& autofill_profile_guid,
               autofill::FieldType field_type,
               const autofill::FormFieldData& field),
              (override));
  MOCK_METHOD(
      void,
      ShowSaveAutofillAiBubble,
      (std::unique_ptr<user_annotations::FormAnnotationResponse>
           form_annotation_response,
       user_annotations::PromptAcceptanceCallback prompt_acceptance_callback),
      (override));
};

}  // namespace autofill_ai

#endif  // COMPONENTS_AUTOFILL_AI_CORE_BROWSER_MOCK_AUTOFILL_AI_CLIENT_H_
