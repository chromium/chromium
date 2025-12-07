// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/346507576): After initial testing period this file should
// replace existing autocomplete_table.cc. Label sensitive prefix should
// be dropped everywhere.

#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table_label_sensitive.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/check_deref.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/unicodestring.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry_label_sensitive.h"
#include "components/autofill/core/browser/webdata/autofill_table_utils.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"
#include "ui/base/l10n/l10n_util.h"
#include "third_party/icu/source/common/unicode/uchar.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"

namespace autofill {

namespace {

constexpr std::string_view kAutocompleteTableLabelSensitive = "autocomplete";
constexpr std::string_view kName = "name";
constexpr std::string_view kLabel = "label";
constexpr std::string_view kLabelNormalized = "label_normalized";
constexpr std::string_view kValue = "value";
constexpr std::string_view kValueLower = "value_lower";
constexpr std::string_view kDateCreated = "date_created";
constexpr std::string_view kDateLastUsed = "date_last_used";
constexpr std::string_view kCount = "count";

// Helper struct for
// AutocompleteTableLabelSensitive::RemoveFormElementsAddedBetween(). Contains
// all the necessary fields to update a row in the 'autocomplete' table.
struct AutocompleteUpdate {
  std::u16string name;
  std::u16string label;
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

// Normalizes a given label string according to the following rules.
//
//   1. Trims leading and trailing non-alphanumeric characters.
//   2. Converts to lowercase.
//   3. Caps length at 50 characters.
//   4. ICU-aware: correctly handles characters from scripts such as emojis,
//   Kanji, Hangul, Greek, Cyrillic, etc.
std::u16string NormalizeLabel(std::u16string_view label_view) {
  icu::UnicodeString uni_label(label_view.data(), label_view.length());

  int32_t start = 0;
  int32_t end = uni_label.length() - 1;

  while (start <= end && !(u_isalnum(uni_label[start]))) {
    start++;
  }
  while (end >= start && !(u_isalnum(uni_label[end]))) {
    end--;
  }

  if (start > end) {  // Label became empty after trimming
    return u"";
  }

  int32_t truncated_label_length = end - start + 1;
  if (truncated_label_length > 50) {
    truncated_label_length = 50;
  }

  uni_label = icu::UnicodeString(uni_label, start, truncated_label_length);
  uni_label.toLower();
  return base::i18n::UnicodeStringToString16(uni_label);
}

}  // namespace

AutocompleteSearchResultLabelSensitive::AutocompleteSearchResultLabelSensitive(
    std::u16string value,
    const MatchingType matching_type,
    const int count)
    : value_(std::move(value)), matching_type_(matching_type), count_(count) {}

AutocompleteSearchResultLabelSensitive::
    ~AutocompleteSearchResultLabelSensitive() = default;

AutocompleteTableLabelSensitive::AutocompleteTableLabelSensitive() = default;

AutocompleteTableLabelSensitive::~AutocompleteTableLabelSensitive() = default;

// static
AutocompleteTableLabelSensitive*
AutocompleteTableLabelSensitive::FromWebDatabase(WebDatabase* db) {
  return static_cast<AutocompleteTableLabelSensitive*>(
      CHECK_DEREF(db).GetTable(GetKey()));
}

WebDatabaseTable::TypeKey AutocompleteTableLabelSensitive::GetTypeKey() const {
  return GetKey();
}

bool AutocompleteTableLabelSensitive::CreateTablesIfNecessary() {
  return InitMainTable();
}

bool AutocompleteTableLabelSensitive::MigrateToVersion(
    int version,
    bool* update_compatible_version) {
  if (!db()->is_open()) {
    return false;
  }
  // Add migration logic here.
  return true;
}

bool AutocompleteTableLabelSensitive::AddFormFieldValues(
    const std::vector<FormFieldData>& elements) {
  const base::Time now = AutofillClock::Now();
  const size_t kMaximumFields = 256;

  std::set<std::u16string> seen_names;
  std::set<std::u16string> seen_labels;

  for (const FormFieldData& element : elements) {
    // Add at most kMaximumFields
    if (seen_names.size() >= kMaximumFields) {
      break;
    }

    // If a field has a name OR labels that we've already seen in the current
    // form, skip it.
    if (seen_names.contains(element.name()) ||
        seen_labels.contains(element.label())) {
      continue;
    }

    seen_names.insert(element.name());
    seen_labels.insert(element.label());

    if (!AddFormFieldValueTime(element, now)) {
      return false;
    }
  }
  return true;
}

bool AutocompleteTableLabelSensitive::GetFormValuesForElementNameAndLabel(
    std::u16string_view name,
    std::u16string_view label,
    std::u16string_view prefix,
    size_t limit,
    std::vector<AutocompleteSearchResultLabelSensitive>& entries) {
  // Matching type in this query is matching type enum value from
  // autofill::MatchingType enum.
  sql::Statement s(db()->GetUniqueStatement(
      "WITH inputs AS (SELECT ? AS _name, ? AS _label, ? AS _prefix)"
      "SELECT "
      "  value, "
      "  CASE "
      // MatchingType::kNameAndLabel (= 3)
      "    WHEN name = inputs._name AND (label != '' AND label_normalized = "
      "inputs._label) THEN 3 "
      // MatchingType::kName (= 2)
      "    WHEN name = inputs._name THEN 2 "
      // MatchingType::kLabel (= 1)
      "    WHEN (label != '' AND label_normalized = inputs._label) THEN 1 "
      // MatchingType::kUnknown (= 0), should never happen.
      "    ELSE 0 "
      "  END AS matching_type, "
      "  MAX(count) AS max_count "
      "FROM autocomplete, inputs "
      "WHERE (name = inputs._name OR (label != '' AND label_normalized = "
      "inputs._label)) AND value_lower LIKE inputs._prefix "
      "GROUP BY value, matching_type "
      "ORDER BY "
      "  CASE "
      "    WHEN matching_type = 3 THEN 2 "
      "    ELSE 1 "
      "  END DESC, "
      "  max_count DESC "
      "LIMIT ?"));
  s.BindString16(0, name);
  s.BindString16(1, NormalizeLabel(label));
  s.BindString16(2, base::i18n::ToLower(prefix) + u"%");

  // Later in this function we remove duplicates. Potentially every matching
  // type can return entries with identical values. So to make sure we will
  // return enough entries after deduplication to satisfy the limit we need to
  // return kPossibleMatchingTypesCount times more entries.
  constexpr int kPossibleMatchingTypesCount = 3;
  s.BindInt64(3, limit * kPossibleMatchingTypesCount);

  entries.clear();
  absl::flat_hash_set<AutocompleteSearchResultLabelSensitive> seen_results;
  while (s.Step() && entries.size() < static_cast<size_t>(limit)) {
    AutocompleteSearchResultLabelSensitive current_result(
        /*value=*/s.ColumnString16(0),
        /*matching_type=*/ToSafeMatchingType(s.ColumnInt(1)),
        /*count=*/s.ColumnInt(2));
    if (seen_results.insert(current_result).second) {
      entries.push_back(current_result);
    }
  }

  return s.Succeeded();
}

bool AutocompleteTableLabelSensitive::RemoveFormElementsAddedBetween(
    base::Time delete_begin,
    base::Time delete_end) {
  const time_t delete_begin_time_t = delete_begin.ToTimeT();
  const time_t delete_end_time_t = GetEndTime(delete_end);

  // Query for the name, label, value, count, and access dates of all form
  // elements that were used between the given times.
  sql::Statement s;
  SelectBuilder(db(), s, kAutocompleteTableLabelSensitive,
                {kName, kLabel, kValue, kCount, kDateCreated, kDateLastUsed},
                "WHERE (date_created >= ? AND date_created < ?) OR "
                "      (date_last_used >= ? AND date_last_used < ?)");
  s.BindInt64(0, delete_begin_time_t);
  s.BindInt64(1, delete_end_time_t);
  s.BindInt64(2, delete_begin_time_t);
  s.BindInt64(3, delete_end_time_t);

  std::vector<AutocompleteUpdate> updates;
  while (s.Step()) {
    std::u16string name = s.ColumnString16(0);
    std::u16string label = s.ColumnString16(1);
    std::u16string value = s.ColumnString16(2);
    int count = s.ColumnInt(3);
    time_t date_created_time_t = s.ColumnInt64(4);
    time_t date_last_used_time_t = s.ColumnInt64(5);

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
    updated_entry.label = label;
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
  if (!s.Succeeded()) {
    return false;
  }

  // As a single transaction, remove or update the elements appropriately.
  sql::Statement s_delete;
  DeleteBuilder(db(), s_delete, kAutocompleteTableLabelSensitive,
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
    UpdateBuilder(db(), s_update, kAutocompleteTableLabelSensitive,
                  {kDateCreated, kDateLastUsed, kCount},
                  "name = ? AND label = ? AND value = ?");
    s_update.BindInt64(0, update.date_created);
    s_update.BindInt64(1, update.date_last_used);
    s_update.BindInt(2, update.count);
    s_update.BindString16(3, update.name);
    s_update.BindString16(4, update.label);
    s_update.BindString16(5, update.value);
    if (!s_update.Run()) {
      return false;
    }
  }
  if (!transaction.Commit()) {
    return false;
  }

  return true;
}

bool AutocompleteTableLabelSensitive::RemoveExpiredFormElements() {
  base::Time expiration_time =
      AutofillClock::Now() - kAutocompleteRetentionPolicyPeriod;

  sql::Statement delete_data_statement;
  DeleteBuilder(db(), delete_data_statement, kAutocompleteTableLabelSensitive,
                "date_last_used < ?");
  delete_data_statement.BindInt64(0, expiration_time.ToTimeT());
  return delete_data_statement.Run();
}

bool AutocompleteTableLabelSensitive::RemoveFormElement(
    std::u16string_view name,
    std::u16string_view label,
    std::u16string_view value) {
  sql::Statement s;
  DeleteBuilder(db(), s, kAutocompleteTableLabelSensitive,
                "(name = ? OR (label_normalized = ? AND "
                "label_normalized != '')) AND value = ?");
  s.BindString16(0, name);
  s.BindString16(1, NormalizeLabel(label));
  s.BindString16(2, value);
  return s.Run();
}

int AutocompleteTableLabelSensitive::GetCountOfValuesContainedBetween(
    base::Time begin,
    base::Time end) {
  const time_t begin_time_t = begin.ToTimeT();
  const time_t end_time_t = GetEndTime(end);

  sql::Statement s(db()->GetUniqueStatement(
      "SELECT COUNT(DISTINCT(value1)) FROM ( "
      "  SELECT value AS value1 FROM autocomplete "
      "  WHERE NOT EXISTS ( "
      "    SELECT value AS value2, date_created, date_last_used FROM "
      "autocomplete "
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

bool AutocompleteTableLabelSensitive::GetAllAutocompleteEntries(
    std::vector<AutocompleteEntryLabelSensitive>* entries) {
  sql::Statement s;
  SelectBuilder(db(), s, kAutocompleteTableLabelSensitive,
                {kName, kLabel, kValue, kDateCreated, kDateLastUsed});

  while (s.Step()) {
    std::u16string name = s.ColumnString16(0);
    std::u16string label = s.ColumnString16(1);
    std::u16string value = s.ColumnString16(2);
    base::Time date_created = base::Time::FromTimeT(s.ColumnInt64(3));
    base::Time date_last_used = base::Time::FromTimeT(s.ColumnInt64(4));
    entries->emplace_back(AutocompleteKeyLabelSensitive(name, label, value),
                          date_created, date_last_used);
  }

  return s.Succeeded();
}

std::optional<AutocompleteEntryLabelSensitive>
AutocompleteTableLabelSensitive::GetAutocompleteEntryLabelSensitive(
    std::u16string_view name,
    std::u16string_view label,
    std::u16string_view value) {
  sql::Statement s;
  SelectBuilder(db(), s, kAutocompleteTableLabelSensitive,
                {kDateCreated, kDateLastUsed},
                "WHERE name = ? AND label = ? AND value = ?");
  s.BindString16(0, name);
  s.BindString16(1, label);
  s.BindString16(2, value);
  if (!s.Step()) {
    return std::nullopt;
  }
  AutocompleteEntryLabelSensitive entry(
      {std::u16string(name), std::u16string(label), std::u16string(value)},
      base::Time::FromTimeT(s.ColumnInt64(0)),
      base::Time::FromTimeT(s.ColumnInt64(1)));
  return entry;
}

bool AutocompleteTableLabelSensitive::AddFormFieldValueTime(
    const FormFieldData& element,
    base::Time time) {
  if (!db()->is_open()) {
    return false;
  }

  // The following UPDATE and INSERT statements do not require a transaction.
  // If the INSERT statement fails, result of the UPDATE statement is still
  // valid.

  // Always try to update counts of all entries that would contribute to correct
  // suggestion.
  sql::Statement update_statement(db()->GetUniqueStatement(
      "UPDATE autocomplete SET date_last_used = ?, count = count + 1 "
      "WHERE (name = ? OR (label_normalized = ? AND label_normalized != '')) "
      "AND value = ?"));
  update_statement.BindInt64(0, time.ToTimeT());
  update_statement.BindString16(1, element.name());
  update_statement.BindString16(2, NormalizeLabel(element.label()));
  update_statement.BindString16(3, element.value());
  if (!update_statement.Run()) {
    return false;
  }

  // If the entry doesn't exist, insert it.
  if (!GetAutocompleteEntryLabelSensitive(element.name(), element.label(),
                                          element.value())
           .has_value()) {
    sql::Statement create_statement;
    InsertBuilder(db(), create_statement, kAutocompleteTableLabelSensitive,
                  {kName, kLabel, kLabelNormalized, kValue, kValueLower,
                   kDateCreated, kDateLastUsed, kCount});
    create_statement.BindString16(0, element.name());
    create_statement.BindString16(1, element.label());
    create_statement.BindString16(2, NormalizeLabel(element.label()));
    create_statement.BindString16(3, element.value());
    create_statement.BindString16(4, base::i18n::ToLower(element.value()));
    create_statement.BindInt64(5, time.ToTimeT());
    create_statement.BindInt64(6, time.ToTimeT());
    create_statement.BindInt(7, 1);
    return create_statement.Run();
  }
  return true;
}

bool AutocompleteTableLabelSensitive::InitMainTable() {
  if (!db()->DoesTableExist(kAutocompleteTableLabelSensitive)) {
    return CreateTable(db(), kAutocompleteTableLabelSensitive,
                       {{kName, "VARCHAR"},
                        {kLabel, "VARCHAR"},
                        {kLabelNormalized, "VARCHAR"},
                        {kValue, "VARCHAR"},
                        {kValueLower, "VARCHAR"},
                        {kDateCreated, "INTEGER DEFAULT 0"},
                        {kDateLastUsed, "INTEGER DEFAULT 0"},
                        {kCount, "INTEGER DEFAULT 1"}},
                       {kName, kLabel, kValue})
           // Used by query in GetFormValuesForElementNameAndLabel
           && CreateIndex(db(), kAutocompleteTableLabelSensitive,
                          {kName, kLabelNormalized, kValueLower})

           // Used by query in GetFormValuesForElementNameAndLabel
           && CreateIndex(db(), kAutocompleteTableLabelSensitive,
                          {kLabelNormalized, kValueLower})

           // Used by query in RemoveFormElement,
           // GetCountOfValuesContainedBetween, AddFormFieldValueTime
           && CreateIndex(db(), kAutocompleteTableLabelSensitive,
                          {kValue, kDateLastUsed})

           // Used by query in GetAutocompleteEntryLabelSensitive
           && CreateIndex(db(), kAutocompleteTableLabelSensitive,
                          {kName, kLabel, kValue});
  }
  return true;
}

}  // namespace autofill
