// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_PREDICTION_IMPROVEMENTS_PREDICTION_IMPROVEMENTS_LOADING_STATE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_PREDICTION_IMPROVEMENTS_PREDICTION_IMPROVEMENTS_LOADING_STATE_VIEW_H_

#include "components/autofill/core/browser/ui/suggestion.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace autofill_prediction_improvements {

// Shows `suggestion.icon` next to `PredictionImprovementsAnimatedGradientView`.
// This view is meant to be shown for
// `SuggestionType::kPredictionImprovementsLoadingState`.
class PredictionImprovementsLoadingStateView : public views::BoxLayoutView {
  METADATA_HEADER(PredictionImprovementsLoadingStateView, views::BoxLayoutView)

 public:
  explicit PredictionImprovementsLoadingStateView(
      const autofill::Suggestion& suggestion);
  ~PredictionImprovementsLoadingStateView() override;
};

}  // namespace autofill_prediction_improvements

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_PREDICTION_IMPROVEMENTS_PREDICTION_IMPROVEMENTS_LOADING_STATE_VIEW_H_
