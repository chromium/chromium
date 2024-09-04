// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table.h"

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/i18n/case_conversion.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/browser/webdata/autofill_table_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// For historical reasons, the table in the SQLite database is named "autofill".
constexpr std::string_view kAutocompleteTable = "autofill";
constexpr std::string_view kName = "name";
constexpr std::string_view kValue = "value";
constexpr std::string_view kValueLower = "value_lower";
constexpr std::string_view kDateCreated = "date_created";
constexpr std::string_view kDateLastUsed = "date_last_used";
constexpr std::string_view kCount = "count";

// Helper struct for AutocompleteTable::RemoveFormElementsAddedBetween().
// Contains all the necessary fields to update a row in the 'autofill' table.
struct AutocompleteUpdate {
  std::u16string name;
  std::u16string value;
  time_t date_created;
  time_t date_last_used;
  int count;
};

WebDatabaseTable::TypeKey GetKey() {
  // We just need a unique constant. Use the address of a static that
  // COMDAT folding won't touch in an optimizing linker.
  static int table_key = 0;
  return reinterpret_cast<void*>(&table_key);
}

time_t GetEndTime(base::Time end) {
  if (end.is_null() || end == base::Time::Max()) {
    return std::numeric_limits<time_t>::max();
  }

  return end.ToTimeT();
}

}  // namespace

AutocompleteTable::AutocompleteTable() = default;

AutocompleteTable::~AutocompleteTable() = default;

// static
AutocompleteTable* AutocompleteTable::FromWebDatabase(WebDatabase* db) {
  return static_cast<AutocompleteTable*>(db->GetTable(GetKey()));
}

WebDatabaseTable::TypeKey AutocompleteTable::GetTypeKey() const {
  return GetKey();
}

bool AutocompleteTable::CreateTablesIfNecessary() {
  return InitMainTable();
}

bool AutocompleteTable::MigrateToVersion(int version,
                                         bool* update_compatible_version) {
  if (!db()->is_open()) {
    return false;
  }
  // Add migration logic here.
  return true;
}

bool AutocompleteTable::AddFormFieldValues(
    const std::vector<FormFieldData>& elements,
    std::vector<AutocompleteChange>* changes) {
  const base::Time now = AutofillClock::Now();
  // Only add one new entry for each unique element name.  Use |seen_names|
  // to track this.  Add up to |kMaximumUniqueNames| unique entries per
  // form.
  const size_t kMaximumUniqueNames = 256;
  std::set<std::u16string> seen_names;
  for (const FormFieldData& element : elements) {
    if (!seen_names.insert(element.name()).second) {
      continue;
    }
    if (seen_names.size() == kMaximumUniqueNames) {
      break;
    }
    if (!AddFormFieldValueTime(element, now, changes)) {
      return false;
    }
  }
  return true;
}

bool AutocompleteTable::GetFormValuesForElementName(
    const std::u16string& name,
    const std::u16string& prefix,
    int limit,
    std::vector<AutocompleteEntry>& entries) {
  sql::Statement s;
  SelectBuilder(db(), s, kAutocompleteTable,
                {kName, kValue, kDateCreated, kDateLastUsed},
                "WHERE name = ? AND value_lower LIKE ? "
                "ORDER BY count DESC LIMIT ?");
  s.BindString16(0, name);
  s.BindString16(1, base::i18n::ToLower(prefix) + u"%");
  s.BindInt(2, limit);

  entries.clear();
  while (s.Step()) {
    entries.emplace_back(
        AutocompleteKey(/*name=*/s.ColumnString16(0),
                        /*value=*/s.ColumnString16(1)),
        /*date_created=*/base::Time::FromTimeT(s.ColumnInt64(2)),
        /*date_last_used=*/base::Time::FromTimeT(s.ColumnInt64(3)));
  }

  return s.Succeeded();
}

bool AutocompleteTable::RemoveFormElementsAddedBetween(
    base::Time delete_begin,
    base::Time delete_end,
    std::vector<AutocompleteChange>& changes) {
  const time_t delete_begin_time_t = delete_begin.ToTimeT();
  const time_t delete_end_time_t = GetEndTime(delete_end);

  // Query for the name, value, count, and access dates of all form elements
  // that were used between the given times.
  sql::Statement s;
  SelectBuilder(db(), s, kAutocompleteTable,
                {kName, kValue, kCount, kDateCreated, kDateLastUsed},
                "WHERE (date_created >= ? AND date_created < ?) OR "
                "      (date_last_used >= ? AND date_last_used < ?)");
  s.BindInt64(0, delete_begin_time_t);
  s.BindInt64(1, delete_end_time_t);
  s.BindInt64(2, delete_begin_time_t);
  s.BindInt64(3, delete_end_time_t);

  std::vector<AutocompleteUpdate> updates;
  std::vector<AutocompleteChange> tentative_changes;
  while (s.Step()) {
    std::u16string name = s.ColumnString16(0);
    std::u16string value = s.ColumnString16(1);
    int count = s.ColumnInt(2);
    time_t date_created_time_t = s.ColumnInt64(3);
    time_t date_last_used_time_t = s.ColumnInt64(4);

    // If *all* uses of the element were between |delete_begin| and
    // |delete_end|, then delete the element.  Otherwise, update the use
    // timestamps and use count.
    AutocompleteChange::Type change_type;
    if (date_created_time_t >= delete_begin_time_t &&
        date_last_used_time_t < delete_end_time_t) {
      change_type = AutocompleteChange::REMOVE;
    } else {
      change_type = AutocompleteChange::UPDATE;

      // For all updated elements, set either date_created or date_last_used so
      // that the range [date_created, date_last_used] no longer overlaps with
      // [delete_begin, delete_end). Update the count by interpolating.
      // Precisely, compute the average amount of time between increments to the
      // count in the original range [date_created, date_last_used]:
      //   avg_delta = (date_last_used_orig - date_created_orig) / (count - 1)
      // The count can be expressed as
      //   count = 1 + (date_last_used - date_created) / avg_delta
      // Hence, update the count to
      //   count_new = 1 + (date_last_used_new - date_created_new) / avg_delta
      //             = 1 + ((count - 1) *
      //                    (date_last_used_new - date_created_new) /
      //                    (date_last_used_orig - date_created_orig))
      // Interpolating might not give a result that completely accurately
      // reflects the user's history, but it's the best that can be done given
      // the information in the database.
      AutocompleteUpdate updated_entry;
      updated_entry.name = name;
      updated_entry.value = value;
      updated_entry.date_created = date_created_time_t < delete_begin_time_t
                                       ? date_created_time_t
                                       : delete_end_time_t;
      updated_entry.date_last_used = date_last_used_time_t >= delete_end_time_t
                                         ? date_last_used_time_t
                                         : delete_begin_time_t - 1;
      updated_entry.count =
          1 + base::ClampRound(
                  1.0 * (count - 1) *
                  (updated_entry.date_last_used - updated_entry.date_created) /
                  (date_last_used_time_t - date_created_time_t));
      updates.push_back(updated_entry);
    }

    tentative_changes.emplace_back(change_type, AutocompleteKey(name, value));
  }
  if (!s.Succeeded()) {
    return false;
  }

  // As a single transaction, remove or update the elements appropriately.
  sql::Statement s_delete;
  DeleteBuilder(db(), s_delete, kAutocompleteTable,
                "date_created >= ? AND date_last_used < ?");
  s_delete.BindInt64(0, delete_begin_time_t);
  s_delete.BindInt64(1, delete_end_time_t);
  sql::Transaction transaction(db());
  if (!transaction.Begin()) {
    return false;
  }
  if (!s_delete.Run()) {
    return false;
  }
  for (const auto& update : updates) {
    sql::Statement s_update;
    UpdateBuilder(db(), s_update, kAutocompleteTable,
                  {kDateCreated, kDateLastUsed, kCount},
                  "name = ? AND value = ?");
    s_update.BindInt64(0, update.date_created);
    s_update.BindInt64(1, update.date_last_used);
    s_update.BindInt(2, update.count);
    s_update.BindString16(3, update.name);
    s_update.BindString16(4, update.value);
    if (!s_update.Run()) {
      return false;
    }
  }
  if (!transaction.Commit()) {
    return false;
  }

  changes = std::move(tentative_changes);
  return true;
}

bool AutocompleteTable::RemoveExpiredFormElements(
    std::vector<AutocompleteChange>& changes) {
  const auto change_type = AutocompleteChange::EXPIRE;

  base::Time expiration_time =
      AutofillClock::Now() - kAutocompleteRetentionPolicyPeriod;

  // Query for the name and value of all form elements that were last used
  // before the |expiration_time|.
  sql::Statement select_for_delete;
  SelectBuilder(db(), select_for_delete, kAutocompleteTable, {kName, kValue},
                "WHERE date_last_used < ?");
  select_for_delete.BindInt64(0, expiration_time.ToTimeT());
  std::vector<AutocompleteChange> tentative_changes;
  while (select_for_delete.Step()) {
    std::u16string name = select_for_delete.ColumnString16(0);
    std::u16string value = select_for_delete.ColumnString16(1);
    tentative_changes.emplace_back(change_type, AutocompleteKey(name, value));
  }

  if (!select_for_delete.Succeeded()) {
    return false;
  }

  sql::Statement delete_data_statement;
  DeleteBuilder(db(), delete_data_statement, kAutocompleteTable,
                "date_last_used < ?");
  delete_data_statement.BindInt64(0, expiration_time.ToTimeT());
  if (!delete_data_statement.Run()) {
    return false;
  }

  changes = std::move(tentative_changes);
  return true;
}

bool AutocompleteTable::RemoveFormElement(const std::u16string& name,
                                          const std::u16string& value) {
  sql::Statement s;
  DeleteBuilder(db(), s, kAutocompleteTable, "name = ? AND value= ?");
  s.BindString16(0, name);
  s.BindString16(1, value);
  return s.Run();
}

int AutocompleteTable::GetCountOfValuesContainedBetween(base::Time begin,
                                                        base::Time end) {
  const time_t begin_time_t = begin.ToTimeT();
  const time_t end_time_t = GetEndTime(end);

  sql::Statement s(db()->GetUniqueStatement(
      "SELECT COUNT(DISTINCT(value1)) FROM ( "
      "  SELECT value AS value1 FROM autofill "
      "  WHERE NOT EXISTS ( "
      "    SELECT value AS value2, date_created, date_last_used FROM autofill "
      "    WHERE value1 = value2 AND "
      "          (date_created < ? OR date_last_used >= ?)))"));
  s.BindInt64(0, begin_time_t);
  s.BindInt64(1, end_time_t);

  if (!s.Step()) {
    // This might happen in case of I/O errors. See crbug.com/332263206.
    return 0;
  }
  return s.ColumnInt(0);
}

bool AutocompleteTable::GetAllAutocompleteEntries(
    std::vector<AutocompleteEntry>* entries) {
  sql::Statement s;
  SelectBuilder(db(), s, kAutocompleteTable,
                {kName, kValue, kDateCreated, kDateLastUsed});

  while (s.Step()) {
    std::u16string name = s.ColumnString16(0);
    std::u16string value = s.ColumnString16(1);
    base::Time date_created = base::Time::FromTimeT(s.ColumnInt64(2));
    base::Time date_last_used = base::Time::FromTimeT(s.ColumnInt64(3));
    entries->emplace_back(AutocompleteKey(name, value), date_created,
                          date_last_used);
  }

  return s.Succeeded();
}

std::optional<AutocompleteEntry> AutocompleteTable::GetAutocompleteEntry(
    const std::u16string& name,
    const std::u16string& value) {
  sql::Statement s;
  SelectBuilder(db(), s, kAutocompleteTable, {kDateCreated, kDateLastUsed},
                "WHERE name = ? AND value = ?");
  s.BindString16(0, name);
  s.BindString16(1, value);
  if (!s.Step()) {
    return std::nullopt;
  }
  AutocompleteEntry entry({name, value},
                          base::Time::FromTimeT(s.ColumnInt64(0)),
                          base::Time::FromTimeT(s.ColumnInt64(1)));
  DCHECK(!s.Step());
  return entry;
}

bool AutocompleteTable::UpdateAutocompleteEntries(
    const std::vector<AutocompleteEntry>& entries) {
  if (entries.empty()) {
    return true;
  }

  // Remove all existing entries.
  for (const auto& entry : entries) {
    sql::Statement s;
    DeleteBuilder(db(), s, kAutocompleteTable, "name = ? AND value = ?");
    s.BindString16(0, entry.key().name());
    s.BindString16(1, entry.key().value());
    if (!s.Run()) {
      return false;
    }
  }

  // Insert all the supplied autofill entries.
  for (const auto& entry : entries) {
    if (!InsertAutocompleteEntry(entry)) {
      return false;
    }
  }

  return true;
}

bool AutocompleteTable::AddFormFieldValueTime(
    const FormFieldData& element,
    base::Time time,
    std::vector<AutocompleteChange>* changes) {
  if (!db()->is_open()) {
    return false;
  }
  AutocompleteChange::Type change_type;
  if (GetAutocompleteEntry(element.name(), element.value()).has_value()) {
    change_type = AutocompleteChange::UPDATE;
    sql::Statement s(db()->GetUniqueStatement(
        "UPDATE autofill SET date_last_used = ?, count = count + 1 "
        "WHERE name = ? AND value = ?"));
    s.BindInt64(0, time.ToTimeT());
    s.BindString16(1, element.name());
    s.BindString16(2, element.value());
    if (!s.Run()) {
      return false;
    }
  } else {
    change_type = AutocompleteChange::ADD;
    if (!InsertAutocompleteEntry({{element.name(), element.value()},
                                  /*date_created=*/time,
                                  /*date_last_used=*/time})) {
      return false;
    }
  }
  changes->emplace_back(change_type,
                        AutocompleteKey(element.name(), element.value()));
  return true;
}

bool AutocompleteTable::InsertAutocompleteEntry(
    const AutocompleteEntry& entry) {
  sql::Statement s;
  InsertBuilder(
      db(), s, kAutocompleteTable,
      {kName, kValue, kValueLower, kDateCreated, kDateLastUsed, kCount});
  s.BindString16(0, entry.key().name());
  s.BindString16(1, entry.key().value());
  s.BindString16(2, base::i18n::ToLower(entry.key().value()));
  s.BindInt64(3, entry.date_created().ToTimeT());
  s.BindInt64(4, entry.date_last_used().ToTimeT());
  // TODO(isherman): The counts column is currently synced implicitly as the
  // number of timestamps.  Sync the value explicitly instead, since the DB
  // now only saves the first and last timestamp, which makes counting
  // timestamps completely meaningless as a way to track frequency of usage.
  s.BindInt(5, entry.date_last_used() == entry.date_created() ? 1 : 2);
  return s.Run();
}

bool AutocompleteTable::InitMainTable() {
  if (!db()->DoesTableExist(kAutocompleteTable)) {
    return CreateTable(db(), kAutocompleteTable,
                       {{kName, "VARCHAR"},
                        {kValue, "VARCHAR"},
                        {kValueLower, "VARCHAR"},
                        {kDateCreated, "INTEGER DEFAULT 0"},
                        {kDateLastUsed, "INTEGER DEFAULT 0"},
                        {kCount, "INTEGER DEFAULT 1"}},
                       {kName, kValue}) &&
           CreateIndex(db(), kAutocompleteTable, {kName}) &&
           CreateIndex(db(), kAutocompleteTable, {kName, kValueLower});
  }
  return true;
}

}  // namespace autofill
