// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_MOCK_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_
#define COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_MOCK_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_

#include "components/autofill_prediction_improvements/core/browser/autofill_prediction_improvements_client.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_prediction_improvements {

class MockAutofillPredictionImprovementsClient
    : public AutofillPredictionImprovementsClient {
 public:
  MockAutofillPredictionImprovementsClient();
  MockAutofillPredictionImprovementsClient(
      const MockAutofillPredictionImprovementsClient&) = delete;
  MockAutofillPredictionImprovementsClient& operator=(
      const MockAutofillPredictionImprovementsClient&) = delete;
  ~MockAutofillPredictionImprovementsClient() override;

  MOCK_METHOD(autofill::AutofillClient&, GetAutofillClient, (), (override));
  MOCK_METHOD(void,
              GetAXTree,
              (AutofillPredictionImprovementsClient::AXTreeCallback callback),
              (override));
  MOCK_METHOD(AutofillPredictionImprovementsManager&,
              GetManager,
              (),
              (override));
  MOCK_METHOD(AutofillPredictionImprovementsFillingEngine*,
              GetFillingEngine,
              (),
              (override));
  MOCK_METHOD(const GURL&, GetLastCommittedURL, (), (override));
  MOCK_METHOD(const url::Origin&, GetLastCommittedOrigin, (), (override));
  MOCK_METHOD(std::string, GetTitle, (), (override));
  MOCK_METHOD(user_annotations::UserAnnotationsService*,
              GetUserAnnotationsService,
              (),
              (override));
  MOCK_METHOD(bool,
              IsAutofillPredictionImprovementsEnabledPref,
              (),
              (const override));
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
      ShowSaveAutofillPredictionImprovementsBubble,
      (std::unique_ptr<user_annotations::FormAnnotationResponse>
           form_annotation_response,
       user_annotations::PromptAcceptanceCallback prompt_acceptance_callback),
      (override));
};

}  // namespace autofill_prediction_improvements

#endif  // COMPONENTS_AUTOFILL_PREDICTION_IMPROVEMENTS_CORE_BROWSER_MOCK_AUTOFILL_PREDICTION_IMPROVEMENTS_CLIENT_H_
