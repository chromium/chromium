// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_prefs.h"

#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/testing_pref_service.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

class SigninPrefsTest : public ::testing::Test {
 public:
  SigninPrefsTest() : signin_prefs_(pref_service_) {
    SigninPrefs::RegisterProfilePrefs(pref_service_.registry());
    pref_registrar_.Init(&pref_service_);
  }

  SigninPrefs& signin_prefs() { return signin_prefs_; }

  PrefChangeRegistrar& pref_change_registrar() { return pref_registrar_; }

  bool HasAccountPrefs(const GaiaId& gaia_id) const {
    return signin_prefs_.HasAccountPrefs(gaia_id);
  }

 private:
  TestingPrefServiceSimple pref_service_;
  PrefChangeRegistrar pref_registrar_;
  SigninPrefs signin_prefs_;
};

TEST_F(SigninPrefsTest, AccountPrefsInitialization) {
  const GaiaId gaia_id("gaia_id");
  ASSERT_FALSE(HasAccountPrefs(gaia_id));

  // Reading a value from a pref dict that do not exist yet should return a
  // default value and not create the pref dict entry.
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionDismissCount(gaia_id), 0);
  EXPECT_FALSE(HasAccountPrefs(gaia_id));

  signin_prefs().IncrementChromeSigninInterceptionDismissCount(gaia_id);
  EXPECT_TRUE(HasAccountPrefs(gaia_id));
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionDismissCount(gaia_id), 1);
}

TEST_F(SigninPrefsTest, RemovingAccountPrefs) {
  const GaiaId gaia_id1("gaia_id1");
  const GaiaId gaia_id2("gaia_id2");
  const GaiaId gaia_id3("gaia_id3");

  // Setting any value should create the dict entry for the given gaia id.
  signin_prefs().IncrementChromeSigninInterceptionDismissCount(gaia_id1);
  signin_prefs().IncrementChromeSigninInterceptionDismissCount(gaia_id2);
  signin_prefs().IncrementChromeSigninInterceptionDismissCount(gaia_id3);
  ASSERT_TRUE(HasAccountPrefs(gaia_id1));
  ASSERT_TRUE(HasAccountPrefs(gaia_id2));
  ASSERT_TRUE(HasAccountPrefs(gaia_id3));

  // Should remove `gaia_id3`.
  EXPECT_EQ(signin_prefs().RemoveAllAccountPrefsExcept({gaia_id1, gaia_id2}),
            1u);
  EXPECT_TRUE(HasAccountPrefs(gaia_id1));
  EXPECT_TRUE(HasAccountPrefs(gaia_id2));
  EXPECT_FALSE(HasAccountPrefs(gaia_id3));

  // Should remove `gaia_id2`. Adding a non existing pref should have no effect
  // (`gaia_id3`).
  EXPECT_EQ(signin_prefs().RemoveAllAccountPrefsExcept({gaia_id1, gaia_id3}),
            1u);
  EXPECT_TRUE(HasAccountPrefs(gaia_id1));
  EXPECT_FALSE(HasAccountPrefs(gaia_id2));
  EXPECT_FALSE(HasAccountPrefs(gaia_id3));
}

TEST_F(SigninPrefsTest, RemovingAllAccountPrefs) {
  const GaiaId gaia_id1("gaia_id1");
  const GaiaId gaia_id2("gaia_id2");
  const GaiaId gaia_id3("gaia_id3");

  // Setting any value should create the dict entry for the given gaia id.
  signin_prefs().IncrementChromeSigninInterceptionDismissCount(gaia_id1);
  signin_prefs().IncrementChromeSigninInterceptionDismissCount(gaia_id2);
  signin_prefs().IncrementChromeSigninInterceptionDismissCount(gaia_id3);
  ASSERT_TRUE(HasAccountPrefs(gaia_id1));
  ASSERT_TRUE(HasAccountPrefs(gaia_id2));
  ASSERT_TRUE(HasAccountPrefs(gaia_id3));

  // Passing no accounts in the arguments should clear all prefs.
  EXPECT_EQ(signin_prefs().RemoveAllAccountPrefsExcept({}), 3u);
  EXPECT_FALSE(HasAccountPrefs(gaia_id1));
  EXPECT_FALSE(HasAccountPrefs(gaia_id2));
  EXPECT_FALSE(HasAccountPrefs(gaia_id3));
}

TEST_F(SigninPrefsTest, ObservingSigninPrefChanges) {
  const GaiaId gaia_id1("gaia_id1");

  base::MockCallback<base::RepeatingClosure> mock_callback;
  signin_prefs().ObserveSigninPrefsChanges(pref_change_registrar(),
                                           mock_callback.Get());

  ASSERT_FALSE(HasAccountPrefs(gaia_id1));
  EXPECT_CALL(mock_callback, Run()).Times(1);
  signin_prefs().SetChromeSigninInterceptionUserChoice(
      gaia_id1, ChromeSigninUserChoice::kSignin);
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Creating a new pref in an existing dictionary should update.
  ASSERT_TRUE(HasAccountPrefs(gaia_id1));
  EXPECT_CALL(mock_callback, Run()).Times(1);
  signin_prefs().IncrementChromeSigninInterceptionDismissCount(gaia_id1);
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Doing any pref change should call an update, even on a different id.
  const GaiaId gaia_id2("gaia_id2");
  EXPECT_CALL(mock_callback, Run()).Times(1);
  signin_prefs().IncrementChromeSigninInterceptionDismissCount(gaia_id2);
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Changing an existing pref should update.
  EXPECT_CALL(mock_callback, Run()).Times(1);
  ChromeSigninUserChoice current_value =
      signin_prefs().GetChromeSigninInterceptionUserChoice(gaia_id1);
  ChromeSigninUserChoice new_value = ChromeSigninUserChoice::kDoNotSignin;
  ASSERT_NE(current_value, new_value);
  signin_prefs().SetChromeSigninInterceptionUserChoice(gaia_id1, new_value);
  testing::Mock::VerifyAndClearExpectations(&mock_callback);

  // Re-setting the same value should not notify.
  EXPECT_CALL(mock_callback, Run()).Times(0);
  ASSERT_EQ(new_value,
            signin_prefs().GetChromeSigninInterceptionUserChoice(gaia_id1));
  signin_prefs().SetChromeSigninInterceptionUserChoice(gaia_id1, new_value);
}

TEST_F(SigninPrefsTest, ChromeSigninInterceptionDismissCount) {
  const GaiaId gaia_id("gaia_id");

  ASSERT_FALSE(HasAccountPrefs(gaia_id));
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionDismissCount(gaia_id), 0);

  signin_prefs().IncrementChromeSigninInterceptionDismissCount(gaia_id);
  EXPECT_TRUE(HasAccountPrefs(gaia_id));
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionDismissCount(gaia_id), 1);

  // Creating the main dict through setting a different pref should still return
  // the default value - 0.
  const GaiaId gaia_id2("gaia_id2");
  signin_prefs().SetChromeSigninInterceptionUserChoice(
      gaia_id2, ChromeSigninUserChoice::kSignin);
  ASSERT_TRUE(HasAccountPrefs(gaia_id2));
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionDismissCount(gaia_id2),
            0);
}

TEST_F(SigninPrefsTest, ChromeSigninInterceptionUserChoice) {
  const GaiaId gaia_id("gaia_id");

  ASSERT_FALSE(HasAccountPrefs(gaia_id));
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionUserChoice(gaia_id),
            ChromeSigninUserChoice::kNoChoice);

  ChromeSigninUserChoice new_value = ChromeSigninUserChoice::kDoNotSignin;
  signin_prefs().SetChromeSigninInterceptionUserChoice(gaia_id, new_value);
  EXPECT_TRUE(HasAccountPrefs(gaia_id));
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionUserChoice(gaia_id),
            new_value);

  // Creating the main dict through setting a different pref should still return
  // the default value - ChromeSigninUserChoice::kNoChoice.
  const GaiaId gaia_id2("gaia_id2");
  signin_prefs().IncrementChromeSigninInterceptionDismissCount(gaia_id2);
  ASSERT_TRUE(HasAccountPrefs(gaia_id2));
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionUserChoice(gaia_id2),
            ChromeSigninUserChoice::kNoChoice);
}

TEST_F(SigninPrefsTest, ChromeSigninInterceptionLastBubbleDeclineTime) {
  const GaiaId gaia_id("gaia_id");

  ASSERT_FALSE(HasAccountPrefs(gaia_id));
  EXPECT_FALSE(signin_prefs()
                   .GetChromeSigninInterceptionLastBubbleDeclineTime(gaia_id)
                   .has_value());

  base::Time last_reprompt_time = base::Time::Now();
  signin_prefs().SetChromeSigninInterceptionLastBubbleDeclineTime(
      gaia_id, last_reprompt_time);

  EXPECT_TRUE(HasAccountPrefs(gaia_id));
  EXPECT_EQ(
      signin_prefs().GetChromeSigninInterceptionLastBubbleDeclineTime(gaia_id),
      last_reprompt_time);

  signin_prefs().ClearChromeSigninInterceptionLastBubbleDeclineTime(gaia_id);
  EXPECT_FALSE(signin_prefs()
                   .GetChromeSigninInterceptionLastBubbleDeclineTime(gaia_id)
                   .has_value());
  EXPECT_TRUE(HasAccountPrefs(gaia_id));

  // Creating the main dict through setting a different pref should still return
  // the default value - no time.
  const GaiaId gaia_id2("gaia_id2");
  signin_prefs().SetChromeSigninInterceptionUserChoice(
      gaia_id2, ChromeSigninUserChoice::kSignin);
  ASSERT_TRUE(HasAccountPrefs(gaia_id2));
  EXPECT_FALSE(signin_prefs()
                   .GetChromeSigninInterceptionLastBubbleDeclineTime(gaia_id2)
                   .has_value());
}

TEST_F(SigninPrefsTest, ChromeSigninInterceptionRepromptCount) {
  const GaiaId gaia_id("gaia_id");

  ASSERT_FALSE(HasAccountPrefs(gaia_id));
  EXPECT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(gaia_id), 0);

  signin_prefs().IncrementChromeSigninBubbleRepromptCount(gaia_id);
  EXPECT_TRUE(HasAccountPrefs(gaia_id));
  EXPECT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(gaia_id), 1);

  signin_prefs().ClearChromeSigninBubbleRepromptCount(gaia_id);
  EXPECT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(gaia_id), 0);

  // Creating the main dict through setting a different pref should still return
  // the default value - 0.
  const GaiaId gaia_id2("gaia_id2");
  signin_prefs().SetChromeSigninInterceptionUserChoice(
      gaia_id2, ChromeSigninUserChoice::kSignin);
  ASSERT_TRUE(HasAccountPrefs(gaia_id2));
  EXPECT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(gaia_id2), 0);
}

TEST_F(SigninPrefsTest, SyncPromoIdentityPillShownCount) {
  const GaiaId gaia_id_1("gaia_id_1");
  EXPECT_EQ(signin_prefs().GetSyncPromoIdentityPillShownCount(gaia_id_1), 0);
  signin_prefs().IncrementSyncPromoIdentityPillShownCount(gaia_id_1);
  EXPECT_EQ(signin_prefs().GetSyncPromoIdentityPillShownCount(gaia_id_1), 1);
  signin_prefs().IncrementSyncPromoIdentityPillShownCount(gaia_id_1);
  EXPECT_EQ(signin_prefs().GetSyncPromoIdentityPillShownCount(gaia_id_1), 2);

  const GaiaId gaia_id_2("gaia_id_2");
  EXPECT_EQ(signin_prefs().GetSyncPromoIdentityPillShownCount(gaia_id_2), 0);
  signin_prefs().IncrementSyncPromoIdentityPillShownCount(gaia_id_2);
  EXPECT_EQ(signin_prefs().GetSyncPromoIdentityPillShownCount(gaia_id_2), 1);
  signin_prefs().IncrementSyncPromoIdentityPillShownCount(gaia_id_2);
  EXPECT_EQ(signin_prefs().GetSyncPromoIdentityPillShownCount(gaia_id_2), 2);
}

TEST_F(SigninPrefsTest, SyncPromoIdentityPillUsedCount) {
  const GaiaId gaia_id_1("gaia_id_1");
  EXPECT_EQ(signin_prefs().GetSyncPromoIdentityPillUsedCount(gaia_id_1), 0);
  signin_prefs().IncrementSyncPromoIdentityPillUsedCount(gaia_id_1);
  EXPECT_EQ(signin_prefs().GetSyncPromoIdentityPillUsedCount(gaia_id_1), 1);
  signin_prefs().IncrementSyncPromoIdentityPillUsedCount(gaia_id_1);
  EXPECT_EQ(signin_prefs().GetSyncPromoIdentityPillUsedCount(gaia_id_1), 2);

  const GaiaId gaia_id_2("gaia_id_2");
  EXPECT_EQ(signin_prefs().GetSyncPromoIdentityPillUsedCount(gaia_id_2), 0);
  signin_prefs().IncrementSyncPromoIdentityPillUsedCount(gaia_id_2);
  EXPECT_EQ(signin_prefs().GetSyncPromoIdentityPillUsedCount(gaia_id_2), 1);
  signin_prefs().IncrementSyncPromoIdentityPillUsedCount(gaia_id_2);
  EXPECT_EQ(signin_prefs().GetSyncPromoIdentityPillUsedCount(gaia_id_2), 2);
}

TEST_F(SigninPrefsTest, BookmarkBatchUploadPromo) {
  const GaiaId gaia_id_1("gaia_id_1");
  auto initial_bookmark_batch_upload_info1 =
      signin_prefs().GetBookmarkBatchUploadPromoDismissCountWithLastTime(
          gaia_id_1);
  EXPECT_EQ(initial_bookmark_batch_upload_info1.first, 0);
  EXPECT_FALSE(initial_bookmark_batch_upload_info1.second.has_value());

  base::Time reference = base::Time::Now();
  signin_prefs().IncrementBookmarkBatchUploadPromoDismissCountWithLastTime(
      gaia_id_1);
  auto bookmark_batch_upload_info =
      signin_prefs().GetBookmarkBatchUploadPromoDismissCountWithLastTime(
          gaia_id_1);
  EXPECT_EQ(bookmark_batch_upload_info.first, 1);
  ASSERT_TRUE(bookmark_batch_upload_info.second.has_value());
  EXPECT_GT(bookmark_batch_upload_info.second.value(), reference);

  const GaiaId gaia_id_2("gaia_id_2");
  auto initial_bookmark_batch_upload_info2 =
      signin_prefs().GetBookmarkBatchUploadPromoDismissCountWithLastTime(
          gaia_id_2);
  EXPECT_EQ(initial_bookmark_batch_upload_info2.first, 0);
  EXPECT_FALSE(initial_bookmark_batch_upload_info2.second.has_value());

  base::Time reference2 = base::Time::Now();
  signin_prefs().IncrementBookmarkBatchUploadPromoDismissCountWithLastTime(
      gaia_id_2);
  auto bookmark_batch_upload_info2 =
      signin_prefs().GetBookmarkBatchUploadPromoDismissCountWithLastTime(
          gaia_id_2);
  EXPECT_EQ(bookmark_batch_upload_info2.first, 1);
  ASSERT_TRUE(bookmark_batch_upload_info2.second.has_value());
  EXPECT_GT(bookmark_batch_upload_info2.second.value(), reference2);
}
