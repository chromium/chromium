// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_AUTOFILL_AI_AUTOFILL_AI_LOADING_STATE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_AUTOFILL_AI_AUTOFILL_AI_LOADING_STATE_VIEW_H_

#include "components/autofill/core/browser/ui/suggestion.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/layout/box_layout_view.h"

namespace autofill_ai {

// Shows `suggestion.icon` next to `AutofillAiAnimatedGradientView`.
// This view is meant to be shown for `SuggestionType::kAutofillAiLoadingState`.
class AutofillAiLoadingStateView : public views::BoxLayoutView {
  METADATA_HEADER(AutofillAiLoadingStateView, views::BoxLayoutView)

 public:
  explicit AutofillAiLoadingStateView(const autofill::Suggestion& suggestion);
  ~AutofillAiLoadingStateView() override;
};

}  // namespace autofill_ai

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_POPUP_AUTOFILL_AI_AUTOFILL_AI_LOADING_STATE_VIEW_H_
