// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill_prediction_improvements/prediction_improvements_loading_state_view.h"

#include "chrome/test/views/chrome_views_test_base.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill_prediction_improvements {
namespace {

using ::autofill::Suggestion;
using ::autofill::SuggestionType;

class PredictionImprovementsLoadingStateViewTest : public ChromeViewsTestBase {
};

TEST_F(PredictionImprovementsLoadingStateViewTest, CanInitialize) {
  Suggestion suggestion{SuggestionType::kPredictionImprovementsLoadingState};
  PredictionImprovementsLoadingStateView v{suggestion};
}
}  // namespace

}  // namespace autofill_prediction_improvements
