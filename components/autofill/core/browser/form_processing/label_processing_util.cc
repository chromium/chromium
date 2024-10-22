// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/label_processing_util.h"

#include <algorithm>
#include <string_view>
#include <vector>

#include "base/strings/string_split.h"

namespace autofill {

// The maximum number of fields that can share a label.
const int kMaxNumberOfFieldsToShareALabel = 3;
// The maximum length of a label that can be shared among fields.
const int kMaxLengthOfShareableLabel = 40;

std::vector<std::u16string_view> GetParseableLabels(
    std::vector<std::u16string_view> labels) {
  // The current label that may be shared with the subsequent fields.
  auto shared_label_candidate_it = labels.begin();
  while (shared_label_candidate_it != labels.end()) {
    std::u16string_view label = *shared_label_candidate_it;
    // If the label is empty or has a size that exceeds
    // `kMaxLengthOfShareableLabel` it can not be shared with subsequent fields.
    if (label.empty() || label.size() > kMaxLengthOfShareableLabel) {
      ++shared_label_candidate_it;
      continue;
    }

    // Otherwise find a range [shared_label_candidate_it, shareable_range_end[
    // that may share `label`. All labels in the range either match `label` or
    // are empty. Checking for the same label is needed, because label inference
    // often derives the same label for consecutive fields.
    auto shareable_range_end = std::find_if(
        shared_label_candidate_it + 1, labels.end(),
        [&](std::u16string_view l) { return !l.empty() && l != label; });

    size_t num_shareable_fields =
        shareable_range_end - shared_label_candidate_it;
    if (num_shareable_fields == 1 ||
        num_shareable_fields > kMaxNumberOfFieldsToShareALabel) {
      shared_label_candidate_it = shareable_range_end;
      continue;
    }

    // Split the label by single character separators or by common separating
    // words, until the right number of components are archived.
    std::vector<std::u16string_view> label_components = base::SplitStringPiece(
        label, u"/,&-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (label_components.size() != num_shareable_fields) {
      for (const char16_t* word : {u" and ", u" und ", u" et ", u" y "}) {
        label_components = base::SplitStringPieceUsingSubstr(
            label, word, base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
        if (label_components.size() == num_shareable_fields) {
          break;
        }
      }
    }

    if (label_components.size() == num_shareable_fields) {
      std::ranges::move(label_components, shared_label_candidate_it);
    }
    shared_label_candidate_it = shareable_range_end;
  }
  return labels;
}

}  // namespace autofill
