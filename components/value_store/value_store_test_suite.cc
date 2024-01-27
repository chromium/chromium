// Copyright 2014 The Chromium Authors
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

}  // namespace

// Returns whether the read result of a storage operation has the expected
// settings.
void ExpectSettingsEq(const base::Value::Dict& expected,
                      ValueStore::ReadResult actual_result) {
  ASSERT_TRUE(actual_result.status().ok()) << actual_result.status().message;
  EXPECT_EQ(expected, actual_result.settings());
}

// Returns whether the write result of a storage operation has the expected
// changes.
void ExpectChangesEq(const ValueStoreChangeList& expected,
                     ValueStore::WriteResult actual_result) {
  ASSERT_TRUE(actual_result.status().ok()) << actual_result.status().message;

  const ValueStoreChangeList& actual = actual_result.changes();
  ASSERT_EQ(expected.size(), actual.size());

  std::map<std::string, const ValueStoreChange*> expected_as_map;
  for (const ValueStoreChange& change : expected)
    expected_as_map[change.key] = &change;

  std::set<std::string> keys_seen;

  for (const auto& it : actual) {
    EXPECT_EQ(keys_seen.count(it.key), 0U);
    keys_seen.insert(it.key);

    EXPECT_EQ(expected_as_map.count(it.key), 1U);

    const auto* expected_change = expected_as_map[it.key];
    EXPECT_EQ(expected_change->new_value, it.new_value);
    EXPECT_EQ(expected_change->old_value, it.old_value);
  }
}

base::Value::Dict MakeTestMap(std::map<std::string, std::string> pairs) {
  base::Value::Dict map;
  for (const auto& it : pairs)
    map.Set(it.first, std::move(it.second));
  return map;
}

ValueStoreTestSuite::ValueStoreTestSuite() = default;
ValueStoreTestSuite::~ValueStoreTestSuite() = default;

void ValueStoreTestSuite::SetUp() {
  ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  storage_.reset((GetParam())(temp_dir_.GetPath().AppendASCII("dbName")));
  ASSERT_TRUE(storage_.get());
}

void ValueStoreTestSuite::TearDown() {
  storage_.reset();
}

TEST_P(ValueStoreTestSuite, NonexistentKeysReturnOk) {
  auto result = storage_->Get("key");
  ASSERT_TRUE(result.status().ok());
  EXPECT_EQ(result.settings(), base::Value::Dict());
}

TEST_P(ValueStoreTestSuite, SetProducesMatchingChanges) {
  ValueStoreChangeList expected_changes;
  expected_changes.push_back(
      ValueStoreChange{"foo", std::nullopt, base::Value{"baz"}});
  expected_changes.push_back(
      ValueStoreChange{"bar", std::nullopt, base::Value{"qux"}});
  base::Value::Dict settings = MakeTestMap({{"foo", "baz"}, {"bar", "qux"}});
  ExpectChangesEq(expected_changes, storage_->Set(DEFAULTS, settings));
}

TEST_P(ValueStoreTestSuite, RemoveMissingProducesNoChange) {
  ExpectChangesEq(ValueStoreChangeList{}, storage_->Remove("foo"));
}

TEST_P(ValueStoreTestSuite, RemoveSingleKeyProducesMatchingChanges) {
  base::Value::Dict settings = MakeTestMap({{"foo", "baz"}, {"bar", "qux"}});
  storage_->Set(DEFAULTS, settings);

  ValueStoreChangeList changes;
  changes.push_back(ValueStoreChange{"foo", base::Value{"baz"}, std::nullopt});
  ExpectChangesEq(changes, storage_->Remove("foo"));
}

TEST_P(ValueStoreTestSuite, RemoveKeyListProducesMatchingChanges) {
  base::Value::Dict settings =
      MakeTestMap({{"foo", "baz"}, {"bar", "qux"}, {"abc", "def"}});
  storage_->Set(DEFAULTS, settings);

  ValueStoreChangeList changes;
  changes.push_back(ValueStoreChange{"foo", base::Value{"baz"}, std::nullopt});
  changes.push_back(ValueStoreChange{"bar", base::Value{"qux"}, std::nullopt});
  ExpectChangesEq(changes,
                  storage_->Remove(std::vector<std::string>{"foo", "bar"}));
}

TEST_P(ValueStoreTestSuite, SetValuesCanBeRetrievedOneAtATime) {
  base::Value::Dict settings = MakeTestMap({{"foo", "baz"}, {"bar", "qux"}});
  storage_->Set(DEFAULTS, settings);
  ExpectSettingsEq(MakeTestMap({{"foo", "baz"}}), storage_->Get("foo"));
  ExpectSettingsEq(MakeTestMap({{"bar", "qux"}}), storage_->Get("bar"));
}

TEST_P(ValueStoreTestSuite, SetValuesCanBeRetrievedWithKeyList) {
  base::Value::Dict settings = MakeTestMap({{"foo", "baz"}, {"bar", "qux"}});
  storage_->Set(DEFAULTS, settings);
  ExpectSettingsEq(settings,
                   storage_->Get(std::vector<std::string>{"foo", "bar"}));
}

TEST_P(ValueStoreTestSuite, MissingKeysSkippedInListRetrieve) {
  base::Value::Dict settings = MakeTestMap({{"foo", "baz"}, {"bar", "qux"}});
  storage_->Set(DEFAULTS, settings);
  ExpectSettingsEq(
      settings, storage_->Get(std::vector<std::string>{"foo", "bar", "baz"}));
}

TEST_P(ValueStoreTestSuite, GetAllDoesGetAll) {
  base::Value::Dict settings = MakeTestMap({{"foo", "baz"}, {"bar", "qux"}});
  storage_->Set(DEFAULTS, settings);
  ExpectSettingsEq(settings, storage_->Get());
}

TEST_P(ValueStoreTestSuite, RemovedSingleValueIsGone) {
  base::Value::Dict settings =
      MakeTestMap({{"foo", "baz"}, {"bar", "qux"}, {"abc", "def"}});
  storage_->Set(DEFAULTS, settings);
  ExpectSettingsEq(MakeTestMap({{"foo", "baz"}}), storage_->Get("foo"));
  storage_->Remove("foo");
  ExpectSettingsEq(base::Value::Dict(), storage_->Get("foo"));
  ExpectSettingsEq(MakeTestMap({{"bar", "qux"}}), storage_->Get("bar"));
}

TEST_P(ValueStoreTestSuite, RemovedMultipleValuesAreGone) {
  base::Value::Dict settings =
      MakeTestMap({{"foo", "baz"}, {"bar", "qux"}, {"abc", "def"}});
  storage_->Set(DEFAULTS, settings);
  ExpectSettingsEq(MakeTestMap({{"foo", "baz"}, {"bar", "qux"}}),
                   storage_->Get(std::vector<std::string>{"foo", "bar"}));
  storage_->Remove(std::vector<std::string>{"foo", "bar"});
  ExpectSettingsEq(base::Value::Dict(), storage_->Get("foo"));
  ExpectSettingsEq(base::Value::Dict(), storage_->Get("bar"));
  ExpectSettingsEq(MakeTestMap({{"abc", "def"}}), storage_->Get("abc"));
}

TEST_P(ValueStoreTestSuite, SetOverwritesOnlyExistingValue) {
  storage_->Set(DEFAULTS, MakeTestMap({{"foo", "bar"}, {"abc", "def"}}));
  storage_->Set(DEFAULTS, MakeTestMap({{"foo", "baz"}}));
  ExpectSettingsEq(MakeTestMap({{"foo", "baz"}, {"abc", "def"}}),
                   storage_->Get(std::vector<std::string>{"foo", "abc"}));
}

TEST_P(ValueStoreTestSuite, ClearGeneratesNoChangesWhenEmpty) {
  ExpectChangesEq({}, storage_->Clear());
}

TEST_P(ValueStoreTestSuite, ClearGeneratesChanges) {
  storage_->Set(DEFAULTS, MakeTestMap({{"foo", "baz"}, {"bar", "qux"}}));

  ValueStoreChangeList changes;
  changes.push_back(ValueStoreChange("foo", base::Value("baz"), std::nullopt));
  changes.push_back(ValueStoreChange("bar", base::Value("qux"), std::nullopt));
  ExpectChangesEq(changes, storage_->Clear());
}

TEST_P(ValueStoreTestSuite, ClearedValuesAreGone) {
  storage_->Set(DEFAULTS, MakeTestMap({{"foo", "baz"}, {"bar", "qux"}}));
  storage_->Clear();
  ExpectSettingsEq(base::Value::Dict(), storage_->Get("foo"));
  ExpectSettingsEq(base::Value::Dict(), storage_->Get("bar"));
}

TEST_P(ValueStoreTestSuite, DotsAllowedInKeyNames) {
  base::Value::Dict dict;
  base::Value::Dict inner_dict;
  inner_dict.Set("bar", "baz");
  dict.Set("foo", std::move(inner_dict));
  dict.Set("foo.bar", "qux");

  storage_->Set(DEFAULTS, dict);

  ExpectSettingsEq(MakeTestMap({{"foo.bar", "qux"}}), storage_->Get("foo.bar"));
}

// This test suite is instantiated by implementers of ValueStore.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(ValueStoreTestSuite);

}  // namespace value_store
