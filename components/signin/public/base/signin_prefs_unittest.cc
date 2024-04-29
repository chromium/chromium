// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_prefs.h"

#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

class SigninPrefsTest : public ::testing::Test {
 public:
  SigninPrefsTest() : signin_prefs_(pref_service_) {
    SigninPrefs::RegisterProfilePrefs(pref_service_.registry());
  }

  SigninPrefs& signin_prefs() { return signin_prefs_; }

  bool HasAccountPrefs(const std::string& gaia_id) const {
    return signin_prefs_.HasAccountPrefs(gaia_id);
  }

 private:
  TestingPrefServiceSimple pref_service_;
  SigninPrefs signin_prefs_;
};

TEST_F(SigninPrefsTest, AccountPrefsInitialization) {
  const std::string gaia_id = "gaia_id";
  ASSERT_FALSE(HasAccountPrefs(gaia_id));

  // Reading a value from a pref dict that do not exist yet should return a
  // default value and not create the pref dict entry.
  EXPECT_EQ(signin_prefs().GetDummyValue(gaia_id), 0);
  EXPECT_FALSE(HasAccountPrefs(gaia_id));

  int dummy_value = 10;
  signin_prefs().SetDummyValue(gaia_id, dummy_value);
  EXPECT_TRUE(HasAccountPrefs(gaia_id));
  EXPECT_EQ(signin_prefs().GetDummyValue(gaia_id), dummy_value);
}

TEST_F(SigninPrefsTest, RemovingAccountPrefs) {
  const std::string gaia_id1 = "gaia_id1";
  const std::string gaia_id2 = "gaia_id2";
  const std::string gaia_id3 = "gaia_id3";

  // Setting values should create the dict entry for the given gaia id.
  signin_prefs().SetDummyValue(gaia_id1, 1);
  signin_prefs().SetDummyValue(gaia_id2, 2);
  signin_prefs().SetDummyValue(gaia_id3, 3);
  ASSERT_TRUE(HasAccountPrefs(gaia_id1));
  ASSERT_TRUE(HasAccountPrefs(gaia_id2));
  ASSERT_TRUE(HasAccountPrefs(gaia_id3));

  // Should remove `gaia_id3`.
  signin_prefs().RemoveAllAccountPrefsExcept({gaia_id1, gaia_id2});
  EXPECT_TRUE(HasAccountPrefs(gaia_id1));
  EXPECT_TRUE(HasAccountPrefs(gaia_id2));
  EXPECT_FALSE(HasAccountPrefs(gaia_id3));

  // Should remove `gaia_id2`. Adding a non existing pref should have no effect
  // (`gaia_id3`).
  signin_prefs().RemoveAllAccountPrefsExcept({gaia_id1, gaia_id3});
  EXPECT_TRUE(HasAccountPrefs(gaia_id1));
  EXPECT_FALSE(HasAccountPrefs(gaia_id2));
  EXPECT_FALSE(HasAccountPrefs(gaia_id3));
}

TEST_F(SigninPrefsTest, RemovingAllAccountPrefs) {
  const std::string gaia_id1 = "gaia_id1";
  const std::string gaia_id2 = "gaia_id2";
  const std::string gaia_id3 = "gaia_id3";

  // Setting values should create the dict entry for the given gaia id.
  signin_prefs().SetDummyValue(gaia_id1, 1);
  signin_prefs().SetDummyValue(gaia_id2, 2);
  signin_prefs().SetDummyValue(gaia_id3, 3);
  ASSERT_TRUE(HasAccountPrefs(gaia_id1));
  ASSERT_TRUE(HasAccountPrefs(gaia_id2));
  ASSERT_TRUE(HasAccountPrefs(gaia_id3));

  // Passing no accounts in the arguments should clear all prefs.
  signin_prefs().RemoveAllAccountPrefsExcept({});
  EXPECT_FALSE(HasAccountPrefs(gaia_id1));
  EXPECT_FALSE(HasAccountPrefs(gaia_id2));
  EXPECT_FALSE(HasAccountPrefs(gaia_id3));
}
