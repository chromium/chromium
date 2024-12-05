// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/popup/autofill_ai/autofill_ai_loading_state_view.h"

#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_ai {
namespace {

using ::autofill::Suggestion;
using ::autofill::SuggestionType;

using AutofillAiLoadingStateViewTest = ChromeViewsTestBase;

TEST_F(AutofillAiLoadingStateViewTest, CanInitialize) {
  Suggestion suggestion{SuggestionType::kAutofillAiLoadingState};
  AutofillAiLoadingStateView v{suggestion};
}
}  // namespace

}  // namespace autofill_ai
