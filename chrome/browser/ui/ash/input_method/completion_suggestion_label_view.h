// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_COMPLETION_SUGGESTION_LABEL_VIEW_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_COMPLETION_SUGGESTION_LABEL_VIEW_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/controls/styled_label.h"

namespace ui {
namespace ime {

// A CompletionSuggestionLabelView renders the text of a completion suggestion.
// A completion suggestion label has two parts:
// - Prefix: The prefix in the suggestion that matches what the user has typed
//   so far. This may be empty for next-word predictions.
// - Prediction: The remaining part of the suggestion that is predicted by the
//   input method.
//
// Examples:
// - User types "how a". The input method suggests "how are you".
//   The prefix is "how a" and the prediction is "re you".
// - User types a space to begin a new word. The input method suggests "how".
//   The prefix is "" and the prediction is "how".
//
// CompletionSuggestionLabelView renders the prefix differently from the
// prediction to distinguish the two.
class UI_CHROMEOS_EXPORT CompletionSuggestionLabelView
    : public views::StyledLabel {
  METADATA_HEADER(CompletionSuggestionLabelView, views::StyledLabel)

 public:
  // TODO(b/233264555): Allow users of this class to set the font instead of
  // hardcoding it.
  static const char kFontName[];
  static const int kFontSize = 13;

  CompletionSuggestionLabelView();

  // Set the prefix and prediction parts of the label.
  void SetPrefixAndPrediction(const std::u16string& prefix,
                              const std::u16string& prediction);

  // Get the width in pixels of the prefix part of the label.
  int GetPrefixWidthPx() const;
};

BEGIN_VIEW_BUILDER(UI_CHROMEOS_EXPORT,
                   CompletionSuggestionLabelView,
                   views::StyledLabel)
END_VIEW_BUILDER

}  // namespace ime
}  // namespace ui

DEFINE_VIEW_BUILDER(UI_CHROMEOS_EXPORT, ui::ime::CompletionSuggestionLabelView)

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_COMPLETION_SUGGESTION_LABEL_VIEW_H_
