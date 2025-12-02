// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_TABLE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_TABLE_H_

#include <optional>
#include <string>
#include <vector>

#include "base/time/time.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_database_table.h"

class WebDatabase;

namespace autofill {

class AutocompleteChange;
class AutocompleteEntry;
class FormFieldData;

// This class manages the Autocomplete table. The table in the SQLite database
// is for historical reasons unfortunately named "autofill".
//
// Note: The database stores time in seconds, UTC.
// -----------------------------------------------------------------------------
// autofill             This table contains autocomplete history data (not
//                      structured information).
//
//   name               The name of the input as specified in the html.
//   value              The literal contents of the text field.
//   value_lower        The contents of the text field made lower_case.
//   date_created       The date on which the user first entered the string
//                      |value| into a field of name |name|.
//   date_last_used     The date on which the user last entered the string
//                      |value| into a field of name |name|.
//   count              How many times the user has entered the string |value|
//                      in a field of name |name|.
// -----------------------------------------------------------------------------
class AutocompleteTable : public WebDatabaseTable {
 public:
  AutocompleteTable();

  AutocompleteTable(const AutocompleteTable&) = delete;
  AutocompleteTable& operator=(const AutocompleteTable&) = delete;

  ~AutocompleteTable() override;

  // Retrieves the AutocompleteTable* owned by |db|.
  static AutocompleteTable* FromWebDatabase(WebDatabase* db);

  // WebDatabaseTable:
  WebDatabaseTable::TypeKey GetTypeKey() const override;
  bool CreateTablesIfNecessary() override;
  bool MigrateToVersion(int version, bool* update_compatible_version) override;

  // Records the form elements in |elements| in the database in the
  // autocomplete table.  A list of all added and updated autocomplete entries
  // is returned in the changes out parameter.
  bool AddFormFieldValues(const std::vector<FormFieldData>& elements,
                          std::vector<AutocompleteChange>* changes);

  // Retrieves a vector of all values which have been recorded in the
  // autocomplete table as the value in a form element with name |name| and
  // which start with |prefix|. The comparison of the prefix is case
  // insensitive.
  bool GetFormValuesForElementName(const std::u16string& name,
                                   const std::u16string& prefix,
                                   int limit,
                                   std::vector<AutocompleteEntry>& entries);

  // Removes rows from the autocomplete table if they were created on or after
  // |delete_begin| and last used strictly before |delete_end|. For rows where
  // the time range [date_created, date_last_used] overlaps with [delete_begin,
  // delete_end), but is not entirely contained within the latter range, updates
  // the rows so that their resulting time range [new_date_created,
  // new_date_last_used] lies entirely outside of [delete_begin, delete_end),
  // updating the count accordingly. A list of all changed keys and whether
  // each was updater or removed is returned in the changes out parameter.
  bool RemoveFormElementsAddedBetween(base::Time delete_begin,
                                      base::Time delete_end,
                                      std::vector<AutocompleteChange>& changes);

  // Removes rows from the autocomplete table if they were last accessed
  // strictly before |AutocompleteEntry::ExpirationTime()|.
  bool RemoveExpiredFormElements(std::vector<AutocompleteChange>& changes);

  // Removes the row from the autocomplete table for the given |name| |value|
  // pair.
  bool RemoveFormElement(const std::u16string& name,
                         const std::u16string& value);

  // Returns the number of unique values such that for all autocomplete entries
  // with that value, the interval between creation date and last usage is
  // entirely contained between [|begin|, |end|).
  int GetCountOfValuesContainedBetween(base::Time begin, base::Time end);

  // Retrieves all of the entries in the autocomplete table.
  bool GetAllAutocompleteEntries(std::vector<AutocompleteEntry>* entries);

  // Retrieves a single entry from the autocomplete table.
  std::optional<AutocompleteEntry> GetAutocompleteEntry(
      const std::u16string& name,
      const std::u16string& value);

  // Replaces existing autocomplete entries with the entries supplied in
  // the argument. If the entry does not already exist, it will be added.
  bool UpdateAutocompleteEntries(const std::vector<AutocompleteEntry>& entries);

 private:
  bool AddFormFieldValueTime(const FormFieldData& element,
                             base::Time time,
                             std::vector<AutocompleteChange>* changes);

  // Insert a single AutocompleteEntry into the autocomplete table.
  bool InsertAutocompleteEntry(const AutocompleteEntry& entry);

  bool InitMainTable();
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_WEBDATA_AUTOCOMPLETE_AUTOCOMPLETE_TABLE_H_
