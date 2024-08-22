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
  // There are two variants to this button.
  // Option 1) Regular (non-legacy) candidate (set create_legacy_candidate =
  // false):
  // - Contains the candidate and the index stacked above each other:
  //   +---+
  //   | A |
  //   |   |
  //   | 1 |
  //   +---+
  // Option 2) Legacy candidate (set create_legacy_candidate = true):
  // - Simple box containing the candidate: eg.  [ A ]
  // - Does not contain any index. This legacy candidate only exists for
  //   compatibility with previous candidate styles.
  // TODO(b/240357416): Remove legacy option when emoji suggestions uses
  // horizontal layout.
  IndexedSuggestionCandidateButton(PressedCallback callback,
                                   const std::u16string& candidate_text,
                                   const std::u16string& index_text,
                                   bool create_legacy_candidate);
  IndexedSuggestionCandidateButton(const IndexedSuggestionCandidateButton&) =
      delete;
  IndexedSuggestionCandidateButton& operator=(
      const IndexedSuggestionCandidateButton&) = delete;
  ~IndexedSuggestionCandidateButton() override;

  void SetHighlight(bool highlight);

 private:
  // TODO(b/240357416): Remove when emoji suggestions uses horizontal layout.
  void BuildLegacyCandidate(const std::u16string& candidate_text);
  void BuildCandidate(const std::u16string& candidate_text,
                      const std::u16string& index_text);
  bool is_legacy_candidate_;
};
}  // namespace ui::ime

#endif  // CHROME_BROWSER_UI_ASH_INPUT_METHOD_INDEXED_SUGGESTION_CANDIDATE_BUTTON_H_
