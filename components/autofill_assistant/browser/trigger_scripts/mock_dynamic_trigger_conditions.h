// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_MOCK_DYNAMIC_TRIGGER_CONDITIONS_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_MOCK_DYNAMIC_TRIGGER_CONDITIONS_H_

#include "components/autofill_assistant/browser/trigger_scripts/dynamic_trigger_conditions.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockDynamicTriggerConditions : public DynamicTriggerConditions {
 public:
  MockDynamicTriggerConditions();
  ~MockDynamicTriggerConditions() override;

  MOCK_CONST_METHOD1(GetSelectorMatches,
                     absl::optional<bool>(const Selector& selector));

  MOCK_CONST_METHOD1(
      GetDocumentReadyState,
      absl::optional<DocumentReadyState>(const Selector& selector));

  MOCK_METHOD1(SetURL, void(const GURL& url));

  MOCK_CONST_METHOD1(GetPathPatternMatches,
                     bool(const std::string& path_pattern));

  MOCK_CONST_METHOD1(GetDomainAndSchemeMatches,
                     bool(const GURL& domain_with_scheme));

  void Update(WebController* web_controller,
              base::OnceCallback<void(void)> callback) override {
    OnUpdate(web_controller, callback);
  }
  MOCK_METHOD2(OnUpdate,
               void(WebController* web_controller,
                    base::OnceCallback<void(void)>& callback));

  MOCK_METHOD1(AddConditionsFromTriggerScript,
               void(const TriggerScriptProto& proto));
  MOCK_METHOD0(ClearConditions, void(void));
  MOCK_CONST_METHOD0(HasResults, bool(void));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_TRIGGER_SCRIPTS_MOCK_DYNAMIC_TRIGGER_CONDITIONS_H_
