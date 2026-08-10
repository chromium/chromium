// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_ACTOR_MOCK_ACTOR_FORM_FILLING_SERVICE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_ACTOR_MOCK_ACTOR_FORM_FILLING_SERVICE_H_

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/types/expected.h"
#include "components/autofill/core/browser/actor/actor_form_filling_service.h"
#include "components/autofill/core/browser/integrators/actor/actor_form_filling_types.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill {

class MockActorFormFillingService : public ActorFormFillingService {
 public:
  MockActorFormFillingService();
  ~MockActorFormFillingService() override;

  MOCK_METHOD(void,
              GetSuggestions,
              (AutofillClient& client,
               base::span<const FillRequest> fill_requests,
               GetSuggestionsCallback callback),
              (override));

  MOCK_METHOD(
      void,
      FillSuggestions,
      (AutofillClient& client,
       base::span<const ActorFormFillingSelection> chosen_suggestions,
       base::OnceCallback<
           void(base::expected<std::string, ActorFormFillingError>)>),
      (override));

  MOCK_METHOD(void, ScrollToForm, (AutofillClient& client, int), (override));
  MOCK_METHOD(void,
              PreviewForm,
              (AutofillClient& client, int, ActorSuggestionId),
              (override));
  MOCK_METHOD(void,
              ClearFormPreview,
              (AutofillClient& client, int),
              (override));
  MOCK_METHOD(void,
              FillForm,
              (AutofillClient& client, int, ActorFormFillingSelection),
              (override));
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_ACTOR_MOCK_ACTOR_FORM_FILLING_SERVICE_H_
