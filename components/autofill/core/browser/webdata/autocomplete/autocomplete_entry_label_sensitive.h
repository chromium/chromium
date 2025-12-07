// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/346507576): After initial testing period this file should
// replace existing autocomplete_entry.h. Label sensitive prefix should
// be dropped everywhere.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_ENTRY_LABEL_SENSITIVE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_ENTRY_LABEL_SENSITIVE_H_

#include <string>

#include "base/time/time.h"

namespace autofill {

class AutocompleteKeyLabelSensitive {
 public:
  AutocompleteKeyLabelSensitive();
  AutocompleteKeyLabelSensitive(std::u16string name,
                                std::u16string label,
                                std::u16string value);
  AutocompleteKeyLabelSensitive(std::string_view name,
                                std::string_view label,
                                std::string_view value);
  AutocompleteKeyLabelSensitive(const AutocompleteKeyLabelSensitive& key);
  virtual ~AutocompleteKeyLabelSensitive();

  const std::u16string& name() const { return name_; }
  const std::u16string& label() const { return label_; }
  const std::u16string& value() const { return value_; }

  friend auto operator<=>(const AutocompleteKeyLabelSensitive& lhs,
                          const AutocompleteKeyLabelSensitive& rhs) = default;
  friend bool operator==(const AutocompleteKeyLabelSensitive& lhs,
                         const AutocompleteKeyLabelSensitive& rhs) = default;

 private:
  std::u16string name_;
  std::u16string label_;
  std::u16string value_;
};

class AutocompleteEntryLabelSensitive {
 public:
  AutocompleteEntryLabelSensitive();
  AutocompleteEntryLabelSensitive(const AutocompleteKeyLabelSensitive& key,
                                  base::Time date_created,
                                  base::Time date_last_used);
  ~AutocompleteEntryLabelSensitive();

  const AutocompleteKeyLabelSensitive& key() const { return key_; }
  const base::Time& date_created() const { return date_created_; }
  const base::Time& date_last_used() const { return date_last_used_; }

  friend bool operator==(const AutocompleteEntryLabelSensitive& lhs,
                         const AutocompleteEntryLabelSensitive& rhs) = default;

 private:
  AutocompleteKeyLabelSensitive key_;
  base::Time date_created_;
  base::Time date_last_used_;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_ENTRY_LABEL_SENSITIVE_H_
