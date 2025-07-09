// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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

using AutocompleteEntryLabelSensitiveSet =
    std::set<AutocompleteEntryLabelSensitive,
             bool (*)(const AutocompleteEntryLabelSensitive&,
                      const AutocompleteEntryLabelSensitive&)>;
using base::Time;
using testing::ElementsAre;

bool CompareAutocompleteEntries(const AutocompleteEntryLabelSensitive& a,
                                const AutocompleteEntryLabelSensitive& b) {
  return std::tie(a.key().name(), a.key().value(), a.date_created(),
                  a.date_last_used()) <
         std::tie(b.key().name(), b.key().value(), b.date_created(),
                  b.date_last_used());
}

AutocompleteEntryLabelSensitive MakeAutocompleteEntryLabelSensitive(
    const std::u16string& name,
    const std::u16string& value,
    time_t date_created,
    time_t date_last_used) {
  if (date_last_used < 0) {
    date_last_used = date_created;
  }
  return AutocompleteEntryLabelSensitive(
      AutocompleteKeyLabelSensitive(name, value), Time::FromTimeT(date_created),
      Time::FromTimeT(date_last_used));
}

// Checks |actual| and |expected| contain the same elements.
void CompareAutocompleteEntryLabelSensitiveSets(
    const AutocompleteEntryLabelSensitiveSet& actual,
    const AutocompleteEntryLabelSensitiveSet& expected) {
  ASSERT_EQ(expected.size(), actual.size());
  size_t count = 0;
  for (const auto& it : actual) {
    count += expected.count(it);
  }
  EXPECT_EQ(actual.size(), count);
}

int GetAutocompleteEntryLabelSensitiveCount(const std::u16string& name,
                                            const std::u16string& value,
                                            WebDatabase* db) {
  sql::Statement s(db->GetSQLConnection()->GetUniqueStatement(
      "SELECT count FROM autofill WHERE name = ? AND value = ?"));
  s.BindString16(0, name);
  s.BindString16(1, value);
  if (!s.Step()) {
    return 0;
  }
  return s.ColumnInt(0);
}

class AutocompleteTableLabelSensitiveTest : public testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");
    table_ = std::make_unique<AutocompleteTableLabelSensitive>();
    db_ = std::make_unique<WebDatabase>();
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK, db_->Init(file_));
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

  WebDatabase& db() { return *db_; }
  AutocompleteTableLabelSensitive& table() { return *table_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AutocompleteTableLabelSensitive> table_;
  std::unique_ptr<WebDatabase> db_;
};

TEST_F(AutocompleteTableLabelSensitiveTest, Autocomplete) {
  const base::Time begin = base::Time::Now();

  // Simulate the submission of a handful of entries in a field called "Name",
  // some more often than others.
  AutocompleteChangeLabelSensitiveList changes;
  FormFieldData field;
  field.set_name(u"Name");
  field.set_value(u"Superman");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  std::vector<AutocompleteEntryLabelSensitive> v;
  for (int i = 0; i < 5; ++i) {
    field.set_value(u"Clark Kent");
    EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
    AdvanceClock(base::Seconds(2));
  }
  for (int i = 0; i < 3; ++i) {
    field.set_value(u"Clark Sutter");
    EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
    AdvanceClock(base::Seconds(2));
  }
  for (int i = 0; i < 2; ++i) {
    field.set_name(u"Favorite Color");
    field.set_value(u"Green");
    EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
    AdvanceClock(base::Seconds(2));
  }

  // We have added the name Clark Kent 5 times, so count should be 5.
  EXPECT_EQ(5, GetAutocompleteEntryLabelSensitiveCount(u"Name", u"Clark Kent",
                                                       &db()));

  // Storing in the data base should be case sensitive, so there should be no
  // database entry for clark kent lowercase.
  EXPECT_EQ(0, GetAutocompleteEntryLabelSensitiveCount(u"Name", u"clark kent",
                                                       &db()));

  EXPECT_EQ(2, GetAutocompleteEntryLabelSensitiveCount(u"Favorite Color",
                                                       u"Green", &db()));

  // This is meant to get a list of suggestions for Name.  The empty prefix
  // in the second argument means it should return all suggestions for a name
  // no matter what they start with.  The order that the names occur in the list
  // should be decreasing order by count.
  EXPECT_TRUE(
      table().GetFormValuesForElementName(u"Name", std::u16string(), 6, v));
  EXPECT_EQ(3U, v.size());
  if (v.size() == 3) {
    EXPECT_EQ(u"Clark Kent", v[0].key().value());
    EXPECT_EQ(u"Clark Sutter", v[1].key().value());
    EXPECT_EQ(u"Superman", v[2].key().value());
  }

  // If we query again limiting the list size to 1, we should only get the most
  // frequent entry.
  EXPECT_TRUE(
      table().GetFormValuesForElementName(u"Name", std::u16string(), 1, v));
  EXPECT_EQ(1U, v.size());
  if (v.size() == 1) {
    EXPECT_EQ(u"Clark Kent", v[0].key().value());
  }

  // Querying for suggestions given a prefix is case-insensitive, so the prefix
  // "cLa" should get suggestions for both Clarks.
  EXPECT_TRUE(table().GetFormValuesForElementName(u"Name", u"cLa", 6, v));
  EXPECT_EQ(2U, v.size());
  if (v.size() == 2) {
    EXPECT_EQ(u"Clark Kent", v[0].key().value());
    EXPECT_EQ(u"Clark Sutter", v[1].key().value());
  }

  // Removing all elements since the beginning of this function should remove
  // everything from the database.
  changes.clear();
  EXPECT_TRUE(table().RemoveFormElementsAddedBetween(begin, Time(), changes));

  const auto kExpectedChanges = std::array{
      AutocompleteChangeLabelSensitive(
          AutocompleteChangeLabelSensitive::REMOVE,
          AutocompleteKeyLabelSensitive(u"Name", u"Superman")),
      AutocompleteChangeLabelSensitive(
          AutocompleteChangeLabelSensitive::REMOVE,
          AutocompleteKeyLabelSensitive(u"Name", u"Clark Kent")),
      AutocompleteChangeLabelSensitive(
          AutocompleteChangeLabelSensitive::REMOVE,
          AutocompleteKeyLabelSensitive(u"Name", u"Clark Sutter")),
      AutocompleteChangeLabelSensitive(
          AutocompleteChangeLabelSensitive::REMOVE,
          AutocompleteKeyLabelSensitive(u"Favorite Color", u"Green")),
  };
  EXPECT_EQ(kExpectedChanges.size(), changes.size());
  for (size_t i = 0; i < std::size(kExpectedChanges); ++i) {
    EXPECT_EQ(kExpectedChanges[i], changes[i]);
  }

  EXPECT_EQ(0, GetAutocompleteEntryLabelSensitiveCount(u"Name", u"Clark Kent",
                                                       &db()));

  EXPECT_TRUE(
      table().GetFormValuesForElementName(u"Name", std::u16string(), 6, v));
  EXPECT_EQ(0U, v.size());

  // Now add some values with empty strings.
  const std::u16string kValue = u"  toto   ";
  field.set_name(u"blank");
  field.set_value(std::u16string());
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  field.set_name(u"blank");
  field.set_value(u" ");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  field.set_name(u"blank");
  field.set_value(u"      ");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  field.set_name(u"blank");
  field.set_value(kValue);
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));

  // They should be stored normally as the DB layer does not check for empty
  // values.
  v.clear();
  EXPECT_TRUE(
      table().GetFormValuesForElementName(u"blank", std::u16string(), 10, v));
  EXPECT_EQ(4U, v.size());
}

TEST_F(AutocompleteTableLabelSensitiveTest, Autocomplete_GetEntry_Populated) {
  SetClock(test::kJune2017);

  AutocompleteChangeLabelSensitiveList changes;
  FormFieldData field;
  field.set_name(u"Name");
  field.set_value(u"Superman");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));

  std::vector<AutocompleteEntryLabelSensitive> prefix_v;
  EXPECT_TRUE(table().GetFormValuesForElementName(field.name(), u"Super", 10,
                                                  prefix_v));

  std::vector<AutocompleteEntryLabelSensitive> no_prefix_v;
  EXPECT_TRUE(
      table().GetFormValuesForElementName(field.name(), u"", 10, no_prefix_v));

  AutocompleteEntryLabelSensitive expected_entry(
      AutocompleteKeyLabelSensitive(field.name(), field.value()),
      base::Time::Now(), base::Time::Now());

  EXPECT_THAT(prefix_v, ElementsAre(expected_entry));
  EXPECT_THAT(no_prefix_v, ElementsAre(expected_entry));

  // Update date_last_used.
  AdvanceClock(base::Seconds(1000));
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  EXPECT_TRUE(table().GetFormValuesForElementName(field.name(), u"Super", 10,
                                                  prefix_v));
  EXPECT_TRUE(
      table().GetFormValuesForElementName(field.name(), u"", 10, no_prefix_v));

  expected_entry = AutocompleteEntryLabelSensitive(
      AutocompleteKeyLabelSensitive(field.name(), field.value()),
      expected_entry.date_created(), base::Time::Now());

  EXPECT_THAT(prefix_v, ElementsAre(expected_entry));
  EXPECT_THAT(no_prefix_v, ElementsAre(expected_entry));
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_GetCountOfValuesContainedBetween) {
  AutocompleteChangeLabelSensitiveList changes;
  // This test makes time comparisons that are precise to a microsecond, but the
  // database uses the time_t format which is only precise to a second.
  // Make sure we use timestamps rounded to a second.
  const auto begin = base::Time::Now();

  struct Entry {
    const char16_t* name;
    const char16_t* value;
  } entries[] = {{u"Alter ego", u"Superman"}, {u"Name", u"Superman"},
                 {u"Name", u"Clark Kent"},    {u"Name", u"Superman"},
                 {u"Name", u"Clark Sutter"},  {u"Nomen", u"Clark Kent"}};

  for (Entry entry : entries) {
    FormFieldData field;
    field.set_name(entry.name);
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

TEST_F(AutocompleteTableLabelSensitiveTest, Autocomplete_RemoveBetweenChanges) {
  const base::Time t1 = base::Time::Now();
  const base::Time t2 = t1 + base::Days(1);

  AutocompleteChangeLabelSensitiveList changes;
  FormFieldData field;
  field.set_name(u"Name");
  field.set_value(u"Superman");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  AdvanceClock(base::Days(1));
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));

  changes.clear();
  EXPECT_TRUE(table().RemoveFormElementsAddedBetween(t1, t2, changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::UPDATE,
                AutocompleteKeyLabelSensitive(u"Name", u"Superman")),
            changes[0]);
  changes.clear();

  EXPECT_TRUE(
      table().RemoveFormElementsAddedBetween(t2, t2 + base::Days(1), changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::REMOVE,
                AutocompleteKeyLabelSensitive(u"Name", u"Superman")),
            changes[0]);
}

TEST_F(AutocompleteTableLabelSensitiveTest, Autocomplete_AddChanges) {
  AutocompleteChangeLabelSensitiveList changes;
  FormFieldData field;
  field.set_name(u"Name");
  field.set_value(u"Superman");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::ADD,
                AutocompleteKeyLabelSensitive(u"Name", u"Superman")),
            changes[0]);

  changes.clear();
  AdvanceClock(base::Days(1));
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::UPDATE,
                AutocompleteKeyLabelSensitive(u"Name", u"Superman")),
            changes[0]);
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_UpdateOneWithOneTimestamp) {
  AutocompleteEntryLabelSensitive entry(
      MakeAutocompleteEntryLabelSensitive(u"foo", u"bar", 1, -1));
  std::vector<AutocompleteEntryLabelSensitive> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table().UpdateAutocompleteEntries(entries));

  EXPECT_EQ(1, GetAutocompleteEntryLabelSensitiveCount(u"foo", u"bar", &db()));

  std::vector<AutocompleteEntryLabelSensitive> all_entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_EQ(entry, all_entries[0]);
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_UpdateOneWithTwoTimestamps) {
  AutocompleteEntryLabelSensitive entry(
      MakeAutocompleteEntryLabelSensitive(u"foo", u"bar", 1, 2));
  std::vector<AutocompleteEntryLabelSensitive> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table().UpdateAutocompleteEntries(entries));

  EXPECT_EQ(2, GetAutocompleteEntryLabelSensitiveCount(u"foo", u"bar", &db()));

  std::vector<AutocompleteEntryLabelSensitive> all_entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_EQ(entry, all_entries[0]);
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_GetAutofillTimestamps) {
  AutocompleteEntryLabelSensitive entry(
      MakeAutocompleteEntryLabelSensitive(u"foo", u"bar", 1, 2));
  std::vector<AutocompleteEntryLabelSensitive> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table().UpdateAutocompleteEntries(entries));

  std::optional<AutocompleteEntryLabelSensitive> table_entry =
      table().GetAutocompleteEntryLabelSensitive(u"foo", u"bar");
  ASSERT_TRUE(table_entry);
  EXPECT_EQ(Time::FromTimeT(1), table_entry->date_created());
  EXPECT_EQ(Time::FromTimeT(2), table_entry->date_last_used());
}

TEST_F(AutocompleteTableLabelSensitiveTest, Autocomplete_UpdateTwo) {
  AutocompleteEntryLabelSensitive entry0(
      MakeAutocompleteEntryLabelSensitive(u"foo", u"bar0", 1, -1));
  AutocompleteEntryLabelSensitive entry1(
      MakeAutocompleteEntryLabelSensitive(u"foo", u"bar1", 2, 3));
  std::vector<AutocompleteEntryLabelSensitive> entries;
  entries.push_back(entry0);
  entries.push_back(entry1);
  ASSERT_TRUE(table().UpdateAutocompleteEntries(entries));

  EXPECT_EQ(1, GetAutocompleteEntryLabelSensitiveCount(u"foo", u"bar0", &db()));
  EXPECT_EQ(2, GetAutocompleteEntryLabelSensitiveCount(u"foo", u"bar1", &db()));
}

TEST_F(AutocompleteTableLabelSensitiveTest, Autocomplete_UpdateNullTerminated) {
  const char16_t kName[] = u"foo";
  const char16_t kValue[] = u"bar";
  // A value which contains terminating character.
  std::u16string value(kValue, std::size(kValue));

  AutocompleteEntryLabelSensitive entry0(
      MakeAutocompleteEntryLabelSensitive(kName, kValue, 1, -1));
  AutocompleteEntryLabelSensitive entry1(
      MakeAutocompleteEntryLabelSensitive(kName, value, 2, 3));
  std::vector<AutocompleteEntryLabelSensitive> entries;
  entries.push_back(entry0);
  entries.push_back(entry1);
  ASSERT_TRUE(table().UpdateAutocompleteEntries(entries));

  EXPECT_EQ(1, GetAutocompleteEntryLabelSensitiveCount(kName, kValue, &db()));
  EXPECT_EQ(2, GetAutocompleteEntryLabelSensitiveCount(kName, value, &db()));

  std::vector<AutocompleteEntryLabelSensitive> all_entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(2U, all_entries.size());
  EXPECT_EQ(entry0, all_entries[0]);
  EXPECT_EQ(entry1, all_entries[1]);
}

TEST_F(AutocompleteTableLabelSensitiveTest, Autocomplete_UpdateReplace) {
  AutocompleteChangeLabelSensitiveList changes;
  // Add a form field.  This will be replaced.
  FormFieldData field;
  field.set_name(u"Name");
  field.set_value(u"Superman");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));

  AutocompleteEntryLabelSensitive entry(
      MakeAutocompleteEntryLabelSensitive(u"Name", u"Superman", 1, 2));
  std::vector<AutocompleteEntryLabelSensitive> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table().UpdateAutocompleteEntries(entries));

  std::vector<AutocompleteEntryLabelSensitive> all_entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_EQ(entry, all_entries[0]);
}

TEST_F(AutocompleteTableLabelSensitiveTest, Autocomplete_UpdateDontReplace) {
  AutocompleteEntryLabelSensitive existing(MakeAutocompleteEntryLabelSensitive(
      u"Name", u"Superman", base::Time::Now().ToTimeT(), -1));

  AutocompleteChangeLabelSensitiveList changes;
  // Add a form field.  This will NOT be replaced.
  FormFieldData field;
  field.set_name(existing.key().name());
  field.set_value(existing.key().value());
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  AutocompleteEntryLabelSensitive entry(
      MakeAutocompleteEntryLabelSensitive(u"Name", u"Clark Kent", 1, 2));
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

TEST_F(AutocompleteTableLabelSensitiveTest, Autocomplete_AddFormFieldValues) {
  // Add multiple values for "firstname" and "lastname" names.  Test that only
  // first value of each gets added. Related to security issue:
  // http://crbug.com/51727.
  std::vector<FormFieldData> elements;
  FormFieldData field;
  field.set_name(u"firstname");
  field.set_value(u"Joe");
  elements.push_back(field);

  field.set_name(u"firstname");
  field.set_value(u"Jane");
  elements.push_back(field);

  field.set_name(u"lastname");
  field.set_value(u"Smith");
  elements.push_back(field);

  field.set_name(u"lastname");
  field.set_value(u"Jones");
  elements.push_back(field);

  std::vector<AutocompleteChangeLabelSensitive> changes;
  table().AddFormFieldValues(elements, &changes);

  ASSERT_EQ(2U, changes.size());
  EXPECT_EQ(changes[0],
            AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::ADD,
                AutocompleteKeyLabelSensitive(u"firstname", u"Joe")));
  EXPECT_EQ(changes[1],
            AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::ADD,
                AutocompleteKeyLabelSensitive(u"lastname", u"Smith")));

  std::vector<AutocompleteEntryLabelSensitive> all_entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(2U, all_entries.size());
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_RemoveFormElementsAddedBetween_UsedOnlyBefore) {
  // Add an entry used only before the targeted range.
  AutocompleteChangeLabelSensitiveList changes;
  FormFieldData field;
  field.set_name(u"Name");
  field.set_value(u"Superman");
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
    AdvanceClock(base::Seconds(10));
  }

  EXPECT_EQ(5, GetAutocompleteEntryLabelSensitiveCount(field.name(),
                                                       field.value(), &db()));

  changes.clear();
  EXPECT_TRUE(table().RemoveFormElementsAddedBetween(
      base::Time::Now() - base::Seconds(9), base::Time::Now(), changes));
  EXPECT_TRUE(changes.empty());
  EXPECT_EQ(5, GetAutocompleteEntryLabelSensitiveCount(field.name(),
                                                       field.value(), &db()));
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_RemoveFormElementsAddedBetween_UsedOnlyAfter) {
  // Add an entry used only after the targeted range.
  AutocompleteChangeLabelSensitiveList changes;
  FormFieldData field;
  field.set_name(u"Name");
  field.set_value(u"Superman");
  for (int i = 0; i < 5; i++) {
    AdvanceClock(base::Seconds(10));
    EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  }

  EXPECT_EQ(5, GetAutocompleteEntryLabelSensitiveCount(field.name(),
                                                       field.value(), &db()));

  changes.clear();
  EXPECT_TRUE(table().RemoveFormElementsAddedBetween(
      base::Time::Now() - base::Seconds(50),
      base::Time::Now() - base::Seconds(41), changes));
  EXPECT_TRUE(changes.empty());
  EXPECT_EQ(5, GetAutocompleteEntryLabelSensitiveCount(field.name(),
                                                       field.value(), &db()));
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_RemoveFormElementsAddedBetween_UsedOnlyDuring) {
  // Add an entry used entirely during the targeted range.
  AutocompleteChangeLabelSensitiveList changes;
  FormFieldData field;
  field.set_name(u"Name");
  field.set_value(u"Superman");
  for (int i = 0; i < 5; i++) {
    EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
    AdvanceClock(base::Seconds(10));
  }

  EXPECT_EQ(5, GetAutocompleteEntryLabelSensitiveCount(field.name(),
                                                       field.value(), &db()));

  changes.clear();
  EXPECT_TRUE(table().RemoveFormElementsAddedBetween(
      base::Time::Now() - base::Seconds(50), base::Time::Now(), changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::REMOVE,
                AutocompleteKeyLabelSensitive(field.name(), field.value())),
            changes[0]);
  EXPECT_EQ(0, GetAutocompleteEntryLabelSensitiveCount(field.name(),
                                                       field.value(), &db()));
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_RemoveFormElementsAddedBetween_UsedBeforeAndDuring) {
  SetClock(test::kJune2017);
  // Add an entry used both before and during the targeted range.
  AutocompleteChangeLabelSensitiveList changes;
  FormFieldData field;
  field.set_name(u"Name");
  field.set_value(u"Superman");
  for (int i = 0; i < 5; i++) {
    AdvanceClock(base::Seconds(10));
    EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  }

  EXPECT_EQ(5, GetAutocompleteEntryLabelSensitiveCount(field.name(),
                                                       field.value(), &db()));

  changes.clear();
  EXPECT_TRUE(table().RemoveFormElementsAddedBetween(
      base::Time::Now() - base::Seconds(10),
      base::Time::Now() + base::Seconds(10), changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::UPDATE,
                AutocompleteKeyLabelSensitive(field.name(), field.value())),
            changes[0]);
  EXPECT_EQ(4, GetAutocompleteEntryLabelSensitiveCount(field.name(),
                                                       field.value(), &db()));
  std::optional<AutocompleteEntryLabelSensitive> entry =
      table().GetAutocompleteEntryLabelSensitive(field.name(), field.value());
  ASSERT_TRUE(entry);
  EXPECT_EQ(base::Time::Now() - base::Seconds(40), entry->date_created());
  EXPECT_EQ(base::Time::Now() - base::Seconds(11), entry->date_last_used());
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_RemoveFormElementsAddedBetween_UsedDuringAndAfter) {
  SetClock(test::kJune2017);
  // Add an entry used both during and after the targeted range.
  AutocompleteChangeLabelSensitiveList changes;
  FormFieldData field;
  field.set_name(u"Name");
  field.set_value(u"Superman");
  for (int i = 0; i < 5; i++) {
    AdvanceClock(base::Seconds(10));
    EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  }

  EXPECT_EQ(5, GetAutocompleteEntryLabelSensitiveCount(field.name(),
                                                       field.value(), &db()));

  changes.clear();
  EXPECT_TRUE(table().RemoveFormElementsAddedBetween(
      base::Time::Now() - base::Seconds(50),
      base::Time::Now() - base::Seconds(10), changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::UPDATE,
                AutocompleteKeyLabelSensitive(field.name(), field.value())),
            changes[0]);
  EXPECT_EQ(2, GetAutocompleteEntryLabelSensitiveCount(field.name(),
                                                       field.value(), &db()));
  std::optional<AutocompleteEntryLabelSensitive> entry =
      table().GetAutocompleteEntryLabelSensitive(field.name(), field.value());
  ASSERT_TRUE(entry);
  EXPECT_EQ(base::Time::Now() - base::Seconds(10), entry->date_created());
  EXPECT_EQ(base::Time::Now(), entry->date_last_used());
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_RemoveFormElementsAddedBetween_OlderThan30Days) {
  // Add some form field entries.
  AutocompleteChangeLabelSensitiveList changes;
  FormFieldData field;
  field.set_name(u"Name");

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
  EXPECT_EQ(AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::REMOVE,
                AutocompleteKeyLabelSensitive(u"Name", u"Clark Sutter")),
            changes[0]);
  EXPECT_EQ(0, GetAutocompleteEntryLabelSensitiveCount(u"Name", u"Clark Sutter",
                                                       &db()));
  EXPECT_EQ(
      1, GetAutocompleteEntryLabelSensitiveCount(u"Name", u"Superman", &db()));
  EXPECT_EQ(1, GetAutocompleteEntryLabelSensitiveCount(u"Name", u"Clark Kent",
                                                       &db()));
  changes.clear();
}

// Tests that we set the change type to EXPIRE for expired elements and we
// delete an old entry.
TEST_F(AutocompleteTableLabelSensitiveTest,
       RemoveExpiredFormElements_Expires_DeleteEntry) {
  AutocompleteChangeLabelSensitiveList changes;
  FormFieldData field;
  field.set_name(u"Name");
  field.set_value(u"Superman");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  AdvanceClock(2 * kAutocompleteRetentionPolicyPeriod);
  changes.clear();

  EXPECT_TRUE(table().RemoveExpiredFormElements(changes));
  EXPECT_EQ(AutocompleteChangeLabelSensitive(
                AutocompleteChangeLabelSensitive::EXPIRE,
                AutocompleteKeyLabelSensitive(field.name(), field.value())),
            changes[0]);
}

// Tests that we don't
// delete non-expired entries' data from the SQLite table.
TEST_F(AutocompleteTableLabelSensitiveTest,
       RemoveExpiredFormElements_NotOldEnough) {
  AutocompleteChangeLabelSensitiveList changes;
  FormFieldData field;
  field.set_name(u"Name");
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
  SetClock(test::kJune2017);
  AutocompleteChangeLabelSensitiveList changes;
  std::map<std::string, std::vector<Time>> name_value_times_map;

  std::vector<Time> timestamps1;
  FormFieldData field;
  field.set_name(u"Name");
  field.set_value(u"Superman");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  timestamps1.push_back(base::Time::Now());
  std::string key1("NameSuperman");
  name_value_times_map.insert(
      std::pair<std::string, std::vector<Time>>(key1, timestamps1));

  AutocompleteEntryLabelSensitiveSet expected_entries(
      CompareAutocompleteEntries);
  AutocompleteKeyLabelSensitive ak1(u"Name", u"Superman");
  AutocompleteEntryLabelSensitive ae1(ak1, timestamps1.front(),
                                      timestamps1.back());

  expected_entries.insert(ae1);

  std::vector<AutocompleteEntryLabelSensitive> entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&entries));
  AutocompleteEntryLabelSensitiveSet entry_set(entries.begin(), entries.end(),
                                               CompareAutocompleteEntries);

  CompareAutocompleteEntryLabelSensitiveSets(entry_set, expected_entries);
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_GetAllAutocompleteEntries_TwoDistinct) {
  SetClock(test::kJune2017);
  AutocompleteChangeLabelSensitiveList changes;
  std::map<std::string, std::vector<Time>> name_value_times_map;

  std::vector<Time> timestamps1;
  FormFieldData field;
  field.set_name(u"Name");
  field.set_value(u"Superman");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  timestamps1.push_back(base::Time::Now());
  std::string key1("NameSuperman");
  name_value_times_map.insert(
      std::pair<std::string, std::vector<Time>>(key1, timestamps1));

  AdvanceClock(base::Seconds(1));
  std::vector<Time> timestamps2;
  field.set_name(u"Name");
  field.set_value(u"Clark Kent");
  EXPECT_TRUE(table().AddFormFieldValues({field}, &changes));
  timestamps2.push_back(base::Time::Now());
  std::string key2("NameClark Kent");
  name_value_times_map.insert(
      std::pair<std::string, std::vector<Time>>(key2, timestamps2));

  AutocompleteEntryLabelSensitiveSet expected_entries(
      CompareAutocompleteEntries);
  AutocompleteKeyLabelSensitive ak1(u"Name", u"Superman");
  AutocompleteKeyLabelSensitive ak2(u"Name", u"Clark Kent");
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

  CompareAutocompleteEntryLabelSensitiveSets(entry_set, expected_entries);
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       Autocomplete_GetAllAutocompleteEntries_TwoSame) {
  SetClock(test::kJune2017);
  AutocompleteChangeLabelSensitiveList changes;
  std::map<std::string, std::vector<Time>> name_value_times_map;

  std::vector<Time> timestamps;
  for (int i = 0; i < 2; ++i) {
    FormFieldData field;
    field.set_name(u"Name");
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
  AutocompleteKeyLabelSensitive ak1(u"Name", u"Superman");
  AutocompleteEntryLabelSensitive ae1(ak1, timestamps.front(),
                                      timestamps.back());

  expected_entries.insert(ae1);

  std::vector<AutocompleteEntryLabelSensitive> entries;
  ASSERT_TRUE(table().GetAllAutocompleteEntries(&entries));
  AutocompleteEntryLabelSensitiveSet entry_set(entries.begin(), entries.end(),
                                               CompareAutocompleteEntries);

  CompareAutocompleteEntryLabelSensitiveSets(entry_set, expected_entries);
}

TEST_F(AutocompleteTableLabelSensitiveTest,
       DontCrashWhenAddingValueToPoisonedDB) {
  // Simulate a preceding fatal error.
  db().GetSQLConnection()->Poison();

  // Simulate the submission of a form.
  AutocompleteChangeLabelSensitiveList changes;
  FormFieldData field;
  field.set_name(u"Name");
  field.set_value(u"Superman");
  EXPECT_FALSE(table().AddFormFieldValues({field}, &changes));
}

}  // namespace

}  // namespace autofill
