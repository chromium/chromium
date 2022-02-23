// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/value_store/value_store_test_suite.h"

#include <memory>
#include <utility>

#include "base/json/json_writer.h"
#include "base/values.h"

namespace value_store {

namespace {

// To save typing ValueStore::DEFAULTS everywhere.
const ValueStore::WriteOptions DEFAULTS = ValueStore::DEFAULTS;

// Gets the pretty-printed JSON for a value.
std::string GetJSON(const base::Value& value) {
  std::string json;
  base::JSONWriter::WriteWithOptions(
      value, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);
  return json;
}

}  // namespace

// Compares two possibly NULL values for equality, filling |error| with an
// appropriate error message if they're different.
bool ValuesEqual(const base::Value* expected,
                 const base::Value* actual,
                 std::string* error) {
  if (expected == actual) {
    return true;
  }
  if (expected && !actual) {
    *error = "Expected: " + GetJSON(*expected) + ", actual: NULL";
    return false;
  }
  if (actual && !expected) {
    *error = "Expected: NULL, actual: " + GetJSON(*actual);
    return false;
  }
  if (!expected->Equals(actual)) {
    *error =
        "Expected: " + GetJSON(*expected) + ", actual: " + GetJSON(*actual);
    return false;
  }
  return true;
}

// Returns whether the read result of a storage operation has the expected
// settings.
testing::AssertionResult SettingsEq(const char* _1,
                                    const char* _2,
                                    const base::DictionaryValue& expected,
                                    ValueStore::ReadResult actual_result) {
  if (!actual_result.status().ok()) {
    return testing::AssertionFailure()
           << "Result has error: " << actual_result.status().message;
  }

  std::string error;
  if (!ValuesEqual(&expected, &actual_result.settings(), &error)) {
    return testing::AssertionFailure() << error;
  }

  return testing::AssertionSuccess();
}

// Returns whether the write result of a storage operation has the expected
// changes.
testing::AssertionResult ChangesEq(const char* _1,
                                   const char* _2,
                                   const ValueStoreChangeList& expected,
                                   ValueStore::WriteResult actual_result) {
  if (!actual_result.status().ok()) {
    return testing::AssertionFailure()
           << "Result has error: " << actual_result.status().message;
  }

  const ValueStoreChangeList& actual = actual_result.changes();
  if (expected.size() != actual.size()) {
    return testing::AssertionFailure()
           << "Actual has wrong size, expecting " << expected.size()
           << " but was " << actual.size();
  }

  std::map<std::string, const ValueStoreChange*> expected_as_map;
  for (const ValueStoreChange& change : expected)
    expected_as_map[change.key()] = &change;

  std::set<std::string> keys_seen;

  for (const auto& it : actual) {
    if (keys_seen.count(it.key())) {
      return testing::AssertionFailure()
             << "Multiple changes seen for key: " << it.key();
    }
    keys_seen.insert(it.key());

    if (!expected_as_map.count(it.key())) {
      return testing::AssertionFailure()
             << "Actual has unexpected change for key: " << it.key();
    }

    const ValueStoreChange* expected_change = expected_as_map[it.key()];
    std::string error;
    if (!ValuesEqual(expected_change->new_value(), it.new_value(), &error)) {
      return testing::AssertionFailure()
             << "New value for " << it.key() << " was unexpected: " << error;
    }
    if (!ValuesEqual(expected_change->old_value(), it.old_value(), &error)) {
      return testing::AssertionFailure()
             << "Old value for " << it.key() << " was unexpected: " << error;
    }
  }

  return testing::AssertionSuccess();
}

ValueStoreTestSuite::ValueStoreTestSuite()
    : key1_("foo"),
      key2_("bar"),
      key3_("baz"),
      empty_dict_(new base::DictionaryValue()),
      dict1_(new base::DictionaryValue()),
      dict3_(new base::DictionaryValue()),
      dict12_(new base::DictionaryValue()),
      dict123_(new base::DictionaryValue()) {
  val1_ = std::make_unique<base::Value>(key1_ + "Value");
  val2_ = std::make_unique<base::Value>(key2_ + "Value");
  val3_ = std::make_unique<base::Value>(key3_ + "Value");

  list1_.push_back(key1_);
  list2_.push_back(key2_);
  list3_.push_back(key3_);
  list12_.push_back(key1_);
  list12_.push_back(key2_);
  list13_.push_back(key1_);
  list13_.push_back(key3_);
  list123_.push_back(key1_);
  list123_.push_back(key2_);
  list123_.push_back(key3_);

  set1_.insert(list1_.begin(), list1_.end());
  set2_.insert(list2_.begin(), list2_.end());
  set3_.insert(list3_.begin(), list3_.end());
  set12_.insert(list12_.begin(), list12_.end());
  set13_.insert(list13_.begin(), list13_.end());
  set123_.insert(list123_.begin(), list123_.end());

  dict1_->SetKey(key1_, val1_->Clone());
  dict3_->SetKey(key3_, val3_->Clone());
  dict12_->SetKey(key1_, val1_->Clone());
  dict12_->SetKey(key2_, val2_->Clone());
  dict123_->SetKey(key1_, val1_->Clone());
  dict123_->SetKey(key2_, val2_->Clone());
  dict123_->SetKey(key3_, val3_->Clone());
}

ValueStoreTestSuite::~ValueStoreTestSuite() = default;

void ValueStoreTestSuite::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  storage_.reset((GetParam())(temp_dir_.GetPath().AppendASCII("dbName")));
  ASSERT_TRUE(storage_.get());
}

void ValueStoreTestSuite::TearDown() {
  storage_.reset();
}

TEST_P(ValueStoreTestSuite, GetWhenEmpty) {
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get());
}

TEST_P(ValueStoreTestSuite, GetWithSingleValue) {
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key1_, absl::nullopt, val1_->Clone()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes,
                        storage_->Set(DEFAULTS, key1_, *val1_));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key2_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key3_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get());
}

TEST_P(ValueStoreTestSuite, GetWithMultipleValues) {
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key1_, absl::nullopt, val1_->Clone()));
    changes.push_back(ValueStoreChange(key2_, absl::nullopt, val2_->Clone()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Set(DEFAULTS, *dict12_));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key3_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get());
}

TEST_P(ValueStoreTestSuite, RemoveWhenEmpty) {
  EXPECT_PRED_FORMAT2(ChangesEq, ValueStoreChangeList(),
                      storage_->Remove(key1_));

  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get());
}

TEST_P(ValueStoreTestSuite, RemoveWithSingleValue) {
  storage_->Set(DEFAULTS, *dict1_);
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key1_, val1_->Clone(), absl::nullopt));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(key1_));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key2_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list12_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get());
}

TEST_P(ValueStoreTestSuite, RemoveWithMultipleValues) {
  storage_->Set(DEFAULTS, *dict123_);
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key3_, val3_->Clone(), absl::nullopt));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(key3_));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key3_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(list1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get(list12_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(list13_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get());

  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key1_, val1_->Clone(), absl::nullopt));
    changes.push_back(ValueStoreChange(key2_, val2_->Clone(), absl::nullopt));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(list12_));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key3_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list12_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list13_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get());
}

TEST_P(ValueStoreTestSuite, SetWhenOverwriting) {
  storage_->Set(DEFAULTS, key1_, *val2_);
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key1_, val2_->Clone(), val1_->Clone()));
    changes.push_back(ValueStoreChange(key2_, absl::nullopt, val2_->Clone()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Set(DEFAULTS, *dict12_));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key3_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(list1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get(list12_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict1_, storage_->Get(list13_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *dict12_, storage_->Get());
}

TEST_P(ValueStoreTestSuite, ClearWhenEmpty) {
  EXPECT_PRED_FORMAT2(ChangesEq, ValueStoreChangeList(), storage_->Clear());

  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get());
}

TEST_P(ValueStoreTestSuite, ClearWhenNotEmpty) {
  storage_->Set(DEFAULTS, *dict12_);
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key1_, val1_->Clone(), absl::nullopt));
    changes.push_back(ValueStoreChange(key2_, val2_->Clone(), absl::nullopt));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Clear());
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(key1_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(empty_list_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(list123_));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get());
}

// Dots should be allowed in key names; they shouldn't be interpreted as
// indexing into a dictionary.
TEST_P(ValueStoreTestSuite, DotsInKeyNames) {
  std::string dot_key("foo.bar");
  base::Value dot_value("baz.qux");
  std::vector<std::string> dot_list;
  dot_list.push_back(dot_key);
  base::DictionaryValue dot_dict;
  dot_dict.SetKey(dot_key, dot_value.Clone());

  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(dot_key));

  {
    ValueStoreChangeList changes;
    changes.push_back(
        ValueStoreChange(dot_key, absl::nullopt, dot_value.Clone()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes,
                        storage_->Set(DEFAULTS, dot_key, dot_value));
  }
  EXPECT_PRED_FORMAT2(ChangesEq, ValueStoreChangeList(),
                      storage_->Set(DEFAULTS, dot_key, dot_value));

  EXPECT_PRED_FORMAT2(SettingsEq, dot_dict, storage_->Get(dot_key));

  {
    ValueStoreChangeList changes;
    changes.push_back(
        ValueStoreChange(dot_key, dot_value.Clone(), absl::nullopt));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(dot_key));
  }
  EXPECT_PRED_FORMAT2(ChangesEq, ValueStoreChangeList(),
                      storage_->Remove(dot_key));
  {
    ValueStoreChangeList changes;
    changes.push_back(
        ValueStoreChange(dot_key, absl::nullopt, dot_value.Clone()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Set(DEFAULTS, dot_dict));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, dot_dict, storage_->Get(dot_list));
  EXPECT_PRED_FORMAT2(SettingsEq, dot_dict, storage_->Get());

  {
    ValueStoreChangeList changes;
    changes.push_back(
        ValueStoreChange(dot_key, dot_value.Clone(), absl::nullopt));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(dot_list));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get(dot_key));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get());
}

TEST_P(ValueStoreTestSuite, DotsInKeyNamesWithDicts) {
  base::DictionaryValue outer_dict;
  base::DictionaryValue inner_dict;
  inner_dict.SetString("bar", "baz");
  outer_dict.Set("foo", inner_dict.CreateDeepCopy());

  {
    ValueStoreChangeList changes;
    changes.push_back(
        ValueStoreChange("foo", absl::nullopt, inner_dict.Clone()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes,
                        storage_->Set(DEFAULTS, outer_dict));
  }

  EXPECT_PRED_FORMAT2(SettingsEq, outer_dict, storage_->Get("foo"));
  EXPECT_PRED_FORMAT2(SettingsEq, *empty_dict_, storage_->Get("foo.bar"));
}

TEST_P(ValueStoreTestSuite, ComplexChangedKeysScenarios) {
  // Test:
  //   - Setting over missing/changed/same keys, combinations.
  //   - Removing over missing and present keys, combinations.
  //   - Clearing.
  std::vector<std::string> complex_list;
  base::DictionaryValue complex_changed_dict;

  storage_->Set(DEFAULTS, key1_, *val1_);
  EXPECT_PRED_FORMAT2(ChangesEq, ValueStoreChangeList(),
                      storage_->Set(DEFAULTS, key1_, *val1_));
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key1_, val1_->Clone(), val2_->Clone()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes,
                        storage_->Set(DEFAULTS, key1_, *val2_));
  }
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key1_, val2_->Clone(), absl::nullopt));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(key1_));
    EXPECT_PRED_FORMAT2(ChangesEq, ValueStoreChangeList(),
                        storage_->Remove(key1_));
  }
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key1_, absl::nullopt, val1_->Clone()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes,
                        storage_->Set(DEFAULTS, key1_, *val1_));
  }
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key1_, val1_->Clone(), absl::nullopt));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Clear());
    EXPECT_PRED_FORMAT2(ChangesEq, ValueStoreChangeList(), storage_->Clear());
  }

  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key1_, absl::nullopt, val1_->Clone()));
    changes.push_back(ValueStoreChange(key2_, absl::nullopt, val2_->Clone()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Set(DEFAULTS, *dict12_));
    EXPECT_PRED_FORMAT2(ChangesEq, ValueStoreChangeList(),
                        storage_->Set(DEFAULTS, *dict12_));
  }
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key3_, absl::nullopt, val3_->Clone()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Set(DEFAULTS, *dict123_));
  }
  {
    base::DictionaryValue to_set;
    to_set.SetKey(key1_, val2_->Clone());
    to_set.SetKey(key2_, val2_->Clone());
    to_set.SetKey("asdf", val1_->Clone());
    to_set.SetKey("qwerty", val3_->Clone());

    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key1_, val1_->Clone(), val2_->Clone()));
    changes.push_back(ValueStoreChange("asdf", absl::nullopt, val1_->Clone()));
    changes.push_back(
        ValueStoreChange("qwerty", absl::nullopt, val3_->Clone()));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Set(DEFAULTS, to_set));
  }
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key1_, val2_->Clone(), absl::nullopt));
    changes.push_back(ValueStoreChange(key2_, val2_->Clone(), absl::nullopt));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(list12_));
  }
  {
    std::vector<std::string> to_remove;
    to_remove.push_back(key1_);
    to_remove.push_back("asdf");

    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange("asdf", val1_->Clone(), absl::nullopt));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Remove(to_remove));
  }
  {
    ValueStoreChangeList changes;
    changes.push_back(ValueStoreChange(key3_, val3_->Clone(), absl::nullopt));
    changes.push_back(
        ValueStoreChange("qwerty", val3_->Clone(), absl::nullopt));
    EXPECT_PRED_FORMAT2(ChangesEq, changes, storage_->Clear());
    EXPECT_PRED_FORMAT2(ChangesEq, ValueStoreChangeList(), storage_->Clear());
  }
}

// This test suite is instantiated by implementers of ValueStore.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ValueStoreTestSuite);

}  // namespace value_store
