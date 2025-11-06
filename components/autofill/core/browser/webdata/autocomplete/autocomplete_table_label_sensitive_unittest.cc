// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// TODO(crbug.com/346507576): After launch, this file should replace the
// existing autocomplete_table_unittest.cc. The "LabelSensitive" suffix should
// be dropped everywhere.

#include "components/autofill/core/browser/webdata/autocomplete/autocomplete_table_label_sensitive.h"

#include <string>
#include <utility>

#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "components/autofill/core/browser/test_utils/autofill_test_utils.h"
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
using ::testing::Eq;
using ::testing::Optional;
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

  // 'Tie' requires lvalues, so a_created, a_used, b_created, and b_used cannot
  // be inlined.
  return std::tie(a.key().name(), a.key().label(), a.key().value(), a_created,
                  a_used) < std::tie(b.key().name(), b.key().label(),
                                     b.key().value(), b_created, b_used);
}

AutocompleteEntryLabelSensitive MakeAutocompleteEntryLabelSensitiveForTest(
    const std::u16string& name,
    const std::u16string& label,
    const std::u16string& value,
    Time date_created,
    Time date_last_used) {
  // Drop sub-second precision, as the database stores time with only
  // second-level precision.
  return AutocompleteEntryLabelSensitive(
      AutocompleteKeyLabelSensitive(name, label, value),
      Time::FromTimeT(date_created.ToTimeT()),
      Time::FromTimeT(date_last_used.ToTimeT()));
}

// Checks that `actual` and `expected` contain the same elements.
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

  // Submits a vector of form fields to the table; returns true if successful.
  [[nodiscard]] bool SubmitFormFields(
      const std::vector<FormFieldData>& elements) {
    return table().AddFormFieldValues(elements);
  }

  // Submits a single form field to the table; returns true if successful.
  [[nodiscard]] bool SubmitFormField(const FormFieldData& field) {
    return SubmitFormFields({field});
  }

  // Will return an optional field on successful submission, or std::nullopt if
  // submission fails.
  [[nodiscard]] std::optional<FormFieldData>
  CreateAndSubmitDefaultFieldWithValue(std::u16string_view value) {
    FormFieldData field = CreateDefaultFieldWithValue(value);
    if (!SubmitFormField(field)) {
      return std::nullopt;
    }
    return field;
  }

  // Will return an optional field on successful submission, or std::nullopt if
  // submission fails.
  [[nodiscard]] std::optional<FormFieldData> CreateAndSubmitDefaultField() {
    return CreateAndSubmitDefaultFieldWithValue(kDefaultValue);
  }

  [[nodiscard]] std::optional<std::u16string>
  GetAutocompleteEntryLabelSensitiveLabelNormalized(const std::u16string& name,
                                                    const std::u16string& label,
                                                    const std::u16string& value) {
    sql::Statement s(db().GetSQLConnection()->GetUniqueStatement(
        "SELECT label_normalized FROM autocomplete WHERE name = ? AND label = "
        "? AND value = ?"));
    s.BindString16(0, name);
    s.BindString16(1, label);
    s.BindString16(2, value);
    if (!s.Step()) {
      return std::nullopt;
    }
    return s.ColumnString16(0);
  }

  [[nodiscard]] bool DoesAutocompleteEntryExist(const std::u16string& name,
                                                  const std::u16string& label,
                                                  const std::u16string& value) {
    sql::Statement s(db().GetSQLConnection()->GetUniqueStatement(
        "SELECT COUNT(*) FROM autocomplete WHERE name = ? AND label = ? AND "
        "value = ?"));
    s.BindString16(0, name);
    s.BindString16(1, label);
    s.BindString16(2, value);
    if (!s.Step()) {
      return false;
    }
    return s.ColumnInt(0) >= 1;
  }

  [[nodiscard]] int GetAutocompleteEntryLabelSensitiveCount(
      const std::u16string& name,
      const std::u16string& label,
      const std::u16string& value) {
    sql::Statement s(db().GetSQLConnection()->GetUniqueStatement(
        "SELECT count FROM autocomplete WHERE name = ? AND label = ? AND value "
        "= ?"));
    s.BindString16(0, name);
    s.BindString16(1, label);
    s.BindString16(2, value);
    if (!s.Step()) {
      return 0;
    }
    return s.ColumnInt(0);
  }

  [[nodiscard]] size_t AutocompleteEntriesCount() {
    sql::Statement s(db().GetSQLConnection()->GetUniqueStatement(
        "SELECT COUNT(*) FROM autocomplete"));
    if (!s.Step()) {
      return 0;
    }
    return s.ColumnInt(0);
  }

  WebDatabase& db() { return *db_; }
  AutocompleteTableLabelSensitive& table() { return *table_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AutocompleteTableLabelSensitive> table_;
  std::unique_ptr<WebDatabase> db_;
  test::AutofillUnitTestEnvironment autofill_test_environment_;
};

// TODO(crbug.com/346507576): Add happy path tests.

using AddFormFieldValuesTest = AutocompleteTableLabelSensitiveTest;

// Checks that AddFormFieldValues correctly creates a new entry in the database.
TEST_F(AddFormFieldValuesTest, InsertsNewEntry) {
  ASSERT_FALSE(
      DoesAutocompleteEntryExist(kDefaultName, kDefaultLabel, kDefaultValue));
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  EXPECT_TRUE(
      DoesAutocompleteEntryExist(kDefaultName, kDefaultLabel, kDefaultValue));
}

// Checks that AddFormFieldValues modifies the underlying database by inserting
// an entry and later incrementing the use count of the previously inserted
// entry.
TEST_F(AddFormFieldValuesTest, UpdatesExistingEntryCount) {
  FormFieldData field = CreateDefaultField();

  // Add new entry.
  ASSERT_TRUE(SubmitFormField(field));

  // Update existing entry.
  ASSERT_TRUE(SubmitFormField(field));

  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue),
            2);
}

// If we add a new entry, we should update the counts of all existing entries
// that have the same LABEL and VALUE as the new entry. This is done to ensure
// that all entry counters that would contribute to a correct suggestion are
// reinforced.
TEST_F(AddFormFieldValuesTest,
       CreatesNewEntryAndUpdatesCountOfExistingEntryOnIdenticalLabelAndValue) {
  FormFieldData field1 = CreateDefaultField();
  FormFieldData field2 = CreateTestFormField(
      kDefaultLabel, u"name", kDefaultValue, FormControlType::kInputText);

  ASSERT_TRUE(SubmitFormField(field1));
  ASSERT_TRUE(SubmitFormField(field2));

  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(
                field1.name(), field1.label(), field1.value()),
            2);
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(
                field2.name(), field2.label(), field2.value()),
            1);
}

// If we add a new entry, we should update the counts of all existing entries
// that have the same NAME and VALUE as the new entry. This is done to ensure
// that all entry counters that would contribute to a correct suggestion are
// reinforced.
TEST_F(AddFormFieldValuesTest,
       CreatesNewEntryAndUpdatesCountOfExistingEntryOnIdenticalNameAndValue) {
  FormFieldData field1 = CreateDefaultField();
  FormFieldData field2 = CreateTestFormField(
      u"Some label", kDefaultName, kDefaultValue, FormControlType::kInputText);

  ASSERT_TRUE(SubmitFormField(field1));
  ASSERT_TRUE(SubmitFormField(field2));

  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(
                field1.name(), field1.label(), field1.value()),
            2);
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(
                field2.name(), field2.label(), field2.value()),
            1);
}

// Checks that AddFormFieldValues updates counts of relevant existing entries
// when several fields with identical labels and values but different names are
// submitted.
TEST_F(AddFormFieldValuesTest, UpdatesCountsOfSeveralExistingEntries) {
  FormFieldData field1 = CreateTestFormField(
      kDefaultLabel, u"your_name", kDefaultValue, FormControlType::kInputText);
  FormFieldData field2 = CreateTestFormField(
      kDefaultLabel, u"name", kDefaultValue, FormControlType::kInputText);
  FormFieldData field3 = CreateTestFormField(
      kDefaultLabel, u"yn", kDefaultValue, FormControlType::kInputText);

  // Should insert entry 1.
  ASSERT_TRUE(SubmitFormField(field1));

  // Should increase count of entry 1 and add entry 2.
  ASSERT_TRUE(SubmitFormField(field2));

  // Should increase count of entry 1 and 2 and add entry 3.
  ASSERT_TRUE(SubmitFormField(field3));

  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(
                field1.name(), field1.label(), field1.value()),
            3);
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(
                field2.name(), field2.label(), field2.value()),
            2);
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(
                field3.name(), field3.label(), field3.value()),
            1);
}

// Checks if AddFormFieldValues stores and queries the value of an autocomplete
// entry in a case-sensitive manner.
TEST_F(AddFormFieldValuesTest, StoresDataCaseSensitive) {
  ASSERT_TRUE(CreateAndSubmitDefaultFieldWithValue(u"Clark Kent").has_value());
  ASSERT_TRUE(CreateAndSubmitDefaultFieldWithValue(u"Clark KENT").has_value());

  EXPECT_TRUE(
      DoesAutocompleteEntryExist(kDefaultName, kDefaultLabel, u"Clark Kent"));
  EXPECT_TRUE(
      DoesAutocompleteEntryExist(kDefaultName, kDefaultLabel, u"Clark KENT"));
  EXPECT_FALSE(
      DoesAutocompleteEntryExist(kDefaultName, kDefaultLabel, u"clark kent"));
}

// Checks if AddFormFieldValues stores empty and whitespace values as is.
TEST_F(AddFormFieldValuesTest, StoresEmptyValuesAsIs) {
  std::optional<FormFieldData> optional_empty_field =
      CreateAndSubmitDefaultFieldWithValue(u"");
  ASSERT_TRUE(optional_empty_field.has_value());
  std::optional<FormFieldData> optional_whitespace_field =
      CreateAndSubmitDefaultFieldWithValue(u"   ");
  ASSERT_TRUE(optional_whitespace_field.has_value());

  EXPECT_EQ(
      GetAutocompleteEntryLabelSensitiveCount(
          kDefaultName, kDefaultLabel, optional_empty_field.value().value()),
      1);
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(
                kDefaultName, kDefaultLabel,
                optional_whitespace_field.value().value()),
            1);
}

// Checks if AddFormFieldValues inserts null-terminated values as is in the
// database.
TEST_F(AddFormFieldValuesTest, InsertsNullTerminatedValuesAsIs) {
  const std::u16string kValueNullTerminated(kDefaultValue,
                                            std::size(kDefaultValue));
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());
  ASSERT_TRUE(
      CreateAndSubmitDefaultFieldWithValue(kValueNullTerminated).has_value());

  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue),
            1);
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kValueNullTerminated),
            1);
}

// Checks if AddFormFieldValues ignores a field if the name AND/OR label
// of the field already appeared in the form before.
TEST_F(AddFormFieldValuesTest, IgnoresIdenticalNameOrLabel) {
  // Will be added to the database.
  auto field1 = test::CreateTestFormField(u"label", u"name", u"Superman",
                                          FormControlType::kInputText);

  // Ignored due to an identical name.
  auto field2 = test::CreateTestFormField(u"label123", u"name", u"Superman",
                                          FormControlType::kInputText);

  // Ignored due to an identical label.
  auto field3 = test::CreateTestFormField(u"label", u"name123", u"Superman",
                                          FormControlType::kInputText);

  // Ignored due to identical name and label.
  auto field4 = test::CreateTestFormField(u"label", u"name", u"Superman",
                                          FormControlType::kInputText);

  ASSERT_TRUE(SubmitFormFields({field1, field2, field3, field4}));

  EXPECT_TRUE(DoesAutocompleteEntryExist(field1.name(), field1.label(),
                                           field1.value()));
}

// Checks if AddFormFieldValues inserts at most 256 entries.
TEST_F(AddFormFieldValuesTest, InsertsAtMost256Entries) {
  std::vector<FormFieldData> elements;
  for (int i = 0; i < 300; ++i) {
    elements.push_back(test::CreateTestFormField(
        u"name" + base::NumberToString16(i),
        u"label" + base::NumberToString16(i),
        u"Superman" + base::NumberToString16(i), FormControlType::kInputText));
  }
  ASSERT_TRUE(SubmitFormFields(elements));

  EXPECT_EQ(AutocompleteEntriesCount(), 256U);
}

// AddFormFieldValues should insert a new entry with the correct normalized
// label.
TEST_F(AddFormFieldValuesTest, InsertsWithCorrectNormalizedLabel) {
  FormFieldData field =
      CreateTestFormField(u" !!!YOuR NaME:!!! ", kDefaultName, kDefaultValue,
                          FormControlType::kInputText);
  ASSERT_TRUE(SubmitFormField(field));

  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveLabelNormalized(
                field.name(), field.label(), field.value()),
            u"your name");
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
// suggestion for the given name/label and empty value prefix.
TEST_F(GetFormValuesForElementNameAndLabelTest, ReturnsTopSuggestion) {
  // Add 3 entries.
  std::optional<FormFieldData> optional_field = CreateAndSubmitDefaultField();
  ASSERT_TRUE(CreateAndSubmitDefaultFieldWithValue(u"Clark Kent").has_value());
  ASSERT_TRUE(optional_field.has_value());
  ASSERT_TRUE(
      CreateAndSubmitDefaultFieldWithValue(u"Clark Sutter").has_value());

  // Reinforce the first entry, which should make it the top suggestion.
  ASSERT_TRUE(SubmitFormField(optional_field.value()));

  std::vector<AutocompleteSearchResultLabelSensitive> entries;
  ASSERT_TRUE(table().GetFormValuesForElementNameAndLabel(
      kDefaultName, kDefaultLabel, /*prefix=*/std::u16string(), /*limit=*/1,
      entries));

  EXPECT_THAT(entries, ElementsAre(EqualsSearchResult(kDefaultValue, 2)));
}

// When asked for multiple results, GetFormValuesForElementNameAndLabel returns
// them in the correct order based on the frequency of previous submissions.
TEST_F(GetFormValuesForElementNameAndLabelTest,
       ReturnsMultipleSuggestionsInCorrectOrder) {
  FormFieldData field1 = CreateDefaultFieldWithValue(u"Clark Kent");
  FormFieldData field2 = CreateDefaultFieldWithValue(u"Clark Sutter");
  // Add 2 entries: one with count 1, the other with count 2.
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

// GetFormValuesForElementNameAndLabel normalizes the label before querying the
// database.
TEST_F(GetFormValuesForElementNameAndLabelTest, NormalizesLabelBeforeQuerying) {
  FormFieldData field = CreateTestFormField(
      u"test label", kDefaultName, kDefaultValue, FormControlType::kInputText);
  ASSERT_TRUE(SubmitFormField(field));

  std::vector<AutocompleteSearchResultLabelSensitive> entries;
  ASSERT_TRUE(table().GetFormValuesForElementNameAndLabel(
      kDefaultName, u"....Test LaBeL!!!!:   ", kDefaultValue, /*limit=*/10,
      entries));

  EXPECT_THAT(entries, ElementsAre(EqualsSearchResult(kDefaultValue, 1)));
}

using GetCountOfValuesContainedBetweenTest =
    AutocompleteTableLabelSensitiveTest;

// Adds several entries to the database with different timestamps and expects
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

// Adds several entries to the database with different timestamps and expects
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

// Adds several entries to the database with different timestamps. Calls
// GetCountOfValuesContainedBetween with an interval such that some entries are
// contained and some are not. Expects the number of entries contained in the
// time interval provided to be returned.
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

// Adds several entries to the database with different timestamps. Calls
// GetCountOfValuesContainedBetween with the interval [0, MAX_VALUE).
// Expects all entries to be returned.
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

// GetCountOfValuesContainedBetween should treat the provided interval as
// closed-open (i.e., include the beginning and exclude the end). Both the
// entry's creation and update time should be in the interval to be counted. An
// interval [1, 5) should not contain an entry with a creation/update timespan
// of [0, 1].
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

// GetCountOfValuesContainedBetween should treat the provided interval as
// closed-open (i.e., include the beginning and exclude the end). Both the
// entry's creation and update time should be in the interval to be counted. An
// interval [1, 5) should contain an entry with a creation/update timespan of
// [1, 1].
TEST_F(GetCountOfValuesContainedBetweenTest,
       ShouldIncludeIfCreateAndUpdateEqualsBegin) {
  const Time begin = base::Time::Now();

  AdvanceClock(base::Seconds(1));
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  EXPECT_EQ(table().GetCountOfValuesContainedBetween(begin + base::Seconds(1),
                                                     begin + base::Seconds(5)),
            1);
}

// GetCountOfValuesContainedBetween should treat the provided interval as
// closed-open (i.e., include the beginning and exclude the end). Both the
// entry's creation and update time should be in the interval to be counted. An
// interval [1, 5) should contain an entry with a creation/update timespan of
// [1, 5].
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

// GetCountOfValuesContainedBetween should treat the provided interval as
// closed-open (i.e., include the beginning and exclude the end). Both the
// entry's creation and update time should be in the interval to be counted. An
// interval [1, 5) should not contain an entry with a creation/update timespan
// of [5, 5].
TEST_F(GetCountOfValuesContainedBetweenTest,
       ShouldNotIncludeIfCreateAndUpdateEqualsEnd) {
  const Time begin = base::Time::Now();

  AdvanceClock(base::Seconds(5));
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  EXPECT_EQ(table().GetCountOfValuesContainedBetween(begin + base::Seconds(1),
                                                     begin + base::Seconds(5)),
            0);
}

// GetCountOfValuesContainedBetween should treat the provided interval as
// closed-open (i.e., include the beginning and exclude the end). Both the
// entry's creation and update time should be in the interval to be counted. An
// interval [1, 5) should not contain an entry with a creation/update timespan
// of [5, 6].
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

// Adds an entry to the database at a specified timestamp and expects it to be
// removed by RemoveFormElementsAddedBetween when the entry's timestamp is in
// the time range provided.
TEST_F(RemoveFormElementsAddedBetweenTest,
       RemovesEntryAddedDuringTheSpecifiedRange) {
  const Time begin = base::Time::Now();

  AdvanceClock(base::Seconds(1));
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  ASSERT_TRUE(
      table().RemoveFormElementsAddedBetween(begin, begin + base::Seconds(2)));

  EXPECT_FALSE(
      DoesAutocompleteEntryExist(kDefaultName, kDefaultLabel, kDefaultValue));
}

// Adds an entry to the database at a specified timestamp and expects it not to
// be removed by RemoveFormElementsAddedBetween when the entry's timestamp is
// outside the time range provided.
TEST_F(RemoveFormElementsAddedBetweenTest,
       DoesNotRemoveEntryAddedOutsideTheRange) {
  const Time begin = base::Time::Now();
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin + base::Seconds(10), begin + base::Seconds(20)));

  EXPECT_TRUE(
      DoesAutocompleteEntryExist(kDefaultName, kDefaultLabel, kDefaultValue));
}

// Adds multiple entries to the database with specified timestamps and expects
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

  ASSERT_TRUE(
      table().RemoveFormElementsAddedBetween(begin, begin + base::Seconds(10)));

  EXPECT_FALSE(
      DoesAutocompleteEntryExist(kDefaultName, kDefaultLabel, kDefaultValue));
  EXPECT_FALSE(DoesAutocompleteEntryExist(
      kDefaultName, kDefaultLabel, optional_second_field.value().value()));
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

  ASSERT_TRUE(
      table().RemoveFormElementsAddedBetween(begin, begin + base::Seconds(10)));

  EXPECT_FALSE(
      DoesAutocompleteEntryExist(kDefaultName, kDefaultLabel, kDefaultValue));
}

// RemoveFormElementsAddedBetween should update an entry's last update time when
// it was added outside of the provided time range, but was updated during the
// provided time range.
TEST_F(RemoveFormElementsAddedBetweenTest,
       UpdatesEntryAddedBeforeAndUpdatedDuringTheRange) {
  const Time begin = base::Time::Now();
  std::optional<FormFieldData> optional_field = CreateAndSubmitDefaultField();
  ASSERT_TRUE(optional_field.has_value());

  AdvanceClock(base::Seconds(10));
  ASSERT_TRUE(SubmitFormField(optional_field.value()));

  // Asserts that the entry was created with a last update timestamp equal to
  // 10s.
  ASSERT_THAT(table().GetAutocompleteEntryLabelSensitive(
                  kDefaultName, kDefaultLabel, kDefaultValue),
              MakeAutocompleteEntryLabelSensitiveForTest(
                  kDefaultName, kDefaultLabel, kDefaultValue,
                  /*date_created=*/begin,
                  /*date_last_used=*/begin + base::Seconds(10)));

  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin + base::Seconds(5), begin + base::Seconds(15)));

  // Expects that the entry's last update time was updated to 4s (5s is the
  // start of the removal time range minus 1s due to the removal time range
  // being closed-open).
  EXPECT_THAT(table().GetAutocompleteEntryLabelSensitive(
                  kDefaultName, kDefaultLabel, kDefaultValue),
              MakeAutocompleteEntryLabelSensitiveForTest(
                  kDefaultName, kDefaultLabel, kDefaultValue,
                  /*date_created=*/begin,
                  /*date_last_used=*/begin + base::Seconds(4)));
}

// RemoveFormElementsAddedBetween should update an entry's creation time when it
// was added inside of the provided time range, but was updated outside of the
// provided time range.
TEST_F(RemoveFormElementsAddedBetweenTest,
       UpdatesEntryAddedDuringAndUpdatedAfterTheRange) {
  const Time begin = base::Time::Now();

  AdvanceClock(base::Seconds(10));
  std::optional<FormFieldData> optional_field = CreateAndSubmitDefaultField();
  ASSERT_TRUE(optional_field.has_value());

  AdvanceClock(base::Seconds(10));
  ASSERT_TRUE(SubmitFormField(optional_field.value()));

  // Asserts that the entry was created with a creation timestamp equal to 10s.
  ASSERT_THAT(table().GetAutocompleteEntryLabelSensitive(
                  kDefaultName, kDefaultLabel, kDefaultValue),
              MakeAutocompleteEntryLabelSensitiveForTest(
                  kDefaultName, kDefaultLabel, kDefaultValue,
                  /*date_created=*/begin + base::Seconds(10),
                  /*date_last_used=*/begin + base::Seconds(20)));

  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin + base::Seconds(5), begin + base::Seconds(15)));

  // Expects that the entry's creation time was updated to 15s.
  EXPECT_THAT(table().GetAutocompleteEntryLabelSensitive(
                  kDefaultName, kDefaultLabel, kDefaultValue),
              MakeAutocompleteEntryLabelSensitiveForTest(
                  kDefaultName, kDefaultLabel, kDefaultValue,
                  /*date_created=*/begin + base::Seconds(15),
                  /*date_last_used=*/begin + base::Seconds(20)));
}

// Adds two entries to the database. Calls RemoveFormElementsAddedBetween with
// a range such that the first entry is fully inside the range, and the second
// entry is partially inside the range. Expects one entry to be REMOVED and the
// other one to be UPDATED.
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

  ASSERT_TRUE(
      table().RemoveFormElementsAddedBetween(begin, begin + base::Seconds(12)));

  EXPECT_FALSE(DoesAutocompleteEntryExist(kDefaultName, kDefaultLabel,
                                            optional_field1.value().value()));
  EXPECT_THAT(table().GetAutocompleteEntryLabelSensitive(
                  kDefaultName, kDefaultLabel, optional_field2.value().value()),
              MakeAutocompleteEntryLabelSensitiveForTest(
                  kDefaultName, kDefaultLabel, optional_field2.value().value(),
                  /*date_created=*/begin + base::Seconds(12),
                  /*date_last_used=*/begin + base::Seconds(15)));
}

// Adds and updates an entry every X seconds. Calls
// RemoveFormElementsAddedBetween with a range that covers half of the entry's
// [create, last_update] span. Expects the use counter to be interpolated to
// half of the original number.
TEST_F(RemoveFormElementsAddedBetweenTest, UpdatesCountCorrectly) {
  const Time begin = base::Time::Now();
  FormFieldData field = CreateDefaultField();

  AdvanceClock(base::Seconds(10));

  for (int i = 0; i < 10; ++i) {
    ASSERT_TRUE(SubmitFormField(field));
    AdvanceClock(base::Seconds(1));
  }

  // Sanity check.
  ASSERT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue),
            10);

  // The element had 10 uses between timestamp 10 (exclusive) and 19
  // (inclusive). Remove entries that were submitted between the 5th second
  // (inclusive) and the 15th second (exclusive). This corresponds to 5 uses in
  // reality. The database applies linear interpolation and also decreases the
  // use counter by 5.
  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin + base::Seconds(5), begin + base::Seconds(15)));

  // The number of usages should be half of the original number.
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue),
            5);
}

// As we store only creation and last update timestamps,
// RemoveFormElementsAddedBetween assumes that all updates during a given time
// range appeared uniformly distributed in time. This test adds and updates the
// same entry multiple times with the same timestamp, then makes another update
// X seconds later. This means that all except the last update happened at the
// beginning of the [create, last_update] timespan. Nevertheless, a call to
// RemoveFormElementsAddedBetween with a time range that covers half of the
// aforementioned timespan would still halve the count instead of decreasing it
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

  // Sanity check.
  ASSERT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue),
            4);

  // Remove half of the entry's timespan (up to second 12 inclusive).
  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin + base::Seconds(5), begin + base::Seconds(12)));

  // The number of usages should be half of the original number.
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue),
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

  // Sanity check.
  ASSERT_EQ(10, GetAutocompleteEntryLabelSensitiveCount(
                    kDefaultName, kDefaultLabel, kDefaultValue));

  // Remove half of the entry's span.
  ASSERT_TRUE(table().RemoveFormElementsAddedBetween(
      begin + base::Seconds(15), begin + base::Seconds(30)));

  // The number of usages should be half of the original number.
  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveCount(kDefaultName, kDefaultLabel,
                                                    kDefaultValue),
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
      base::Time(), base::Time::Now() - base::Days(30)));

  EXPECT_FALSE(
      DoesAutocompleteEntryExist(kDefaultName, kDefaultLabel, kDefaultValue));
}

using RemoveFormElementTest = AutocompleteTableLabelSensitiveTest;

// RemoveFormElement should remove a specified entry from the database.
TEST_F(RemoveFormElementTest, RemovesEntry) {
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  ASSERT_TRUE(
      table().RemoveFormElement(kDefaultName, kDefaultLabel, kDefaultValue));

  EXPECT_THAT(table().GetAutocompleteEntryLabelSensitive(
                  kDefaultName, kDefaultLabel, kDefaultValue),
              Eq(std::nullopt));
}

// RemoveFormElement should do nothing if the entry does not exist.
TEST_F(RemoveFormElementTest, DoesNothingIfEntryDoesNotExist) {
  // The database stores timestamps with second precision. The test needs to
  // do the same to be able to compare entries.
  base::Time seconds_precision_now = base::Time::FromSecondsSinceUnixEpoch(
      base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000);

  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  ASSERT_TRUE(
      table().RemoveFormElement(kDefaultName, kDefaultLabel, u"Wrong Value"));

  EXPECT_THAT(table().GetAutocompleteEntryLabelSensitive(
                  kDefaultName, kDefaultLabel, kDefaultValue),
              Optional(AutocompleteEntryLabelSensitive(
                  AutocompleteKeyLabelSensitive(kDefaultName, kDefaultLabel,
                                                kDefaultValue),
                  seconds_precision_now, seconds_precision_now)));
}

// RemoveFormElement should match entries to be removed by the normalized label.
TEST_F(RemoveFormElementTest, NormalizesLabelBeforeRemoving) {
  // All three fields have the same normalized label.
  FormFieldData field1 = CreateTestFormField(
      u"label", kDefaultName, kDefaultValue, FormControlType::kInputText);
  FormFieldData field2 = CreateTestFormField(
      u"   label  ", kDefaultName, kDefaultValue, FormControlType::kInputText);
  FormFieldData field3 =
      CreateTestFormField(u"!!!!!LaBeL!!!!!:", kDefaultName, kDefaultValue,
                          FormControlType::kInputText);
  ASSERT_TRUE(SubmitFormFields({field1, field2, field3}));

  ASSERT_TRUE(
      table().RemoveFormElement(kDefaultName, field1.label(), kDefaultValue));

  EXPECT_EQ(AutocompleteEntriesCount(), 0U);
}

using RemoveExpiredFormElementsTest = AutocompleteTableLabelSensitiveTest;

// Adds an entry and advances the clock to make it expired.
// RemoveExpiredFormElements should remove the entry.
TEST_F(RemoveExpiredFormElementsTest, RemovesExpiredEntries) {
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());
  AdvanceClock(2 * autofill::kAutocompleteRetentionPolicyPeriod);

  ASSERT_TRUE(table().RemoveExpiredFormElements());

  EXPECT_FALSE(
      DoesAutocompleteEntryExist(kDefaultName, kDefaultLabel, kDefaultValue));
}

// Adds an entry and advances the clock but not enough to make it expired.
// RemoveExpiredFormElements should not remove the entry.
TEST_F(RemoveExpiredFormElementsTest, DoesNotRemoveNonExpiredEntries) {
  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());
  ASSERT_LT(base::Days(2), autofill::kAutocompleteRetentionPolicyPeriod);
  AdvanceClock(base::Days(2));

  ASSERT_TRUE(table().RemoveExpiredFormElements());

  EXPECT_TRUE(
      DoesAutocompleteEntryExist(kDefaultName, kDefaultLabel, kDefaultValue));
}

using GetAllAutocompleteEntriesTest = AutocompleteTableLabelSensitiveTest;

// If the database is empty, GetAllAutocompleteEntries should return no results.
TEST_F(GetAllAutocompleteEntriesTest, ReturnsNoResults) {
  std::vector<AutocompleteEntryLabelSensitive> entries;

  ASSERT_TRUE(table().GetAllAutocompleteEntries(&entries));

  EXPECT_EQ(entries.size(), 0U);
}

// If the database contains one entry, GetAllAutocompleteEntries should return
// it.
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

// If the database contains two distinct entries, GetAllAutocompleteEntries
// should return both of them.
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

using GetAutocompleteEntryLabelSensitiveTest =
    AutocompleteTableLabelSensitiveTest;

// If the database contains a specific entry (with a given name, label, and
// value), GetAutocompleteEntryLabelSensitive should return it.
TEST_F(GetAutocompleteEntryLabelSensitiveTest, ReturnsEntry) {
  // The database stores timestamps with second precision. The test needs to
  // do the same to be able to compare entries.
  base::Time seconds_precision_now = base::Time::FromSecondsSinceUnixEpoch(
      base::Time::Now().InMillisecondsSinceUnixEpoch() / 1000);

  ASSERT_TRUE(CreateAndSubmitDefaultField().has_value());

  std::optional<AutocompleteEntryLabelSensitive> entry =
      table().GetAutocompleteEntryLabelSensitive(kDefaultName, kDefaultLabel,
                                                 kDefaultValue);

  EXPECT_THAT(entry, Optional(AutocompleteEntryLabelSensitive(
                         AutocompleteKeyLabelSensitive(
                             kDefaultName, kDefaultLabel, kDefaultValue),
                         seconds_precision_now, seconds_precision_now)));
}

// If the database does not contain a specific entry (with a given name, label,
// and value), GetAutocompleteEntryLabelSensitive should return nullopt.
TEST_F(GetAutocompleteEntryLabelSensitiveTest,
       ReturnsNulloptIfEntryDoesNotExist) {
  std::optional<AutocompleteEntryLabelSensitive> entry =
      table().GetAutocompleteEntryLabelSensitive(kDefaultName, kDefaultLabel,
                                                 kDefaultValue);

  EXPECT_FALSE(entry.has_value());
}

// Verify label normalization:
//   * Remove leading and trailing non-alphanumeric characters.
//   * Convert to lowercase.
//   * Cap normalized string length at 50 characters.
//   * Ensure ICU awareness for proper handling of emojis, Kanji, and other
//   scripts.
TEST_F(AutocompleteTableLabelSensitiveTest, CorrectlyNormalizesLabel) {
  FormFieldData field = CreateTestFormField(
      u" !!!Long and!Case SeNsItIvE❤️Label 你好世界日本語안녕하세요Γειά σου "
      u"🌐✨:   ",
      kDefaultName, kDefaultValue, FormControlType::kInputText);
  ASSERT_TRUE(SubmitFormField(field));

  EXPECT_EQ(GetAutocompleteEntryLabelSensitiveLabelNormalized(
                field.name(), field.label(), field.value())
                .value(),
            u"long and!case sensitive❤️label 你好世界日本語안녕하세요γειά σο");
}

// Poisons the database and checks that we don't crash when adding a value.
TEST_F(AutocompleteTableLabelSensitiveTest,
       DontCrashWhenAddingValueToPoisonedDB) {
  // Simulates a preceding fatal error.
  db().GetSQLConnection()->Poison();

  // Simulates the submission of a form.
  FormFieldData field = CreateDefaultField();
  EXPECT_FALSE(table().AddFormFieldValues({field}));
}

}  // namespace

}  // namespace autofill
