// Copyright 2013 The Chromium Authors
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
  ScopedUserPrefUpdateTest()
      : dict_observer_(&prefs_), list_observer_(&prefs_) {}
  ~ScopedUserPrefUpdateTest() override = default;

 protected:
  void SetUp() override {
    prefs_.registry()->RegisterDictionaryPref(kDictPref);
    prefs_.registry()->RegisterListPref(kListPref);
    registrar_.Init(&prefs_);
    registrar_.Add(kDictPref, dict_observer_.GetCallback());
    registrar_.Add(kListPref, list_observer_.GetCallback());
  }

  static const char kDictPref[];
  static const char kListPref[];
  static const char kKey[];
  static const char kValue[];

  TestingPrefServiceSimple prefs_;
  MockPrefChangeCallback dict_observer_;
  MockPrefChangeCallback list_observer_;
  PrefChangeRegistrar registrar_;
};

const char ScopedUserPrefUpdateTest::kDictPref[] = "dict_pref_name";
const char ScopedUserPrefUpdateTest::kListPref[] = "list_pref_name";
const char ScopedUserPrefUpdateTest::kKey[] = "key";
const char ScopedUserPrefUpdateTest::kValue[] = "value";

TEST_F(ScopedUserPrefUpdateTest, ScopedDictPrefUpdateRegularUse) {
  // Dictionary that will be expected to be set at the end.
  base::Value expected_dictionary(base::Value::Type::DICT);
  expected_dictionary.GetDict().Set(kKey, kValue);

  {
    EXPECT_CALL(dict_observer_, OnPreferenceChanged(_)).Times(0);
    ScopedDictPrefUpdate update(&prefs_, kDictPref);
    update->Set(kKey, kValue);

    // The dictionary was created for us but the creation should have happened
    // silently without notifications.
    Mock::VerifyAndClearExpectations(&dict_observer_);

    // Modifications happen online and are instantly visible, though.
    EXPECT_EQ(expected_dictionary, prefs_.GetDict(kDictPref));

    // Now we are leaving the scope of the update so we should be notified.
    dict_observer_.Expect(kDictPref, &expected_dictionary);
  }
  Mock::VerifyAndClearExpectations(&dict_observer_);

  EXPECT_EQ(expected_dictionary, prefs_.GetDict(kDictPref));
}

TEST_F(ScopedUserPrefUpdateTest, ScopedDictPrefUpdateNeverTouchAnything) {
  const base::Value::Dict& old_value = prefs_.GetDict(kDictPref);
  EXPECT_CALL(dict_observer_, OnPreferenceChanged(_)).Times(0);
  { ScopedDictPrefUpdate update(&prefs_, kDictPref); }
  const base::Value::Dict& new_value = prefs_.GetDict(kDictPref);
  EXPECT_EQ(old_value, new_value);
  Mock::VerifyAndClearExpectations(&dict_observer_);
}

TEST_F(ScopedUserPrefUpdateTest, ScopedDictPrefUpdateWithDefaults) {
  auto defaults =
      base::Value::Dict().Set("firstkey", "value").Set("secondkey", "value");

  std::string pref_name = "mypref";
  prefs_.registry()->RegisterDictionaryPref(pref_name, std::move(defaults));
  EXPECT_EQ(2u, prefs_.GetDict(pref_name).size());

  ScopedDictPrefUpdate update(&prefs_, pref_name);
  update->Set("thirdkey", "value");
  EXPECT_EQ(3u, prefs_.GetDict(pref_name).size());
}

TEST_F(ScopedUserPrefUpdateTest, ScopedListPrefUpdateRegularUse) {
  // List that will be expected to be set at the end.
  base::Value expected_list(base::Value::Type::LIST);
  expected_list.GetList().Append(kValue);

  {
    EXPECT_CALL(list_observer_, OnPreferenceChanged(_)).Times(0);
    ScopedListPrefUpdate update(&prefs_, kListPref);
    update->Append(kValue);

    // The list was created for us but the creation should have happened
    // silently without notifications.
    Mock::VerifyAndClearExpectations(&list_observer_);

    // Modifications happen online and are instantly visible, though.
    EXPECT_EQ(expected_list, prefs_.GetList(kListPref));

    // Now we are leaving the scope of the update so we should be notified.
    list_observer_.Expect(kListPref, &expected_list);
  }
  Mock::VerifyAndClearExpectations(&list_observer_);

  EXPECT_EQ(expected_list.GetList(), prefs_.GetList(kListPref));
}

TEST_F(ScopedUserPrefUpdateTest, ScopedListPrefUpdateNeverTouchAnything) {
  const base::Value::List& old_value = prefs_.GetList(kListPref);
  EXPECT_CALL(dict_observer_, OnPreferenceChanged(_)).Times(0);
  { ScopedListPrefUpdate update(&prefs_, kListPref); }
  const base::Value::List& new_value = prefs_.GetList(kListPref);
  EXPECT_EQ(old_value, new_value);
  Mock::VerifyAndClearExpectations(&dict_observer_);
}

TEST_F(ScopedUserPrefUpdateTest, ScopedListPrefUpdateWithDefaults) {
  base::Value::List defaults;
  defaults.Append("firstvalue");
  defaults.Append("secondvalue");

  std::string pref_name = "mypref";
  prefs_.registry()->RegisterListPref(pref_name, std::move(defaults));
  EXPECT_EQ(2u, prefs_.GetList(pref_name).size());

  ScopedListPrefUpdate update(&prefs_, pref_name);
  update->Append("thirdvalue");
  EXPECT_EQ(3u, prefs_.GetList(pref_name).size());
}
