// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_SUGGESTION_FLOW_H_
#define COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_SUGGESTION_FLOW_H_

#include "components/password_manager/core/browser/password_suggestion_flow.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace password_manager {

class MockPasswordSuggestionFlow : public PasswordSuggestionFlow {
 public:
  MockPasswordSuggestionFlow();
  ~MockPasswordSuggestionFlow() override;
  MOCK_METHOD(void,
              RunFlow,
              (autofill::FieldRendererId,
               const gfx::RectF&,
               base::i18n::TextDirection),
              (override));
};

}  // namespace password_manager

#endif  // COMPONENTS_PASSWORD_MANAGER_CORE_BROWSER_MOCK_PASSWORD_SUGGESTION_FLOW_H_
