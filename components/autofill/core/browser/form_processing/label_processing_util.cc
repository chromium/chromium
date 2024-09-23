// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/label_processing_util.h"

#include <string_view>

#include "base/ranges/algorithm.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"

namespace autofill {

using LabelPieces = std::vector<std::u16string_view>;

// The maximum number of fields that can share a label.
const int kMaxNumberOfFieldsToShareALabel = 3;
// The maximum length of a label that can be shared among fields.
const int kMaxLengthOfShareableLabel = 40;

std::optional<std::vector<std::u16string>> GetParseableLabels(
    const LabelPieces& labels) {
  // Make a copy of the labels.
  LabelPieces shared_labels = labels;

  // Tracks if at least one shared label was found.
  bool shared_labels_found = false;

  // The index of the current field that may be eligible to share its label with
  // the subsequent fields.
  size_t label_index = 0;
  while (label_index < labels.size()) {
    const std::u16string_view& label = labels[label_index];
    // If the label is empty or has a size that exceeds
    // |kMaxLengthOfShareableLabel| it can not be shared with subsequent fields.
    if (label.empty() || label.size() > kMaxLengthOfShareableLabel) {
      ++label_index;
      continue;
    }

    // Otherwise search if the subsequent fields are empty or share the same
    // label. Checking for the same label is needed, because label inference
    // often derives the same label for consecutive fields.
    size_t scan_index = label_index + 1;
    while (scan_index < labels.size() &&
           (labels[scan_index].empty() || labels[scan_index] == label)) {
      ++scan_index;
    }
    // After the loop, the `scan_index` points to the first subsequent field
    // which the label cannot be shared with or to the first out-of-bound index.

    // Calculate the number of fields that may share a label.
    size_t fields_to_share_label = scan_index - label_index;

    // Remember the current index and increment it to continue with the next
    // non-empty field.
    size_t shared_label_starting_index = label_index;
    label_index = scan_index;

    // Determine if there is the correct number of fields that may share a
    // label.
    if (fields_to_share_label == 1 ||
        fields_to_share_label > kMaxNumberOfFieldsToShareALabel) {
      continue;
    }

    // Otherwise, try to split the label by single character separators.
    LabelPieces label_components = base::SplitStringPiece(
        label, u"/,&-", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

    // If the number of components does not match, try to split by common
    // separating words.
    if (label_components.size() != fields_to_share_label) {
      for (const char* word : {" and ", " und ", " et ", " y "}) {
        label_components = base::SplitStringPieceUsingSubstr(
            label, base::ASCIIToUTF16(word), base::TRIM_WHITESPACE,
            base::SPLIT_WANT_NONEMPTY);
        if (label_components.size() == fields_to_share_label)
          break;
      }
    }

    // Continue to the next field if the right number of components has not
    // been found.
    if (label_components.size() != fields_to_share_label)
      continue;

    shared_labels_found = true;
    // Otherwise assign the label components to the fields.
    for (size_t i = 0; i < label_components.size(); ++i) {
      shared_labels[shared_label_starting_index + i] = label_components[i];
    }
  }

  if (!shared_labels_found) {
    return std::nullopt;
  }

  // Otherwise convert the shared label string pieces into strings for memory
  // safety.
  std::vector<std::u16string> result;
  result.reserve(shared_labels.size());
  base::ranges::transform(shared_labels, std::back_inserter(result),
                          [](auto& s) { return std::u16string(s); });
  return std::make_optional(std::move(result));
}

}  // namespace autofill
