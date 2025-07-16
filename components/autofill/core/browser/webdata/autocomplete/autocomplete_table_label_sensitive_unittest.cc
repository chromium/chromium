// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/346507576): After launch, this file should replace existing
// autocomplete_table_unittest.cc. The "LabelSensitive" suffix should be dropped
// everywhere.

#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table_label_sensitive.h"

#include <string>
#include <utility>

#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_change_label_sensitive.h"
#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_entry_label_sensitive.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

namespace {

using ::autofill::test::CreateTestFormField;
using AutocompleteEntryLabelSensitiveSet =
    std::set<AutocompleteEntryLabelSensitive,
             bool (*)(const AutocompleteEntryLabelSensitive&,
                      const AutocompleteEntryLabelSensitive&)>;
using ::base::Time;
using ::testing::ElementsAre;
using ::testing::Property;
using ::testing::UnorderedElementsAre;

bool CompareAutocompleteEntries(const AutocompleteEntryLabelSensitive& a,
                                const AutocompleteEntryLabelSensitive& b) {
  return std::tie(a.key().name(), a.key().label(), a.key().value(),
                  a.date_created(), a.date_last_used()) <
         std::tie(b.key().name(), b.key().label(), b.key().value(),
                  b.date_created(), b.date_last_used());
}

AutocompleteEntryLabelSensitive MakeAutocompleteEntryLabelSensitive(
    const std::u16string& name,
    const std::u16string& label,
    const std::u16string& value,
    time_t date_created,
    time_t date_last_used) {
  if (date_last_used < 0) {
    date_last_used = date_created;
  }
  return AutocompleteEntryLabelSensitive(
      AutocompleteKeyLabelSensitive(name, label, value),
      Time::FromTimeT(date_created), Time::FromTimeT(date_last_used));
}

// Checks |actual| and |expected| contain the same elements.
[[nodiscard]] testing::AssertionResult
CompareAutocompleteEntryLabelSensitiveSets(
    const AutocompleteEntryLabelSensitiveSet& actual,
    const AutocompleteEntryLabelSensitiveSet& expected) {
  if (actual.size() != expected.size()) {
    return testing::AssertionFailure() << "Mismatching sizes: " << actual.size()
                                       << " vs. " << expected.size();
  }
  size_t count = 0;
  for (const auto& it : actual) {
    count += expected.count(it);
  }
  return (actual.size() == count) ? testing::AssertionSuccess()
                                  : testing::AssertionFailure()
                                        << "actual.size() = " << actual.size()
                                        << " but count = " << count;
}

int GetAutocompleteEntryLabelSensitiveCount(const std::u16string& name,
                                            const std::u16string& label,
                                            const std::u16string& value,
                                            WebDatabase* db) {
  sql::Statement s(db->GetSQLConnection()->GetUniqueStatement(
      "SELECT count FROM autocomplete WHERE name = ? AND label = ? AND value = "
      "?"));
  s.BindString16(0, name);
  s.BindString16(1, label);
  s.BindString16(2, value);
  if (!s.Step()) {
    return 0;
  }
  return s.ColumnInt(0);
}

auto EqualsSearchResult(std::u16string value, int count) {
  return AllOf(Property("AutocompleteSearchResultLabelSensitive::value",
                        &AutocompleteSearchResultLabelSensitive::value, value),
               Property("AutocompleteSearchResultLabelSensitive::count",
                        &AutocompleteSearchResultLabelSensitive::count, count));
}

class AutocompleteTableLabelSensitiveTest : public testing::Test {
 protected:
  const std::u16string kDefaultLabel = u"Your Name";
  const std::u16string kDefaultName = u"your_name";
  const std::u16string kDefaultValue = u"Superman";

  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");
    table_ = std::make_unique<AutocompleteTableLabelSensitive>();
    db_ = std::make_unique<WebDatabase>();
    db_->AddTable(table_.get());
    changes_.clear();
    ASSERT_EQ(db_->Init(file_), sql::INIT_OK);
  }

  void SetClock(base::Time target) {
    // When we compare last used dates, we fast forward the current time to a
    // fixed date that has no sub-second component. This is because creation and
    // last_used dates are serialized to seconds and sub-second components are
    // lost.
    base::Time rounded_target = base::Time::FromSecondsSinceUnixEpoch(
        target.InMillisecondsSinceUnixEpoch() / 1000);
    AdvanceClock(rounded_target - base::Time::Now());
    ASSERT_EQ(base::Time::Now().InMillisecondsSinceUnixEpoch() % 1000, 0);
  }

  void AdvanceClock(base::TimeDelta delta) {
    task_environment_.AdvanceClock(delta);
  }

  [[nodiscard]] FormFieldData CreateDefaultFieldWithValue(
      std::u16string_view value) {
    return test::CreateTestFormField(kDefaultLabel, kDefaultName, value,
                                     FormControlType::kInputText);
  }

  [[nodiscard]] FormFieldData CreateDefaultField() {
    return CreateDefaultFieldWithValue(kDefaultValue);
  }

  // Submits a vector of form fields to the table, returns true if successful.
  [[nodiscard]] bool SubmitFormFields(
      const std::vector<FormFieldData>& elements) {
    return table().AddFormFieldValues(elements, &changes_);
  }

  // Submits a single form field to the table, returns true if successful.
  [[nodiscard]] bool SubmitFormField(const FormFieldData& field) {
    return SubmitFormFields({field});
  }

  // Will return optional field on successful submission, or std::nullopt if
  // submission fails
  [[nodiscard]] std::optional<FormFieldData>
  CreateAndSubmitDefaultFieldWithValue(std::u16string_view value) {
    FormFieldData field = CreateDefaultFieldWithValue(value);
    if (!SubmitFormField(field)) {
      return std::nullopt;
    }
    return field;
  }

  // Will return optional field on successful submission, or std::nullopt if
  // submission fails
  [[nodiscard]] std::optional<FormFieldData> CreateAndSubmitDefaultField() {
    return CreateAndSubmitDefaultFieldWithValue(kDefaultValue);
  }

  WebDatabase& db() { return *db_; }
  AutocompleteTableLabelSensitive& table() { return *table_; }
  AutocompleteChangeLabelSensitiveList& changes() { return changes_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AutocompleteTableLabelSensitive> table_;
  std::unique_ptr<WebDatabase> db_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
  AutocompleteChangeLabelSensitiveList changes_;
};

// TODO(crbug.com/346507576): Add happy path tests

using AddFormFieldValuesTest = AutocompleteTableLabelSensitiveTest;

// Check that AddFormFieldValues correctly appends an ADD entry to change log
// that is passed as an output parameter to AddFormFieldValues if no
// autocomplete entry for the same key (field label + name + value) exists in
// the database, yet.
TEST_F(AddFormFieldValuesTest, InsertsNewEntry) {
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  EXPECT_THAT(changes(), ElementsAre(AutocompleteChangeLabelSensitive(
                             AutocompleteChangeLabelSensitive::ADD,
                             AutocompleteKeyLabelSensitive(
                                 kDefaultName, kDefaultLabel, kDefaultValue))));
}

// Check that AddFormFieldValues creates an UPDATE entry in the change log
// that is passed as an output parameter to AddFormFieldValues if an
// autocomplete entry with the same key (field label + name + value) exists in
// the database already.
TEST_F(AddFormFieldValuesTest, UpdatesExistingEntry) {
  FormFieldData field = CreateDefaultField();

  // Add new entry
  ASSERT_TRUE(SubmitFormField(field));
  changes().clear();

  // Update existing entry
  ASSERT_TRUE(SubmitFormField(field));

  EXPECT_THAT(changes(), ElementsAre(AutocompleteChangeLabelSensitive(
                             AutocompleteChangeLabelSensitive::UPDATE,
                             AutocompleteKeyLabelSensitive(
                                 kDefaultName, kDefaultLabel, kDefaultValue))));
}

// Check that AddFormFieldValues modifies the underlying database by inserting
// an entry and later incrementing the use count of the previously inserting
// entry.
TEST_F(AddFormFieldValuesTest, UpdatesExistingEntryCount) {
  FormFieldData field = CreateDefaultField();

  // Add new entry
  ASSERT_TRUE(SubmitFormField(field));

  // Update existing entry
  ASSERT_TRUE(SubmitFormField(field));

  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue, &db()),
            2);
}

// Check if AddFormFieldValues stores and queries the value of autocomplete
// entry in case-sensitive manner.
TEST_F(AddFormFieldValuesTest, StoresDataCaseSensitive) {
  ASSERT_TRUE(CreateAndSubmitDefaultFieldWithValue(u"Clark Kent").has_value());

  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    u"clark kent", &db()),
            0);
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    u"Clark Kent", &db()),
            1);
}

// Check if AddFormFieldValues stores empty and whitespace values without any
// changes.
TEST_F(AddFormFieldValuesTest, StoresEmptyValuesAsIs) {
  std::optional<FormFieldData> optional_empty_field =
      CreateAndSubmitDefaultFieldWithValue(u"");
  ASSERT_TRUE(optional_empty_field.has_value());
  std::optional<FormFieldData> optional_whitespace_field =
      CreateAndSubmitDefaultFieldWithValue(u"   ");
  ASSERT_TRUE(optional_whitespace_field.has_value());

  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(
                kDefaultName, kDefaultLabel,
                optional_empty_field.value().value(), &db()),
            1);
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(
                kDefaultName, kDefaultLabel,
                optional_whitespace_field.value().value(), &db()),
            1);
}

// Check if AddFormFieldValues stores null terminated values as is in the
// database.
TEST_F(AddFormFieldValuesTest, InsertsNullTerminatedValuesAsIs) {
  const std::u16string kValueNullTerminated(kDefaultValue,
                                            std::size(kDefaultValue));
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());
  ASSERT_TRUE(
      CreateAndSubmitDefaultFieldWithValue(kValueNullTerminated).has_value());

  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue, &db()),
            1);
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(
                kDefaultName, kDefaultLabel, kValueNullTerminated, &db()),
            1);
}

// Check if additional form fields with identical name/label signature are
// ignored by AddFormFieldValues.
TEST_F(AddFormFieldValuesTest, InsertsOnlyOneEntryPerUniqueFieldName) {
  // Add multiple values for "firstname" and "lastname" names.  Test that only
  // first value of each gets added. Related to security issue:
  // http://crbug.com/51727.

  std::vector<FormFieldData> elements = {
      CreateTestFormField(u"First Name", u"first_name", u"Joe",
                          FormControlType::kInputText),
      CreateTestFormField(u"First Name", u"first_name", u"Jane",
                          FormControlType::kInputText),
      CreateTestFormField(u"Last Name", u"last_name", u"Smith",
                          FormControlType::kInputText),
      CreateTestFormField(u"Last Name", u"last_name", u"Jones",
                          FormControlType::kInputText)};

  ASSERT_TRUE(SubmitFormFields(elements));

  EXPECT_THAT(changes(),
              ElementsAre(AutocompleteChangeLabelSensitive(
                              AutocompleteChangeLabelSensitive::ADD,
                              AutocompleteKeyLabelSensitive(
                                  u"first_name", u"First Name", u"Joe")),
                          AutocompleteChangeLabelSensitive(
                              AutocompleteChangeLabelSensitive::ADD,
                              AutocompleteKeyLabelSensitive(
                                  u"last_name", u"Last Name", u"Smith"))));

  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(
                u"first_name", u"First Name", u"Joe", &db()),
            1);
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(u"last_name", u"Last Name",
                                                    u"Smith", &db()),
            1);
}

using GetFormValuesForElementNameAndLabelTest =
    AutocompleteTableLabelSensitiveTest;

// GetFormValuesForElementNameAndLabel returns the correct set of suggestions
// for a given name/label and empty value prefix.
TEST_F(GetFormValuesForElementNameAndLabelTest, ReturnsSuggestion) {
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  std::vector<AutocompleteSearchResultLabelSensitive> entries;

  ASSERT_TRUE(table().GetFormValuesForElementNameAndLabel(
      kDefaultName, kDefaultLabel, /*prefix=*/std::u16string(), /*limit=*/10,
      entries));

  EXPECT_THAT(entries, ElementsAre(EqualsSearchResult(kDefaultValue, 1)));
}

// When asked for 1 result, GetFormValuesForElementNameAndLabel returns the top
// suggestion for given name/label and empty value prefix.
TEST_F(GetFormValuesForElementNameAndLabelTest, ReturnsTopSuggestion) {
  // Add 3 entries
  std::optional<FormFieldData> optional_field = CreateAndSubmitDefaultField();
  ASSERT_TRUE(CreateAndSubmitDefaultFieldWithValue(u"Clark Kent").has_value());
  ASSERT_TRUE(optional_field.has_value());
  ASSERT_TRUE(
      CreateAndSubmitDefaultFieldWithValue(u"Clark Sutter").has_value());

  // Reinforce the first entry which should make it the top suggestion
  ASSERT_TRUE(SubmitFormField(optional_field.value()));

  std::vector<AutocompleteSearchResultLabelSensitive> entries;
  ASSERT_TRUE(table().GetFormValuesForElementNameAndLabel(
      kDefaultName, kDefaultLabel, /*prefix=*/std::u16string(), /*limit=*/1,
      entries));

  EXPECT_THAT(entries, ElementsAre(EqualsSearchResult(kDefaultValue, 2)));
}

// When asked for multiple results, GetFormValuesForElementNameAndLabel returns
// them in correct order based on frequency of previous submissions.
TEST_F(GetFormValuesForElementNameAndLabelTest,
       ReturnsMultipleSuggestionsInCorrectOrder) {
  FormFieldData field1 = CreateDefaultFieldWithValue(u"Clark Kent");
  FormFieldData field2 = CreateDefaultFieldWithValue(u"Clark Sutter");
  // Add 2 entries, one with count 1, the other with count 2
  ASSERT_TRUE(SubmitFormField(field1));
  ASSERT_TRUE(SubmitFormField(field2));
  ASSERT_TRUE(SubmitFormField(field2));

  std::vector<AutocompleteSearchResultLabelSensitive> entries;
  ASSERT_TRUE(table().GetFormValuesForElementNameAndLabel(
      kDefaultName, kDefaultLabel, /*prefix=*/std::u16string(), /*limit=*/10,
      entries));

  EXPECT_THAT(entries, ElementsAre(EqualsSearchResult(field2.value(), 2),
                                   EqualsSearchResult(field1.value(), 1)));
}

// GetFormValuesForElementNameAndLabelTest should match the value prefix
// case-insensitively.
TEST_F(GetFormValuesForElementNameAndLabelTest, MatchesPrefixCaseInsensitive) {
  std::optional<FormFieldData> optional_field =
      CreateAndSubmitDefaultFieldWithValue(u"SUPERMAN");
  ASSERT_TRUE(optional_field.has_value());

  std::vector<AutocompleteSearchResultLabelSensitive> entries;
  ASSERT_TRUE(table().GetFormValuesForElementNameAndLabel(
      optional_field.value().name(), optional_field.value().label(),
      /*prefix=*/u"superman",
      /*limit=*/1, entries));

  EXPECT_THAT(entries, ElementsAre(EqualsSearchResult(
                           optional_field.value().value(), 1)));
}

// GetFormValuesForElementNameAndLabelTest should return the correct set of
// suggestions when the provided value prefix narrows down the results.
TEST_F(GetFormValuesForElementNameAndLabelTest, PrefixNarrowsDownResults) {
  std::optional<FormFieldData> optional_field1 =
      CreateAndSubmitDefaultFieldWithValue(u"Clark Kent");
  ASSERT_TRUE(optional_field1.has_value());
  std::optional<FormFieldData> optional_field2 =
      CreateAndSubmitDefaultFieldWithValue(u"Clark Sutter");
  ASSERT_TRUE(optional_field2.has_value());

  std::vector<AutocompleteSearchResultLabelSensitive> entries;
  ASSERT_TRUE(table().GetFormValuesForElementNameAndLabel(
      kDefaultName, kDefaultLabel, /*prefix=*/u"clark ", /*limit=*/10,
      entries));

  std::vector<AutocompleteSearchResultLabelSensitive> entries_narrowed_down;
  ASSERT_TRUE(table().GetFormValuesForElementNameAndLabel(
      kDefaultName, kDefaultLabel, /*prefix=*/u"clark k", /*limit=*/10,
      entries_narrowed_down));

  EXPECT_THAT(entries,
              UnorderedElementsAre(
                  EqualsSearchResult(optional_field1.value().value(), 1),
                  EqualsSearchResult(optional_field2.value().value(), 1)));
  EXPECT_THAT(entries_narrowed_down, ElementsAre(EqualsSearchResult(
                                         optional_field1.value().value(), 1)));
}

// TODO(crbug.com/346507576): Refactor into multiple tests:
// 1. GetCountOfValuesContainedBetween_ReturnsCorrectCount
// 2. GetCountOfValuesContainedBetween_ReturnsZeroIfNothingIsInTheInterval
// 3. GetCountOfValuesContainedBetween_ReturnsValuesOnlyInTheInterval
// 4. GetCountOfValuesContainedBetween_ReturnsEverythingForUnlimitedInterval
TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_GetCountOfValuesContainedBetween) {
  AutocompleteChangeLabelSensitiveList changes;
  // This test makes time comparisons that are precise to a microsecond, but the
  // database uses the time_t format which is only precise to a second.
  // Make sure we use timestamps rounded to a second.
  const auto begin = base::Time::Now();

  struct Entry {
    const char16_t* name;
    const char16_t* label;
    const char16_t* value;
  } entries[] = {{u"alter_ego", u"Alter ego", u"Superman"},
                 {u"name", u"Name", u"Superman"},
                 {u"name", u"Name", u"Clark Kent"},
                 {u"name", u"Name", u"Superman"},
                 {u"name", u"Name", u"Clark Sutter"},
                 {u"name", u"Nomen", u"Clark Kent"}};

  for (Entry entry : entries) {
    autofill::FormFieldData field;
    field.set_name(entry.name);
    field.set_label(entry.label);
    field.set_value(entry.value);
    ASSERT_TRUE(table().AddFormFieldValues({field}, &changes));
    AdvanceClock(base::Seconds(1));
  }

  // While the entry "Alter ego" : "Superman" is entirely contained within
  // the first second, the value "Superman" itself appears in another entry,
  // so it is not contained.
  EXPECT_EQ(0, table().GetCountOfValuesContainedBetween(
                   begin, begin + base::Seconds(1)));

  // No values are entirely contained within the first three seconds either
  // (note that the second time constraint is exclusive).
  EXPECT_EQ(0, table().GetCountOfValuesContainedBetween(
                   begin, begin + base::Seconds(3)));

  // Only "Superman" is entirely contained within the first four seconds.
  EXPECT_EQ(1, table().GetCountOfValuesContainedBetween(
                   begin, begin + base::Seconds(4)));

  // "Clark Kent" and "Clark Sutter" are contained between the first
  // and seventh second.
  EXPECT_EQ(2, table().GetCountOfValuesContainedBetween(
                   begin + base::Seconds(1), begin + base::Seconds(7)));

  // Beginning from the third second, "Clark Kent" is not contained.
  EXPECT_EQ(1, table().GetCountOfValuesContainedBetween(
                   begin + base::Seconds(3), begin + base::Seconds(7)));

  // We have three distinct values total.
  EXPECT_EQ(3, table().GetCountOfValuesContainedBetween(
                   begin, begin + base::Seconds(7)));

  // And we should get the same result for unlimited time interval.
  EXPECT_EQ(3, table().GetCountOfValuesContainedBetween(Time(), Time::Max()));

  // The null time interval is also interpreted as unlimited.
  EXPECT_EQ(3, table().GetCountOfValuesContainedBetween(Time(), Time()));

  // An interval that does not fully contain any entries returns zero.
  EXPECT_EQ(0, table().GetCountOfValuesContainedBetween(
                   begin + base::Seconds(1), begin + base::Seconds(2)));

  // So does an interval which has no intersection with any entry.
  EXPECT_EQ(0, table().GetCountOfValuesContainedBetween(Time(), begin));
}

// TODO(crbug.com/346507576): Refactor into multiple tests:
// 1. RemoveFormElementsAddedBetween_RemovesWhatIsInRange
// 2. RemoveFormElementsAddedBetween_RemovesEverything
// 3. RemoveFormElementsAddedBetween_EdistWhatIsBeforeAndDuringTheRange
// 4. RemoveFormElementsAddedBetween_EdistWhatIsAfterAndDuringTheRange
// 5. RemoveFormElementsAddedBetween_DoesNotRemoveBeforeTheRange
// 6. RemoveFormElementsAddedBetween_DoesNotRemoveAfterTheRange
// 7. RemoveFormElementsAddedBetween_DeletesEverythingOlderThan30Days
TEST_F(AutocompleteTableLabelSensitiveTest, Autocomplete_RemoveBetweenChanges) {
  const base::Time t1 = base::Time::Now();
  const base::Time t2 = t1 + base::Days(1);

  AutocompleteChangeLabelSensitiveList changes;
  autofill::FormFieldData field;
  field.set_name(u"name");
  field.set_label(u"Name");
  field.set_value(u"Superman");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  AdvanceClock(base::Days(1));
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));

  changes.clear();
  EXPECT_TRUE(table().RemoveFormElementsAddedBetween(t1, t2, changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::UPDATE,
                AutocompleteKeyLabelSensitive(u"name", u"Name", u"Superman")),
            changes[0]);
  changes.clear();

  EXPECT_TRUE(
      table().RemoveFormElementsAddedBetween(t2, t2 + base::Days(1), changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::REMOVE,
                AutocompleteKeyLabelSensitive(u"name", u"Name", u"Superman")),
            changes[0]);
}

// TODO(crbug.com/346507576): Refactor into
// UpdateAutocompleteEntries_AddsNewEntry
TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_UpdateOneWithOneTimestamp) {
  AutocompleteEntryLabelSensitive entry(
      MakeAutocompleteEntryLabelSensitive(u"foo", u"bar", u"baz", 1, -1));
  std::vector<AutocompleteEntryLabelSensitive> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table().UpdateAutocompleteEntries(entries));

  EXPECT_EQ(1, GetAutocompleteEntryLabelSensitiveCount(u"foo", u"bar", u"baz",
                                                       &db()));

  std::vector<AutocompleteEntryLabelSensitive> all_entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_EQ(entry, all_entries[0]);
}

// TODO(crbug.com/346507576): Refactor into
// UpdateAutocompleteEntries_OverridesExistingEntryAndSetsProperCount
TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_UpdateOneWithTwoTimestamps) {
  AutocompleteEntryLabelSensitive entry(
      MakeAutocompleteEntryLabelSensitive(u"foo", u"bar", u"baz", 1, 2));
  std::vector<AutocompleteEntryLabelSensitive> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table().UpdateAutocompleteEntries(entries));

  EXPECT_EQ(2, GetAutocompleteEntryLabelSensitiveCount(u"foo", u"bar", u"baz",
                                                       &db()));

  std::vector<AutocompleteEntryLabelSensitive> all_entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_EQ(entry, all_entries[0]);
}

// TODO(crbug.com/346507576): should be covered in
// UpdateAutocompleteEntries_AddsNewEntry
TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_GetAutofillTimestamps) {
  AutocompleteEntryLabelSensitive entry(
      MakeAutocompleteEntryLabelSensitive(u"foo", u"bar", u"baz", 1, 2));
  std::vector<AutocompleteEntryLabelSensitive> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table().UpdateAutocompleteEntries(entries));

  std::optional<AutocompleteEntryLabelSensitive> table_entry =
      table().GetAutocompleteEntryLabelSensitive(u"foo", u"bar", u"baz");
  ASSERT_TRUE(table_entry);
  EXPECT_EQ(Time::FromTimeT(1), table_entry->date_created());
  EXPECT_EQ(Time::FromTimeT(2), table_entry->date_last_used());
}

// TODO(crbug.com/346507576): Refactor into
// UpdateAutocompleteEntries_AddsSeveralNewEntries
TEST_F(AutocompleteTableLabelSensitiveTest, Autocomplete_UpdateTwo) {
  AutocompleteEntryLabelSensitive entry0(
      MakeAutocompleteEntryLabelSensitive(u"foo", u"bar", u"baz0", 1, -1));
  AutocompleteEntryLabelSensitive entry1(
      MakeAutocompleteEntryLabelSensitive(u"foo", u"bar", u"baz1", 2, 3));
  std::vector<AutocompleteEntryLabelSensitive> entries;
  entries.push_back(entry0);
  entries.push_back(entry1);
  ASSERT_TRUE(table().UpdateAutocompleteEntries(entries));

  EXPECT_EQ(1, GetAutocompleteEntryLabelSensitiveCount(u"foo", u"bar", u"baz0",
                                                       &db()));
  EXPECT_EQ(2, GetAutocompleteEntryLabelSensitiveCount(u"foo", u"bar", u"baz1",
                                                       &db()));
}

// TODO(crbug.com/346507576): Refactor into
// UpdateAutocompleteEntries_OverridesExistingEntry
TEST_F(AutocompleteTableLabelSensitiveTest, Autocomplete_UpdateReplace) {
  AutocompleteChangeLabelSensitiveList changes;
  // Add a form field.  This will be replaced.
  autofill::FormFieldData field;
  field.set_name(u"name");
  field.set_label(u"Name");
  field.set_value(u"Superman");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));

  AutocompleteEntryLabelSensitive entry(
      MakeAutocompleteEntryLabelSensitive(u"name", u"Name", u"Superman", 1, 2));
  std::vector<AutocompleteEntryLabelSensitive> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table().UpdateAutocompleteEntries(entries));

  std::vector<AutocompleteEntryLabelSensitive> all_entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_EQ(entry, all_entries[0]);
}

// TODO(crbug.com/346507576): Refactor into
// UpdateAutocompleteEntries_AddsWithoutReplacing
TEST_F(AutocompleteTableLabelSensitiveTest, Autocomplete_UpdateDontReplace) {
  AutocompleteEntryLabelSensitive existing(MakeAutocompleteEntryLabelSensitive(
      u"name", u"Name", u"Superman", base::Time::Now().ToTimeT(), -1));

  AutocompleteChangeLabelSensitiveList changes;
  // Add a form field.  This will NOT be replaced.
  autofill::FormFieldData field;
  field.set_name(existing.key().name());
  field.set_label(existing.key().label());
  field.set_value(existing.key().value());
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  AutocompleteEntryLabelSensitive entry(MakeAutocompleteEntryLabelSensitive(
      u"name", u"Name", u"Clark Kent", 1, 2));
  std::vector<AutocompleteEntryLabelSensitive> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table().UpdateAutocompleteEntries(entries));

  std::vector<AutocompleteEntryLabelSensitive> all_entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(2U, all_entries.size());
  AutocompleteEntryLabelSensitiveSet expected_entries(
      all_entries.begin(), all_entries.end(), CompareAutocompleteEntries);
  EXPECT_EQ(1U, expected_entries.count(existing));
  EXPECT_EQ(1U, expected_entries.count(entry));
}

// TODO(crbug.com/346507576): remove, should be covered in
// RemoveFormElementsAddedBetween
TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_RemoveFormElementsAddedBetween_UsedOnlyBefore) {
  // Add an entry used only before the targeted range.
  AutocompleteChangeLabelSensitiveList changes;
  autofill::FormFieldData field;
  field.set_name(u"name");
  field.set_label(u"Name");
  field.set_value(u"Superman");
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
    AdvanceClock(base::Seconds(10));
  }

  EXPECT_EQ(5, GetAutocompleteEntryLabelSensitiveCount(
                   field.name(), field.label(), field.value(), &db()));

  changes.clear();
  EXPECT_TRUE(table().RemoveFormElementsAddedBetween(
      base::Time::Now() - base::Seconds(9), base::Time::Now(), changes));
  EXPECT_TRUE(changes.empty());
  EXPECT_EQ(5, GetAutocompleteEntryLabelSensitiveCount(
                   field.name(), field.label(), field.value(), &db()));
}

// TODO(crbug.com/346507576): remove, should be covered in
// RemoveFormElementsAddedBetween
TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_RemoveFormElementsAddedBetween_UsedOnlyAfter) {
  // Add an entry used only after the targeted range.
  AutocompleteChangeLabelSensitiveList changes;
  autofill::FormFieldData field;
  field.set_name(u"name");
  field.set_label(u"Name");
  field.set_value(u"Superman");
  for (int i = 0; i < 5; i++) {
    AdvanceClock(base::Seconds(10));
    EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  }

  EXPECT_EQ(5, GetAutocompleteEntryLabelSensitiveCount(
                   field.name(), field.label(), field.value(), &db()));

  changes.clear();
  EXPECT_TRUE(table().RemoveFormElementsAddedBetween(
      base::Time::Now() - base::Seconds(50),
      base::Time::Now() - base::Seconds(41), changes));
  EXPECT_TRUE(changes.empty());
  EXPECT_EQ(5, GetAutocompleteEntryLabelSensitiveCount(
                   field.name(), field.label(), field.value(), &db()));
}

// TODO(crbug.com/346507576): remove, should be covered in
// RemoveFormElementsAddedBetween
TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_RemoveFormElementsAddedBetween_UsedOnlyDuring) {
  // Add an entry used entirely during the targeted range.
  AutocompleteChangeLabelSensitiveList changes;
  autofill::FormFieldData field;
  field.set_name(u"name");
  field.set_label(u"Name");
  field.set_value(u"Superman");
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
    AdvanceClock(base::Seconds(10));
  }

  EXPECT_EQ(5, GetAutocompleteEntryLabelSensitiveCount(
                   field.name(), field.label(), field.value(), &db()));

  changes.clear();
  EXPECT_TRUE(table().RemoveFormElementsAddedBetween(
      base::Time::Now() - base::Seconds(50), base::Time::Now(), changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::REMOVE,
                AutocompleteKeyLabelSensitive(field.name(), field.label(),
                                              field.value())),
            changes[0]);
  EXPECT_EQ(0, GetAutocompleteEntryLabelSensitiveCount(
                   field.name(), field.label(), field.value(), &db()));
}

// TODO(crbug.com/346507576): remove, should be covered in
// RemoveFormElementsAddedBetween
TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_RemoveFormElementsAddedBetween_UsedBeforeAndDuring) {
  SetClock(autofill::test::kJune2017);
  // Add an entry used both before and during the targeted range.
  AutocompleteChangeLabelSensitiveList changes;
  autofill::FormFieldData field;
  field.set_name(u"name");
  field.set_label(u"Name");
  field.set_value(u"Superman");
  for (int i = 0; i < 5; i++) {
    AdvanceClock(base::Seconds(10));
    EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  }

  EXPECT_EQ(5, GetAutocompleteEntryLabelSensitiveCount(
                   field.name(), field.label(), field.value(), &db()));

  changes.clear();
  EXPECT_TRUE(table().RemoveFormElementsAddedBetween(
      base::Time::Now() - base::Seconds(10),
      base::Time::Now() + base::Seconds(10), changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::UPDATE,
                AutocompleteKeyLabelSensitive(field.name(), field.label(),
                                              field.value())),
            changes[0]);
  EXPECT_EQ(4, GetAutocompleteEntryLabelSensitiveCount(
                   field.name(), field.label(), field.value(), &db()));
  std::optional<AutocompleteEntryLabelSensitive> entry =
      table().GetAutocompleteEntryLabelSensitive(field.name(), field.label(),
                                                 field.value());
  ASSERT_TRUE(entry);
  EXPECT_EQ(base::Time::Now() - base::Seconds(40), entry->date_created());
  EXPECT_EQ(base::Time::Now() - base::Seconds(11), entry->date_last_used());
}

// TODO(crbug.com/346507576): remove, should be covered in
// RemoveFormElementsAddedBetween
TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_RemoveFormElementsAddedBetween_UsedDuringAndAfter) {
  SetClock(autofill::test::kJune2017);
  // Add an entry used both during and after the targeted range.
  AutocompleteChangeLabelSensitiveList changes;
  autofill::FormFieldData field;
  field.set_name(u"name");
  field.set_label(u"Name");
  field.set_value(u"Superman");
  for (int i = 0; i < 5; i++) {
    AdvanceClock(base::Seconds(10));
    EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  }

  EXPECT_EQ(5, GetAutocompleteEntryLabelSensitiveCount(
                   field.name(), field.label(), field.value(), &db()));

  changes.clear();
  EXPECT_TRUE(table().RemoveFormElementsAddedBetween(
      base::Time::Now() - base::Seconds(50),
      base::Time::Now() - base::Seconds(10), changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::UPDATE,
                AutocompleteKeyLabelSensitive(field.name(), field.label(),
                                              field.value())),
            changes[0]);
  EXPECT_EQ(2, GetAutocompleteEntryLabelSensitiveCount(
                   field.name(), field.label(), field.value(), &db()));
  std::optional<AutocompleteEntryLabelSensitive> entry =
      table().GetAutocompleteEntryLabelSensitive(field.name(), field.label(),
                                                 field.value());
  ASSERT_TRUE(entry);
  EXPECT_EQ(base::Time::Now() - base::Seconds(10), entry->date_created());
  EXPECT_EQ(base::Time::Now(), entry->date_last_used());
}

// TODO(crbug.com/346507576): remove, should be covered in
// RemoveFormElementsAddedBetween
TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_RemoveFormElementsAddedBetween_OlderThan30Days) {
  // Add some form field entries.
  AutocompleteChangeLabelSensitiveList changes;
  autofill::FormFieldData field;
  field.set_name(u"name");
  field.set_label(u"Name");

  field.set_value(u"Clark Sutter");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  AdvanceClock(base::Days(2));

  field.set_value(u"Clark Kent");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  AdvanceClock(base::Days(29));

  field.set_value(u"Superman");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));

  EXPECT_EQ(3U, changes.size());

  // Removing all elements added before 30 days from the database.
  changes.clear();
  EXPECT_TRUE(table().RemoveFormElementsAddedBetween(
      base::Time(), base::Time::Now() - base::Days(30), changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(
      AutocompleteChangeLabelSensitive(
          AutocompleteChangeLabelSensitive::REMOVE,
          AutocompleteKeyLabelSensitive(u"name", u"Name", u"Clark Sutter")),
      changes[0]);
  EXPECT_EQ(0, GetAutocompleteEntryLabelSensitiveCount(u"name", u"Name",
                                                       u"Clark Sutter", &db()));
  EXPECT_EQ(1, GetAutocompleteEntryLabelSensitiveCount(u"name", u"Name",
                                                       u"Superman", &db()));
  EXPECT_EQ(1, GetAutocompleteEntryLabelSensitiveCount(u"name", u"Name",
                                                       u"Clark Kent", &db()));
  changes.clear();
}

// TODO(crbug.com/346507576): refactor to
// "RemoveExpiredFormElements_RemovesExpiredEntries"
TEST_F(AutocompleteTableLabelSensitiveTest,
       RemoveExpiredFormElements_Expires_DeleteEntry) {
  AutocompleteChangeLabelSensitiveList changes;
  autofill::FormFieldData field;
  field.set_name(u"name");
  field.set_label(u"Name");
  field.set_value(u"Superman");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  AdvanceClock(2 * autofill::kAutocompleteRetentionPolicyPeriod);
  changes.clear();

  EXPECT_TRUE(table().RemoveExpiredFormElements(changes));
  EXPECT_EQ(AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::EXPIRE,
                AutocompleteKeyLabelSensitive(field.name(), field.label(),
                                              field.value())),
            changes[0]);
}

// TODO(crbug.com/346507576): refactor to
// "RemoveExpiredFormElements_DoesNotRemoveNonExpiredEntries"
TEST_F(AutocompleteTableLabelSensitiveTest,
       RemoveExpiredFormElements_NotOldEnough) {
  AutocompleteChangeLabelSensitiveList changes;
  autofill::FormFieldData field;
  field.set_name(u"name");
  field.set_label(u"Name");
  field.set_value(u"Superman");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  AdvanceClock(base::Days(2));
  changes.clear();

  EXPECT_TRUE(table().RemoveExpiredFormElements(changes));
  EXPECT_TRUE(changes.empty());
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_GetAllAutocompleteEntries_NoResults) {
  std::vector<AutocompleteEntryLabelSensitive> entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&entries));

  EXPECT_EQ(0U, entries.size());
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_GetAllAutocompleteEntries_OneResult) {
  SetClock(autofill::test::kJune2017);
  AutocompleteChangeLabelSensitiveList changes;
  std::map<std::string, std::vector<Time>> name_value_times_map;

  std::vector<Time> timestamps1;
  autofill::FormFieldData field;
  field.set_name(u"name");
  field.set_label(u"Name");
  field.set_value(u"Superman");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  timestamps1.push_back(base::Time::Now());
  std::string key1("NameSuperman");
  name_value_times_map.insert(
      std::pair<std::string, std::vector<Time>>(key1, timestamps1));

  AutocompleteEntryLabelSensitiveSet expected_entries(
      CompareAutocompleteEntries);
  AutocompleteKeyLabelSensitive ak1(u"name", u"Name", u"Superman");
  AutocompleteEntryLabelSensitive ae1(ak1, timestamps1.front(),
                                      timestamps1.back());

  expected_entries.insert(ae1);

  std::vector<AutocompleteEntryLabelSensitive> entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&entries));
  AutocompleteEntryLabelSensitiveSet entry_set(entries.begin(), entries.end(),
                                               CompareAutocompleteEntries);

  EXPECT_TRUE(
      CompareAutocompleteEntryLabelSensitiveSets(entry_set, expected_entries));
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_GetAllAutocompleteEntries_TwoDistinct) {
  SetClock(autofill::test::kJune2017);
  AutocompleteChangeLabelSensitiveList changes;
  std::map<std::string, std::vector<Time>> name_value_times_map;

  std::vector<Time> timestamps1;
  autofill::FormFieldData field;
  field.set_name(u"name");
  field.set_label(u"Name");
  field.set_value(u"Superman");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  timestamps1.push_back(base::Time::Now());
  std::string key1("NameSuperman");
  name_value_times_map.insert(
      std::pair<std::string, std::vector<Time>>(key1, timestamps1));

  AdvanceClock(base::Seconds(1));
  std::vector<Time> timestamps2;
  field.set_name(u"name");
  field.set_label(u"Name");
  field.set_value(u"Clark Kent");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  timestamps2.push_back(base::Time::Now());
  std::string key2("NameClark Kent");
  name_value_times_map.insert(
      std::pair<std::string, std::vector<Time>>(key2, timestamps2));

  AutocompleteEntryLabelSensitiveSet expected_entries(
      CompareAutocompleteEntries);
  AutocompleteKeyLabelSensitive ak1(u"name", u"Name", u"Superman");
  AutocompleteKeyLabelSensitive ak2(u"name", u"Name", u"Clark Kent");
  AutocompleteEntryLabelSensitive ae1(ak1, timestamps1.front(),
                                      timestamps1.back());
  AutocompleteEntryLabelSensitive ae2(ak2, timestamps2.front(),
                                      timestamps2.back());

  expected_entries.insert(ae1);
  expected_entries.insert(ae2);

  std::vector<AutocompleteEntryLabelSensitive> entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&entries));
  AutocompleteEntryLabelSensitiveSet entry_set(entries.begin(), entries.end(),
                                               CompareAutocompleteEntries);

  EXPECT_TRUE(
      CompareAutocompleteEntryLabelSensitiveSets(entry_set, expected_entries));
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_GetAllAutocompleteEntries_TwoSame) {
  SetClock(autofill::test::kJune2017);
  AutocompleteChangeLabelSensitiveList changes;
  std::map<std::string, std::vector<Time>> name_value_times_map;

  std::vector<Time> timestamps;
  for (int i = 0; i < 2; ++i) {
    autofill::FormFieldData field;
    field.set_name(u"name");
    field.set_label(u"Name");
    field.set_value(u"Superman");
    AdvanceClock(base::Seconds(1));
    EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
    timestamps.push_back(base::Time::Now());
  }

  std::string key("NameSuperman");
  name_value_times_map.insert(
      std::pair<std::string, std::vector<Time>>(key, timestamps));

  AutocompleteEntryLabelSensitiveSet expected_entries(
      CompareAutocompleteEntries);
  AutocompleteKeyLabelSensitive ak1(u"name", u"Name", u"Superman");
  AutocompleteEntryLabelSensitive ae1(ak1, timestamps.front(),
                                      timestamps.back());

  expected_entries.insert(ae1);

  std::vector<AutocompleteEntryLabelSensitive> entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&entries));
  AutocompleteEntryLabelSensitiveSet entry_set(entries.begin(), entries.end(),
                                               CompareAutocompleteEntries);

  EXPECT_TRUE(
      CompareAutocompleteEntryLabelSensitiveSets(entry_set, expected_entries));
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       DontCrashWhenAddingValueToPoisonedDB) {
  // Simulate a preceding fatal error.
  db().GetSQLConnection()->Poison();

  // Simulate the submission of a form.
  AutocompleteChangeLabelSensitiveList changes;
  autofill::FormFieldData field;
  field.set_name(u"name");
  field.set_label(u"Name");
  field.set_value(u"Superman");
  EXPECT_FALSE(table().AddFormFieldValues({field}, &changes));
}

}  // namespace

}  // namespace autofill
