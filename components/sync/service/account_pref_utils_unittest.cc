// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/base/account_pref_utils.h"

#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/gaia_id_hash.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace syncer {

namespace {

// Note that while the per-Gaia-ID values are scalar, the pref itself is still
// a dictionary.
constexpr char kPrefPathScalar[] = "pref_path.scalar";
constexpr char kPrefPathDict[] = "pref_path.dict";

TEST(AccountPrefUtils, ShouldGetAndSetScalarPref) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(kPrefPathScalar);

  const signin::GaiaIdHash gaia_id_hash =
      signin::GaiaIdHash::FromGaiaId("gaia_id");

  ASSERT_FALSE(
      GetAccountKeyedPrefValue(&pref_service, kPrefPathScalar, gaia_id_hash));

  SetAccountKeyedPrefValue(&pref_service, kPrefPathScalar, gaia_id_hash,
                           base::Value("value"));

  {
    const base::Value* value =
        GetAccountKeyedPrefValue(&pref_service, kPrefPathScalar, gaia_id_hash);
    ASSERT_TRUE(value);
    EXPECT_EQ(*value, base::Value("value"));
  }

  ClearAccountKeyedPrefValue(&pref_service, kPrefPathScalar, gaia_id_hash);
  EXPECT_FALSE(
      GetAccountKeyedPrefValue(&pref_service, kPrefPathScalar, gaia_id_hash));
}

TEST(AccountPrefUtils, ShouldKeepGaiaIdsSeparateForScalarPref) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(kPrefPathScalar);

  const signin::GaiaIdHash gaia_id_hash_1 =
      signin::GaiaIdHash::FromGaiaId("gaia_id_1");
  const signin::GaiaIdHash gaia_id_hash_2 =
      signin::GaiaIdHash::FromGaiaId("gaia_id_2");

  SetAccountKeyedPrefValue(&pref_service, kPrefPathScalar, gaia_id_hash_1,
                           base::Value("value_1"));
  SetAccountKeyedPrefValue(&pref_service, kPrefPathScalar, gaia_id_hash_2,
                           base::Value("value_2"));

  // Setting the same pref for the second GaiaID shouldn't have affected the
  // first one's value.
  {
    const base::Value* value_1 = GetAccountKeyedPrefValue(
        &pref_service, kPrefPathScalar, gaia_id_hash_1);
    ASSERT_TRUE(value_1);
    EXPECT_EQ(*value_1, base::Value("value_1"));
  }
}

TEST(AccountPrefUtils, ShouldGetAndSetDictPrefEntries) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(kPrefPathDict);

  const signin::GaiaIdHash gaia_id_hash =
      signin::GaiaIdHash::FromGaiaId("gaia_id");

  const char kKey1[] = "key1";
  const char kKey2[] = "key2";

  ASSERT_FALSE(GetAccountKeyedPrefDictEntry(&pref_service, kPrefPathDict,
                                            gaia_id_hash, kKey1));
  ASSERT_FALSE(GetAccountKeyedPrefDictEntry(&pref_service, kPrefPathDict,
                                            gaia_id_hash, kKey2));

  SetAccountKeyedPrefDictEntry(&pref_service, kPrefPathDict, gaia_id_hash,
                               kKey1, base::Value("value_1"));
  SetAccountKeyedPrefDictEntry(&pref_service, kPrefPathDict, gaia_id_hash,
                               kKey2, base::Value(123));

  {
    const base::Value* value_1 = GetAccountKeyedPrefDictEntry(
        &pref_service, kPrefPathDict, gaia_id_hash, kKey1);
    ASSERT_TRUE(value_1);
    EXPECT_EQ(*value_1, base::Value("value_1"));
  }
  {
    const base::Value* value_2 = GetAccountKeyedPrefDictEntry(
        &pref_service, kPrefPathDict, gaia_id_hash, kKey2);
    ASSERT_TRUE(value_2);
    EXPECT_EQ(*value_2, base::Value(123));
  }

  ClearAccountKeyedPrefValue(&pref_service, kPrefPathDict, gaia_id_hash);
  EXPECT_FALSE(GetAccountKeyedPrefDictEntry(&pref_service, kPrefPathDict,
                                            gaia_id_hash, kKey1));
  EXPECT_FALSE(GetAccountKeyedPrefDictEntry(&pref_service, kPrefPathDict,
                                            gaia_id_hash, kKey2));
}

TEST(AccountPrefUtils, ShouldKeepGaiaIdsSeparateForDictPref) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(kPrefPathDict);

  const signin::GaiaIdHash gaia_id_hash_1 =
      signin::GaiaIdHash::FromGaiaId("gaia_id_1");
  const signin::GaiaIdHash gaia_id_hash_2 =
      signin::GaiaIdHash::FromGaiaId("gaia_id_2");

  const char kKey[] = "key";

  SetAccountKeyedPrefDictEntry(&pref_service, kPrefPathDict, gaia_id_hash_1,
                               kKey, base::Value("value_1"));
  SetAccountKeyedPrefDictEntry(&pref_service, kPrefPathDict, gaia_id_hash_2,
                               kKey, base::Value("value_2"));

  // Setting the same pref key for the second GaiaID shouldn't have affected the
  // first one's value.
  {
    const base::Value* value_1 = GetAccountKeyedPrefDictEntry(
        &pref_service, kPrefPathDict, gaia_id_hash_1, kKey);
    ASSERT_TRUE(value_1);
    EXPECT_EQ(*value_1, base::Value("value_1"));
  }
}

TEST(AccountPrefUtils, ShouldClearValuesForUnlistedAccounts) {
  TestingPrefServiceSimple pref_service;
  pref_service.registry()->RegisterDictionaryPref(kPrefPathScalar);
  pref_service.registry()->RegisterDictionaryPref(kPrefPathDict);

  const signin::GaiaIdHash gaia_id_hash_1 =
      signin::GaiaIdHash::FromGaiaId("gaia_id_1");
  const signin::GaiaIdHash gaia_id_hash_2 =
      signin::GaiaIdHash::FromGaiaId("gaia_id_2");
  const signin::GaiaIdHash gaia_id_hash_3 =
      signin::GaiaIdHash::FromGaiaId("gaia_id_3");

  const char kKey[] = "key";

  // There are values set for the first two Gaia IDs.
  SetAccountKeyedPrefValue(&pref_service, kPrefPathScalar, gaia_id_hash_1,
                           base::Value("value_1"));
  SetAccountKeyedPrefValue(&pref_service, kPrefPathScalar, gaia_id_hash_2,
                           base::Value("value_2"));
  SetAccountKeyedPrefDictEntry(&pref_service, kPrefPathDict, gaia_id_hash_1,
                               kKey, base::Value("value_3"));
  SetAccountKeyedPrefDictEntry(&pref_service, kPrefPathDict, gaia_id_hash_2,
                               kKey, base::Value("value_4"));

  // The first Gaia ID is not available anymore.
  KeepAccountKeyedPrefValuesOnlyForUsers(&pref_service, kPrefPathScalar,
                                         {gaia_id_hash_2, gaia_id_hash_3});

  // The first Gaia ID's value should've been cleared, the second one should
  // still be there, and the third was never there in the first place.
  EXPECT_FALSE(
      GetAccountKeyedPrefValue(&pref_service, kPrefPathScalar, gaia_id_hash_1));
  EXPECT_TRUE(
      GetAccountKeyedPrefValue(&pref_service, kPrefPathScalar, gaia_id_hash_2));
  EXPECT_FALSE(
      GetAccountKeyedPrefValue(&pref_service, kPrefPathScalar, gaia_id_hash_3));

  // Same for the dictionary-valued pref.
  KeepAccountKeyedPrefValuesOnlyForUsers(&pref_service, kPrefPathDict,
                                         {gaia_id_hash_2, gaia_id_hash_3});
  EXPECT_FALSE(GetAccountKeyedPrefDictEntry(&pref_service, kPrefPathDict,
                                            gaia_id_hash_1, kKey));
  EXPECT_TRUE(GetAccountKeyedPrefDictEntry(&pref_service, kPrefPathDict,
                                           gaia_id_hash_2, kKey));
  EXPECT_FALSE(GetAccountKeyedPrefDictEntry(&pref_service, kPrefPathDict,
                                            gaia_id_hash_3, kKey));
}

}  // namespace

}  // namespace syncer
