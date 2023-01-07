// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_ASSISTANT_FIELD_TRIAL_UTIL_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_ASSISTANT_FIELD_TRIAL_UTIL_H_

#include "base/strings/string_piece.h"
#include "components/autofill_assistant/browser/assistant_field_trial_util.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace autofill_assistant {

class MockAssistantFieldTrialUtil : public AssistantFieldTrialUtil {
 public:
  MockAssistantFieldTrialUtil();
  ~MockAssistantFieldTrialUtil() override;

  MOCK_METHOD(bool,
              RegisterSyntheticFieldTrial,
              (base::StringPiece, base::StringPiece),
              (const, override));
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_MOCK_ASSISTANT_FIELD_TRIAL_UTIL_H_
