// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/installer/util/experiment_labels.h"

#include <vector>

#include "base/i18n/time_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"

namespace installer {

namespace {

constexpr base::WStringPiece kNameValueSeparator = L"=";
constexpr base::WStringPiece kValueExpirationSeparator = L"|";
constexpr base::WStringPiece kLabelSeparator = L";";

// Returns a vector of string pieces, one for each "name=value|expiration"
// group in |value|.
std::vector<base::WStringPiece> Parse(base::WStringPiece value) {
  return base::SplitStringPiece(value, kLabelSeparator, base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY);
}

// Returns a formatted string given a date that is compatible with Omaha (see
// https://github.com/google/omaha/blob/master/omaha/base/time.cc#L132).
std::wstring FormatDate(base::Time date) {
  // "Fri, 14 Aug 2015 16:13:03 GMT"
  return base::UTF8ToWide(base::TimeFormatHTTP(date));
}

// Appends "label_name=label_value|expiration" to |label|.
void AppendLabel(base::WStringPiece label_name,
                 base::WStringPiece label_value,
                 base::Time expiration,
                 std::wstring* label) {
  base::StrAppend(label, {label_name, kNameValueSeparator, label_value,
                          kValueExpirationSeparator, FormatDate(expiration)});
}

}  // namespace

ExperimentLabels::ExperimentLabels(const std::wstring& value) : value_(value) {}

base::WStringPiece ExperimentLabels::GetValueForLabel(
    base::WStringPiece label_name) const {
  DCHECK(!label_name.empty());

  return FindLabel(label_name).second;
}

void ExperimentLabels::SetValueForLabel(base::WStringPiece label_name,
                                        base::WStringPiece label_value,
                                        base::TimeDelta lifetime) {
  DCHECK(!label_name.empty());
  DCHECK(!label_value.empty());
  DCHECK(!lifetime.is_zero());

  SetValueForLabel(label_name, label_value, base::Time::Now() + lifetime);
}

void ExperimentLabels::SetValueForLabel(base::WStringPiece label_name,
                                        base::WStringPiece label_value,
                                        base::Time expiration) {
  DCHECK(!label_name.empty());
  DCHECK(!label_value.empty());

  LabelAndValue label_and_value = FindLabel(label_name);
  if (label_and_value.first.empty()) {
    // This label doesn't already exist -- append it to the raw value.
    if (!value_.empty())
      value_.push_back(kLabelSeparator[0]);
    AppendLabel(label_name, label_value, expiration, &value_);
  } else {
    // Replace the existing value and expiration.
    // Get the stuff before the old label.
    std::wstring new_label(value_, 0,
                           label_and_value.first.data() - value_.data());
    // Append the new label.
    AppendLabel(label_name, label_value, expiration, &new_label);
    // Find the stuff after the old label and append it.
    size_t next_separator = value_.find(
        kLabelSeparator[0],
        (label_and_value.second.data() + label_and_value.second.size()) -
            value_.data());
    if (next_separator != std::wstring::npos)
      new_label.append(value_, next_separator, std::wstring::npos);
    // Presto.
    new_label.swap(value_);
  }
}

ExperimentLabels::LabelAndValue ExperimentLabels::FindLabel(
    base::WStringPiece label_name) const {
  DCHECK(!label_name.empty());

  std::vector<base::WStringPiece> labels = Parse(value_);
  for (const auto& label : labels) {
    if (label.size() < label_name.size() + 2 ||
        !base::StartsWith(label, label_name) ||
        label[label_name.size()] != kNameValueSeparator[0]) {
      continue;
    }
    size_t value_start = label_name.size() + 1;
    size_t value_end = label.find(kValueExpirationSeparator, value_start);
    if (value_end == base::WStringPiece::npos)
      break;
    return std::make_pair(label,
                          label.substr(value_start, value_end - value_start));
  }
  return LabelAndValue();
}

}  // namespace installer
