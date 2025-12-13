// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_INPUT_METHOD_INDEXED_SUGGESTION_CANDIDATE_BUTTON_H_
#define CHROME_BROWSER_UI_ASH_INPUT_METHOD_INDEXED_SUGGESTION_CANDIDATE_BUTTON_H_

#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/chromeos/ui_chromeos_export.h"
#include "ui/views/controls/button/button.h"

namespace ui::ime {

class UI_CHROMEOS_EXPORT IndexedSuggestionCandidateButton
    : public views::Button {
  METADATA_HEADER(IndexedSuggestionCandidateButton, views::Button)

 public:
  // Build a suggestion candidate button.
  // - Contains the candidate and the index stacked above each other:
  //   +---+
  //   | A |
  //   |   |
  //   | 1 |
  //   +---+
  IndexedSuggestionCandidateButton(PressedCallback callback,
                                   const std::u16string& candidate_text,
                                   const std::u16string& index_text);
  IndexedSuggestionCandidateButton(const IndexedSuggestionCandidateButton&) =
      delete;
  IndexedSuggestionCandidateButton& operator=(
      const IndexedSuggestionCandidateButton&) = delete;
  ~IndexedSuggestionCandidateButton() override;

  void SetHighlight(bool highlight);

 private:
  void BuildCandidate(const std::u16string& candidate_text,
                      const std::u16string& index_text);
};
}  // namespace ui::ime

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_INDEXED_SUGGESTION_CANDIDATE_BUTTON_H_
