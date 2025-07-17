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
  // AutocompleteEntryLabelSensitive contains dates of type base::Time, but the
  // database stores them with sub-second precision. To avoid mismatches, we
  // call ToTimeT(), which returns time in seconds.
  time_t a_created = a.date_created().ToTimeT();
  time_t a_used = a.date_last_used().ToTimeT();
  time_t b_created = b.date_created().ToTimeT();
  time_t b_used = b.date_last_used().ToTimeT();

  // Tie requires lvalues, so a_created, a_used, b_created, and b_used cannot be
  // inlined.
  return std::tie(a.key().name(), a.key().label(), a.key().value(), a_created,
                  a_used) < std::tie(b.key().name(), b.key().label(),
                                     b.key().value(), b_created, b_used);
}

AutocompleteEntryLabelSensitive MakeAutocompleteEntryLabelSensitive(
    const std::u16string& name,
    const std::u16string& label,
    const std::u16string& value,
    time_t date_created,
    time_t date_last_used) {
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

auto HasCreationAndUseDate(base::Time date_created, base::Time date_last_used) {
  return AllOf(
      Property("AutocompleteEntryLabelSensitive::date_created",
               &AutocompleteEntryLabelSensitive::date_created, date_created),
      Property("AutocompleteEntryLabelSensitive::date_last_used",
               &AutocompleteEntryLabelSensitive::date_last_used,
               date_last_used));
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

using GetCountOfValuesContainedBetweenTest =
    AutocompleteTableLabelSensitiveTest;

// Add several entries to the database with different timestamps and expect
// GetCountOfValuesContainedBetween to return all of them if the time interval
// is large enough.
TEST_F(GetCountOfValuesContainedBetweenTest, ReturnsCorrectCount) {
  const Time begin = base::Time::Now();
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  AdvanceClock(base::Seconds(1));
  ASSERT_TRUE(CreateAndSubmitDefaultFieldWithValue(u"Clark Kent").has_value());

  AdvanceClock(base::Seconds(1));
  ASSERT_TRUE(
      CreateAndSubmitDefaultFieldWithValue(u"Clark Sutter").has_value());

  EXPECT_EQ(
      table().GetCountOfValuesContainedBetween(begin, begin + base::Seconds(4)),
      3);
}

// Add several entries to the database with different timestamps and expect
// GetCountOfValuesContainedBetween to return nothing if the time interval
// provided does not contain any of the entries.
TEST_F(GetCountOfValuesContainedBetweenTest,
       ReturnsZeroIfNothingIsInTheInterval) {
  const Time begin = base::Time::Now();
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  AdvanceClock(base::Seconds(1));
  ASSERT_TRUE(CreateAndSubmitDefaultFieldWithValue(u"Clark Kent").has_value());

  EXPECT_EQ(table().GetCountOfValuesContainedBetween(begin + base::Seconds(5),
                                                     begin + base::Seconds(6)),
            0);
}

// Add several entries to the database with different timestamps. Call
// GetCountOfValuesContainedBetween with such an interval that some entries are
// contained and some are not. Expect to return the number of entries that are
// contained in the time interval provided.
TEST_F(GetCountOfValuesContainedBetweenTest,
       ReturnsCountOfEntriesOnlyInTheInterval) {
  const Time begin = base::Time::Now();
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  AdvanceClock(base::Seconds(2));
  ASSERT_TRUE(CreateAndSubmitDefaultFieldWithValue(u"Clark Kent").has_value());

  AdvanceClock(base::Seconds(2));
  ASSERT_TRUE(
      CreateAndSubmitDefaultFieldWithValue(u"Clark Sutter").has_value());

  EXPECT_EQ(table().GetCountOfValuesContainedBetween(begin + base::Seconds(1),
                                                     begin + base::Seconds(3)),
            1);
}

// Add several entries to the database with different timestamps. Call
// GetCountOfValuesContainedBetween with interval [0, MAX_VALUE) interval.
// Expect to return all the entries.
TEST_F(GetCountOfValuesContainedBetweenTest,
       ReturnsEverythingForUnboundedInterval) {
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  AdvanceClock(base::Seconds(2));
  ASSERT_TRUE(CreateAndSubmitDefaultFieldWithValue(u"Clark Kent").has_value());

  AdvanceClock(base::Seconds(2));
  ASSERT_TRUE(
      CreateAndSubmitDefaultFieldWithValue(u"Clark Sutter").has_value());

  EXPECT_EQ(table().GetCountOfValuesContainedBetween(base::Time(), Time::Max()),
            3);
}

// GetCountOfValuesContainedBetween should treat provided interval as
// closed-open, e.g. include begin and exclude end. Both entry's creation and
// update time, should be in the interval to be counted. Interval [1, 5) should
// not contain an entry with creation/update timespan [0, 1].
TEST_F(GetCountOfValuesContainedBetweenTest,
       ShouldNotIncludeIfUpdateEqualsBegin) {
  const Time begin = base::Time::Now();

  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());
  AdvanceClock(base::Seconds(1));
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  EXPECT_EQ(table().GetCountOfValuesContainedBetween(begin + base::Seconds(1),
                                                     begin + base::Seconds(5)),
            0);
}

// GetCountOfValuesContainedBetween should treat provided interval as
// closed-open, e.g. include begin and exclude end. Both entry's creation and
// update time, should be in the interval to be counted. Interval [1, 5) should
// contain an entry with creation/update timespan [1, 1].
TEST_F(GetCountOfValuesContainedBetweenTest,
       ShouldIncludeIfCreateAndUpdateEqualsBegin) {
  const Time begin = base::Time::Now();

  AdvanceClock(base::Seconds(1));
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  EXPECT_EQ(table().GetCountOfValuesContainedBetween(begin + base::Seconds(1),
                                                     begin + base::Seconds(5)),
            1);
}

// GetCountOfValuesContainedBetween should treat provided interval as
// closed-open, e.g. include begin and exclude end. Both entry's creation and
// update time, should be in the interval to be counted. Interval [1, 5) should
// contain an entry with creation/update timespan [1, 5].
TEST_F(GetCountOfValuesContainedBetweenTest,
       ShouldNotIncludeIfCreateEqualsBeginAndUpdateEqualsEnd) {
  const Time begin = base::Time::Now();

  AdvanceClock(base::Seconds(1));
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());
  AdvanceClock(base::Seconds(4));
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  EXPECT_EQ(table().GetCountOfValuesContainedBetween(begin + base::Seconds(1),
                                                     begin + base::Seconds(5)),
            0);
}

// GetCountOfValuesContainedBetween should treat provided interval as
// closed-open, e.g. include begin and exclude end. Both entry's creation and
// update time, should be in the interval to be counted. Interval [1, 5) should
// not contain an entry with creation/update timespan [5, 5].
TEST_F(GetCountOfValuesContainedBetweenTest,
       ShouldNotIncludeIfCreateAndUpdateEqualsEnd) {
  const Time begin = base::Time::Now();

  AdvanceClock(base::Seconds(5));
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  EXPECT_EQ(table().GetCountOfValuesContainedBetween(begin + base::Seconds(1),
                                                     begin + base::Seconds(5)),
            0);
}

// GetCountOfValuesContainedBetween should treat provided interval as
// closed-open, e.g. include begin and exclude end. Both entry's creation and
// update time, should be in the interval to be counted. Interval [1, 5) should
// not contain an entry with creation/update timespan [5, 6].
TEST_F(GetCountOfValuesContainedBetweenTest,
       ShouldNotIncludeIfCreateEqualsEnd) {
  const Time begin = base::Time::Now();

  AdvanceClock(base::Seconds(5));
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());
  AdvanceClock(base::Seconds(1));
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  EXPECT_EQ(table().GetCountOfValuesContainedBetween(begin + base::Seconds(1),
                                                     begin + base::Seconds(5)),
            0);
}

using RemoveFormElementsAddedBetweenTest = AutocompleteTableLabelSensitiveTest;

// Add an entry to the database at a specified timestamp and expect it to be
// removed by RemoveFormElementsAddedBetween when the entry's timestamp is in
// the time range provided.
TEST_F(RemoveFormElementsAddedBetweenTest,
       RemovesEntryAddedDuringTheSpecifiedRange) {
  const Time begin = base::Time::Now();

  AdvanceClock(base::Seconds(1));
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin, begin + base::Seconds(2), changes()));

  EXPECT_THAT(changes(), ElementsAre(AutocompleteChangeLabelSensitive(
                             AutocompleteChangeLabelSensitive::REMOVE,
                             AutocompleteKeyLabelSensitive(
                                 kDefaultName, kDefaultLabel, kDefaultValue))));
}

// Add an entry to the database at a specified timestamp and expect it not to be
// removed by RemoveFormElementsAddedBetween when the entry's timestamp is
// outside the time range provided.
TEST_F(RemoveFormElementsAddedBetweenTest,
       DoesNotRemoveEntryAddedOutsideTheRange) {
  const Time begin = base::Time::Now();
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin + base::Seconds(10), begin + base::Seconds(20), changes()));

  EXPECT_EQ(changes().size(), 0U);
}

// Add multiple entries to the database with specified timestamps and expect
// them all to be removed by RemoveFormElementsAddedBetween when the entries'
// timestamps are in the time range provided.
TEST_F(RemoveFormElementsAddedBetweenTest,
       RemovesMultipleEntriesAddedDuringTheRange) {
  const Time begin = base::Time::Now();
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  AdvanceClock(base::Seconds(1));
  std::optional<FormFieldData> optional_second_field =
      CreateAndSubmitDefaultFieldWithValue(u"Clark Kent");
  ASSERT_TRUE(optional_second_field.has_value());

  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin, begin + base::Seconds(10), changes()));

  EXPECT_THAT(changes(),
              ElementsAre(AutocompleteChangeLabelSensitive(
                              AutocompleteChangeLabelSensitive::REMOVE,
                              AutocompleteKeyLabelSensitive(
                                  kDefaultName, kDefaultLabel, kDefaultValue)),
                          AutocompleteChangeLabelSensitive(
                              AutocompleteChangeLabelSensitive::REMOVE,
                              AutocompleteKeyLabelSensitive(
                                  kDefaultName, kDefaultLabel,
                                  optional_second_field.value().value()))));
}

// RemoveFormElementsAddedBetween should remove an entry when it was added and
// updated during the provided time range.
TEST_F(RemoveFormElementsAddedBetweenTest,
       RemovesEntryAddedAndUpdatedDuringTheRange) {
  const Time begin = base::Time::Now();
  std::optional<FormFieldData> optional_field = CreateAndSubmitDefaultField();
  ASSERT_TRUE(optional_field.has_value());

  AdvanceClock(base::Seconds(1));
  ASSERT_TRUE(SubmitFormField(optional_field.value()));

  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin, begin + base::Seconds(10), changes()));

  EXPECT_THAT(changes(), ElementsAre(AutocompleteChangeLabelSensitive(
                             AutocompleteChangeLabelSensitive::REMOVE,
                             AutocompleteKeyLabelSensitive(
                                 kDefaultName, kDefaultLabel, kDefaultValue))));
}

// RemoveFormElementsAddedBetween should UPDATE entry when it was added outside
// of the provided time range, but was updated during the provided time range.
TEST_F(RemoveFormElementsAddedBetweenTest,
       UpdatesEntryAddedBeforeAndUpdatedDuringTheRange) {
  const Time begin = base::Time::Now();
  std::optional<FormFieldData> optional_field = CreateAndSubmitDefaultField();
  ASSERT_TRUE(optional_field.has_value());

  AdvanceClock(base::Seconds(10));
  ASSERT_TRUE(SubmitFormField(optional_field.value()));

  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin + base::Seconds(5), begin + base::Seconds(15), changes()));

  EXPECT_THAT(changes(), ElementsAre(AutocompleteChangeLabelSensitive(
                             AutocompleteChangeLabelSensitive::UPDATE,
                             AutocompleteKeyLabelSensitive(
                                 kDefaultName, kDefaultLabel, kDefaultValue))));
}

// RemoveFormElementsAddedBetween should update entry when it was added inside
// of the provided time range, but was updated outside of the provided time
// range.
TEST_F(RemoveFormElementsAddedBetweenTest,
       UpdatesEntryAddedDuringAndUpdatedAfterTheRange) {
  const Time begin = base::Time::Now();

  AdvanceClock(base::Seconds(10));
  std::optional<FormFieldData> optional_field = CreateAndSubmitDefaultField();
  ASSERT_TRUE(optional_field.has_value());

  AdvanceClock(base::Seconds(10));
  ASSERT_TRUE(SubmitFormField(optional_field.value()));

  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin + base::Seconds(5), begin + base::Seconds(15), changes()));

  EXPECT_THAT(changes(), ElementsAre(AutocompleteChangeLabelSensitive(
                             AutocompleteChangeLabelSensitive::UPDATE,
                             AutocompleteKeyLabelSensitive(
                                 kDefaultName, kDefaultLabel, kDefaultValue))));
}

// Add two entries to the database. Call RemoveFormElementsAddedBetween with
// such range that first entry is fully inside of the range, second entry is
// partially inside of the range. Expect one entry to be REMOVED and another
// one to be UPDATED.
TEST_F(RemoveFormElementsAddedBetweenTest, RemovesAndUpdatesAtTheSameTime) {
  const Time begin = base::Time::Now();
  std::optional<FormFieldData> optional_field1 = CreateAndSubmitDefaultField();
  ASSERT_TRUE(optional_field1.has_value());

  AdvanceClock(base::Seconds(5));
  ASSERT_TRUE(SubmitFormField(optional_field1.value()));

  AdvanceClock(base::Seconds(5));
  std::optional<FormFieldData> optional_field2 =
      CreateAndSubmitDefaultFieldWithValue(u"Clark Kent");
  ASSERT_TRUE(optional_field2.has_value());

  AdvanceClock(base::Seconds(5));
  ASSERT_TRUE(SubmitFormField(optional_field2.value()));

  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin, begin + base::Seconds(12), changes()));

  EXPECT_THAT(
      changes(),
      ElementsAre(
          AutocompleteChangeLabelSensitive(
              AutocompleteChangeLabelSensitive::REMOVE,
              AutocompleteKeyLabelSensitive(kDefaultName, kDefaultLabel,
                                            optional_field1.value().value())),
          AutocompleteChangeLabelSensitive(
              AutocompleteChangeLabelSensitive::UPDATE,
              AutocompleteKeyLabelSensitive(kDefaultName, kDefaultLabel,
                                            optional_field2.value().value()))));
}

// Add and update entry every X seconds. Call RemoveFormElementsAddedBetween
// with such range that covers half of the entry's [create, last_update] span.
// Expect the use counter to be updated to become interpolated to half of the
// original number.
TEST_F(RemoveFormElementsAddedBetweenTest, UpdatesCountCorrectly) {
  const Time begin = base::Time::Now();
  FormFieldData field = CreateDefaultField();

  AdvanceClock(base::Seconds(10));

  for (int i = 0; i < 10; ++i) {
    ASSERT_TRUE(SubmitFormField(field));
    AdvanceClock(base::Seconds(1));
  }

  // Sanity check
  ASSERT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue, &db()),
            10);

  // The element had 10 uses between timestamp 10 (exclusive) and 19
  // (inclusive). Remove entries that were submitted between 5th second
  // (inclusive) and 15th second (exclusive). This corresponds to 5 uses in
  // reality. The database applies linear interpolation and also decreases the
  // use counter by 5.
  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin + base::Seconds(5), begin + base::Seconds(15), changes()));

  EXPECT_THAT(changes(), ElementsAre(AutocompleteChangeLabelSensitive(
                             AutocompleteChangeLabelSensitive::UPDATE,
                             AutocompleteKeyLabelSensitive(
                                 kDefaultName, kDefaultLabel, kDefaultValue))));
  // The number of usages should be half of the original number.
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue, &db()),
            5);
}

// As we store only creation and last update timestamps,
// RemoveFormElementsAddedBetween assumes that all updates during given time
// range appeared uniformly distributed in time. This test adds and update the
// same entry multiple times with the same timestamp than makes another update X
// seconds later. This means that all except the last update happened in the
// beginning of [create, last_update] timespan. Nevertheless, call of
// RemoveFormElementsAddedBetween with the time range that covers the half of
// aforementioned timespan would still half the count instead of decreasing it
// to 1.
TEST_F(RemoveFormElementsAddedBetweenTest,
       AssumesUniformallyDistributedTimestampsOnUpdate) {
  const Time begin = base::Time::Now();
  FormFieldData field = CreateDefaultField();

  AdvanceClock(base::Seconds(10));

  // Create a timespan [creation_time, last_update_time] where all usages except
  // the last one happen at the beginning of the timespan, creation_time = 10s,
  // last_update_time = 13s.
  for (int i = 0; i < 3; ++i) {
    ASSERT_TRUE(SubmitFormField(field));
  }
  AdvanceClock(base::Seconds(3));
  ASSERT_TRUE(SubmitFormField(field));

  // Sanity check
  ASSERT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue, &db()),
            4);

  // Remove half of the entry's timespan (up to second 12 inclusive).
  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin + base::Seconds(5), begin + base::Seconds(12), changes()));

  EXPECT_THAT(changes(), ElementsAre(AutocompleteChangeLabelSensitive(
                             AutocompleteChangeLabelSensitive::UPDATE,
                             AutocompleteKeyLabelSensitive(
                                 kDefaultName, kDefaultLabel, kDefaultValue))));
  // The number of usages should be half of the original number.
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue, &db()),
            2);
}

// Previous tests were always removing the first half of the entry's [create,
// last_update] span. This test checks that RemoveFormElementsAddedBetween works
// the same when removing the second half of the span.
TEST_F(RemoveFormElementsAddedBetweenTest,
       UpdatesCountCorrectlyWhenRemovingSecondHalfOfEntrySpan) {
  const Time begin = base::Time::Now();
  FormFieldData field = CreateDefaultField();

  AdvanceClock(base::Seconds(10));

  for (int i = 0; i < 10; ++i) {
    ASSERT_TRUE(SubmitFormField(field));
    AdvanceClock(base::Seconds(1));
  }

  // Sanity check
  ASSERT_EQ(10, GetAutocompleteEntryLabelSensitiveCount(
                    kDefaultName, kDefaultLabel, kDefaultValue, &db()));

  // Remove half of the entry's span.
  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin + base::Seconds(15), begin + base::Seconds(30), changes()));

  EXPECT_THAT(changes(), ElementsAre(AutocompleteChangeLabelSensitive(
                             AutocompleteChangeLabelSensitive::UPDATE,
                             AutocompleteKeyLabelSensitive(
                                 kDefaultName, kDefaultLabel, kDefaultValue))));
  // The number of usages should be half of the original number.
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue, &db()),
            5);
}

// RemoveFormElementsAddedBetween should work correctly when called to remove
// everything older than 30 days.
TEST_F(RemoveFormElementsAddedBetweenTest,
       CorrectlyRemovesEverythingOlderThan30Days) {
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  AdvanceClock(base::Days(2));
  ASSERT_TRUE(CreateAndSubmitDefaultFieldWithValue(u"Clark Kent").has_value());

  AdvanceClock(base::Days(29));
  ASSERT_TRUE(
      CreateAndSubmitDefaultFieldWithValue(u"Clark Sutter").has_value());

  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      base::Time(), base::Time::Now() - base::Days(30), changes()));

  EXPECT_THAT(changes(), ElementsAre(AutocompleteChangeLabelSensitive(
                             AutocompleteChangeLabelSensitive::REMOVE,
                             AutocompleteKeyLabelSensitive(
                                 kDefaultName, kDefaultLabel, kDefaultValue))));
}

using UpdateAutocompleteEntriesTest = AutocompleteTableLabelSensitiveTest;

// UpdateAutocompleteEntries works in `update or insert` fashion. If given entry
// does not exist in the table, it should be inserted.
TEST_F(UpdateAutocompleteEntriesTest, AddsNewEntry) {
  AutocompleteEntryLabelSensitive entry(MakeAutocompleteEntryLabelSensitive(
      kDefaultName, kDefaultLabel, kDefaultValue, 5, 5));
  std::vector<AutocompleteEntryLabelSensitive> entries;
  entries.push_back(entry);

  ASSERT_TRUE(table().UpdateAutocompleteEntries(entries));

  std::optional<AutocompleteEntryLabelSensitive> table_entry =
      table().GetAutocompleteEntryLabelSensitive(kDefaultName, kDefaultLabel,
                                                 kDefaultValue);
  EXPECT_THAT(table_entry, Optional(HasCreationAndUseDate(Time::FromTimeT(5),
                                                          Time::FromTimeT(5))));
}

// UpdateAutocompleteEntries should add several new entries if the user submits
// different values for the same field. The use count can be updated only to
// value 1 or 2, depending on timestamps provided to UpdateAutocompleteEntries
// for given entry. This is a workaround for count sync not be implemented. If
// the creation timestamp is equal to the last used timestamp, then the use
// count will be set to 1, otherwise count will be set to 2.
TEST_F(UpdateAutocompleteEntriesTest, AddsSeveralNewEntriesWithProperCounts) {
  const std::u16string kAnotherValue = u"Clark Sutter";
  std::vector<AutocompleteEntryLabelSensitive> entries = {
      MakeAutocompleteEntryLabelSensitive(kDefaultName, kDefaultLabel,
                                          kDefaultValue, 1, 1),
      MakeAutocompleteEntryLabelSensitive(kDefaultName, kDefaultLabel,
                                          kAnotherValue, 2, 3)};

  ASSERT_TRUE(table().UpdateAutocompleteEntries(entries));

  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue, &db()),
            1);
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kAnotherValue, &db()),
            2);
}

// If nan entry already exists in the table, its timestamps and use count should
// be updated with value provided in UpdateAutocompleteEntries.
TEST_F(UpdateAutocompleteEntriesTest,
       OverridesExistingEntrysTimestampsAndCount) {
  // Add one entry ten times to make its count reach 10.
  for (int i = 0; i < 10; ++i) {
    ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());
  }
  std::vector<AutocompleteEntryLabelSensitive> entries = {
      MakeAutocompleteEntryLabelSensitive(kDefaultName, kDefaultLabel,
                                          kDefaultValue, 1, 1)};

  ASSERT_TRUE(table().UpdateAutocompleteEntries(entries));

  std::vector<AutocompleteEntryLabelSensitive> all_entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&all_entries));
  EXPECT_THAT(all_entries, ElementsAre(entries[0]));
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue, &db()),
            1);
}

// UpdateAutocompleteEntries should insert new entry and not replace existing
// entry.
TEST_F(UpdateAutocompleteEntriesTest, AddsWithoutReplacing) {
  AutocompleteEntryLabelSensitive existing_entry(
      MakeAutocompleteEntryLabelSensitive(
          kDefaultName, kDefaultLabel, kDefaultValue,
          base::Time::Now().ToTimeT(), base::Time::Now().ToTimeT()));
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  AutocompleteEntryLabelSensitive new_entry(MakeAutocompleteEntryLabelSensitive(
      kDefaultName, kDefaultLabel, u"Clark Sutter", 1, 2));

  ASSERT_TRUE(table().UpdateAutocompleteEntries({new_entry}));

  std::vector<AutocompleteEntryLabelSensitive> all_entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(all_entries.size(), 2U);
  AutocompleteEntryLabelSensitiveSet expected_entries(
      all_entries.begin(), all_entries.end(), CompareAutocompleteEntries);
  EXPECT_EQ(expected_entries.count(existing_entry), 1U);
  EXPECT_EQ(expected_entries.count(new_entry), 1U);
}

using RemoveExpiredFormElementsTest = AutocompleteTableLabelSensitiveTest;

// Add an entry and advance the clock to make it expired.
// RemoveExpiredFormElements should remove the entry.
TEST_F(RemoveExpiredFormElementsTest, RemovesExpiredEntries) {
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());
  AdvanceClock(2 * autofill::kAutocompleteRetentionPolicyPeriod);
  changes().clear();

  ASSERT_TRUE(table().RemoveExpiredFormElements(changes()));

  EXPECT_THAT(changes(), ElementsAre(AutocompleteChangeLabelSensitive(
                             AutocompleteChangeLabelSensitive::EXPIRE,
                             AutocompleteKeyLabelSensitive(
                                 kDefaultName, kDefaultLabel, kDefaultValue))));
}

// Add an entry and advance the clock but not enough to make it expired.
// RemoveExpiredFormElements should not remove the entry.
TEST_F(RemoveExpiredFormElementsTest, DoesNotRemoveNonExpiredEntries) {
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());
  ASSERT_LT(base::Days(2), autofill::kAutocompleteRetentionPolicyPeriod);
  AdvanceClock(base::Days(2));
  changes().clear();

  ASSERT_TRUE(table().RemoveExpiredFormElements(changes()));

  EXPECT_TRUE(changes().empty());
}

using GetAllAutocompleteEntriesTest = AutocompleteTableLabelSensitiveTest;

// If database is empty, GetAllAutocompleteEntries should return no results.
TEST_F(GetAllAutocompleteEntriesTest, ReturnsNoResults) {
  std::vector<AutocompleteEntryLabelSensitive> entries;

  ASSERT_TRUE(table().GetAllAutocompleteEntries(&entries));

  EXPECT_EQ(entries.size(), 0U);
}

// If database contains one entry, GetAllAutocompleteEntries should return it.
TEST_F(GetAllAutocompleteEntriesTest, ReturnsOneResult) {
  AutocompleteEntryLabelSensitive entry(
      {kDefaultName, kDefaultLabel, kDefaultValue}, base::Time::Now(),
      base::Time::Now());

  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  AutocompleteEntryLabelSensitiveSet expected_entries(
      CompareAutocompleteEntries);
  expected_entries.insert(entry);

  std::vector<AutocompleteEntryLabelSensitive> entries;

  ASSERT_TRUE(table().GetAllAutocompleteEntries(&entries));

  AutocompleteEntryLabelSensitiveSet entry_set(entries.begin(), entries.end(),
                                               CompareAutocompleteEntries);

  EXPECT_TRUE(
      CompareAutocompleteEntryLabelSensitiveSets(entry_set, expected_entries));
}

// If database contains two distinct entries, GetAllAutocompleteEntries should
// return both of them.
TEST_F(GetAllAutocompleteEntriesTest, ReturnsTwoDistinct) {
  std::optional<FormFieldData> optional_field1 = CreateAndSubmitDefaultField();
  ASSERT_TRUE(optional_field1.has_value());
  Time timestamps1(base::Time::Now());

  AdvanceClock(base::Seconds(1));
  std::optional<FormFieldData> optional_field2 =
      CreateAndSubmitDefaultFieldWithValue(u"Clark Sutter");
  ASSERT_TRUE(optional_field2.has_value());
  Time timestamps2(base::Time::Now());

  AutocompleteEntryLabelSensitiveSet expected_entries(
      CompareAutocompleteEntries);
  AutocompleteEntryLabelSensitive entry1(
      {kDefaultName, kDefaultLabel, optional_field1.value().value()},
      timestamps1, timestamps1);
  AutocompleteEntryLabelSensitive entry2(
      {kDefaultName, kDefaultLabel, optional_field2.value().value()},
      timestamps2, timestamps2);

  expected_entries.insert(entry1);
  expected_entries.insert(entry2);

  std::vector<AutocompleteEntryLabelSensitive> entries;

  ASSERT_TRUE(table().GetAllAutocompleteEntries(&entries));

  AutocompleteEntryLabelSensitiveSet entry_set(entries.begin(), entries.end(),
                                               CompareAutocompleteEntries);

  EXPECT_TRUE(
      CompareAutocompleteEntryLabelSensitiveSets(entry_set, expected_entries));
}

// If we add one entry twice, GetAllAutocompleteEntries should return it once.
TEST_F(GetAllAutocompleteEntriesTest, ReturnsTwoIdentical) {
  std::optional<FormFieldData> optional_field = CreateAndSubmitDefaultField();
  ASSERT_TRUE(optional_field.has_value());
  Time timestamp1(base::Time::Now());

  AdvanceClock(base::Seconds(1));
  ASSERT_TRUE(SubmitFormField(optional_field.value()));
  Time timestamp2(base::Time::Now());

  AutocompleteEntryLabelSensitiveSet expected_entries(
      CompareAutocompleteEntries);
  AutocompleteEntryLabelSensitive entry(
      {kDefaultName, kDefaultLabel, kDefaultValue}, timestamp1, timestamp2);

  expected_entries.insert(entry);

  std::vector<AutocompleteEntryLabelSensitive> entries;

  ASSERT_TRUE(table().GetAllAutocompleteEntries(&entries));

  AutocompleteEntryLabelSensitiveSet entry_set(entries.begin(), entries.end(),
                                               CompareAutocompleteEntries);

  EXPECT_TRUE(
      CompareAutocompleteEntryLabelSensitiveSets(entry_set, expected_entries));
}

// Poison the database and check that we don't crash when adding a value.
TEST_F(AutocompleteTableLabelSensitiveTest,
       DontCrashWhenAddingValueToPoisonedDB) {
  // Simulate a preceding fatal error.
  db().GetSQLConnection()->Poison();

  // Simulate the submission of a form.
  FormFieldData field = CreateDefaultField();
  EXPECT_FALSE(table().AddFormFieldValues({field}, &changes()));
}

}  // namespace

}  // namespace autofill
