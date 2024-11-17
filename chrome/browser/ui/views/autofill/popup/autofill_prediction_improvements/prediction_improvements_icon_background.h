// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_AUTOFILL_PREDICTION_IMPROVEMENTS_PREDICTION_IMPROVEMENTS_ICON_BACKGROUND_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_AUTOFILL_PREDICTION_IMPROVEMENTS_PREDICTION_IMPROVEMENTS_ICON_BACKGROUND_H_

#include "ui/views/background.h"
#include "ui/views/layout/layout_provider.h"

namespace autofill_prediction_improvements {

class PredictionImprovementsIconBackground : public views::Background {
 public:
  explicit PredictionImprovementsIconBackground(views::Emphasis radius_);
  ~PredictionImprovementsIconBackground() override;

  PredictionImprovementsIconBackground(
      const PredictionImprovementsIconBackground&) = delete;
  PredictionImprovementsIconBackground& operator=(
      const PredictionImprovementsIconBackground&) = delete;

  // views::Background:
  void OnViewThemeChanged(views::View* view) override;
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

 private:
  const views::Emphasis radius_;
};

}  // namespace autofill_prediction_improvements

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_AUTOFILL_PREDICTION_IMPROVEMENTS_PREDICTION_IMPROVEMENTS_ICON_BACKGROUND_H_
