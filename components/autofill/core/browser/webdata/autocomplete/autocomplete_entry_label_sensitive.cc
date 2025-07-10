// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/346507576): After initial testing period this file should
// replace existing autocomplete_entry_label.cc. Label sensitive prefix should
// be dropped everywhere.

#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry_label_sensitive.h"

#include <string>
#include <utility>

#include "base/strings/utf_string_conversions.h"

namespace autofill {

AutocompleteKeyLabelSensitive::AutocompleteKeyLabelSensitive() = default;

AutocompleteKeyLabelSensitive::AutocompleteKeyLabelSensitive(
    std::u16string name,
    std::u16string label,
    std::u16string value)
    : name_(std::move(name)),
      label_(std::move(label)),
      value_(std::move(value)) {}

AutocompleteKeyLabelSensitive::AutocompleteKeyLabelSensitive(
    std::string_view name,
    std::string_view label,
    std::string_view value)
    : name_(base::UTF8ToUTF16(name)),
      label_(base::UTF8ToUTF16(label)),
      value_(base::UTF8ToUTF16(value)) {}

AutocompleteKeyLabelSensitive::AutocompleteKeyLabelSensitive(
    const AutocompleteKeyLabelSensitive& key)
    : name_(key.name()), label_(key.label()), value_(key.value()) {}

AutocompleteKeyLabelSensitive::~AutocompleteKeyLabelSensitive() = default;

AutocompleteEntryLabelSensitive::AutocompleteEntryLabelSensitive() = default;

AutocompleteEntryLabelSensitive::AutocompleteEntryLabelSensitive(
    const AutocompleteKeyLabelSensitive& key,
    base::Time date_created,
    base::Time date_last_used)
    : key_(key), date_created_(date_created), date_last_used_(date_last_used) {}

AutocompleteEntryLabelSensitive::~AutocompleteEntryLabelSensitive() = default;

}  // namespace autofill
