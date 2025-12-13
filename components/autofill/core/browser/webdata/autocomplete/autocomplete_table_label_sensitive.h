// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/346507576): After initial testing period this file should
// replace existing autocomplete_table.h. Label sensitive prefix should
// be dropped everywhere.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_TABLE_LABEL_SENSITIVE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_TABLE_LABEL_SENSITIVE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry_label_sensitive.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

namespace autofill {

class AutocompleteEntryLabelSensitive;

enum class MatchingType {
  kUnknown = 0,
  kLabel = 1,
  kName = 2,
  kNameAndLabel = 3,
  kMaxValue = kNameAndLabel,
};

constexpr MatchingType ToSafeMatchingType(
    std::underlying_type_t<MatchingType> matching_type) {
  const bool is_valid =
      matching_type >= 0 &&
      matching_type <= static_cast<int>(MatchingType::kMaxValue);
  return is_valid ? static_cast<MatchingType>(matching_type)
                  : MatchingType::kUnknown;
}

// This class encompasses all necessary information to present an autocomplete
// suggestion in the UI layer and report appropriate metrics.
class AutocompleteSearchResultLabelSensitive {
 public:
  AutocompleteSearchResultLabelSensitive(std::u16string value,
                                         MatchingType matching_type,
                                         int count);
  ~AutocompleteSearchResultLabelSensitive();

  // Getters
  const std::u16string& value() const { return value_; }
  MatchingType matching_type() const { return matching_type_; }
  int count() const { return count_; }

  bool operator==(const AutocompleteSearchResultLabelSensitive& other) const {
    return value_ == other.value_;
  }

  // Hash function used for deduplication of search results. Takes into
  // account only the value of the string that can be filled.
  template <typename H>
  friend H AbslHashValue(H h,
                         const AutocompleteSearchResultLabelSensitive& result) {
    return H::combine(std::move(h), result.value());
  }

 private:
  std::u16string value_;
  MatchingType
      matching_type_;  // Required for metrics. We want to know if the
                       // suggestion was found via name, label, or both. Also
                       // used in search query for ranking suggestions (both
                       // name and label suggestions first).
  int count_;
};

// This class manages the Autocomplete table.
//
// Note: The database stores time in seconds, UTC.
// -----------------------------------------------------------------------------
// autocomplete         This table contains autocomplete history data (not
//                      structured information).
//
//   name               The name of the input as specified in the html.
//   label              The label of the input as specified in the html.
//   label_normalized   The label normalized using NormalizeLabel() method.
//   value              The literal contents of the text field.
//   value_lower        The contents of the text field made lower_case.
//   date_created       The date on which the user first entered the string
//                      `value` into a field of name `name` and label `label`.
//   date_last_used     The date on which the user last entered the string
//                      `value` into a field of label `label`.
//   count              How many times the user has entered the string `value`
//                      in a field of name `name` and label `label`.
//
// Primary key: name, label, value.
// Indexes:
// - name, label_normalized, value_lower
// - label_normalized, value_lower
// - value, date_last_used
// - name, label, value
// -----------------------------------------------------------------------------
class AutocompleteTableLabelSensitive : public WebDatabaseTable {
 public:
  AutocompleteTableLabelSensitive();

  AutocompleteTableLabelSensitive(const AutocompleteTableLabelSensitive&) =
      delete;
  AutocompleteTableLabelSensitive& operator=(
      const AutocompleteTableLabelSensitive&) = delete;

  ~AutocompleteTableLabelSensitive() override;

  // Retrieves the AutocompleteTableLabelSensitive* owned by `db`.
  static AutocompleteTableLabelSensitive* FromWebDatabase(WebDatabase* db);

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Records the form elements in `elements` in the database in the
  // autocomplete table.
  [[nodiscard]] bool AddFormFieldValues(
      const std::vector<autofill::FormFieldData>& elements);

  // Retrieves a vector of all values which have been recorded in the
  // autocomplete table as the value in a form element with label `label`, name
  // `name` and which start with `prefix`. The comparison of the prefix is case
  // insensitive.
  [[nodiscard]] bool GetFormValuesForElementNameAndLabel(
      std::u16string_view name,
      std::u16string_view label,
      std::u16string_view prefix,
      size_t limit,
      std::vector<AutocompleteSearchResultLabelSensitive>& entries);

  // Removes rows from the autocomplete table if they were created on or after
  // `delete_begin` and last used strictly before `delete_end`. For rows where
  // the time range [date_created, date_last_used] overlaps with [delete_begin,
  // delete_end), but is not entirely contained within the latter range, updates
  // the rows so that their resulting time range [new_date_created,
  // new_date_last_used] lies entirely outside of [delete_begin, delete_end),
  // updating the count accordingly.
  [[nodiscard]] bool RemoveFormElementsAddedBetween(base::Time delete_begin,
                                                    base::Time delete_end);

  // Removes rows from the autocomplete table if they were last accessed
  // strictly before `AutocompleteEntryLabelSensitive::ExpirationTime()`.
  [[nodiscard]] bool RemoveExpiredFormElements();

  // Removes the row from the autocomplete table for the given `name`, `label`
  // and `value` triple.
  [[nodiscard]] bool RemoveFormElement(std::u16string_view name,
                                       std::u16string_view label,
                                       std::u16string_view value);

  // Returns the number of unique values such that for all autocomplete entries
  // with that value, the interval between creation date and last usage is
  // entirely contained between [`begin`, `end`).
  [[nodiscard]] int GetCountOfValuesContainedBetween(base::Time begin,
                                                     base::Time end);

  // Retrieves all of the entries in the autocomplete table.
  [[nodiscard]] bool GetAllAutocompleteEntries(
      std::vector<AutocompleteEntryLabelSensitive>* entries);

  // Retrieves a single entry from the autocomplete table.
  [[nodiscard]] std::optional<AutocompleteEntryLabelSensitive>
  GetAutocompleteEntryLabelSensitive(const std::u16string_view name,
                                     const std::u16string_view label,
                                     const std::u16string_view value);

 private:
  bool AddFormFieldValueTime(const FormFieldData& element, base::Time time);

  // Insert a single AutocompleteEntryLabelSensitive into the autocomplete
  // table.
  bool InsertAutocompleteEntryLabelSensitive(
      const AutocompleteEntryLabelSensitive& entry);

  bool InitMainTable();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_TABLE_LABEL_SENSITIVE_H_
