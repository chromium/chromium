// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/prefs/mock_pref_change_callback.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Mock;

class ScopedUserPrefUpdateTest : public testing::Test {
 public:
  ScopedUserPrefUpdateTest() : observer_(&prefs_) {}
  ~ScopedUserPrefUpdateTest() override {}

 protected:
  void SetUp() override {
    prefs_.registry()->RegisterDictionaryPref(kPref);
    registrar_.Init(&prefs_);
    registrar_.Add(kPref, observer_.GetCallback());
  }

  static const char kPref[];
  static const char kKey[];
  static const char kValue[];

  TestingPrefServiceSimple prefs_;
  MockPrefChangeCallback observer_;
  PrefChangeRegistrar registrar_;
};

const char ScopedUserPrefUpdateTest::kPref[] = "name";
const char ScopedUserPrefUpdateTest::kKey[] = "key";
const char ScopedUserPrefUpdateTest::kValue[] = "value";

TEST_F(ScopedUserPrefUpdateTest, RegularUse) {
  // Dictionary that will be expected to be set at the end.
  base::Value expected_dictionary(base::Value::Type::DICTIONARY);
  expected_dictionary.SetStringKey(kKey, kValue);

  {
    EXPECT_CALL(observer_, OnPreferenceChanged(_)).Times(0);
    DictionaryPrefUpdate update(&prefs_, kPref);
    base::Value* value = update.Get();
    ASSERT_TRUE(value);
    value->SetStringKey(kKey, kValue);

    // The dictionary was created for us but the creation should have happened
    // silently without notifications.
    Mock::VerifyAndClearExpectations(&observer_);

    // Modifications happen online and are instantly visible, though.
    const base::Value* current_value = prefs_.GetDictionary(kPref);
    ASSERT_TRUE(current_value);
    EXPECT_EQ(expected_dictionary, *current_value);

    // Now we are leaving the scope of the update so we should be notified.
    observer_.Expect(kPref, &expected_dictionary);
  }
  Mock::VerifyAndClearExpectations(&observer_);

  const base::Value* current_value = prefs_.GetDictionary(kPref);
  ASSERT_TRUE(current_value);
  EXPECT_EQ(expected_dictionary, *current_value);
}

TEST_F(ScopedUserPrefUpdateTest, NeverTouchAnything) {
  const base::Value* old_value = prefs_.GetDictionary(kPref);
  EXPECT_CALL(observer_, OnPreferenceChanged(_)).Times(0);
  { DictionaryPrefUpdate update(&prefs_, kPref); }
  const base::Value* new_value = prefs_.GetDictionary(kPref);
  EXPECT_EQ(old_value, new_value);
  Mock::VerifyAndClearExpectations(&observer_);
}

TEST_F(ScopedUserPrefUpdateTest, UpdatingListPrefWithDefaults) {
  base::Value::List defaults;
  defaults.Append("firstvalue");
  defaults.Append("secondvalue");

  std::string pref_name = "mypref";
  prefs_.registry()->RegisterListPref(pref_name, std::move(defaults));
  EXPECT_EQ(2u, prefs_.GetValueList(pref_name)->size());

  ListPrefUpdate update(&prefs_, pref_name);
  update->Append("thirdvalue");
  EXPECT_EQ(3u, prefs_.GetValueList(pref_name)->size());
}

TEST_F(ScopedUserPrefUpdateTest, UpdatingDictionaryPrefWithDefaults) {
  base::Value::Dict defaults;
  defaults.Set("firstkey", "value");
  defaults.Set("secondkey", "value");

  std::string pref_name = "mypref";
  prefs_.registry()->RegisterDictionaryPref(pref_name, std::move(defaults));
  EXPECT_EQ(2u, prefs_.GetDictionary(pref_name)->DictSize());

  DictionaryPrefUpdate update(&prefs_, pref_name);
  update->SetStringKey("thirdkey", "value");
  EXPECT_EQ(3u, prefs_.GetDictionary(pref_name)->DictSize());
}
