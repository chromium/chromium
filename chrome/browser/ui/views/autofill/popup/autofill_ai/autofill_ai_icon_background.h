// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_AUTOFILL_AI_AUTOFILL_AI_ICON_BACKGROUND_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_AUTOFILL_AI_AUTOFILL_AI_ICON_BACKGROUND_H_

#include "ui/views/background.h"
#include "ui/views/layout/layout_provider.h"

namespace autofill_ai {

class AutofillAiIconBackground : public views::Background {
 public:
  explicit AutofillAiIconBackground(views::Emphasis radius_);
  ~AutofillAiIconBackground() override;

  AutofillAiIconBackground(const AutofillAiIconBackground&) = delete;
  AutofillAiIconBackground& operator=(const AutofillAiIconBackground&) = delete;

  // views::Background:
  void OnViewThemeChanged(views::View* view) override;
  void Paint(gfx::Canvas* canvas, views::View* view) const override;

 private:
  const views::Emphasis radius_;
};

}  // namespace autofill_ai

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_AUTOFILL_AI_AUTOFILL_AI_ICON_BACKGROUND_H_
