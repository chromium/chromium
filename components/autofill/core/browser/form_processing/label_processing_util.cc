// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/form_processing/label_processing_util.h"

#include <algorithm>
#include <concepts>
#include <memory>
#include <string_view>
#include <vector>

#include "base/feature_list.h"
#include "base/strings/string_split.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/form_field_data.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

namespace {

// The maximum number of fields that can share a label.
constexpr int kMaxNumberOfFieldsToShareALabel = 3;
// The maximum length of a label that can be shared among fields.
constexpr int kMaxLengthOfShareableLabel = 40;
// Common separators to split the labels by.
constexpr std::u16string_view kSeparators[] = {
    u"/", u",", u"&", u" - ", u" and ", u" und ", u" et ", u" y "};

// Splits the `label` at all `kSeparators`.
std::vector<std::u16string_view> SplitBySeparators(std::u16string_view label) {
  std::vector<std::u16string_view> components = {label};
  for (std::u16string_view separator : kSeparators) {
    std::vector<std::u16string_view> new_components;
    for (std::u16string_view component : components) {
      std::vector<std::u16string_view> subcomponents =
          base::SplitStringPieceUsingSubstr(component, separator,
                                            base::TRIM_WHITESPACE,
                                            base::SPLIT_WANT_NONEMPTY);
      // TODO(crbug.com/40100455): Use `std::vector::append_range()` when
      // C++23 is available.
      new_components.insert(new_components.end(), subcomponents.begin(),
                            subcomponents.end());
    }
    components = std::move(new_components);
  }
  return components;
}

// The function returns a vector of the same size as `labels`, containing the
// processed labels. Since the function operates on string_views, `label[i]` of
// the result might reference (part of) `label[i-1]` of the input.
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

    std::vector<std::u16string_view> label_components =
        SplitBySeparators(label);
    if (label_components.size() == num_shareable_fields) {
      std::ranges::move(label_components, shared_label_candidate_it);
    }
    shared_label_candidate_it = shareable_range_end;
  }
  return labels;
}

template <typename T>
  requires(std::same_as<T, FormFieldData> ||
           std::same_as<T, std::unique_ptr<AutofillField>>)
base::flat_map<FieldGlobalId, std::u16string> GetParseableLabels(
    base::span<const T> fields) {
  auto get = absl::Overload{
      [](const FormFieldData& field) -> const FormFieldData& { return field; },
      [](const std::unique_ptr<AutofillField>& field) -> const FormFieldData& {
        return *field;
      }};

  std::vector<std::u16string_view> field_labels;
  field_labels.reserve(fields.size());
  for (const T& field : fields) {
    if (!get(field).IsTextInputElement() || !get(field).IsFocusable()) {
      continue;
    }
    field_labels.push_back(get(field).label());
  }

  std::vector<std::pair<FieldGlobalId, std::u16string>> label_map;
  std::vector<std::u16string_view> parseable_labels =
      GetParseableLabels(std::move(field_labels));
  // Iterating through the fields in reverse order is necessary for memory
  // safety: `field_labels` contains string_views pointing to the labels of the
  // `fields`. By splitting shared labels, `field_labels[i]` might reference
  // `field_labels[i-1]`, meaning that earlier labels need to be overwritten
  // later.
  auto it = parseable_labels.rbegin();
  for (const T& field : base::Reversed(fields)) {
    if (!get(field).IsTextInputElement() || !get(field).IsFocusable()) {
      continue;
    }
    CHECK(it != parseable_labels.rend());
    if (get(field).label() != *it &&
        base::FeatureList::IsEnabled(
            features::kAutofillEnableSupportForParsingWithSharedLabels)) {
      label_map.emplace_back(get(field).global_id(), std::u16string(*it));
    }
    it++;
  }
  return label_map;
}

}  // namespace

base::flat_map<FieldGlobalId, std::u16string> GetParseableLabels(
    base::span<const FormFieldData> fields) {
  return GetParseableLabels<FormFieldData>(fields);
}

base::flat_map<FieldGlobalId, std::u16string> GetParseableLabels(
    base::span<const std::unique_ptr<AutofillField>> fields) {
  return GetParseableLabels<std::unique_ptr<AutofillField>>(fields);
}

std::vector<std::u16string_view> GetParseableLabelsForTest(  // IN-TEST
    std::vector<std::u16string_view> labels) {
  return GetParseableLabels(labels);
}

}  // namespace autofill
