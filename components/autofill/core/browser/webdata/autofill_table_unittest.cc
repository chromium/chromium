// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/browser/field_types.h"

#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>

#include "base/command_line.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "base/uuid.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_metadata.h"
#include "components/autofill/core/browser/data_model/autofill_offer_data.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/autofill_wallet_usage_data.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/profile_token_quality.h"
#include "components/autofill/core/browser/profile_token_quality_test_api.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autocomplete_entry.h"
#include "components/autofill/core/browser/webdata/autofill_change.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_switches.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/os_crypt/sync/os_crypt_mocker.h"
#include "components/sync/model/metadata_batch.h"
#include "components/sync/protocol/entity_metadata.pb.h"
#include "components/sync/protocol/model_type_state.pb.h"
#include "components/webdata/common/web_database.h"
#include "sql/statement.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/origin.h"

using base::Time;
using sync_pb::EntityMetadata;
using sync_pb::ModelTypeState;
using syncer::EntityMetadataMap;
using syncer::MetadataBatch;
using testing::ElementsAre;
using testing::UnorderedElementsAre;

namespace autofill {

// So we can compare AutocompleteKeys with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutocompleteKey& key) {
  return os << base::UTF16ToASCII(key.name()) << ", "
            << base::UTF16ToASCII(key.value());
}

// So we can compare AutocompleteChanges with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const AutocompleteChange& change) {
  switch (change.type()) {
    case AutocompleteChange::ADD: {
      os << "ADD";
      break;
    }
    case AutocompleteChange::UPDATE: {
      os << "UPDATE";
      break;
    }
    case AutocompleteChange::REMOVE: {
      os << "REMOVE";
      break;
    }
    case AutocompleteChange::EXPIRE: {
      os << "EXPIRED";
      break;
    }
  }
  return os << " " << change.key();
}

namespace {

typedef std::set<AutocompleteEntry,
                 bool (*)(const AutocompleteEntry&, const AutocompleteEntry&)>
    AutocompleteEntrySet;
typedef AutocompleteEntrySet::iterator AutocompleteEntrySetIterator;

bool CompareAutocompleteEntries(const AutocompleteEntry& a,
                                const AutocompleteEntry& b) {
  return std::tie(a.key().name(), a.key().value(), a.date_created(),
                  a.date_last_used()) <
         std::tie(b.key().name(), b.key().value(), b.date_created(),
                  b.date_last_used());
}

AutocompleteEntry MakeAutocompleteEntry(const std::u16string& name,
                                        const std::u16string& value,
                                        time_t date_created,
                                        time_t date_last_used) {
  if (date_last_used < 0)
    date_last_used = date_created;
  return AutocompleteEntry(AutocompleteKey(name, value),
                           Time::FromTimeT(date_created),
                           Time::FromTimeT(date_last_used));
}

// Checks |actual| and |expected| contain the same elements.
void CompareAutocompleteEntrySets(const AutocompleteEntrySet& actual,
                                  const AutocompleteEntrySet& expected) {
  ASSERT_EQ(expected.size(), actual.size());
  size_t count = 0;
  for (auto it = actual.begin(); it != actual.end(); ++it) {
    count += expected.count(*it);
  }
  EXPECT_EQ(actual.size(), count);
}

int GetAutocompleteEntryCount(const std::u16string& name,
                              const std::u16string& value,
                              WebDatabase* db) {
  sql::Statement s(db->GetSQLConnection()->GetUniqueStatement(
      "SELECT count FROM autofill WHERE name = ? AND value = ?"));
  s.BindString16(0, name);
  s.BindString16(1, value);
  if (!s.Step())
    return 0;
  return s.ColumnInt(0);
}

}  // namespace

class AutofillTableTest : public testing::Test {
 public:
  AutofillTableTest() = default;
  AutofillTableTest(const AutofillTableTest&) = delete;
  AutofillTableTest& operator=(const AutofillTableTest&) = delete;
  ~AutofillTableTest() override = default;

 protected:
  void SetUp() override {
    OSCryptMocker::SetUp();
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    file_ = temp_dir_.GetPath().AppendASCII("TestWebDatabase");

    table_ = std::make_unique<AutofillTable>();
    db_ = std::make_unique<WebDatabase>();
    db_->AddTable(table_.get());
    ASSERT_EQ(sql::INIT_OK, db_->Init(file_));
  }

  void TearDown() override { OSCryptMocker::TearDown(); }

  // Get date_modifed `column` of `table_name` with specific `instrument_id` or
  // `guid`.
  time_t GetDateModified(base::StringPiece table_name,
                         base::StringPiece column,
                         absl::variant<std::string, int64_t> id) {
    sql::Statement s(db_->GetSQLConnection()->GetUniqueStatement(
        base::StrCat({"SELECT ", column, " FROM ", table_name, " WHERE ",
                      absl::holds_alternative<std::string>(id)
                          ? "guid"
                          : "instrument_id",
                      " = ?"})
            .c_str()));
    if (const std::string* guid = absl::get_if<std::string>(&id)) {
      s.BindString(0, *guid);
    } else {
      s.BindInt64(0, absl::get<int64_t>(id));
    }
    EXPECT_TRUE(s.Step());
    return s.ColumnInt64(0);
  }

  base::FilePath file_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<AutofillTable> table_;
  std::unique_ptr<WebDatabase> db_;
};

// Tests for the AutofillProfil CRUD interface are tested with both profile
// sources.
class AutofillTableProfileTest
    : public AutofillTableTest,
      public testing::WithParamInterface<AutofillProfile::Source> {
 public:
  void SetUp() override {
    AutofillTableTest::SetUp();
    features_.InitWithFeatures(
        {features::kAutofillEnableSupportForLandmark,
         features::kAutofillEnableSupportForBetweenStreets,
         features::kAutofillEnableSupportForAdminLevel2,
         features::kAutofillEnableSupportForAddressOverflow,
         features::kAutofillEnableSupportForAddressOverflowAndLandmark,
         features::kAutofillEnableSupportForBetweenStreetsOrLandmark},
        {});
  }
  AutofillProfile::Source profile_source() const { return GetParam(); }

  // Creates an `AutofillProfile` with `profile_source()` as its source.
  AutofillProfile CreateAutofillProfile() const {
    return AutofillProfile(profile_source());
  }

  // Depending on the `profile_source()`, the AutofillProfiles are stored in a
  // different master table.
  base::StringPiece GetProfileTable() const {
    return profile_source() == AutofillProfile::Source::kLocalOrSyncable
               ? "local_addresses"
               : "contact_info";
  }

 private:
  base::test::ScopedFeatureList features_;
};

INSTANTIATE_TEST_SUITE_P(
    ,
    AutofillTableProfileTest,
    testing::ValuesIn({AutofillProfile::Source::kLocalOrSyncable,
                       AutofillProfile::Source::kAccount}));

TEST_F(AutofillTableTest, Autocomplete) {
  Time t1 = AutofillClock::Now();

  // Simulate the submission of a handful of entries in a field called "Name",
  // some more often than others.
  AutocompleteChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  base::Time now = AutofillClock::Now();
  base::TimeDelta two_seconds = base::Seconds(2);
  EXPECT_TRUE(table_->AddFormFieldValue(field, &changes));
  std::vector<AutocompleteEntry> v;
  for (int i = 0; i < 5; ++i) {
    field.value = u"Clark Kent";
    EXPECT_TRUE(
        table_->AddFormFieldValueTime(field, &changes, now + i * two_seconds));
  }
  for (int i = 0; i < 3; ++i) {
    field.value = u"Clark Sutter";
    EXPECT_TRUE(
        table_->AddFormFieldValueTime(field, &changes, now + i * two_seconds));
  }
  for (int i = 0; i < 2; ++i) {
    field.name = u"Favorite Color";
    field.value = u"Green";
    EXPECT_TRUE(
        table_->AddFormFieldValueTime(field, &changes, now + i * two_seconds));
  }

  // We have added the name Clark Kent 5 times, so count should be 5.
  EXPECT_EQ(5, GetAutocompleteEntryCount(u"Name", u"Clark Kent", db_.get()));

  // Storing in the data base should be case sensitive, so there should be no
  // database entry for clark kent lowercase.
  EXPECT_EQ(0, GetAutocompleteEntryCount(u"Name", u"clark kent", db_.get()));

  EXPECT_EQ(2,
            GetAutocompleteEntryCount(u"Favorite Color", u"Green", db_.get()));

  // This is meant to get a list of suggestions for Name.  The empty prefix
  // in the second argument means it should return all suggestions for a name
  // no matter what they start with.  The order that the names occur in the list
  // should be decreasing order by count.
  EXPECT_TRUE(
      table_->GetFormValuesForElementName(u"Name", std::u16string(), &v, 6));
  EXPECT_EQ(3U, v.size());
  if (v.size() == 3) {
    EXPECT_EQ(u"Clark Kent", v[0].key().value());
    EXPECT_EQ(u"Clark Sutter", v[1].key().value());
    EXPECT_EQ(u"Superman", v[2].key().value());
  }

  // If we query again limiting the list size to 1, we should only get the most
  // frequent entry.
  EXPECT_TRUE(
      table_->GetFormValuesForElementName(u"Name", std::u16string(), &v, 1));
  EXPECT_EQ(1U, v.size());
  if (v.size() == 1) {
    EXPECT_EQ(u"Clark Kent", v[0].key().value());
  }

  // Querying for suggestions given a prefix is case-insensitive, so the prefix
  // "cLa" shoud get suggestions for both Clarks.
  EXPECT_TRUE(table_->GetFormValuesForElementName(u"Name", u"cLa", &v, 6));
  EXPECT_EQ(2U, v.size());
  if (v.size() == 2) {
    EXPECT_EQ(u"Clark Kent", v[0].key().value());
    EXPECT_EQ(u"Clark Sutter", v[1].key().value());
  }

  // Removing all elements since the beginning of this function should remove
  // everything from the database.
  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(t1, Time(), &changes));

  const AutocompleteChange kExpectedChanges[] = {
      AutocompleteChange(AutocompleteChange::REMOVE,
                         AutocompleteKey(u"Name", u"Superman")),
      AutocompleteChange(AutocompleteChange::REMOVE,
                         AutocompleteKey(u"Name", u"Clark Kent")),
      AutocompleteChange(AutocompleteChange::REMOVE,
                         AutocompleteKey(u"Name", u"Clark Sutter")),
      AutocompleteChange(AutocompleteChange::REMOVE,
                         AutocompleteKey(u"Favorite Color", u"Green")),
  };
  EXPECT_EQ(std::size(kExpectedChanges), changes.size());
  for (size_t i = 0; i < std::size(kExpectedChanges); ++i) {
    EXPECT_EQ(kExpectedChanges[i], changes[i]);
  }

  EXPECT_EQ(0, GetAutocompleteEntryCount(u"Name", u"Clark Kent", db_.get()));

  EXPECT_TRUE(
      table_->GetFormValuesForElementName(u"Name", std::u16string(), &v, 6));
  EXPECT_EQ(0U, v.size());

  // Now add some values with empty strings.
  const std::u16string kValue = u"  toto   ";
  field.name = u"blank";
  field.value = std::u16string();
  EXPECT_TRUE(table_->AddFormFieldValue(field, &changes));
  field.name = u"blank";
  field.value = u" ";
  EXPECT_TRUE(table_->AddFormFieldValue(field, &changes));
  field.name = u"blank";
  field.value = u"      ";
  EXPECT_TRUE(table_->AddFormFieldValue(field, &changes));
  field.name = u"blank";
  field.value = kValue;
  EXPECT_TRUE(table_->AddFormFieldValue(field, &changes));

  // They should be stored normally as the DB layer does not check for empty
  // values.
  v.clear();
  EXPECT_TRUE(
      table_->GetFormValuesForElementName(u"blank", std::u16string(), &v, 10));
  EXPECT_EQ(4U, v.size());
}

TEST_F(AutofillTableTest, Autocomplete_GetEntry_Populated) {
  AutocompleteChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  base::Time now = base::Time::FromSecondsSinceUnixEpoch(1546889367);

  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, now));

  std::vector<AutocompleteEntry> prefix_v;
  EXPECT_TRUE(
      table_->GetFormValuesForElementName(field.name, u"Super", &prefix_v, 10));

  std::vector<AutocompleteEntry> no_prefix_v;
  EXPECT_TRUE(
      table_->GetFormValuesForElementName(field.name, u"", &no_prefix_v, 10));

  AutocompleteEntry expected_entry(AutocompleteKey(field.name, field.value),
                                   now, now);

  EXPECT_THAT(prefix_v, ElementsAre(expected_entry));
  EXPECT_THAT(no_prefix_v, ElementsAre(expected_entry));

  // Update date_last_used.
  base::Time new_time = now + base::Seconds(1000);
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, new_time));
  EXPECT_TRUE(
      table_->GetFormValuesForElementName(field.name, u"Super", &prefix_v, 10));
  EXPECT_TRUE(
      table_->GetFormValuesForElementName(field.name, u"", &no_prefix_v, 10));

  expected_entry = AutocompleteEntry(AutocompleteKey(field.name, field.value),
                                     now, new_time);

  EXPECT_THAT(prefix_v, ElementsAre(expected_entry));
  EXPECT_THAT(no_prefix_v, ElementsAre(expected_entry));
}

TEST_F(AutofillTableTest, Autocomplete_GetCountOfValuesContainedBetween) {
  AutocompleteChangeList changes;
  // This test makes time comparisons that are precise to a microsecond, but the
  // database uses the time_t format which is only precise to a second.
  // Make sure we use timestamps rounded to a second.
  Time begin = Time::FromTimeT(AutofillClock::Now().ToTimeT());
  Time now = begin;
  base::TimeDelta second = base::Seconds(1);

  struct Entry {
    const char16_t* name;
    const char16_t* value;
  } entries[] = {{u"Alter ego", u"Superman"}, {u"Name", u"Superman"},
                 {u"Name", u"Clark Kent"},    {u"Name", u"Superman"},
                 {u"Name", u"Clark Sutter"},  {u"Nomen", u"Clark Kent"}};

  for (Entry entry : entries) {
    FormFieldData field;
    field.name = entry.name;
    field.value = entry.value;
    ASSERT_TRUE(table_->AddFormFieldValueTime(field, &changes, now));
    now += second;
  }

  // While the entry "Alter ego" : "Superman" is entirely contained within
  // the first second, the value "Superman" itself appears in another entry,
  // so it is not contained.
  EXPECT_EQ(0, table_->GetCountOfValuesContainedBetween(begin, begin + second));

  // No values are entirely contained within the first three seconds either
  // (note that the second time constraint is exclusive).
  EXPECT_EQ(
      0, table_->GetCountOfValuesContainedBetween(begin, begin + 3 * second));

  // Only "Superman" is entirely contained within the first four seconds.
  EXPECT_EQ(
      1, table_->GetCountOfValuesContainedBetween(begin, begin + 4 * second));

  // "Clark Kent" and "Clark Sutter" are contained between the first
  // and seventh second.
  EXPECT_EQ(2, table_->GetCountOfValuesContainedBetween(begin + second,
                                                        begin + 7 * second));

  // Beginning from the third second, "Clark Kent" is not contained.
  EXPECT_EQ(1, table_->GetCountOfValuesContainedBetween(begin + 3 * second,
                                                        begin + 7 * second));

  // We have three distinct values total.
  EXPECT_EQ(
      3, table_->GetCountOfValuesContainedBetween(begin, begin + 7 * second));

  // And we should get the same result for unlimited time interval.
  EXPECT_EQ(3, table_->GetCountOfValuesContainedBetween(Time(), Time::Max()));

  // The null time interval is also interpreted as unlimited.
  EXPECT_EQ(3, table_->GetCountOfValuesContainedBetween(Time(), Time()));

  // An interval that does not fully contain any entries returns zero.
  EXPECT_EQ(0, table_->GetCountOfValuesContainedBetween(begin + second,
                                                        begin + 2 * second));

  // So does an interval which has no intersection with any entry.
  EXPECT_EQ(0, table_->GetCountOfValuesContainedBetween(Time(), begin));
}

TEST_F(AutofillTableTest, Autocomplete_RemoveBetweenChanges) {
  base::TimeDelta one_day(base::Days(1));
  Time t1 = AutofillClock::Now();
  Time t2 = t1 + one_day;

  AutocompleteChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, t1));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, t2));

  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(t1, t2, &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChange(AutocompleteChange::UPDATE,
                               AutocompleteKey(u"Name", u"Superman")),
            changes[0]);
  changes.clear();

  EXPECT_TRUE(
      table_->RemoveFormElementsAddedBetween(t2, t2 + one_day, &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChange(AutocompleteChange::REMOVE,
                               AutocompleteKey(u"Name", u"Superman")),
            changes[0]);
}

TEST_F(AutofillTableTest, Autocomplete_AddChanges) {
  base::TimeDelta one_day(base::Days(1));
  Time t1 = AutofillClock::Now();
  Time t2 = t1 + one_day;

  AutocompleteChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, t1));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChange(AutocompleteChange::ADD,
                               AutocompleteKey(u"Name", u"Superman")),
            changes[0]);

  changes.clear();
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, t2));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChange(AutocompleteChange::UPDATE,
                               AutocompleteKey(u"Name", u"Superman")),
            changes[0]);
}

TEST_F(AutofillTableTest, Autocomplete_UpdateOneWithOneTimestamp) {
  AutocompleteEntry entry(MakeAutocompleteEntry(u"foo", u"bar", 1, -1));
  std::vector<AutocompleteEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table_->UpdateAutocompleteEntries(entries));

  EXPECT_EQ(1, GetAutocompleteEntryCount(u"foo", u"bar", db_.get()));

  std::vector<AutocompleteEntry> all_entries;
  ASSERT_TRUE(table_->GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_EQ(entry, all_entries[0]);
}

TEST_F(AutofillTableTest, Autocomplete_UpdateOneWithTwoTimestamps) {
  AutocompleteEntry entry(MakeAutocompleteEntry(u"foo", u"bar", 1, 2));
  std::vector<AutocompleteEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table_->UpdateAutocompleteEntries(entries));

  EXPECT_EQ(2, GetAutocompleteEntryCount(u"foo", u"bar", db_.get()));

  std::vector<AutocompleteEntry> all_entries;
  ASSERT_TRUE(table_->GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_EQ(entry, all_entries[0]);
}

TEST_F(AutofillTableTest, Autocomplete_GetAutofillTimestamps) {
  AutocompleteEntry entry(MakeAutocompleteEntry(u"foo", u"bar", 1, 2));
  std::vector<AutocompleteEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table_->UpdateAutocompleteEntries(entries));

  Time date_created, date_last_used;
  ASSERT_TRUE(table_->GetAutofillTimestamps(u"foo", u"bar", &date_created,
                                            &date_last_used));
  EXPECT_EQ(Time::FromTimeT(1), date_created);
  EXPECT_EQ(Time::FromTimeT(2), date_last_used);
}

TEST_F(AutofillTableTest, Autocomplete_UpdateTwo) {
  AutocompleteEntry entry0(MakeAutocompleteEntry(u"foo", u"bar0", 1, -1));
  AutocompleteEntry entry1(MakeAutocompleteEntry(u"foo", u"bar1", 2, 3));
  std::vector<AutocompleteEntry> entries;
  entries.push_back(entry0);
  entries.push_back(entry1);
  ASSERT_TRUE(table_->UpdateAutocompleteEntries(entries));

  EXPECT_EQ(1, GetAutocompleteEntryCount(u"foo", u"bar0", db_.get()));
  EXPECT_EQ(2, GetAutocompleteEntryCount(u"foo", u"bar1", db_.get()));
}

TEST_F(AutofillTableTest, Autocomplete_UpdateNullTerminated) {
  const char16_t kName[] = u"foo";
  const char16_t kValue[] = u"bar";
  // A value which contains terminating character.
  std::u16string value(kValue, std::size(kValue));

  AutocompleteEntry entry0(MakeAutocompleteEntry(kName, kValue, 1, -1));
  AutocompleteEntry entry1(MakeAutocompleteEntry(kName, value, 2, 3));
  std::vector<AutocompleteEntry> entries;
  entries.push_back(entry0);
  entries.push_back(entry1);
  ASSERT_TRUE(table_->UpdateAutocompleteEntries(entries));

  EXPECT_EQ(1, GetAutocompleteEntryCount(kName, kValue, db_.get()));
  EXPECT_EQ(2, GetAutocompleteEntryCount(kName, value, db_.get()));

  std::vector<AutocompleteEntry> all_entries;
  ASSERT_TRUE(table_->GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(2U, all_entries.size());
  EXPECT_EQ(entry0, all_entries[0]);
  EXPECT_EQ(entry1, all_entries[1]);
}

TEST_F(AutofillTableTest, Autocomplete_UpdateReplace) {
  AutocompleteChangeList changes;
  // Add a form field.  This will be replaced.
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValue(field, &changes));

  AutocompleteEntry entry(MakeAutocompleteEntry(u"Name", u"Superman", 1, 2));
  std::vector<AutocompleteEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table_->UpdateAutocompleteEntries(entries));

  std::vector<AutocompleteEntry> all_entries;
  ASSERT_TRUE(table_->GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(1U, all_entries.size());
  EXPECT_EQ(entry, all_entries[0]);
}

TEST_F(AutofillTableTest, Autocomplete_UpdateDontReplace) {
  Time t = AutofillClock::Now();
  AutocompleteEntry existing(
      MakeAutocompleteEntry(u"Name", u"Superman", t.ToTimeT(), -1));

  AutocompleteChangeList changes;
  // Add a form field.  This will NOT be replaced.
  FormFieldData field;
  field.name = existing.key().name();
  field.value = existing.key().value();
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, t));
  AutocompleteEntry entry(MakeAutocompleteEntry(u"Name", u"Clark Kent", 1, 2));
  std::vector<AutocompleteEntry> entries;
  entries.push_back(entry);
  ASSERT_TRUE(table_->UpdateAutocompleteEntries(entries));

  std::vector<AutocompleteEntry> all_entries;
  ASSERT_TRUE(table_->GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(2U, all_entries.size());
  AutocompleteEntrySet expected_entries(all_entries.begin(), all_entries.end(),
                                        CompareAutocompleteEntries);
  EXPECT_EQ(1U, expected_entries.count(existing));
  EXPECT_EQ(1U, expected_entries.count(entry));
}

TEST_F(AutofillTableTest, Autocomplete_AddFormFieldValues) {
  Time t = AutofillClock::Now();

  // Add multiple values for "firstname" and "lastname" names.  Test that only
  // first value of each gets added. Related to security issue:
  // http://crbug.com/51727.
  std::vector<FormFieldData> elements;
  FormFieldData field;
  field.name = u"firstname";
  field.value = u"Joe";
  elements.push_back(field);

  field.name = u"firstname";
  field.value = u"Jane";
  elements.push_back(field);

  field.name = u"lastname";
  field.value = u"Smith";
  elements.push_back(field);

  field.name = u"lastname";
  field.value = u"Jones";
  elements.push_back(field);

  std::vector<AutocompleteChange> changes;
  table_->AddFormFieldValuesTime(elements, &changes, t);

  ASSERT_EQ(2U, changes.size());
  EXPECT_EQ(changes[0],
            AutocompleteChange(AutocompleteChange::ADD,
                               AutocompleteKey(u"firstname", u"Joe")));
  EXPECT_EQ(changes[1],
            AutocompleteChange(AutocompleteChange::ADD,
                               AutocompleteKey(u"lastname", u"Smith")));

  std::vector<AutocompleteEntry> all_entries;
  ASSERT_TRUE(table_->GetAllAutocompleteEntries(&all_entries));
  ASSERT_EQ(2U, all_entries.size());
}

TEST_F(AutofillTableTest,
       Autocomplete_RemoveFormElementsAddedBetween_UsedOnlyBefore) {
  // Add an entry used only before the targeted range.
  AutocompleteChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(10)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(20)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(30)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(40)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(50)));

  EXPECT_EQ(5, GetAutocompleteEntryCount(field.name, field.value, db_.get()));

  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(
      base::Time::FromTimeT(51), base::Time::FromTimeT(60), &changes));
  EXPECT_TRUE(changes.empty());
  EXPECT_EQ(5, GetAutocompleteEntryCount(field.name, field.value, db_.get()));
}

TEST_F(AutofillTableTest,
       Autocomplete_RemoveFormElementsAddedBetween_UsedOnlyAfter) {
  // Add an entry used only after the targeted range.
  AutocompleteChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(50)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(60)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(70)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(80)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(90)));

  EXPECT_EQ(5, GetAutocompleteEntryCount(field.name, field.value, db_.get()));

  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(
      base::Time::FromTimeT(40), base::Time::FromTimeT(50), &changes));
  EXPECT_TRUE(changes.empty());
  EXPECT_EQ(5, GetAutocompleteEntryCount(field.name, field.value, db_.get()));
}

TEST_F(AutofillTableTest,
       Autocomplete_RemoveFormElementsAddedBetween_UsedOnlyDuring) {
  // Add an entry used entirely during the targeted range.
  AutocompleteChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(10)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(20)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(30)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(40)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(50)));

  EXPECT_EQ(5, GetAutocompleteEntryCount(field.name, field.value, db_.get()));

  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(
      base::Time::FromTimeT(10), base::Time::FromTimeT(51), &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChange(AutocompleteChange::REMOVE,
                               AutocompleteKey(field.name, field.value)),
            changes[0]);
  EXPECT_EQ(0, GetAutocompleteEntryCount(field.name, field.value, db_.get()));
}

TEST_F(AutofillTableTest,
       Autocomplete_RemoveFormElementsAddedBetween_UsedBeforeAndDuring) {
  // Add an entry used both before and during the targeted range.
  AutocompleteChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(10)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(20)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(30)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(40)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(50)));

  EXPECT_EQ(5, GetAutocompleteEntryCount(field.name, field.value, db_.get()));

  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(
      base::Time::FromTimeT(40), base::Time::FromTimeT(60), &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChange(AutocompleteChange::UPDATE,
                               AutocompleteKey(field.name, field.value)),
            changes[0]);
  EXPECT_EQ(4, GetAutocompleteEntryCount(field.name, field.value, db_.get()));
  base::Time date_created, date_last_used;
  EXPECT_TRUE(table_->GetAutofillTimestamps(field.name, field.value,
                                            &date_created, &date_last_used));
  EXPECT_EQ(base::Time::FromTimeT(10), date_created);
  EXPECT_EQ(base::Time::FromTimeT(39), date_last_used);
}

TEST_F(AutofillTableTest,
       Autocomplete_RemoveFormElementsAddedBetween_UsedDuringAndAfter) {
  // Add an entry used both during and after the targeted range.
  AutocompleteChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(50)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(60)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(70)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(80)));
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes,
                                            base::Time::FromTimeT(90)));

  EXPECT_EQ(5, GetAutocompleteEntryCount(field.name, field.value, db_.get()));

  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(
      base::Time::FromTimeT(40), base::Time::FromTimeT(80), &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChange(AutocompleteChange::UPDATE,
                               AutocompleteKey(field.name, field.value)),
            changes[0]);
  EXPECT_EQ(2, GetAutocompleteEntryCount(field.name, field.value, db_.get()));
  base::Time date_created, date_last_used;
  EXPECT_TRUE(table_->GetAutofillTimestamps(field.name, field.value,
                                            &date_created, &date_last_used));
  EXPECT_EQ(base::Time::FromTimeT(80), date_created);
  EXPECT_EQ(base::Time::FromTimeT(90), date_last_used);
}

TEST_F(AutofillTableTest,
       Autocomplete_RemoveFormElementsAddedBetween_OlderThan30Days) {
  const base::Time kNow = AutofillClock::Now();
  const base::Time k29DaysOld = kNow - base::Days(29);
  const base::Time k30DaysOld = kNow - base::Days(30);
  const base::Time k31DaysOld = kNow - base::Days(31);

  // Add some form field entries.
  AutocompleteChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, kNow));
  field.value = u"Clark Kent";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, k29DaysOld));
  field.value = u"Clark Sutter";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, k31DaysOld));
  EXPECT_EQ(3U, changes.size());

  // Removing all elements added before 30days from the database.
  changes.clear();
  EXPECT_TRUE(table_->RemoveFormElementsAddedBetween(base::Time(), k30DaysOld,
                                                     &changes));
  ASSERT_EQ(1U, changes.size());
  EXPECT_EQ(AutocompleteChange(AutocompleteChange::REMOVE,
                               AutocompleteKey(u"Name", u"Clark Sutter")),
            changes[0]);
  EXPECT_EQ(0, GetAutocompleteEntryCount(u"Name", u"Clark Sutter", db_.get()));
  EXPECT_EQ(1, GetAutocompleteEntryCount(u"Name", u"Superman", db_.get()));
  EXPECT_EQ(1, GetAutocompleteEntryCount(u"Name", u"Clark Kent", db_.get()));
  changes.clear();
}

// Tests that we set the change type to EXPIRE for expired elements and we
// delete an old entry.
TEST_F(AutofillTableTest, RemoveExpiredFormElements_Expires_DeleteEntry) {
  auto kNow = AutofillClock::Now();
  auto k2YearsOld = kNow - 2 * kAutocompleteRetentionPolicyPeriod;

  AutocompleteChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, k2YearsOld));
  changes.clear();

  EXPECT_TRUE(table_->RemoveExpiredFormElements(&changes));

  EXPECT_EQ(AutocompleteChange(AutocompleteChange::EXPIRE,
                               AutocompleteKey(field.name, field.value)),
            changes[0]);
}

// Tests that we don't
// delete non-expired entries' data from the SQLite table.
TEST_F(AutofillTableTest, RemoveExpiredFormElements_NotOldEnough) {
  auto kNow = AutofillClock::Now();
  auto k2DaysOld = kNow - base::Days(2);

  AutocompleteChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(table_->AddFormFieldValueTime(field, &changes, k2DaysOld));
  changes.clear();

  EXPECT_TRUE(table_->RemoveExpiredFormElements(&changes));
  EXPECT_TRUE(changes.empty());
}

// Tests reading/writing name, email, company, address, phone number and
// birthdate information.
TEST_P(AutofillTableProfileTest, AutofillProfile) {
  AutofillProfile home_profile = CreateAutofillProfile();

  // TODO(crbug.com/1113617): Honorifics are temporally disabled.
  // home_profile.SetRawInfoWithVerificationStatus(
  // NAME_HONORIFIC_PREFIX, u"Dr.",
  // VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(NAME_HONORIFIC_PREFIX, u"Dr.",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(NAME_FIRST, u"John",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(NAME_MIDDLE, u"Q.",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(NAME_LAST_FIRST, u"Agent",
                                                VerificationStatus::kParsed);

  home_profile.SetRawInfoWithVerificationStatus(NAME_LAST_CONJUNCTION, u"007",
                                                VerificationStatus::kParsed);

  home_profile.SetRawInfoWithVerificationStatus(NAME_LAST_SECOND, u"Smith",
                                                VerificationStatus::kParsed);

  home_profile.SetRawInfoWithVerificationStatus(NAME_LAST, u"Agent 007 Smith",
                                                VerificationStatus::kParsed);

  home_profile.SetRawInfoWithVerificationStatus(
      NAME_FULL, u"John Q. Agent 007 Smith", VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(NAME_FULL_WITH_HONORIFIC_PREFIX,
                                                u"Dr. John Q. Agent 007 Smith",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfo(EMAIL_ADDRESS, u"js@smith.xyz");
  home_profile.SetRawInfo(COMPANY_NAME, u"Google");

  home_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_ADDRESS,
      u"Street Name between streets House Number Premise APT 10 Floor 2 "
      u"Landmark",
      VerificationStatus::kUserVerified);
  home_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_STREET_NAME, u"Street Name", VerificationStatus::kFormatted);
  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_DEPENDENT_LOCALITY,
                                                u"Dependent Locality",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_CITY, u"City",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_STATE, u"State",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_SORTING_CODE,
                                                u"Sorting Code",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_ZIP, u"ZIP",
                                                VerificationStatus::kObserved);

  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_COUNTRY, u"DE",
                                                VerificationStatus::kObserved);
  home_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_HOUSE_NUMBER, u"House Number",
      VerificationStatus::kUserVerified);
  home_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_SUBPREMISE, u"APT 10 Floor 2",
      VerificationStatus::kUserVerified);
  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_APT_NUM, u"10",
                                                VerificationStatus::kParsed);
  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_FLOOR, u"2",
                                                VerificationStatus::kParsed);
  ASSERT_EQ(home_profile.GetRawInfo(ADDRESS_HOME_STREET_NAME), u"Street Name");
  home_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_LANDMARK, u"Landmark", VerificationStatus::kObserved);
  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_OVERFLOW,
                                                u"Andar 1, Apto. 12",
                                                VerificationStatus::kObserved);
  home_profile.SetRawInfoWithVerificationStatus(ADDRESS_HOME_BETWEEN_STREETS,
                                                u"between streets",
                                                VerificationStatus::kObserved);
  home_profile.SetRawInfoWithVerificationStatus(
      ADDRESS_HOME_ADMIN_LEVEL2, u"Oxaca", VerificationStatus::kObserved);

  home_profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"18181234567");
  home_profile.SetRawInfoAsInt(BIRTHDATE_DAY, 14);
  home_profile.SetRawInfoAsInt(BIRTHDATE_MONTH, 3);
  home_profile.SetRawInfoAsInt(BIRTHDATE_4_DIGIT_YEAR, 1997);
  home_profile.set_language_code("en");

  // Add the profile to the table.
  EXPECT_TRUE(table_->AddAutofillProfile(home_profile));

  // Get the 'Home' profile from the table.
  std::unique_ptr<AutofillProfile> db_profile =
      table_->GetAutofillProfile(home_profile.guid(), home_profile.source());
  ASSERT_TRUE(db_profile);

  // Verify that it is correct.
  EXPECT_EQ(home_profile, *db_profile);

  // Remove the profile and expect that no profiles remain.
  EXPECT_TRUE(
      table_->RemoveAutofillProfile(home_profile.guid(), profile_source()));
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  EXPECT_TRUE(table_->GetAutofillProfiles(profile_source(), &profiles));
  EXPECT_TRUE(profiles.empty());
}

// Tests that `GetAutofillProfiles(source, profiles)` clears `profiles` and
// only returns profiles from the correct `source`.
// Not part of the `AutofillTableProfileTest` fixture, as it doesn't benefit
// from parameterization on the `profile_source()`.
TEST_F(AutofillTableTest, GetAutofillProfiles) {
  AutofillProfile local_profile(AutofillProfile::Source::kLocalOrSyncable);
  AutofillProfile account_profile(AutofillProfile::Source::kAccount);
  EXPECT_TRUE(table_->AddAutofillProfile(local_profile));
  EXPECT_TRUE(table_->AddAutofillProfile(account_profile));

  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  EXPECT_TRUE(table_->GetAutofillProfiles(
      AutofillProfile::Source::kLocalOrSyncable, &profiles));
  EXPECT_THAT(profiles, ElementsAre(testing::Pointee(local_profile)));
  EXPECT_TRUE(table_->GetAutofillProfiles(AutofillProfile::Source::kAccount,
                                          &profiles));
  EXPECT_THAT(profiles, ElementsAre(testing::Pointee(account_profile)));
}

// Tests that `RemoveAllAutofillProfiles()` clears all profiles of the given
// source.
TEST_P(AutofillTableProfileTest, RemoveAllAutofillProfiles) {
  ASSERT_TRUE(table_->AddAutofillProfile(
      AutofillProfile(AutofillProfile::Source::kLocalOrSyncable)));
  ASSERT_TRUE(table_->AddAutofillProfile(
      AutofillProfile(AutofillProfile::Source::kAccount)));

  EXPECT_TRUE(table_->RemoveAllAutofillProfiles(profile_source()));

  // Expect that the profiles from `profile_source()` are gone.
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  ASSERT_TRUE(table_->GetAutofillProfiles(profile_source(), &profiles));
  EXPECT_TRUE(profiles.empty());

  // Expect that the profile from the opposite source remains.
  const auto other_source =
      profile_source() == AutofillProfile::Source::kAccount
          ? AutofillProfile::Source::kLocalOrSyncable
          : AutofillProfile::Source::kAccount;
  ASSERT_TRUE(table_->GetAutofillProfiles(other_source, &profiles));
  EXPECT_EQ(profiles.size(), 1u);
}

// Tests that `ProfileTokenQuality` observations are read and written.
TEST_P(AutofillTableProfileTest, ProfileTokenQuality) {
  AutofillProfile profile = CreateAutofillProfile();
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST,
                      ProfileTokenQuality::ObservationType::kAccepted,
                      ProfileTokenQualityTestApi::FormSignatureHash(12));

  // Add
  table_->AddAutofillProfile(profile);
  profile = *table_->GetAutofillProfile(profile.guid(), profile.source());
  EXPECT_THAT(
      profile.token_quality().GetObservationTypesForFieldType(NAME_FIRST),
      UnorderedElementsAre(ProfileTokenQuality::ObservationType::kAccepted));
  EXPECT_THAT(
      test_api(profile.token_quality()).GetHashesForStoredType(NAME_FIRST),
      UnorderedElementsAre(ProfileTokenQualityTestApi::FormSignatureHash(12)));

  // Update
  test_api(profile.token_quality())
      .AddObservation(NAME_FIRST,
                      ProfileTokenQuality::ObservationType::kEditedFallback,
                      ProfileTokenQualityTestApi::FormSignatureHash(21));
  table_->UpdateAutofillProfile(profile);
  profile = *table_->GetAutofillProfile(profile.guid(), profile.source());
  EXPECT_THAT(
      profile.token_quality().GetObservationTypesForFieldType(NAME_FIRST),
      UnorderedElementsAre(
          ProfileTokenQuality::ObservationType::kAccepted,
          ProfileTokenQuality::ObservationType::kEditedFallback));
  EXPECT_THAT(
      test_api(profile.token_quality()).GetHashesForStoredType(NAME_FIRST),
      UnorderedElementsAre(ProfileTokenQualityTestApi::FormSignatureHash(12),
                           ProfileTokenQualityTestApi::FormSignatureHash(21)));
}

TEST_F(AutofillTableTest, Iban) {
  // Add a valid IBAN.
  Iban iban;
  std::string guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  iban.set_identifier(Iban::Guid(guid));
  iban.SetRawInfo(IBAN_VALUE, u"IE12 BOFI 9000 0112 3456 78");
  iban.set_nickname(u"My doctor's IBAN");

  EXPECT_TRUE(table_->AddLocalIban(iban));

  // Get the inserted IBAN.
  std::unique_ptr<Iban> db_iban = table_->GetLocalIban(iban.guid());
  ASSERT_TRUE(db_iban);
  EXPECT_EQ(guid, db_iban->guid());
  sql::Statement s_work(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, use_count, use_date, "
      "value_encrypted, nickname FROM local_ibans WHERE guid = ?"));
  s_work.BindString(0, iban.guid());
  ASSERT_TRUE(s_work.is_valid());
  ASSERT_TRUE(s_work.Step());
  EXPECT_FALSE(s_work.Step());

  // Add another valid IBAN.
  Iban another_iban;
  std::string another_guid = base::Uuid::GenerateRandomV4().AsLowercaseString();
  another_iban.set_identifier(Iban::Guid(another_guid));
  another_iban.SetRawInfo(IBAN_VALUE, u"DE91 1000 0000 0123 4567 89");
  another_iban.set_nickname(u"My brother's IBAN");

  EXPECT_TRUE(table_->AddLocalIban(another_iban));

  db_iban = table_->GetLocalIban(another_iban.guid());
  ASSERT_TRUE(db_iban);

  EXPECT_EQ(another_guid, db_iban->guid());
  sql::Statement s_target(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, use_count, use_date, "
      "value_encrypted, nickname FROM local_ibans WHERE guid = ?"));
  s_target.BindString(0, another_iban.guid());
  ASSERT_TRUE(s_target.is_valid());
  ASSERT_TRUE(s_target.Step());
  EXPECT_FALSE(s_target.Step());

  // Update the another_iban.
  another_iban.SetRawInfo(IBAN_VALUE, u"GB98 MIDL 0700 9312 3456 78");
  another_iban.set_nickname(u"My teacher's IBAN");
  EXPECT_TRUE(table_->UpdateLocalIban(another_iban));
  db_iban = table_->GetLocalIban(another_iban.guid());
  ASSERT_TRUE(db_iban);
  EXPECT_EQ(another_guid, db_iban->guid());
  sql::Statement s_target_updated(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, use_count, use_date, "
      "value_encrypted, nickname FROM local_ibans WHERE guid = ?"));
  s_target_updated.BindString(0, another_iban.guid());
  ASSERT_TRUE(s_target_updated.is_valid());
  ASSERT_TRUE(s_target_updated.Step());
  EXPECT_FALSE(s_target_updated.Step());

  // Remove the 'Target' IBAN.
  EXPECT_TRUE(table_->RemoveLocalIban(another_iban.guid()));
  db_iban = table_->GetLocalIban(another_iban.guid());
  EXPECT_FALSE(db_iban);
}

// Test that masked IBANs can be added and loaded successfully.
TEST_F(AutofillTableTest, MaskedServerIban) {
  Iban iban_0 = test::GetServerIban();
  Iban iban_1 = test::GetServerIban2();
  Iban iban_2 = test::GetServerIban3();
  std::vector<Iban> ibans = {iban_0, iban_1, iban_2};

  EXPECT_TRUE(table_->SetServerIbans(ibans));

  std::vector<std::unique_ptr<Iban>> masked_server_ibans =
      table_->GetServerIbans();
  EXPECT_EQ(3U, masked_server_ibans.size());
  EXPECT_THAT(ibans, UnorderedElementsAre(*masked_server_ibans[0],
                                          *masked_server_ibans[1],
                                          *masked_server_ibans[2]));
}

TEST_F(AutofillTableTest, CreditCard) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  // Add a 'Work' credit card.
  CreditCard work_creditcard;
  work_creditcard.set_origin("https://www.example.com/");
  work_creditcard.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  work_creditcard.SetRawInfo(CREDIT_CARD_NUMBER, u"1234567890123456");
  work_creditcard.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"04");
  work_creditcard.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2013");
  work_creditcard.SetNickname(u"Corporate card");
  work_creditcard.set_cvc(u"123");

  Time pre_creation_time = AutofillClock::Now();
  EXPECT_TRUE(table_->AddCreditCard(work_creditcard));
  Time post_creation_time = AutofillClock::Now();

  // Get the 'Work' credit card.
  std::unique_ptr<CreditCard> db_creditcard =
      table_->GetCreditCard(work_creditcard.guid());
  ASSERT_TRUE(db_creditcard);
  EXPECT_EQ(work_creditcard, *db_creditcard);
  // Check GetCreditCard statement
  sql::Statement s_credit_card_work(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified, nickname "
      "FROM credit_cards WHERE guid=?"));
  s_credit_card_work.BindString(0, work_creditcard.guid());
  ASSERT_TRUE(s_credit_card_work.is_valid());
  ASSERT_TRUE(s_credit_card_work.Step());
  EXPECT_GE(s_credit_card_work.ColumnInt64(5), pre_creation_time.ToTimeT());
  EXPECT_LE(s_credit_card_work.ColumnInt64(5), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_credit_card_work.Step());
  // Check GetLocalStoredCvc statement
  sql::Statement s_cvc_work(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT value_encrypted,  last_updated_timestamp "
      "FROM local_stored_cvc WHERE guid=?"));
  s_cvc_work.BindString(0, work_creditcard.guid());
  ASSERT_TRUE(s_cvc_work.is_valid());
  ASSERT_TRUE(s_cvc_work.Step());
  EXPECT_GE(s_cvc_work.ColumnInt64(1), pre_creation_time.ToTimeT());
  EXPECT_LE(s_cvc_work.ColumnInt64(1), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_cvc_work.Step());

  // Add a 'Target' credit card.
  CreditCard target_creditcard;
  target_creditcard.set_origin(std::string());
  target_creditcard.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  target_creditcard.SetRawInfo(CREDIT_CARD_NUMBER, u"1111222233334444");
  target_creditcard.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"06");
  target_creditcard.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2012");
  target_creditcard.SetNickname(u"Grocery card");
  target_creditcard.set_cvc(u"234");

  pre_creation_time = AutofillClock::Now();
  EXPECT_TRUE(table_->AddCreditCard(target_creditcard));
  post_creation_time = AutofillClock::Now();
  db_creditcard = table_->GetCreditCard(target_creditcard.guid());
  ASSERT_TRUE(db_creditcard);
  EXPECT_EQ(target_creditcard, *db_creditcard);
  // Check GetCreditCard statement.
  sql::Statement s_credit_card_target(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT guid, name_on_card, expiration_month, expiration_year, "
          "card_number_encrypted, date_modified, nickname "
          "FROM credit_cards WHERE guid=?"));
  s_credit_card_target.BindString(0, target_creditcard.guid());
  ASSERT_TRUE(s_credit_card_target.is_valid());
  ASSERT_TRUE(s_credit_card_target.Step());
  EXPECT_GE(s_credit_card_target.ColumnInt64(5), pre_creation_time.ToTimeT());
  EXPECT_LE(s_credit_card_target.ColumnInt64(5), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_credit_card_target.Step());
  // Check GetLocalStoredCvc statement
  sql::Statement s_cvc_target(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT value_encrypted,  last_updated_timestamp "
      "FROM local_stored_cvc WHERE guid=?"));
  s_cvc_target.BindString(0, target_creditcard.guid());
  ASSERT_TRUE(s_cvc_target.is_valid());
  ASSERT_TRUE(s_cvc_target.Step());
  EXPECT_GE(s_cvc_target.ColumnInt64(1), pre_creation_time.ToTimeT());
  EXPECT_LE(s_cvc_target.ColumnInt64(1), post_creation_time.ToTimeT());
  EXPECT_FALSE(s_cvc_target.Step());

  // Update the 'Target' credit card.
  target_creditcard.set_origin("Interactive Autofill dialog");
  target_creditcard.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Charles Grady");
  target_creditcard.SetNickname(u"Supermarket");
  target_creditcard.set_cvc(u"234");
  Time pre_modification_time = AutofillClock::Now();
  EXPECT_TRUE(table_->UpdateCreditCard(target_creditcard));
  Time post_modification_time = AutofillClock::Now();
  db_creditcard = table_->GetCreditCard(target_creditcard.guid());
  ASSERT_TRUE(db_creditcard);
  EXPECT_EQ(target_creditcard, *db_creditcard);
  sql::Statement s_target_updated(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid, name_on_card, expiration_month, expiration_year, "
      "card_number_encrypted, date_modified, nickname "
      "FROM credit_cards WHERE guid=?"));
  s_target_updated.BindString(0, target_creditcard.guid());
  ASSERT_TRUE(s_target_updated.is_valid());
  ASSERT_TRUE(s_target_updated.Step());
  EXPECT_GE(s_target_updated.ColumnInt64(5), pre_modification_time.ToTimeT());
  EXPECT_LE(s_target_updated.ColumnInt64(5), post_modification_time.ToTimeT());
  EXPECT_FALSE(s_target_updated.Step());

  // Remove the 'Target' credit card.
  EXPECT_TRUE(table_->RemoveCreditCard(target_creditcard.guid()));
  db_creditcard = table_->GetCreditCard(target_creditcard.guid());
  EXPECT_FALSE(db_creditcard);
}

TEST_F(AutofillTableTest, AddCreditCardCvcWithFlagOff) {
  base::test::ScopedFeatureList features;
  features.InitAndDisableFeature(features::kAutofillEnableCvcStorageAndFilling);
  CreditCard card = test::WithCvc(test::GetCreditCard());
  EXPECT_TRUE(table_->AddCreditCard(card));
  std::unique_ptr<CreditCard> db_card = table_->GetCreditCard(card.guid());
  EXPECT_EQ(u"", db_card->cvc());

  card.set_cvc(u"234");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  db_card = table_->GetCreditCard(card.guid());
  EXPECT_EQ(u"", db_card->cvc());
}

// Tests that verify ClearLocalPaymentMethodsData function working as expected.
TEST_F(AutofillTableTest, ClearLocalPaymentMethodsData) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  CreditCard card = test::WithCvc(test::GetCreditCard());
  EXPECT_TRUE(table_->AddCreditCard(card));
  std::unique_ptr<CreditCard> db_card = table_->GetCreditCard(card.guid());
  EXPECT_EQ(card.cvc(), db_card->cvc());
  Iban iban = test::GetLocalIban();
  EXPECT_TRUE(table_->AddLocalIban(iban));

  // After calling ClearLocalPaymentMethodsData, the local_stored_cvc,
  // credit_cards, and local_ibans tables should be empty.
  table_->ClearLocalPaymentMethodsData();
  EXPECT_FALSE(table_->GetCreditCard(card.guid()));
  EXPECT_FALSE(table_->GetLocalIban(iban.guid()));
  sql::Statement s(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid FROM local_stored_cvc WHERE guid=?"));
  s.BindString(0, card.guid());
  ASSERT_TRUE(s.is_valid());
  EXPECT_FALSE(s.Step());
}

// Tests that adding credit card with cvc, get credit card with cvc and update
// credit card with only cvc change will not update credit_card table
// modification_date.
TEST_F(AutofillTableTest, CreditCardCvc) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  const base::Time arbitrary_time = base::Time::FromSecondsSinceUnixEpoch(25);
  // Create the test clock and set the time to a specific value.
  TestAutofillClock test_clock;
  test_clock.SetNow(arbitrary_time);
  CreditCard card = test::WithCvc(test::GetCreditCard());
  EXPECT_TRUE(table_->AddCreditCard(card));

  // Get the credit card, cvc should match.
  std::unique_ptr<CreditCard> db_card = table_->GetCreditCard(card.guid());
  EXPECT_EQ(card.cvc(), db_card->cvc());

  // Verify last_updated_timestamp in local_stored_cvc table is set correctly.
  EXPECT_EQ(GetDateModified("local_stored_cvc", "last_updated_timestamp",
                            card.guid()),
            arbitrary_time.ToTimeT());

  // Set the current time to another value.
  const base::Time some_later_time =
      base::Time::FromSecondsSinceUnixEpoch(1000);
  test_clock.SetNow(some_later_time);

  // Update the credit card but CVC is same.
  card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Charles Grady");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  // credit_card table date_modified should be updated.
  EXPECT_EQ(GetDateModified("credit_cards", "date_modified", card.guid()),
            some_later_time.ToTimeT());
  // local_stored_cvc table timestamp should not be updated.
  EXPECT_EQ(GetDateModified("local_stored_cvc", "last_updated_timestamp",
                            card.guid()),
            arbitrary_time.ToTimeT());

  // Set the current time to another value.
  const base::Time much_later_time =
      base::Time::FromSecondsSinceUnixEpoch(5000);
  test_clock.SetNow(much_later_time);

  // Update the credit card and CVC is different.
  card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  card.set_cvc(u"234");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  db_card = table_->GetCreditCard(card.guid());
  // CVC should be updated to new CVC.
  EXPECT_EQ(u"234", db_card->cvc());
  // local_stored_cvc table timestamp should be updated.
  EXPECT_EQ(GetDateModified("local_stored_cvc", "last_updated_timestamp",
                            card.guid()),
            much_later_time.ToTimeT());

  // Remove the credit card. It should also remove cvc from local_stored_cvc
  // table.
  EXPECT_TRUE(table_->RemoveCreditCard(card.guid()));
  sql::Statement cvc_removed_statement(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT guid FROM local_stored_cvc WHERE guid=?"));
  cvc_removed_statement.BindString(0, card.guid());
  ASSERT_TRUE(cvc_removed_statement.is_valid());
  EXPECT_FALSE(cvc_removed_statement.Step());
}

// Tests that update a credit card CVC that doesn't have CVC set initially
// inserts a new CVC record.
TEST_F(AutofillTableTest, UpdateCreditCardCvc_Add) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  CreditCard card = test::GetCreditCard();
  ASSERT_TRUE(card.cvc().empty());
  ASSERT_TRUE(table_->AddCreditCard(card));

  // Update the credit card CVC, we should expect success and CVC gets updated.
  card.set_cvc(u"123");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  std::unique_ptr<CreditCard> db_card = table_->GetCreditCard(card.guid());
  EXPECT_EQ(u"123", db_card->cvc());
}

// Tests that updating a credit card CVC that is different from CVC set
// initially.
TEST_F(AutofillTableTest, UpdateCreditCardCvc_Update) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  CreditCard card = test::GetCreditCard();
  ASSERT_TRUE(card.cvc().empty());
  ASSERT_TRUE(table_->AddCreditCard(card));

  // INSERT
  // Updating a card that doesn't have a CVC is the same as inserting a new CVC
  // record.
  card.set_cvc(u"123");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  std::unique_ptr<CreditCard> db_card = table_->GetCreditCard(card.guid());
  EXPECT_EQ(u"123", db_card->cvc());

  // UPDATE
  // Update the credit card CVC.
  card.set_cvc(u"234");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  db_card = table_->GetCreditCard(card.guid());
  EXPECT_EQ(u"234", db_card->cvc());
}

// Tests that updating a credit card CVC with empty CVC will delete CVC
// record. This is necessary because if inserting a CVC, UPDATE is chosen over
// INSERT, it will causes a crash.
TEST_F(AutofillTableTest, UpdateCreditCardCvc_Delete) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  CreditCard card = test::GetCreditCard();
  ASSERT_TRUE(card.cvc().empty());
  ASSERT_TRUE(table_->AddCreditCard(card));

  // INSERT
  // Updating a card that doesn't have a CVC is the same as inserting a new CVC
  // record.
  card.set_cvc(u"123");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  std::unique_ptr<CreditCard> db_card = table_->GetCreditCard(card.guid());
  EXPECT_EQ(u"123", db_card->cvc());

  // DELETE
  // Updating a card with empty CVC is the same as deleting the CVC record.
  card.set_cvc(u"");
  EXPECT_TRUE(table_->UpdateCreditCard(card));
  sql::Statement cvc_statement(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT guid FROM local_stored_cvc WHERE guid=?"));
  cvc_statement.BindString(0, card.guid());
  ASSERT_TRUE(cvc_statement.is_valid());
  EXPECT_FALSE(cvc_statement.Step());
}

// Tests that verify add, update and clear server cvc function working as
// expected.
TEST_F(AutofillTableTest, ServerCvc) {
  const base::Time kArbitraryTime = base::Time::FromSecondsSinceUnixEpoch(25);
  int64_t kInstrumentId = 111111111111;
  const std::u16string kCvc = u"123";
  const ServerCvc kServerCvc{kInstrumentId, kCvc, kArbitraryTime};
  EXPECT_TRUE(table_->AddServerCvc(kServerCvc));
  // Database does not allow adding same instrument_id twice.
  EXPECT_FALSE(table_->AddServerCvc(kServerCvc));
  EXPECT_THAT(table_->GetAllServerCvcs(),
              UnorderedElementsAre(testing::Pointee(kServerCvc)));

  const base::Time kSomeLaterTime = base::Time::FromSecondsSinceUnixEpoch(1000);
  const std::u16string kNewCvc = u"234";
  const ServerCvc kNewServerCvcUnderSameInstrumentId{kInstrumentId, kNewCvc,
                                                     kSomeLaterTime};
  EXPECT_TRUE(table_->UpdateServerCvc(kNewServerCvcUnderSameInstrumentId));
  EXPECT_THAT(table_->GetAllServerCvcs(),
              UnorderedElementsAre(
                  testing::Pointee(kNewServerCvcUnderSameInstrumentId)));

  // Remove the server cvc. It should also remove cvc from server_stored_cvc
  // table.
  EXPECT_TRUE(table_->RemoveServerCvc(kInstrumentId));
  EXPECT_TRUE(table_->GetAllServerCvcs().empty());

  // Remove non-exist cvc will return false.
  EXPECT_FALSE(table_->RemoveServerCvc(kInstrumentId));

  // Clear the server_stored_cvc table.
  table_->AddServerCvc(kServerCvc);
  EXPECT_TRUE(table_->ClearServerCvcs());
  EXPECT_TRUE(table_->GetAllServerCvcs().empty());

  // Clear the server_stored_cvc table when table is empty will return false.
  EXPECT_FALSE(table_->ClearServerCvcs());
}

// Tests that verify reconcile server cvc function working as expected.
TEST_F(AutofillTableTest, ReconcileServerCvcs) {
  const base::Time kArbitraryTime = base::Time::FromSecondsSinceUnixEpoch(25);
  // Add 2 server credit cards.
  CreditCard card1 = test::WithCvc(test::GetMaskedServerCard());
  CreditCard card2 = test::WithCvc(test::GetMaskedServerCard2());
  test::SetServerCreditCards(table_.get(), {card1, card2});

  // Add 1 server cvc that doesn't have a credit card associate with. We
  // should have 3 cvcs in server_stored_cvc table.
  EXPECT_TRUE(table_->AddServerCvc(ServerCvc{3333, u"456", kArbitraryTime}));
  EXPECT_EQ(3U, table_->GetAllServerCvcs().size());

  // After we reconcile server cvc, we should only see 2 cvcs in
  // server_stored_cvc table because obsolete cvc has been reconciled.
  EXPECT_TRUE(table_->ReconcileServerCvcs());
  EXPECT_EQ(2U, table_->GetAllServerCvcs().size());
}

TEST_F(AutofillTableTest, AddFullServerCreditCard) {
  CreditCard credit_card;
  credit_card.set_record_type(CreditCard::RecordType::kFullServerCard);
  credit_card.set_server_id("server_id");
  credit_card.set_origin("https://www.example.com/");
  credit_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  credit_card.SetRawInfo(CREDIT_CARD_NUMBER, u"1234567890123456");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"04");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2013");

  EXPECT_TRUE(table_->AddFullServerCreditCard(credit_card));

  std::vector<std::unique_ptr<CreditCard>> outputs;
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(0, credit_card.Compare(*outputs[0]));
}

TEST_P(AutofillTableProfileTest, UpdateAutofillProfile) {
  // Add a profile to the db.
  AutofillProfile profile = CreateAutofillProfile();
  profile.SetRawInfo(NAME_FIRST, u"John");
  profile.SetRawInfo(NAME_MIDDLE, u"Q.");
  profile.SetRawInfo(NAME_LAST, u"Smith");
  profile.SetRawInfo(EMAIL_ADDRESS, u"js@example.com");
  profile.SetRawInfo(COMPANY_NAME, u"Google");
  profile.SetRawInfo(ADDRESS_HOME_LINE1, u"1234 Apple Way");
  profile.SetRawInfo(ADDRESS_HOME_LINE2, u"unit 5");
  profile.SetRawInfo(ADDRESS_HOME_CITY, u"Los Angeles");
  profile.SetRawInfo(ADDRESS_HOME_STATE, u"CA");
  profile.SetRawInfo(ADDRESS_HOME_ZIP, u"90025");
  profile.SetRawInfo(ADDRESS_HOME_COUNTRY, u"US");
  profile.SetRawInfo(ADDRESS_HOME_OVERFLOW, u"Andar 1, Apto. 12");
  profile.SetRawInfo(ADDRESS_HOME_LANDMARK, u"Landmark");
  profile.SetRawInfo(ADDRESS_HOME_BETWEEN_STREETS, u"Marcos y Oliva");
  profile.SetRawInfo(PHONE_HOME_WHOLE_NUMBER, u"18181234567");
  profile.SetRawInfoAsInt(BIRTHDATE_DAY, 14);
  profile.SetRawInfoAsInt(BIRTHDATE_MONTH, 3);
  profile.SetRawInfoAsInt(BIRTHDATE_4_DIGIT_YEAR, 1997);
  profile.set_language_code("en");
  profile.FinalizeAfterImport();
  table_->AddAutofillProfile(profile);

  // Get the profile.
  std::unique_ptr<AutofillProfile> db_profile =
      table_->GetAutofillProfile(profile.guid(), profile.source());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(profile, *db_profile);

  // Now, update the profile and save the update to the database.
  // The modification date should change to reflect the update.
  profile.SetRawInfo(EMAIL_ADDRESS, u"js@smith.xyz");
  table_->UpdateAutofillProfile(profile);

  // Get the profile.
  db_profile = table_->GetAutofillProfile(profile.guid(), profile.source());
  ASSERT_TRUE(db_profile);
  EXPECT_EQ(profile, *db_profile);
}

TEST_F(AutofillTableTest, UpdateCreditCard) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  // Add a credit card to the db.
  CreditCard credit_card;
  credit_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  credit_card.SetRawInfo(CREDIT_CARD_NUMBER, u"1234567890123456");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"04");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2013");
  table_->AddCreditCard(credit_card);

  // Set a mocked value for the credit card's creation time.
  const time_t kMockCreationDate = AutofillClock::Now().ToTimeT() - 13;
  sql::Statement s_mock_creation_date(
      db_->GetSQLConnection()->GetUniqueStatement(
          "UPDATE credit_cards SET date_modified = ?"));
  ASSERT_TRUE(s_mock_creation_date.is_valid());
  s_mock_creation_date.BindInt64(0, kMockCreationDate);
  ASSERT_TRUE(s_mock_creation_date.Run());

  // Get the credit card.
  std::unique_ptr<CreditCard> db_credit_card =
      table_->GetCreditCard(credit_card.guid());
  ASSERT_TRUE(db_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_original(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_original.is_valid());
  ASSERT_TRUE(s_original.Step());
  EXPECT_EQ(kMockCreationDate, s_original.ColumnInt64(0));
  EXPECT_FALSE(s_original.Step());

  // Now, update the credit card and save the update to the database.
  // The modification date should change to reflect the update.
  credit_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"01");
  table_->UpdateCreditCard(credit_card);

  // Get the credit card.
  db_credit_card = table_->GetCreditCard(credit_card.guid());
  ASSERT_TRUE(db_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_updated(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_updated.is_valid());
  ASSERT_TRUE(s_updated.Step());
  EXPECT_LT(kMockCreationDate, s_updated.ColumnInt64(0));
  EXPECT_FALSE(s_updated.Step());

  // Set a mocked value for the credit card's modification time.
  const time_t mock_modification_date = AutofillClock::Now().ToTimeT() - 7;
  sql::Statement s_mock_modification_date(
      db_->GetSQLConnection()->GetUniqueStatement(
          "UPDATE credit_cards SET date_modified = ?"));
  ASSERT_TRUE(s_mock_modification_date.is_valid());
  s_mock_modification_date.BindInt64(0, mock_modification_date);
  ASSERT_TRUE(s_mock_modification_date.Run());

  // Finally, call into |UpdateCreditCard()| without changing the credit card.
  // The modification date should not change.
  table_->UpdateCreditCard(credit_card);

  // Get the credit card.
  db_credit_card = table_->GetCreditCard(credit_card.guid());
  ASSERT_TRUE(db_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_unchanged(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_unchanged.is_valid());
  ASSERT_TRUE(s_unchanged.Step());
  EXPECT_EQ(mock_modification_date, s_unchanged.ColumnInt64(0));
  EXPECT_FALSE(s_unchanged.Step());
}

TEST_F(AutofillTableTest, UpdateCreditCardOriginOnly) {
  base::test::ScopedFeatureList features(
      features::kAutofillEnableCvcStorageAndFilling);
  // Add a credit card to the db.
  CreditCard credit_card;
  credit_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jack Torrance");
  credit_card.SetRawInfo(CREDIT_CARD_NUMBER, u"1234567890123456");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"04");
  credit_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2013");
  table_->AddCreditCard(credit_card);

  // Set a mocked value for the credit card's creation time.
  const time_t kMockCreationDate = AutofillClock::Now().ToTimeT() - 13;
  sql::Statement s_mock_creation_date(
      db_->GetSQLConnection()->GetUniqueStatement(
          "UPDATE credit_cards SET date_modified = ?"));
  ASSERT_TRUE(s_mock_creation_date.is_valid());
  s_mock_creation_date.BindInt64(0, kMockCreationDate);
  ASSERT_TRUE(s_mock_creation_date.Run());

  // Get the credit card.
  std::unique_ptr<CreditCard> db_credit_card =
      table_->GetCreditCard(credit_card.guid());
  ASSERT_TRUE(db_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_original(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_original.is_valid());
  ASSERT_TRUE(s_original.Step());
  EXPECT_EQ(kMockCreationDate, s_original.ColumnInt64(0));
  EXPECT_FALSE(s_original.Step());

  // Now, update just the credit card's origin and save the update to the
  // database.  The modification date should change to reflect the update.
  credit_card.set_origin("https://www.example.com/");
  table_->UpdateCreditCard(credit_card);

  // Get the credit card.
  db_credit_card = table_->GetCreditCard(credit_card.guid());
  ASSERT_TRUE(db_credit_card);
  EXPECT_EQ(credit_card, *db_credit_card);
  sql::Statement s_updated(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_updated.is_valid());
  ASSERT_TRUE(s_updated.Step());
  EXPECT_LT(kMockCreationDate, s_updated.ColumnInt64(0));
  EXPECT_FALSE(s_updated.Step());
}

TEST_F(AutofillTableTest, RemoveAutofillDataModifiedBetween) {
  // Populate the autofill_profiles and credit_cards tables.
  ASSERT_TRUE(db_->GetSQLConnection()->Execute(
      "INSERT INTO local_addresses (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000000', 11);"
      "INSERT INTO local_addresses_type_tokens (guid, type, value) "
      "VALUES('00000000-0000-0000-0000-000000000000', 3, 'first name0');"
      "INSERT INTO local_addresses (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000001', 21);"
      "INSERT INTO local_addresses_type_tokens (guid, type, value) "
      "VALUES('00000000-0000-0000-0000-000000000001', 3, 'first name1');"
      "INSERT INTO local_addresses (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000002', 31);"
      "INSERT INTO local_addresses_type_tokens (guid, type, value) "
      "VALUES('00000000-0000-0000-0000-000000000002', 3, 'first name2');"
      "INSERT INTO local_addresses (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000003', 41);"
      "INSERT INTO local_addresses_type_tokens (guid, type, value) "
      "VALUES('00000000-0000-0000-0000-000000000003', 3, 'first name3');"
      "INSERT INTO local_addresses (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000004', 51);"
      "INSERT INTO local_addresses_type_tokens (guid, type, value) "
      "VALUES('00000000-0000-0000-0000-000000000004', 3, 'first name4');"
      "INSERT INTO local_addresses (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000005', 61);"
      "INSERT INTO local_addresses_type_tokens (guid, type, value) "
      "VALUES('00000000-0000-0000-0000-000000000005', 3, 'first name5');"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000006', 17);"
      "INSERT INTO local_stored_cvc (guid, value_encrypted, "
      "last_updated_timestamp) "
      "VALUES('00000000-0000-0000-0000-000000000006', '', 17);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000007', 27);"
      "INSERT INTO local_stored_cvc (guid, value_encrypted, "
      "last_updated_timestamp) "
      "VALUES('00000000-0000-0000-0000-000000000007', '', 27);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000008', 37);"
      "INSERT INTO local_stored_cvc (guid, value_encrypted, "
      "last_updated_timestamp) "
      "VALUES('00000000-0000-0000-0000-000000000008', '', 37);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000009', 47);"
      "INSERT INTO local_stored_cvc (guid, value_encrypted, "
      "last_updated_timestamp) "
      "VALUES('00000000-0000-0000-0000-000000000009', '', 47);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000010', 57);"
      "INSERT INTO local_stored_cvc (guid, value_encrypted, "
      "last_updated_timestamp) "
      "VALUES('00000000-0000-0000-0000-000000000010', '', 57);"
      "INSERT INTO credit_cards (guid, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000011', 67);"
      "INSERT INTO local_stored_cvc (guid, value_encrypted, "
      "last_updated_timestamp) "
      "VALUES('00000000-0000-0000-0000-000000000011', '', 67);"));

  // Remove all entries modified in the bounded time range [17,41).
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  std::vector<std::unique_ptr<CreditCard>> credit_cards;
  table_->RemoveAutofillDataModifiedBetween(
      Time::FromTimeT(17), Time::FromTimeT(41), &profiles, &credit_cards);

  // Two profiles should have been removed.
  ASSERT_EQ(2UL, profiles.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000001", profiles[0]->guid());
  EXPECT_EQ("00000000-0000-0000-0000-000000000002", profiles[1]->guid());

  // Make sure that only the expected profiles are still present.
  sql::Statement s_autofill_profiles_bounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM local_addresses ORDER BY guid"));
  ASSERT_TRUE(s_autofill_profiles_bounded.is_valid());
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(11, s_autofill_profiles_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(41, s_autofill_profiles_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(51, s_autofill_profiles_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_bounded.Step());
  EXPECT_EQ(61, s_autofill_profiles_bounded.ColumnInt64(0));
  EXPECT_FALSE(s_autofill_profiles_bounded.Step());

  // Make sure that only the expected profile names are still present.
  sql::Statement s_autofill_profile_names_bounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT value FROM local_addresses_type_tokens ORDER BY guid"));
  ASSERT_TRUE(s_autofill_profile_names_bounded.is_valid());
  ASSERT_TRUE(s_autofill_profile_names_bounded.Step());
  EXPECT_EQ("first name0", s_autofill_profile_names_bounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_names_bounded.Step());
  EXPECT_EQ("first name3", s_autofill_profile_names_bounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_names_bounded.Step());
  EXPECT_EQ("first name4", s_autofill_profile_names_bounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_names_bounded.Step());
  EXPECT_EQ("first name5", s_autofill_profile_names_bounded.ColumnString(0));
  EXPECT_FALSE(s_autofill_profile_names_bounded.Step());

  // Three cards should have been removed.
  ASSERT_EQ(3UL, credit_cards.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000006", credit_cards[0]->guid());
  EXPECT_EQ("00000000-0000-0000-0000-000000000007", credit_cards[1]->guid());
  EXPECT_EQ("00000000-0000-0000-0000-000000000008", credit_cards[2]->guid());

  // Make sure the expected cards are still present.
  sql::Statement s_credit_cards_bounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM credit_cards ORDER BY guid"));
  ASSERT_TRUE(s_credit_cards_bounded.is_valid());
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(47, s_credit_cards_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(57, s_credit_cards_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(67, s_credit_cards_bounded.ColumnInt64(0));
  EXPECT_FALSE(s_credit_cards_bounded.Step());

  // Make sure the expected card cvcs are still present.
  sql::Statement s_cvc_bounded(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT last_updated_timestamp FROM local_stored_cvc ORDER BY guid"));
  ASSERT_TRUE(s_cvc_bounded.is_valid());
  ASSERT_TRUE(s_cvc_bounded.Step());
  EXPECT_EQ(47, s_cvc_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_cvc_bounded.Step());
  EXPECT_EQ(57, s_cvc_bounded.ColumnInt64(0));
  ASSERT_TRUE(s_cvc_bounded.Step());
  EXPECT_EQ(67, s_cvc_bounded.ColumnInt64(0));
  EXPECT_FALSE(s_cvc_bounded.Step());

  // Remove all entries modified on or after time 51 (unbounded range).
  table_->RemoveAutofillDataModifiedBetween(Time::FromTimeT(51), Time(),
                                            &profiles, &credit_cards);
  ASSERT_EQ(2UL, profiles.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000004", profiles[0]->guid());
  EXPECT_EQ("00000000-0000-0000-0000-000000000005", profiles[1]->guid());

  // Make sure that only the expected profiles are still present.
  sql::Statement s_autofill_profiles_unbounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM local_addresses ORDER BY guid"));
  ASSERT_TRUE(s_autofill_profiles_unbounded.is_valid());
  ASSERT_TRUE(s_autofill_profiles_unbounded.Step());
  EXPECT_EQ(11, s_autofill_profiles_unbounded.ColumnInt64(0));
  ASSERT_TRUE(s_autofill_profiles_unbounded.Step());
  EXPECT_EQ(41, s_autofill_profiles_unbounded.ColumnInt64(0));
  EXPECT_FALSE(s_autofill_profiles_unbounded.Step());

  // Make sure that only the expected profile names are still present.
  sql::Statement s_autofill_profile_names_unbounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT value FROM local_addresses_type_tokens ORDER BY guid"));
  ASSERT_TRUE(s_autofill_profile_names_unbounded.is_valid());
  ASSERT_TRUE(s_autofill_profile_names_unbounded.Step());
  EXPECT_EQ("first name0", s_autofill_profile_names_unbounded.ColumnString(0));
  ASSERT_TRUE(s_autofill_profile_names_unbounded.Step());
  EXPECT_EQ("first name3", s_autofill_profile_names_unbounded.ColumnString(0));
  EXPECT_FALSE(s_autofill_profile_names_unbounded.Step());

  // Two cards should have been removed.
  ASSERT_EQ(2UL, credit_cards.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000010", credit_cards[0]->guid());
  EXPECT_EQ("00000000-0000-0000-0000-000000000011", credit_cards[1]->guid());

  // Make sure the remaining card is the expected one.
  sql::Statement s_credit_cards_unbounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_credit_cards_unbounded.is_valid());
  ASSERT_TRUE(s_credit_cards_unbounded.Step());
  EXPECT_EQ(47, s_credit_cards_unbounded.ColumnInt64(0));
  EXPECT_FALSE(s_credit_cards_unbounded.Step());

  // Make sure the remaining card cvc is the expected one.
  sql::Statement s_cvc_unbounded(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT last_updated_timestamp FROM local_stored_cvc"));
  ASSERT_TRUE(s_cvc_unbounded.is_valid());
  ASSERT_TRUE(s_cvc_unbounded.Step());
  EXPECT_EQ(47, s_cvc_unbounded.ColumnInt64(0));
  EXPECT_FALSE(s_cvc_unbounded.Step());

  // Remove all remaining entries.
  table_->RemoveAutofillDataModifiedBetween(Time(), Time(), &profiles,
                                            &credit_cards);

  // Two profiles should have been removed.
  ASSERT_EQ(2UL, profiles.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000000", profiles[0]->guid());
  EXPECT_EQ("00000000-0000-0000-0000-000000000003", profiles[1]->guid());

  // Make sure there are no profiles remaining.
  sql::Statement s_autofill_profiles_empty(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM local_addresses"));
  ASSERT_TRUE(s_autofill_profiles_empty.is_valid());
  EXPECT_FALSE(s_autofill_profiles_empty.Step());

  // Make sure there are no profile names remaining.
  sql::Statement s_autofill_profile_names_empty(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT value FROM local_addresses_type_tokens"));
  ASSERT_TRUE(s_autofill_profile_names_empty.is_valid());
  EXPECT_FALSE(s_autofill_profile_names_empty.Step());

  // One credit card should have been deleted.
  ASSERT_EQ(1UL, credit_cards.size());
  EXPECT_EQ("00000000-0000-0000-0000-000000000009", credit_cards[0]->guid());

  // There should be no cards left.
  sql::Statement s_credit_cards_empty(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified FROM credit_cards"));
  ASSERT_TRUE(s_credit_cards_empty.is_valid());
  EXPECT_FALSE(s_credit_cards_empty.Step());

  // There should be no card cvcs left.
  sql::Statement s_cvc_empty(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT last_updated_timestamp FROM local_stored_cvc"));
  ASSERT_TRUE(s_cvc_empty.is_valid());
  EXPECT_FALSE(s_cvc_empty.Step());
}

TEST_F(AutofillTableTest, RemoveOriginURLsModifiedBetween) {
  // Populate the credit_cards table.
  ASSERT_TRUE(db_->GetSQLConnection()->Execute(
      "INSERT INTO credit_cards (guid, origin, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000003', '', 17);"
      "INSERT INTO credit_cards (guid, origin, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000004', "
      "       'https://www.example.com/', 27);"
      "INSERT INTO credit_cards (guid, origin, date_modified) "
      "VALUES('00000000-0000-0000-0000-000000000005', 'Chrome settings', "
      "       37);"));

  // Remove all origin URLs set in the bounded time range [21,27).
  table_->RemoveOriginURLsModifiedBetween(Time::FromTimeT(21),
                                          Time::FromTimeT(27));
  sql::Statement s_credit_cards_bounded(
      db_->GetSQLConnection()->GetUniqueStatement(
          "SELECT date_modified, origin FROM credit_cards"));
  ASSERT_TRUE(s_credit_cards_bounded.is_valid());
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(17, s_credit_cards_bounded.ColumnInt64(0));
  EXPECT_EQ(std::string(), s_credit_cards_bounded.ColumnString(1));
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(27, s_credit_cards_bounded.ColumnInt64(0));
  EXPECT_EQ("https://www.example.com/", s_credit_cards_bounded.ColumnString(1));
  ASSERT_TRUE(s_credit_cards_bounded.Step());
  EXPECT_EQ(37, s_credit_cards_bounded.ColumnInt64(0));
  EXPECT_EQ(kSettingsOrigin, s_credit_cards_bounded.ColumnString(1));

  // Remove all origin URLS.
  table_->RemoveOriginURLsModifiedBetween(Time(), Time());
  sql::Statement s_credit_cards_all(db_->GetSQLConnection()->GetUniqueStatement(
      "SELECT date_modified, origin FROM credit_cards"));
  ASSERT_TRUE(s_credit_cards_all.is_valid());
  ASSERT_TRUE(s_credit_cards_all.Step());
  EXPECT_EQ(17, s_credit_cards_all.ColumnInt64(0));
  EXPECT_EQ(std::string(), s_credit_cards_all.ColumnString(1));
  ASSERT_TRUE(s_credit_cards_all.Step());
  EXPECT_EQ(27, s_credit_cards_all.ColumnInt64(0));
  EXPECT_EQ(std::string(), s_credit_cards_all.ColumnString(1));
  ASSERT_TRUE(s_credit_cards_all.Step());
  EXPECT_EQ(37, s_credit_cards_all.ColumnInt64(0));
  EXPECT_EQ(kSettingsOrigin, s_credit_cards_all.ColumnString(1));
}

TEST_F(AutofillTableTest, Autocomplete_GetAllAutocompleteEntries_NoResults) {
  std::vector<AutocompleteEntry> entries;
  ASSERT_TRUE(table_->GetAllAutocompleteEntries(&entries));

  EXPECT_EQ(0U, entries.size());
}

TEST_F(AutofillTableTest, Autocomplete_GetAllAutocompleteEntries_OneResult) {
  AutocompleteChangeList changes;
  std::map<std::string, std::vector<Time>> name_value_times_map;

  time_t start = 0;
  std::vector<Time> timestamps1;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(
      table_->AddFormFieldValueTime(field, &changes, Time::FromTimeT(start)));
  timestamps1.push_back(Time::FromTimeT(start));
  std::string key1("NameSuperman");
  name_value_times_map.insert(
      std::pair<std::string, std::vector<Time>>(key1, timestamps1));

  AutocompleteEntrySet expected_entries(CompareAutocompleteEntries);
  AutocompleteKey ak1(u"Name", u"Superman");
  AutocompleteEntry ae1(ak1, timestamps1.front(), timestamps1.back());

  expected_entries.insert(ae1);

  std::vector<AutocompleteEntry> entries;
  ASSERT_TRUE(table_->GetAllAutocompleteEntries(&entries));
  AutocompleteEntrySet entry_set(entries.begin(), entries.end(),
                                 CompareAutocompleteEntries);

  CompareAutocompleteEntrySets(entry_set, expected_entries);
}

TEST_F(AutofillTableTest, Autocomplete_GetAllAutocompleteEntries_TwoDistinct) {
  AutocompleteChangeList changes;
  std::map<std::string, std::vector<Time>> name_value_times_map;
  time_t start = 0;

  std::vector<Time> timestamps1;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_TRUE(
      table_->AddFormFieldValueTime(field, &changes, Time::FromTimeT(start)));
  timestamps1.push_back(Time::FromTimeT(start));
  std::string key1("NameSuperman");
  name_value_times_map.insert(
      std::pair<std::string, std::vector<Time>>(key1, timestamps1));

  ++start;
  std::vector<Time> timestamps2;
  field.name = u"Name";
  field.value = u"Clark Kent";
  EXPECT_TRUE(
      table_->AddFormFieldValueTime(field, &changes, Time::FromTimeT(start)));
  timestamps2.push_back(Time::FromTimeT(start));
  std::string key2("NameClark Kent");
  name_value_times_map.insert(
      std::pair<std::string, std::vector<Time>>(key2, timestamps2));

  AutocompleteEntrySet expected_entries(CompareAutocompleteEntries);
  AutocompleteKey ak1(u"Name", u"Superman");
  AutocompleteKey ak2(u"Name", u"Clark Kent");
  AutocompleteEntry ae1(ak1, timestamps1.front(), timestamps1.back());
  AutocompleteEntry ae2(ak2, timestamps2.front(), timestamps2.back());

  expected_entries.insert(ae1);
  expected_entries.insert(ae2);

  std::vector<AutocompleteEntry> entries;
  ASSERT_TRUE(table_->GetAllAutocompleteEntries(&entries));
  AutocompleteEntrySet entry_set(entries.begin(), entries.end(),
                                 CompareAutocompleteEntries);

  CompareAutocompleteEntrySets(entry_set, expected_entries);
}

TEST_F(AutofillTableTest, Autocomplete_GetAllAutocompleteEntries_TwoSame) {
  AutocompleteChangeList changes;
  std::map<std::string, std::vector<Time>> name_value_times_map;

  std::vector<Time> timestamps;
  time_t start = 0;
  for (int i = 0; i < 2; ++i, ++start) {
    FormFieldData field;
    field.name = u"Name";
    field.value = u"Superman";
    EXPECT_TRUE(
        table_->AddFormFieldValueTime(field, &changes, Time::FromTimeT(start)));
    timestamps.push_back(Time::FromTimeT(start));
  }

  std::string key("NameSuperman");
  name_value_times_map.insert(
      std::pair<std::string, std::vector<Time>>(key, timestamps));

  AutocompleteEntrySet expected_entries(CompareAutocompleteEntries);
  AutocompleteKey ak1(u"Name", u"Superman");
  AutocompleteEntry ae1(ak1, timestamps.front(), timestamps.back());

  expected_entries.insert(ae1);

  std::vector<AutocompleteEntry> entries;
  ASSERT_TRUE(table_->GetAllAutocompleteEntries(&entries));
  AutocompleteEntrySet entry_set(entries.begin(), entries.end(),
                                 CompareAutocompleteEntries);

  CompareAutocompleteEntrySets(entry_set, expected_entries);
}

TEST_F(AutofillTableTest, SetGetServerCards) {
  std::vector<CreditCard> inputs;
  inputs.emplace_back(CreditCard::RecordType::kFullServerCard, "a123");
  inputs[0].SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul F. Tompkins");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
  inputs[0].SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  inputs[0].set_card_issuer(CreditCard::Issuer::kGoogle);
  inputs[0].set_instrument_id(321);
  inputs[0].set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kUnenrolled);
  inputs[0].set_virtual_card_enrollment_type(
      CreditCard::VirtualCardEnrollmentType::kIssuer);
  inputs[0].set_product_description(u"Fake description");
  inputs[0].set_cvc(u"000");

  inputs.emplace_back(CreditCard::RecordType::kMaskedServerCard, "b456");
  inputs[1].SetRawInfo(CREDIT_CARD_NAME_FULL, u"Rick Roman");
  inputs[1].SetRawInfo(CREDIT_CARD_EXP_MONTH, u"12");
  inputs[1].SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"1997");
  inputs[1].SetRawInfo(CREDIT_CARD_NUMBER, u"1111");
  inputs[1].SetNetworkForMaskedCard(kVisaCard);
  std::u16string nickname = u"Grocery card";
  inputs[1].SetNickname(nickname);
  inputs[1].set_card_issuer(CreditCard::Issuer::kExternalIssuer);
  inputs[1].set_issuer_id("amex");
  inputs[1].set_instrument_id(123);
  inputs[1].set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  inputs[1].set_virtual_card_enrollment_type(
      CreditCard::VirtualCardEnrollmentType::kNetwork);
  inputs[1].set_card_art_url(GURL("https://www.example.com"));
  inputs[1].set_cvc(u"111");

  test::SetServerCreditCards(table_.get(), inputs);

  std::vector<std::unique_ptr<CreditCard>> outputs;
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(inputs.size(), outputs.size());

  // Ordering isn't guaranteed, so fix the ordering if it's backwards.
  if (outputs[1]->server_id() == inputs[0].server_id())
    std::swap(outputs[0], outputs[1]);

  // GUIDs for server cards are dynamically generated so will be different
  // after reading from the DB. Check they're valid, but otherwise don't count
  // them in the comparison.
  inputs[0].set_guid(std::string());
  inputs[1].set_guid(std::string());
  outputs[0]->set_guid(std::string());
  outputs[1]->set_guid(std::string());

  EXPECT_EQ(inputs[0], *outputs[0]);
  EXPECT_EQ(inputs[1], *outputs[1]);

  EXPECT_TRUE(outputs[0]->nickname().empty());
  EXPECT_EQ(nickname, outputs[1]->nickname());

  EXPECT_EQ(CreditCard::Issuer::kGoogle, outputs[0]->card_issuer());
  EXPECT_EQ(CreditCard::Issuer::kExternalIssuer, outputs[1]->card_issuer());
  EXPECT_EQ("", outputs[0]->issuer_id());
  EXPECT_EQ("amex", outputs[1]->issuer_id());

  EXPECT_EQ(321, outputs[0]->instrument_id());
  EXPECT_EQ(123, outputs[1]->instrument_id());

  EXPECT_EQ(CreditCard::VirtualCardEnrollmentState::kUnenrolled,
            outputs[0]->virtual_card_enrollment_state());
  EXPECT_EQ(CreditCard::VirtualCardEnrollmentState::kEnrolled,
            outputs[1]->virtual_card_enrollment_state());

  EXPECT_EQ(CreditCard::VirtualCardEnrollmentType::kIssuer,
            outputs[0]->virtual_card_enrollment_type());
  EXPECT_EQ(CreditCard::VirtualCardEnrollmentType::kNetwork,
            outputs[1]->virtual_card_enrollment_type());

  EXPECT_EQ(GURL(), outputs[0]->card_art_url());
  EXPECT_EQ(GURL("https://www.example.com"), outputs[1]->card_art_url());

  EXPECT_EQ(u"Fake description", outputs[0]->product_description());

  EXPECT_EQ(inputs[0].cvc(), outputs[0]->cvc());
  EXPECT_EQ(inputs[1].cvc(), outputs[1]->cvc());
}

TEST_F(AutofillTableTest, SetGetRemoveServerCardMetadata) {
  // Create and set the metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.billing_address_id = "billing id";
  EXPECT_TRUE(table_->AddServerCardMetadata(input));

  // Make sure it was added correctly.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);

  // Remove the metadata from the table.
  EXPECT_TRUE(table_->RemoveServerCardMetadata(input.id));

  // Make sure it was removed correctly.
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  EXPECT_EQ(0U, outputs.size());
}

TEST_F(AutofillTableTest, SetGetRemoveServerAddressMetadata) {
  // Create and set the metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.has_converted = true;
  table_->AddServerAddressMetadata(input);

  // Make sure it was added correctly.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);

  // Remove the metadata from the table.
  EXPECT_TRUE(table_->RemoveServerAddressMetadata(input.id));

  // Make sure it was removed correctly.
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  EXPECT_EQ(0U, outputs.size());
}

// Test that masked IBAN metadata can be added, retrieved and removed
// successfully.
TEST_F(AutofillTableTest, SetGetRemoveServerIbanMetadata) {
  Iban iban = test::GetServerIban();
  // Set the metadata.
  iban.set_use_count(50);
  iban.set_use_date(AutofillClock::Now());
  EXPECT_TRUE(table_->AddOrUpdateServerIbanMetadata(iban));

  // Make sure it was added correctly.
  std::vector<AutofillMetadata> outputs = table_->GetServerIbansMetadata();
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(iban.GetMetadata(), outputs[0]);

  // Remove the metadata from the table.
  EXPECT_TRUE(table_->RemoveServerIbanMetadata(outputs[0].id));

  // Make sure it was removed correctly.
  outputs = table_->GetServerIbansMetadata();
  EXPECT_EQ(0u, outputs.size());
}

TEST_F(AutofillTableTest, AddUpdateServerAddressMetadata) {
  // Create and set the metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.has_converted = true;
  ASSERT_TRUE(table_->AddServerAddressMetadata(input));

  // Make sure it was added correctly.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  ASSERT_EQ(input, outputs[input.id]);

  // Update the metadata in the table.
  input.use_count = 51;
  EXPECT_TRUE(table_->UpdateServerAddressMetadata(input));

  // Make sure it was updated correctly.
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);

  // Insert a new entry using update - that should also be legal.
  input.id = "another server id";
  EXPECT_TRUE(table_->UpdateServerAddressMetadata(input));
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  ASSERT_EQ(2U, outputs.size());
}

TEST_F(AutofillTableTest, AddUpdateServerCardMetadata) {
  // Create and set the metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.billing_address_id = "billing id";
  ASSERT_TRUE(table_->AddServerCardMetadata(input));

  // Make sure it was added correctly.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  ASSERT_EQ(input, outputs[input.id]);

  // Update the metadata in the table.
  input.use_count = 51;
  EXPECT_TRUE(table_->UpdateServerCardMetadata(input));

  // Make sure it was updated correctly.
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);

  // Insert a new entry using update - that should also be legal.
  input.id = "another server id";
  EXPECT_TRUE(table_->UpdateServerCardMetadata(input));
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  ASSERT_EQ(2U, outputs.size());
}

TEST_F(AutofillTableTest, UpdateServerAddressMetadataDoesNotChangeData) {
  AutofillProfile one(AutofillProfile::SERVER_PROFILE, "a123");
  std::vector<AutofillProfile> inputs;
  inputs.push_back(one);
  table_->SetServerProfiles(inputs);

  std::vector<std::unique_ptr<AutofillProfile>> outputs;
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(one.server_id(), outputs[0]->server_id());

  // Update metadata in the profile.
  ASSERT_NE(outputs[0]->use_count(), 51u);
  outputs[0]->set_use_count(51);

  AutofillMetadata input_metadata = outputs[0]->GetMetadata();
  EXPECT_TRUE(table_->UpdateServerAddressMetadata(input_metadata));

  // Make sure it was updated correctly.
  std::map<std::string, AutofillMetadata> output_metadata;
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&output_metadata));
  ASSERT_EQ(1U, output_metadata.size());
  EXPECT_EQ(input_metadata, output_metadata[input_metadata.id]);

  // Make sure nothing else got updated.
  std::vector<std::unique_ptr<AutofillProfile>> outputs2;
  table_->GetServerProfiles(&outputs2);
  ASSERT_EQ(1u, outputs2.size());
  EXPECT_TRUE(outputs[0]->EqualsForLegacySyncPurposes(*outputs2[0]));
}

TEST_F(AutofillTableTest, UpdateServerCardMetadataDoesNotChangeData) {
  std::vector<CreditCard> inputs;
  inputs.emplace_back(CreditCard::RecordType::kFullServerCard, "a123");
  inputs[0].SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul F. Tompkins");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
  inputs[0].SetRawInfo(CREDIT_CARD_NUMBER, u"4111111111111111");
  test::SetServerCreditCards(table_.get(), inputs);

  std::vector<std::unique_ptr<CreditCard>> outputs;
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(inputs[0].server_id(), outputs[0]->server_id());

  // Update metadata in the profile.
  ASSERT_NE(outputs[0]->use_count(), 51u);
  outputs[0]->set_use_count(51);

  AutofillMetadata input_metadata = outputs[0]->GetMetadata();
  EXPECT_TRUE(table_->UpdateServerCardMetadata(input_metadata));

  // Make sure it was updated correctly.
  std::map<std::string, AutofillMetadata> output_metadata;
  ASSERT_TRUE(table_->GetServerCardsMetadata(&output_metadata));
  ASSERT_EQ(1U, output_metadata.size());
  EXPECT_EQ(input_metadata, output_metadata[input_metadata.id]);

  // Make sure nothing else got updated.
  std::vector<std::unique_ptr<CreditCard>> outputs2;
  table_->GetServerCreditCards(&outputs2);
  ASSERT_EQ(1u, outputs2.size());
  EXPECT_EQ(0, outputs[0]->Compare(*outputs2[0]));
}

// Test that updating masked IBAN metadata won't affect IBAN data.
TEST_F(AutofillTableTest, UpdateServerIbanMetadataDoesNotChangeData) {
  std::vector<Iban> inputs = {test::GetServerIban()};
  table_->SetServerIbans(inputs);

  std::vector<std::unique_ptr<Iban>> outputs = table_->GetServerIbans();
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(inputs[0].instrument_id(), outputs[0]->instrument_id());

  // Update metadata in the IBAN.
  outputs[0]->set_use_count(outputs[0]->use_count() + 1);

  EXPECT_TRUE(table_->AddOrUpdateServerIbanMetadata(*outputs[0]));

  // Make sure it was updated correctly.
  std::vector<AutofillMetadata> output_metadata =
      table_->GetServerIbansMetadata();
  ASSERT_EQ(1U, output_metadata.size());
  EXPECT_EQ(outputs[0]->GetMetadata(), output_metadata[0]);

  // Make sure nothing else got updated.
  std::vector<std::unique_ptr<Iban>> outputs2 = table_->GetServerIbans();
  ASSERT_EQ(1U, outputs2.size());
  EXPECT_EQ(0, outputs[0]->Compare(*outputs2[0]));
}

TEST_F(AutofillTableTest, RemoveWrongServerCardMetadata) {
  // Crete and set some metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.billing_address_id = "billing id";
  table_->AddServerCardMetadata(input);

  // Make sure it was added correctly.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);

  // Try removing some non-existent metadata.
  EXPECT_FALSE(table_->RemoveServerCardMetadata("a_wrong_id"));

  // Make sure the metadata was not removed.
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
}

TEST_F(AutofillTableTest, SetServerCardsData) {
  // Set a card data.
  std::vector<CreditCard> inputs;
  inputs.emplace_back(CreditCard::RecordType::kMaskedServerCard, "card1");
  inputs[0].SetRawInfo(CREDIT_CARD_NAME_FULL, u"Rick Roman");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_MONTH, u"12");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"1997");
  inputs[0].SetRawInfo(CREDIT_CARD_NUMBER, u"1111");
  inputs[0].SetNetworkForMaskedCard(kVisaCard);
  inputs[0].SetNickname(u"Grocery card");
  inputs[0].set_card_issuer(CreditCard::Issuer::kExternalIssuer);
  inputs[0].set_issuer_id("amex");
  inputs[0].set_instrument_id(1);
  inputs[0].set_virtual_card_enrollment_state(
      CreditCard::VirtualCardEnrollmentState::kEnrolled);
  inputs[0].set_virtual_card_enrollment_type(
      CreditCard::VirtualCardEnrollmentType::kIssuer);
  inputs[0].set_card_art_url(GURL("https://www.example.com"));
  inputs[0].set_product_description(u"Fake description");

  table_->SetServerCardsData(inputs);

  // Make sure the card was added correctly.
  std::vector<std::unique_ptr<CreditCard>> outputs;
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(inputs.size(), outputs.size());

  // GUIDs for server cards are dynamically generated so will be different
  // after reading from the DB. Check they're valid, but otherwise don't count
  // them in the comparison.
  inputs[0].set_guid(std::string());
  outputs[0]->set_guid(std::string());

  EXPECT_EQ(inputs[0], *outputs[0]);

  EXPECT_EQ(CreditCard::VirtualCardEnrollmentState::kEnrolled,
            outputs[0]->virtual_card_enrollment_state());

  EXPECT_EQ(CreditCard::VirtualCardEnrollmentType::kIssuer,
            outputs[0]->virtual_card_enrollment_type());

  EXPECT_EQ(CreditCard::Issuer::kExternalIssuer, outputs[0]->card_issuer());
  EXPECT_EQ("amex", outputs[0]->issuer_id());

  EXPECT_EQ(GURL("https://www.example.com"), outputs[0]->card_art_url());
  EXPECT_EQ(u"Fake description", outputs[0]->product_description());

  // Make sure no metadata was added.
  std::map<std::string, AutofillMetadata> metadata_map;
  ASSERT_TRUE(table_->GetServerCardsMetadata(&metadata_map));
  ASSERT_EQ(0U, metadata_map.size());

  // Set a different card.
  inputs[0] = CreditCard(CreditCard::RecordType::kMaskedServerCard, "card2");
  table_->SetServerCardsData(inputs);

  // The original one should have been replaced.
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ("card2", outputs[0]->server_id());
  EXPECT_EQ(CreditCard::Issuer::kIssuerUnknown, outputs[0]->card_issuer());
  EXPECT_EQ("", outputs[0]->issuer_id());

  // Make sure no metadata was added.
  ASSERT_TRUE(table_->GetServerCardsMetadata(&metadata_map));
  ASSERT_EQ(0U, metadata_map.size());
}

// Tests that adding server cards data does not delete the existing metadata.
TEST_F(AutofillTableTest, SetServerCardsData_ExistingMetadata) {
  // Create and set some metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.billing_address_id = "billing id";
  table_->AddServerCardMetadata(input);

  // Set a card data.
  std::vector<CreditCard> inputs;
  inputs.emplace_back(CreditCard::RecordType::kMaskedServerCard, "server id");
  table_->SetServerCardsData(inputs);

  // Make sure the metadata is still intact.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerCardsMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);
}

TEST_F(AutofillTableTest, SetServerAddressesData) {
  AutofillProfile one(AutofillProfile::SERVER_PROFILE, "a123");
  std::vector<AutofillProfile> inputs;
  inputs.push_back(one);
  table_->SetServerAddressesData(inputs);

  // Make sure the address was added correctly.
  std::vector<std::unique_ptr<AutofillProfile>> outputs;
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(one.server_id(), outputs[0]->server_id());

  outputs.clear();

  // Make sure no metadata was added.
  std::map<std::string, AutofillMetadata> metadata_map;
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&metadata_map));
  ASSERT_EQ(0U, metadata_map.size());

  // Set a different profile.
  AutofillProfile two(AutofillProfile::SERVER_PROFILE, "b456");
  inputs[0] = two;
  table_->SetServerAddressesData(inputs);

  // The original one should have been replaced.
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(two.server_id(), outputs[0]->server_id());

  // Make sure no metadata was added.
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&metadata_map));
  ASSERT_EQ(0U, metadata_map.size());
}

// Tests that adding server addresses data does not delete the existing
// metadata.
TEST_F(AutofillTableTest, SetServerAddressesData_ExistingMetadata) {
  // Create and set some metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.has_converted = true;
  table_->AddServerAddressMetadata(input);

  // Set an address data.
  std::vector<AutofillProfile> inputs;
  inputs.push_back(
      AutofillProfile(AutofillProfile::SERVER_PROFILE, "server id"));
  table_->SetServerAddressesData(inputs);

  // Make sure the metadata is still intact.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);
}

TEST_F(AutofillTableTest, RemoveWrongServerAddressMetadata) {
  // Crete and set some metadata.
  AutofillMetadata input;
  input.id = "server id";
  input.use_count = 50;
  input.use_date = AutofillClock::Now();
  input.has_converted = true;
  table_->AddServerAddressMetadata(input);

  // Make sure it was added correctly.
  std::map<std::string, AutofillMetadata> outputs;
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
  EXPECT_EQ(input, outputs[input.id]);

  // Try removing some non-existent metadata.
  EXPECT_FALSE(table_->RemoveServerAddressMetadata("a_wrong_id"));

  // Make sure the metadata was not removed.
  ASSERT_TRUE(table_->GetServerAddressesMetadata(&outputs));
  ASSERT_EQ(1U, outputs.size());
}

TEST_F(AutofillTableTest, MaskUnmaskServerCards) {
  std::u16string masked_number(u"1111");
  std::vector<CreditCard> inputs;
  inputs.emplace_back(CreditCard::RecordType::kMaskedServerCard, "a123");
  inputs[0].SetRawInfo(CREDIT_CARD_NAME_FULL, u"Jay Johnson");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
  inputs[0].SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
  inputs[0].SetRawInfo(CREDIT_CARD_NUMBER, masked_number);
  inputs[0].SetNetworkForMaskedCard(kVisaCard);
  test::SetServerCreditCards(table_.get(), inputs);

  // Unmask the number. The full number should be available.
  std::u16string full_number(u"4111111111111111");
  ASSERT_TRUE(table_->UnmaskServerCreditCard(inputs[0], full_number));

  std::vector<std::unique_ptr<CreditCard>> outputs;
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_TRUE(CreditCard::RecordType::kFullServerCard ==
              outputs[0]->record_type());
  EXPECT_EQ(full_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));

  outputs.clear();

  // Re-mask the number, we should only get the last 4 digits out.
  ASSERT_TRUE(table_->MaskServerCreditCard(inputs[0].server_id()));
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_TRUE(CreditCard::RecordType::kMaskedServerCard ==
              outputs[0]->record_type());
  EXPECT_EQ(masked_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));

  outputs.clear();
}

// Calling SetServerCreditCards should replace all existing cards, but unmasked
// cards should not be re-masked.
TEST_F(AutofillTableTest, SetServerCardModify) {
  // Add a masked card.
  CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  masked_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul F. Tompkins");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
  masked_card.SetRawInfo(CREDIT_CARD_NUMBER, u"1111");
  masked_card.SetNetworkForMaskedCard(kVisaCard);

  std::vector<CreditCard> inputs;
  inputs.push_back(masked_card);
  test::SetServerCreditCards(table_.get(), inputs);

  // Now unmask it.
  std::u16string full_number = u"4111111111111111";
  table_->UnmaskServerCreditCard(masked_card, full_number);

  // The card should now be unmasked.
  std::vector<std::unique_ptr<CreditCard>> outputs;
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_TRUE(outputs[0]->record_type() ==
              CreditCard::RecordType::kFullServerCard);
  EXPECT_EQ(full_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));

  outputs.clear();

  // Call set again with the masked number.
  inputs[0] = masked_card;
  test::SetServerCreditCards(table_.get(), inputs);

  // The card should stay unmasked.
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_TRUE(outputs[0]->record_type() ==
              CreditCard::RecordType::kFullServerCard);
  EXPECT_EQ(full_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));

  outputs.clear();

  // Set inputs that do not include our old card.
  CreditCard random_card(CreditCard::RecordType::kMaskedServerCard, "b456");
  random_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Rick Roman");
  random_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"12");
  random_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"1997");
  random_card.SetRawInfo(CREDIT_CARD_NUMBER, u"2222");
  random_card.SetNetworkForMaskedCard(kVisaCard);
  inputs[0] = random_card;
  test::SetServerCreditCards(table_.get(), inputs);

  // We should have only the new card, the other one should have been deleted.
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_TRUE(outputs[0]->record_type() ==
              CreditCard::RecordType::kMaskedServerCard);
  EXPECT_EQ(random_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(u"2222", outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));

  outputs.clear();

  // Putting back the original card masked should make it masked (this tests
  // that the unmasked data was really deleted).
  inputs[0] = masked_card;
  test::SetServerCreditCards(table_.get(), inputs);
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_TRUE(outputs[0]->record_type() ==
              CreditCard::RecordType::kMaskedServerCard);
  EXPECT_EQ(masked_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(u"1111", outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));

  outputs.clear();
}

TEST_F(AutofillTableTest, SetServerCardUpdateUsageStatsAndBillingAddress) {
  // Add a masked card.
  CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  masked_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul F. Tompkins");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
  masked_card.SetRawInfo(CREDIT_CARD_NUMBER, u"1111");
  masked_card.set_billing_address_id("1");
  masked_card.SetNetworkForMaskedCard(kVisaCard);

  std::vector<CreditCard> inputs;
  inputs.push_back(masked_card);
  test::SetServerCreditCards(table_.get(), inputs);

  std::vector<std::unique_ptr<CreditCard>> outputs;
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(masked_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(1U, outputs[0]->use_count());
  EXPECT_NE(base::Time(), outputs[0]->use_date());
  // We don't track modification date for server cards. It should always be
  // base::Time().
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  outputs.clear();

  // Update the usage stats; make sure they're reflected in GetServerProfiles.
  inputs.back().set_use_count(4U);
  inputs.back().set_use_date(base::Time());
  inputs.back().set_billing_address_id("2");
  table_->UpdateServerCardMetadata(inputs.back());
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(masked_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(4U, outputs[0]->use_count());
  EXPECT_EQ(base::Time(), outputs[0]->use_date());
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  EXPECT_EQ("2", outputs[0]->billing_address_id());
  outputs.clear();

  // Setting the cards again shouldn't delete the usage stats.
  table_->SetServerCreditCards(inputs);
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(masked_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(4U, outputs[0]->use_count());
  EXPECT_EQ(base::Time(), outputs[0]->use_date());
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  EXPECT_EQ("2", outputs[0]->billing_address_id());
  outputs.clear();

  // Set a card list where the card is missing --- this should clear metadata.
  CreditCard masked_card2(CreditCard::RecordType::kMaskedServerCard, "b456");
  inputs.back() = masked_card2;
  table_->SetServerCreditCards(inputs);

  // Back to the original card list.
  inputs.back() = masked_card;
  table_->SetServerCreditCards(inputs);
  table_->GetServerCreditCards(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(masked_card.server_id(), outputs[0]->server_id());
  EXPECT_EQ(1U, outputs[0]->use_count());
  EXPECT_NE(base::Time(), outputs[0]->use_date());
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  EXPECT_EQ("1", outputs[0]->billing_address_id());
  outputs.clear();
}

TEST_F(AutofillTableTest, SetServerProfile) {
  AutofillProfile one(AutofillProfile::SERVER_PROFILE, "a123");
  std::vector<AutofillProfile> inputs;
  inputs.push_back(one);
  table_->SetServerProfiles(inputs);

  std::vector<std::unique_ptr<AutofillProfile>> outputs;
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(one.server_id(), outputs[0]->server_id());

  outputs.clear();

  // Set a different profile.
  AutofillProfile two(AutofillProfile::SERVER_PROFILE, "b456");
  inputs[0] = two;
  table_->SetServerProfiles(inputs);

  // The original one should have been replaced.
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(two.server_id(), outputs[0]->server_id());

  outputs.clear();
}

TEST_F(AutofillTableTest, SetServerProfileUpdateUsageStats) {
  AutofillProfile one(AutofillProfile::SERVER_PROFILE, "a123");
  std::vector<AutofillProfile> inputs;
  inputs.push_back(one);
  table_->SetServerProfiles(inputs);

  std::vector<std::unique_ptr<AutofillProfile>> outputs;
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(one.server_id(), outputs[0]->server_id());
  EXPECT_EQ(1U, outputs[0]->use_count());
  EXPECT_NE(base::Time(), outputs[0]->use_date());
  // We don't track modification date for server profiles. It should always be
  // base::Time().
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  outputs.clear();

  // Update the usage stats; make sure they're reflected in GetServerProfiles.
  inputs.back().set_use_count(4U);
  inputs.back().set_use_date(AutofillClock::Now());
  table_->UpdateServerAddressMetadata(inputs.back());
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(one.server_id(), outputs[0]->server_id());
  EXPECT_EQ(4U, outputs[0]->use_count());
  EXPECT_NE(base::Time(), outputs[0]->use_date());
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  outputs.clear();

  // Setting the profiles again shouldn't delete the usage stats.
  table_->SetServerProfiles(inputs);
  table_->GetServerProfiles(&outputs);
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(one.server_id(), outputs[0]->server_id());
  EXPECT_EQ(4U, outputs[0]->use_count());
  EXPECT_NE(base::Time(), outputs[0]->use_date());
  EXPECT_EQ(base::Time(), outputs[0]->modification_date());
  outputs.clear();
}

// Tests that deleting time ranges re-masks server credit cards that were
// unmasked in that time.
TEST_F(AutofillTableTest, DeleteUnmaskedCard) {
  // This isn't the exact unmasked time, since the database will use the
  // current time that it is called. The code below has to be approximate.
  base::Time unmasked_time = AutofillClock::Now();

  // Add a masked card.
  std::u16string masked_number = u"1111";
  CreditCard masked_card(CreditCard::RecordType::kMaskedServerCard, "a123");
  masked_card.SetRawInfo(CREDIT_CARD_NAME_FULL, u"Paul F. Tompkins");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_MONTH, u"1");
  masked_card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, u"2020");
  masked_card.SetRawInfo(CREDIT_CARD_NUMBER, masked_number);
  masked_card.SetNetworkForMaskedCard(kVisaCard);

  std::vector<CreditCard> inputs;
  inputs.push_back(masked_card);
  table_->SetServerCreditCards(inputs);

  // Unmask it.
  std::u16string full_number = u"4111111111111111";
  table_->UnmaskServerCreditCard(masked_card, full_number);

  // Delete data in a range a year in the future.
  std::vector<std::unique_ptr<AutofillProfile>> profiles;
  std::vector<std::unique_ptr<CreditCard>> credit_cards;
  ASSERT_TRUE(table_->RemoveAutofillDataModifiedBetween(
      unmasked_time + base::Days(365), unmasked_time + base::Days(530),
      &profiles, &credit_cards));

  // This should not affect the unmasked card (should be unmasked).
  std::vector<std::unique_ptr<CreditCard>> outputs;
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(CreditCard::RecordType::kFullServerCard, outputs[0]->record_type());
  EXPECT_EQ(full_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));
  outputs.clear();

  // Delete data in the range of the last 24 hours.
  // Fudge |now| to make sure it's strictly greater than the |now| that
  // the database uses.
  base::Time now = AutofillClock::Now() + base::Seconds(1);
  ASSERT_TRUE(table_->RemoveAutofillDataModifiedBetween(
      now - base::Days(1), now, &profiles, &credit_cards));

  // This should re-mask.
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(CreditCard::RecordType::kMaskedServerCard,
            outputs[0]->record_type());
  EXPECT_EQ(masked_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));
  outputs.clear();

  // Unmask again, the card should be back.
  table_->UnmaskServerCreditCard(masked_card, full_number);
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(CreditCard::RecordType::kFullServerCard, outputs[0]->record_type());
  EXPECT_EQ(full_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));
  outputs.clear();

  // Delete all data.
  ASSERT_TRUE(table_->RemoveAutofillDataModifiedBetween(
      base::Time(), base::Time::Max(), &profiles, &credit_cards));

  // Should be masked again.
  ASSERT_TRUE(table_->GetServerCreditCards(&outputs));
  ASSERT_EQ(1u, outputs.size());
  EXPECT_EQ(CreditCard::RecordType::kMaskedServerCard,
            outputs[0]->record_type());
  EXPECT_EQ(masked_number, outputs[0]->GetRawInfo(CREDIT_CARD_NUMBER));
  outputs.clear();
}

// Test that we can get what we set.
TEST_F(AutofillTableTest, SetGetPaymentsCustomerData) {
  PaymentsCustomerData input{/*customer_id=*/"deadbeef"};
  table_->SetPaymentsCustomerData(&input);

  std::unique_ptr<PaymentsCustomerData> output;
  ASSERT_TRUE(table_->GetPaymentsCustomerData(&output));
  EXPECT_EQ(input, *output);
}

// We don't set anything in the table. Test that we don't crash.
TEST_F(AutofillTableTest, GetPaymentsCustomerData_NoData) {
  std::unique_ptr<PaymentsCustomerData> output;
  ASSERT_TRUE(table_->GetPaymentsCustomerData(&output));
  EXPECT_FALSE(output);
}

// The latest PaymentsCustomerData that was set is returned.
TEST_F(AutofillTableTest, SetGetPaymentsCustomerData_MultipleSet) {
  PaymentsCustomerData input{/*customer_id=*/"deadbeef"};
  table_->SetPaymentsCustomerData(&input);

  PaymentsCustomerData input2{/*customer_id=*/"wallet"};
  table_->SetPaymentsCustomerData(&input2);

  PaymentsCustomerData input3{/*customer_id=*/"latest"};
  table_->SetPaymentsCustomerData(&input3);

  std::unique_ptr<PaymentsCustomerData> output;
  ASSERT_TRUE(table_->GetPaymentsCustomerData(&output));
  EXPECT_EQ(input3, *output);
}

TEST_F(AutofillTableTest, SetGetCreditCardCloudData_OneTimeSet) {
  std::vector<CreditCardCloudTokenData> inputs;
  inputs.push_back(test::GetCreditCardCloudTokenData1());
  inputs.push_back(test::GetCreditCardCloudTokenData2());
  table_->SetCreditCardCloudTokenData(inputs);

  std::vector<std::unique_ptr<CreditCardCloudTokenData>> outputs;
  ASSERT_TRUE(table_->GetCreditCardCloudTokenData(&outputs));
  EXPECT_EQ(outputs.size(), inputs.size());
  EXPECT_EQ(0, outputs[0]->Compare(test::GetCreditCardCloudTokenData1()));
  EXPECT_EQ(0, outputs[1]->Compare(test::GetCreditCardCloudTokenData2()));
}

TEST_F(AutofillTableTest, SetGetCreditCardCloudData_MultipleSet) {
  std::vector<CreditCardCloudTokenData> inputs;
  CreditCardCloudTokenData input1 = test::GetCreditCardCloudTokenData1();
  inputs.push_back(input1);
  table_->SetCreditCardCloudTokenData(inputs);

  inputs.clear();
  CreditCardCloudTokenData input2 = test::GetCreditCardCloudTokenData2();
  inputs.push_back(input2);
  table_->SetCreditCardCloudTokenData(inputs);

  std::vector<std::unique_ptr<CreditCardCloudTokenData>> outputs;
  ASSERT_TRUE(table_->GetCreditCardCloudTokenData(&outputs));
  EXPECT_EQ(1u, outputs.size());
  EXPECT_EQ(0, outputs[0]->Compare(test::GetCreditCardCloudTokenData2()));
}

TEST_F(AutofillTableTest, GetCreditCardCloudData_NoData) {
  std::vector<std::unique_ptr<CreditCardCloudTokenData>> output;
  ASSERT_TRUE(table_->GetCreditCardCloudTokenData(&output));
  EXPECT_TRUE(output.empty());
}

class AutofillTableTestPerModelType
    : public AutofillTableTest,
      public testing::WithParamInterface<syncer::ModelType> {
 public:
  AutofillTableTestPerModelType() = default;
  AutofillTableTestPerModelType(const AutofillTableTestPerModelType&) = delete;
  AutofillTableTestPerModelType& operator=(
      const AutofillTableTestPerModelType&) = delete;
  ~AutofillTableTestPerModelType() override = default;
};

TEST_P(AutofillTableTestPerModelType, AutofillNoMetadata) {
  syncer::ModelType model_type = GetParam();
  MetadataBatch metadata_batch;
  EXPECT_TRUE(table_->GetAllSyncMetadata(model_type, &metadata_batch));
  EXPECT_EQ(0u, metadata_batch.TakeAllMetadata().size());
  EXPECT_EQ(ModelTypeState().SerializeAsString(),
            metadata_batch.GetModelTypeState().SerializeAsString());
}

TEST_P(AutofillTableTestPerModelType, AutofillGetAllSyncMetadata) {
  syncer::ModelType model_type = GetParam();
  EntityMetadata metadata;
  std::string storage_key = "storage_key";
  std::string storage_key2 = "storage_key2";
  metadata.set_sequence_number(1);

  EXPECT_TRUE(table_->UpdateEntityMetadata(model_type, storage_key, metadata));

  ModelTypeState model_type_state;
  model_type_state.set_initial_sync_state(
      sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  EXPECT_TRUE(table_->UpdateModelTypeState(model_type, model_type_state));

  metadata.set_sequence_number(2);
  EXPECT_TRUE(table_->UpdateEntityMetadata(model_type, storage_key2, metadata));

  MetadataBatch metadata_batch;
  EXPECT_TRUE(table_->GetAllSyncMetadata(model_type, &metadata_batch));

  EXPECT_EQ(metadata_batch.GetModelTypeState().initial_sync_state(),
            sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  EntityMetadataMap metadata_records = metadata_batch.TakeAllMetadata();

  EXPECT_EQ(metadata_records.size(), 2u);
  EXPECT_EQ(metadata_records[storage_key]->sequence_number(), 1);
  EXPECT_EQ(metadata_records[storage_key2]->sequence_number(), 2);

  // Now check that a model type state update replaces the old value
  model_type_state.set_initial_sync_state(
      sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);
  EXPECT_TRUE(table_->UpdateModelTypeState(model_type, model_type_state));

  EXPECT_TRUE(table_->GetAllSyncMetadata(model_type, &metadata_batch));
  EXPECT_EQ(
      metadata_batch.GetModelTypeState().initial_sync_state(),
      sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_STATE_UNSPECIFIED);
}

TEST_P(AutofillTableTestPerModelType, AutofillWriteThenDeleteSyncMetadata) {
  syncer::ModelType model_type = GetParam();
  EntityMetadata metadata;
  MetadataBatch metadata_batch;
  std::string storage_key = "storage_key";
  ModelTypeState model_type_state;

  model_type_state.set_initial_sync_state(
      sync_pb::ModelTypeState_InitialSyncState_INITIAL_SYNC_DONE);

  metadata.set_client_tag_hash("client_hash");

  // Write the data into the store.
  EXPECT_TRUE(table_->UpdateEntityMetadata(model_type, storage_key, metadata));
  EXPECT_TRUE(table_->UpdateModelTypeState(model_type, model_type_state));
  // Delete the data we just wrote.
  EXPECT_TRUE(table_->ClearEntityMetadata(model_type, storage_key));
  // It shouldn't be there any more.
  EXPECT_TRUE(table_->GetAllSyncMetadata(model_type, &metadata_batch));

  EntityMetadataMap metadata_records = metadata_batch.TakeAllMetadata();
  EXPECT_EQ(metadata_records.size(), 0u);

  // Now delete the model type state.
  EXPECT_TRUE(table_->ClearModelTypeState(model_type));
  EXPECT_TRUE(table_->GetAllSyncMetadata(model_type, &metadata_batch));
  EXPECT_EQ(ModelTypeState().SerializeAsString(),
            metadata_batch.GetModelTypeState().SerializeAsString());
}

TEST_P(AutofillTableTestPerModelType, AutofillCorruptSyncMetadata) {
  syncer::ModelType model_type = GetParam();
  MetadataBatch metadata_batch;
  sql::Statement s(db_->GetSQLConnection()->GetUniqueStatement(
      "INSERT OR REPLACE INTO autofill_sync_metadata "
      "(model_type, storage_key, value) VALUES(?, ?, ?)"));
  s.BindInt(0, syncer::ModelTypeToStableIdentifier(model_type));
  s.BindString(1, "storage_key");
  s.BindString(2, "unparseable");
  EXPECT_TRUE(s.Run());

  EXPECT_FALSE(table_->GetAllSyncMetadata(model_type, &metadata_batch));
}

TEST_P(AutofillTableTestPerModelType, AutofillCorruptModelTypeState) {
  syncer::ModelType model_type = GetParam();
  MetadataBatch metadata_batch;
  sql::Statement s(db_->GetSQLConnection()->GetUniqueStatement(
      "INSERT OR REPLACE INTO autofill_model_type_state "
      "(model_type, value) VALUES(?, ?)"));
  s.BindInt(0, syncer::ModelTypeToStableIdentifier(model_type));
  s.BindString(1, "unparseable");
  EXPECT_TRUE(s.Run());

  EXPECT_FALSE(table_->GetAllSyncMetadata(model_type, &metadata_batch));
}

INSTANTIATE_TEST_SUITE_P(AutofillTableTest,
                         AutofillTableTestPerModelType,
                         testing::Values(syncer::AUTOFILL,
                                         syncer::AUTOFILL_PROFILE));

TEST_F(AutofillTableTest, SetAndGetCreditCardOfferData) {
  // Set Offer ID.
  int64_t offer_id_1 = 1;
  int64_t offer_id_2 = 2;
  int64_t offer_id_3 = 3;

  // Set reward amounts for card-linked offers on offer 1 and 2.
  std::string offer_reward_amount_1 = "$5";
  std::string offer_reward_amount_2 = "10%";

  // Set promo code for offer 3.
  std::string promo_code_3 = "5PCTOFFSHOES";

  // Set expiry.
  base::Time expiry_1 = base::Time::FromSecondsSinceUnixEpoch(1000);
  base::Time expiry_2 = base::Time::FromSecondsSinceUnixEpoch(2000);
  base::Time expiry_3 = base::Time::FromSecondsSinceUnixEpoch(3000);

  // Set details URL.
  GURL offer_details_url_1 = GURL("https://www.offer_1_example.com/");
  GURL offer_details_url_2 = GURL("https://www.offer_2_example.com/");
  GURL offer_details_url_3 = GURL("https://www.offer_3_example.com/");

  // Set merchant domains for offer 1.
  std::vector<GURL> merchant_origins_1;
  merchant_origins_1.emplace_back("http://www.merchant_domain_1_1.com/");
  std::vector<GURL> merchant_origins_2;
  merchant_origins_2.emplace_back("http://www.merchant_domain_1_2.com/");
  std::vector<GURL> merchant_origins_3;
  merchant_origins_3.emplace_back("http://www.merchant_domain_1_3.com/");
  // Set merchant domains for offer 2.
  merchant_origins_2.emplace_back("http://www.merchant_domain_2_1.com/");
  // Set merchant domains for offer 3.
  merchant_origins_3.emplace_back("http://www.merchant_domain_3_1.com/");
  merchant_origins_3.emplace_back("http://www.merchant_domain_3_2.com/");

  DisplayStrings display_strings_1;
  DisplayStrings display_strings_2;
  DisplayStrings display_strings_3;
  // Set display strings for all 3 offers.
  display_strings_1.value_prop_text = "$5 off your purchase";
  display_strings_2.value_prop_text = "10% off your purchase";
  display_strings_3.value_prop_text = "5% off shoes. Up to $50.";
  display_strings_1.see_details_text = "Terms apply.";
  display_strings_2.see_details_text = "Terms apply.";
  display_strings_3.see_details_text = "See details.";
  display_strings_1.usage_instructions_text =
      "Check out with this card to activate.";
  display_strings_2.usage_instructions_text =
      "Check out with this card to activate.";
  display_strings_3.usage_instructions_text =
      "Click the promo code field at checkout to autofill it.";

  std::vector<int64_t> eligible_instrument_id_1;
  std::vector<int64_t> eligible_instrument_id_2;
  std::vector<int64_t> eligible_instrument_id_3;

  // Set eligible card-linked instrument ID for offer 1.
  eligible_instrument_id_1.push_back(10);
  eligible_instrument_id_1.push_back(11);
  // Set eligible card-linked instrument ID for offer 2.
  eligible_instrument_id_2.push_back(20);
  eligible_instrument_id_2.push_back(21);
  eligible_instrument_id_2.push_back(22);

  // Create vector of offer data.
  std::vector<AutofillOfferData> autofill_offer_data;
  autofill_offer_data.push_back(AutofillOfferData::GPayCardLinkedOffer(
      offer_id_1, expiry_1, merchant_origins_1, offer_details_url_1,
      display_strings_2, eligible_instrument_id_1, offer_reward_amount_1));
  autofill_offer_data.push_back(AutofillOfferData::GPayCardLinkedOffer(
      offer_id_2, expiry_2, merchant_origins_2, offer_details_url_2,
      display_strings_2, eligible_instrument_id_2, offer_reward_amount_2));
  autofill_offer_data.push_back(AutofillOfferData::GPayPromoCodeOffer(
      offer_id_3, expiry_3, merchant_origins_3, offer_details_url_3,
      display_strings_3, promo_code_3));

  table_->SetAutofillOffers(autofill_offer_data);

  std::vector<std::unique_ptr<AutofillOfferData>> output_offer_data;

  EXPECT_TRUE(table_->GetAutofillOffers(&output_offer_data));
  EXPECT_EQ(autofill_offer_data.size(), output_offer_data.size());

  for (const auto& data : autofill_offer_data) {
    // Find output data with corresponding Offer ID.
    size_t output_index = 0;
    while (output_index < output_offer_data.size()) {
      if (data.GetOfferId() == output_offer_data[output_index]->GetOfferId()) {
        break;
      }
      output_index++;
    }

    // Expect to find matching Offer ID's.
    EXPECT_NE(output_index, output_offer_data.size());

    // All corresponding fields must be equal.
    EXPECT_EQ(data.GetOfferId(), output_offer_data[output_index]->GetOfferId());
    EXPECT_EQ(data.GetOfferRewardAmount(),
              output_offer_data[output_index]->GetOfferRewardAmount());
    EXPECT_EQ(data.GetPromoCode(),
              output_offer_data[output_index]->GetPromoCode());
    EXPECT_EQ(data.GetExpiry(), output_offer_data[output_index]->GetExpiry());
    EXPECT_EQ(data.GetOfferDetailsUrl().spec(),
              output_offer_data[output_index]->GetOfferDetailsUrl().spec());
    EXPECT_EQ(
        data.GetDisplayStrings().value_prop_text,
        output_offer_data[output_index]->GetDisplayStrings().value_prop_text);
    EXPECT_EQ(
        data.GetDisplayStrings().see_details_text,
        output_offer_data[output_index]->GetDisplayStrings().see_details_text);
    EXPECT_EQ(data.GetDisplayStrings().usage_instructions_text,
              output_offer_data[output_index]
                  ->GetDisplayStrings()
                  .usage_instructions_text);
    ASSERT_THAT(data.GetMerchantOrigins(),
                testing::UnorderedElementsAreArray(
                    output_offer_data[output_index]->GetMerchantOrigins()));
    ASSERT_THAT(
        data.GetEligibleInstrumentIds(),
        testing::UnorderedElementsAreArray(
            output_offer_data[output_index]->GetEligibleInstrumentIds()));
  }
}

TEST_F(AutofillTableTest, SetAndGetVirtualCardUsageData) {
  // Create test data.
  VirtualCardUsageData virtual_card_usage_data_1 =
      test::GetVirtualCardUsageData1();
  VirtualCardUsageData virtual_card_usage_data_2 =
      test::GetVirtualCardUsageData2();

  // Create vector of VCN usage data.
  std::vector<VirtualCardUsageData> virtual_card_usage_data;
  virtual_card_usage_data.push_back(virtual_card_usage_data_1);
  virtual_card_usage_data.push_back(virtual_card_usage_data_2);

  table_->SetVirtualCardUsageData(virtual_card_usage_data);

  std::vector<std::unique_ptr<VirtualCardUsageData>> output_data;

  EXPECT_TRUE(table_->GetAllVirtualCardUsageData(&output_data));
  EXPECT_EQ(virtual_card_usage_data.size(), output_data.size());

  for (const auto& data : virtual_card_usage_data) {
    // Find output data with corresponding data.
    auto it = base::ranges::find(output_data, data.instrument_id(),
                                 &VirtualCardUsageData::instrument_id);

    // Expect to find a usage data match in the vector.
    EXPECT_NE(it, output_data.end());

    // All corresponding fields must be equal.
    EXPECT_EQ(data.usage_data_id(), (*it)->usage_data_id());
    EXPECT_EQ(data.instrument_id(), (*it)->instrument_id());
    EXPECT_EQ(data.virtual_card_last_four(), (*it)->virtual_card_last_four());
    EXPECT_EQ(data.merchant_origin().Serialize(),
              (*it)->merchant_origin().Serialize());
  }
}

TEST_F(AutofillTableTest, AddUpdateRemoveVirtualCardUsageData) {
  // Add a valid VirtualCardUsageData.
  VirtualCardUsageData virtual_card_usage_data =
      test::GetVirtualCardUsageData1();
  EXPECT_TRUE(table_->AddOrUpdateVirtualCardUsageData(virtual_card_usage_data));

  // Get the inserted VirtualCardUsageData.
  std::string usage_data_id = *virtual_card_usage_data.usage_data_id();
  std::unique_ptr<VirtualCardUsageData> usage_data =
      table_->GetVirtualCardUsageData(usage_data_id);
  ASSERT_TRUE(usage_data);
  EXPECT_EQ(virtual_card_usage_data, *usage_data);

  // Update the virtual card usage data.
  VirtualCardUsageData virtual_card_usage_data_update =
      VirtualCardUsageData(virtual_card_usage_data.usage_data_id(),
                           virtual_card_usage_data.instrument_id(),
                           VirtualCardUsageData::VirtualCardLastFour(u"4444"),
                           virtual_card_usage_data.merchant_origin());
  EXPECT_TRUE(
      table_->AddOrUpdateVirtualCardUsageData(virtual_card_usage_data_update));
  usage_data = table_->GetVirtualCardUsageData(usage_data_id);
  ASSERT_TRUE(usage_data);
  EXPECT_EQ(virtual_card_usage_data_update, *usage_data);

  // Remove the virtual card usage data.
  EXPECT_TRUE(table_->RemoveVirtualCardUsageData(usage_data_id));
  usage_data = table_->GetVirtualCardUsageData(usage_data_id);
  EXPECT_FALSE(usage_data);
}

TEST_F(AutofillTableTest, RemoveAllVirtualCardUsageData) {
  EXPECT_TRUE(table_->AddOrUpdateVirtualCardUsageData(
      test::GetVirtualCardUsageData1()));

  EXPECT_TRUE(table_->RemoveAllVirtualCardUsageData());

  std::vector<std::unique_ptr<VirtualCardUsageData>> usage_data;
  EXPECT_TRUE(table_->GetAllVirtualCardUsageData(&usage_data));
  EXPECT_TRUE(usage_data.empty());
}

TEST_F(AutofillTableTest, DontCrashWhenAddingValueToPoisonedDB) {
  // Simulate a preceding fatal error.
  db_->GetSQLConnection()->Poison();

  // Simulate the submission of a form.
  AutocompleteChangeList changes;
  FormFieldData field;
  field.name = u"Name";
  field.value = u"Superman";
  EXPECT_FALSE(table_->AddFormFieldValue(field, &changes));
}

}  // namespace autofill
