// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/signin/public/base/signin_prefs.h"

#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/prefs/testing_pref_service.h"
#include "components/signin/public/base/signin_prefs_accessor.h"
#include "components/signin/public/base/signin_prefs_keys.h"
#include "components/signin/public/base/signin_prefs_registry.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kSigninAccountPrefs[] = "signin.accounts_metadata_dict";

}  // namespace

class SigninPrefsTest : public ::testing::Test {
 public:
  SigninPrefsTest() : signin_prefs_(pref_service_) {
    SigninPrefs::RegisterProfilePrefs(pref_service_.registry());
    pref_registrar_.Init(&pref_service_);
  }

  SigninPrefs& signin_prefs() { return signin_prefs_; }

  PrefChangeRegistrar& pref_change_registrar() { return pref_registrar_; }

  TestingPrefServiceSimple& pref_service() { return pref_service_; }

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

TEST_F(SigninPrefsTest, BatchUploadLastUploadRemainingLocalDataCount) {
  const GaiaId gaia_id_1("gaia_id_1");
  const GaiaId gaia_id_2("gaia_id_2");

  EXPECT_EQ(
      signin_prefs().GetBatchUploadLastUploadRemainingLocalDataCount(gaia_id_1),
      std::nullopt);
  EXPECT_EQ(
      signin_prefs().GetBatchUploadLastUploadRemainingLocalDataCount(gaia_id_2),
      std::nullopt);

  signin_prefs().SetBatchUploadLastUploadRemainingLocalDataCount(gaia_id_1, 3);
  EXPECT_EQ(
      signin_prefs().GetBatchUploadLastUploadRemainingLocalDataCount(gaia_id_1),
      3);
  EXPECT_EQ(
      signin_prefs().GetBatchUploadLastUploadRemainingLocalDataCount(gaia_id_2),
      std::nullopt);

  signin_prefs().SetBatchUploadLastUploadRemainingLocalDataCount(gaia_id_1, 0);
  EXPECT_EQ(
      signin_prefs().GetBatchUploadLastUploadRemainingLocalDataCount(gaia_id_1),
      0);
}

TEST_F(SigninPrefsTest, DeprecatingPrefsInAccountDict) {
  const GaiaId gaia_id("gaia_id_1");

  // Increment a random valid pref.
  signin_prefs().IncrementChromeSigninBubbleRepromptCount(gaia_id);
  ASSERT_TRUE(HasAccountPrefs(gaia_id));
  ASSERT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(gaia_id), 1);

  ASSERT_FALSE(signin_prefs().GetDeprecatedPrefForTesting(gaia_id).has_value());

  signin_prefs().SetDeprecatedPrefForTesting(gaia_id);
  ASSERT_TRUE(signin_prefs().GetDeprecatedPrefForTesting(gaia_id).has_value());

  // This should clear the deprecated prefs and keep the valid pref.
  signin_prefs().MigrateObsoleteSigninPrefs();
  EXPECT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(gaia_id), 1);
  EXPECT_FALSE(signin_prefs().GetDeprecatedPrefForTesting(gaia_id).has_value());
}

TEST_F(SigninPrefsTest, BookmarkManagerSigninPromoCounts) {
  const GaiaId gaia_id_1("gaia_id_1");
  const GaiaId gaia_id_2("gaia_id_2");

  EXPECT_EQ(
      signin_prefs().GetBookmarkManagerSigninPromoImpressionCount(gaia_id_1),
      0);
  EXPECT_EQ(signin_prefs().GetBookmarkManagerSigninPromoDismissCount(gaia_id_1),
            0);

  signin_prefs().IncrementBookmarkManagerSigninPromoImpressionCount(gaia_id_1);
  signin_prefs().IncrementBookmarkManagerSigninPromoImpressionCount(gaia_id_1);
  signin_prefs().IncrementBookmarkManagerSigninPromoDismissCount(gaia_id_1);

  EXPECT_EQ(
      signin_prefs().GetBookmarkManagerSigninPromoImpressionCount(gaia_id_1),
      2);
  EXPECT_EQ(signin_prefs().GetBookmarkManagerSigninPromoDismissCount(gaia_id_1),
            1);
  EXPECT_EQ(
      signin_prefs().GetBookmarkManagerSigninPromoImpressionCount(gaia_id_2),
      0);
  EXPECT_EQ(signin_prefs().GetBookmarkManagerSigninPromoDismissCount(gaia_id_2),
            0);
}

TEST_F(SigninPrefsTest, AvatarButtonPromoCounts) {
  const GaiaId gaia_id("gaia_id_1");

  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            0);
  signin_prefs().IncrementAvatarButtonHistorySyncPromoShownCount(gaia_id);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            1);

  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoUsedCount(gaia_id),
            0);
  signin_prefs().IncrementAvatarButtonHistorySyncPromoUsedCount(gaia_id);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoUsedCount(gaia_id),
            1);

  EXPECT_EQ(signin_prefs().GetAvatarButtonBatchUploadPromoShownCount(gaia_id),
            0);
  signin_prefs().IncrementAvatarButtonBatchUploadPromoShownCount(gaia_id);
  EXPECT_EQ(signin_prefs().GetAvatarButtonBatchUploadPromoShownCount(gaia_id),
            1);

  EXPECT_EQ(signin_prefs().GetAvatarButtonBatchUploadPromoUsedCount(gaia_id),
            0);
  signin_prefs().IncrementAvatarButtonBatchUploadPromoUsedCount(gaia_id);
  EXPECT_EQ(signin_prefs().GetAvatarButtonBatchUploadPromoUsedCount(gaia_id),
            1);

  EXPECT_EQ(
      signin_prefs().GetAvatarButtonBatchUploadBookmarkPromoShownCount(gaia_id),
      0);
  signin_prefs().IncrementAvatarButtonBatchUploadBookmarkPromoShownCount(
      gaia_id);
  EXPECT_EQ(
      signin_prefs().GetAvatarButtonBatchUploadBookmarkPromoShownCount(gaia_id),
      1);

  EXPECT_EQ(
      signin_prefs().GetAvatarButtonBatchUploadBookmarkPromoUsedCount(gaia_id),
      0);
  signin_prefs().IncrementAvatarButtonBatchUploadBookmarkPromoUsedCount(
      gaia_id);
  EXPECT_EQ(
      signin_prefs().GetAvatarButtonBatchUploadBookmarkPromoUsedCount(gaia_id),
      1);

  EXPECT_EQ(signin_prefs()
                .GetAvatarButtonBatchUploadWindows10DepreciationPromoShownCount(
                    gaia_id),
            0);
  signin_prefs()
      .IncrementAvatarButtonBatchUploadWindows10DepreciationPromoShownCount(
          gaia_id);
  EXPECT_EQ(signin_prefs()
                .GetAvatarButtonBatchUploadWindows10DepreciationPromoShownCount(
                    gaia_id),
            1);

  EXPECT_EQ(signin_prefs()
                .GetAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount(
                    gaia_id),
            0);
  signin_prefs()
      .IncrementAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount(
          gaia_id);
  EXPECT_EQ(signin_prefs()
                .GetAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount(
                    gaia_id),
            1);

  EXPECT_EQ(signin_prefs().GetAvatarButtonSigninPromoShownCount(gaia_id), 0);
  EXPECT_FALSE(signin_prefs()
                   .GetAvatarButtonSigninPromoLastShownTime(gaia_id)
                   .has_value());
  const base::Time now = base::Time::Now();
  signin_prefs().IncrementAvatarButtonSigninPromoShownCount(gaia_id);
  signin_prefs().SetAvatarButtonSigninPromoLastShownTime(gaia_id, now);
  EXPECT_EQ(signin_prefs().GetAvatarButtonSigninPromoShownCount(gaia_id), 1);
  EXPECT_EQ(signin_prefs().GetAvatarButtonSigninPromoLastShownTime(gaia_id),
            now);

  EXPECT_EQ(signin_prefs().GetAvatarButtonSigninPromoUsedCount(gaia_id), 0);
  signin_prefs().IncrementAvatarButtonSigninPromoUsedCount(gaia_id);
  EXPECT_EQ(signin_prefs().GetAvatarButtonSigninPromoUsedCount(gaia_id), 1);
}

TEST_F(SigninPrefsTest, CrossDeviceHistoryPromo) {
  const GaiaId gaia_id("gaia_id_1");

  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoShownCount(gaia_id), 0);
  EXPECT_FALSE(
      signin_prefs().GetCrossDeviceHistoryPromoShownAfterDismissal(gaia_id));
  EXPECT_FALSE(signin_prefs()
                   .GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id)
                   .has_value());

  const base::Time now = base::Time::Now();
  signin_prefs().SetCrossDeviceHistoryPromoShownCount(gaia_id, 3);
  signin_prefs().SetCrossDeviceHistoryPromoShownAfterDismissal(gaia_id, true);
  signin_prefs().SetCrossDeviceHistoryPromoLastDismissedTime(gaia_id, now);

  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoShownCount(gaia_id), 3);
  EXPECT_TRUE(
      signin_prefs().GetCrossDeviceHistoryPromoShownAfterDismissal(gaia_id));
  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id),
            now);

  signin_prefs().ClearCrossDeviceHistoryPromoLastDismissedTime(gaia_id);
  EXPECT_FALSE(signin_prefs()
                   .GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id)
                   .has_value());
}

TEST_F(SigninPrefsTest, PromoPrefsUseExpectedOnDiskPaths) {
  const GaiaId gaia_id("gaia_id_golden");
  const base::Time test_time =
      base::Time::FromSecondsSinceUnixEpoch(1700000000);

  // Seed the pref store directly using the exact string keys and hierarchy
  // expected on disk, matching the serialized JSON format.
  base::DictValue avatar_dict;
  avatar_dict.Set("AvatarButtonHistorySyncPromoShownCount", 2);
  avatar_dict.Set("AvatarButtonHistorySyncPromoUsedCount", 1);
  avatar_dict.Set("AvatarButtonBatchUploadPromoShownCount", 4);
  avatar_dict.Set("AvatarButtonBatchUploadPromoUsedCount", 3);
  avatar_dict.Set("AvatarButtonBatchUploadBookmarkPromoShownCount", 6);
  avatar_dict.Set("AvatarButtonBatchUploadBookmarkPromoUsedCount", 5);
  avatar_dict.Set("AvatarButtonBatchUploadWindows10DepreciationPromoShownCount",
                  8);
  avatar_dict.Set("AvatarButtonBatchUploadWindows10DepreciationPromoUsedCount",
                  7);
  avatar_dict.Set("AvatarButtonSigninPromoShownCount", 10);
  avatar_dict.Set("AvatarButtonSigninPromoUsedCount", 9);
  avatar_dict.Set("AvatarButtonSigninPromoLastShownTime",
                  base::TimeToValue(test_time));

  base::DictValue cross_device_history_dict;
  cross_device_history_dict.Set("shown_count", 11);
  cross_device_history_dict.Set("shown_after_dismissal", true);
  cross_device_history_dict.Set("last_dismissed_time",
                                base::TimeToValue(test_time));
  cross_device_history_dict.Set("unknown_future_sibling_key", "preserved");

  base::DictValue cross_device_dict;
  cross_device_dict.Set("history", std::move(cross_device_history_dict));

  base::DictValue account_dict;
  account_dict.Set("AvatarButtonPromoCountDictionary", std::move(avatar_dict));
  account_dict.Set("CrossDevicePromoPrefs", std::move(cross_device_dict));
  base::DictValue expected_account = account_dict.Clone();

  {
    ScopedDictPrefUpdate update(&pref_service(), kSigninAccountPrefs);
    update->Set(gaia_id.ToString(), std::move(account_dict));
  }

  // Verify that typed accessors read the seeded values from the exact expected
  // paths.
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            2);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoUsedCount(gaia_id),
            1);
  EXPECT_EQ(signin_prefs().GetAvatarButtonBatchUploadPromoShownCount(gaia_id),
            4);
  EXPECT_EQ(signin_prefs().GetAvatarButtonBatchUploadPromoUsedCount(gaia_id),
            3);
  EXPECT_EQ(
      signin_prefs().GetAvatarButtonBatchUploadBookmarkPromoShownCount(gaia_id),
      6);
  EXPECT_EQ(
      signin_prefs().GetAvatarButtonBatchUploadBookmarkPromoUsedCount(gaia_id),
      5);
  EXPECT_EQ(signin_prefs()
                .GetAvatarButtonBatchUploadWindows10DepreciationPromoShownCount(
                    gaia_id),
            8);
  EXPECT_EQ(signin_prefs()
                .GetAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount(
                    gaia_id),
            7);
  EXPECT_EQ(signin_prefs().GetAvatarButtonSigninPromoShownCount(gaia_id), 10);
  EXPECT_EQ(signin_prefs().GetAvatarButtonSigninPromoUsedCount(gaia_id), 9);
  EXPECT_EQ(signin_prefs().GetAvatarButtonSigninPromoLastShownTime(gaia_id),
            test_time);

  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoShownCount(gaia_id), 11);
  EXPECT_TRUE(
      signin_prefs().GetCrossDeviceHistoryPromoShownAfterDismissal(gaia_id));
  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id),
            test_time);

  // Now verify the reverse: mutating through typed accessors updates only the
  // targeted leaf keys at the exact expected paths while preserving all sibling
  // keys (including unknown keys under `history`).
  signin_prefs().IncrementAvatarButtonHistorySyncPromoShownCount(gaia_id);
  signin_prefs().SetCrossDeviceHistoryPromoShownCount(gaia_id, 12);
  signin_prefs().ClearCrossDeviceHistoryPromoLastDismissedTime(gaia_id);

  expected_account.FindDict("AvatarButtonPromoCountDictionary")
      ->Set("AvatarButtonHistorySyncPromoShownCount", 3);
  base::DictValue* expected_history =
      expected_account.FindDict("CrossDevicePromoPrefs")->FindDict("history");
  expected_history->Set("shown_count", 12);
  expected_history->Remove("last_dismissed_time");

  const base::DictValue* raw_account =
      pref_service().GetDict(kSigninAccountPrefs).FindDict(gaia_id.ToString());
  ASSERT_TRUE(raw_account);
  EXPECT_EQ(*raw_account, expected_account);
}

TEST_F(SigninPrefsTest, PromoPrefReadsHaveNoSideEffects) {
  const GaiaId gaia_id("gaia_id_read_only");
  base::MockCallback<base::RepeatingClosure> mock_observer;
  SigninPrefs::ObserveSigninPrefsChanges(pref_change_registrar(),
                                         mock_observer.Get());

  // Reading or clearing nested promo prefs for a non-existent account must not
  // create an account dictionary entry or notify pref observers.
  EXPECT_CALL(mock_observer, Run()).Times(0);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            0);
  EXPECT_FALSE(signin_prefs()
                   .GetAvatarButtonSigninPromoLastShownTime(gaia_id)
                   .has_value());
  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoShownCount(gaia_id), 0);
  EXPECT_FALSE(
      signin_prefs().GetCrossDeviceHistoryPromoShownAfterDismissal(gaia_id));
  EXPECT_FALSE(signin_prefs()
                   .GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id)
                   .has_value());
  signin_prefs().ClearCrossDeviceHistoryPromoLastDismissedTime(gaia_id);
  EXPECT_FALSE(HasAccountPrefs(gaia_id));
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Each write operation notifies pref observers once and creates the account
  // entry.
  EXPECT_CALL(mock_observer, Run()).Times(1);
  signin_prefs().IncrementAvatarButtonHistorySyncPromoShownCount(gaia_id);
  EXPECT_TRUE(HasAccountPrefs(gaia_id));
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Subsequent reads or no-op clears on an existing account still do not
  // notify.
  EXPECT_CALL(mock_observer, Run()).Times(0);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            1);
  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoShownCount(gaia_id), 0);
  signin_prefs().ClearCrossDeviceHistoryPromoLastDismissedTime(gaia_id);
}

TEST_F(SigninPrefsTest, PromoCountIncrementSaturatesAtIntMax) {
  const GaiaId gaia_id("gaia_id_saturation");
  {
    ScopedDictPrefUpdate update(&pref_service(), kSigninAccountPrefs);
    update->EnsureDict(gaia_id.ToString())
        ->EnsureDict("AvatarButtonPromoCountDictionary")
        ->Set("AvatarButtonHistorySyncPromoShownCount",
              std::numeric_limits<int>::max());
  }

  signin_prefs().IncrementAvatarButtonHistorySyncPromoShownCount(gaia_id);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            std::numeric_limits<int>::max());
}

// `ScopedDictPrefUpdate` only mutates the user pref store, so a value that is
// only present in another store (here: recommended) cannot be cleared and must
// be left untouched without crashing.
TEST_F(SigninPrefsTest, ClearPrefIgnoresValuesOutsideUserStore) {
  const GaiaId gaia_id("gaia_id_recommended");
  const base::Time time = base::Time::FromSecondsSinceUnixEpoch(1700000000);
  base::DictValue accounts;
  accounts.EnsureDict(gaia_id.ToString())
      ->EnsureDict("CrossDevicePromoPrefs")
      ->EnsureDict("history")
      ->Set("last_dismissed_time", base::TimeToValue(time));
  pref_service().SetRecommendedPref(kSigninAccountPrefs,
                                    base::Value(std::move(accounts)));
  ASSERT_EQ(signin_prefs().GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id),
            time);

  signin_prefs().ClearCrossDeviceHistoryPromoLastDismissedTime(gaia_id);

  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id),
            time);
  EXPECT_FALSE(pref_service().GetUserPrefValue(kSigninAccountPrefs));
}

// Corrupted on-disk data: the accounts pref itself is not a dict. Clearing must
// be a no-op and must not assert (`PrefService::GetUserPrefValue()` would).
TEST_F(SigninPrefsTest, ClearPrefIgnoresNonDictAccountsPref) {
  const GaiaId gaia_id("gaia_id_corrupted");
  pref_service().SetUserPref(kSigninAccountPrefs, base::Value(42));

  signin_prefs().ClearCrossDeviceHistoryPromoLastDismissedTime(gaia_id);
  signin_prefs().ClearChromeSigninBubbleRepromptCount(gaia_id);

  EXPECT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(gaia_id), 0);
}

TEST_F(SigninPrefsTest, PromoPrefsMalformedIntermediateDicts) {
  const GaiaId gaia_id("gaia_id_malformed");

  // Seed non-dict values where intermediate sub-dictionaries are expected.
  {
    ScopedDictPrefUpdate update(&pref_service(), kSigninAccountPrefs);
    base::DictValue* account_dict = update->EnsureDict(gaia_id.ToString());
    account_dict->Set("AvatarButtonPromoCountDictionary", "not_a_dict");
    account_dict->Set("CrossDevicePromoPrefs", 42);
  }

  // Reads and clears should return default values / no-op without crashing.
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            0);
  EXPECT_FALSE(signin_prefs()
                   .GetAvatarButtonSigninPromoLastShownTime(gaia_id)
                   .has_value());
  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoShownCount(gaia_id), 0);
  EXPECT_FALSE(
      signin_prefs().GetCrossDeviceHistoryPromoShownAfterDismissal(gaia_id));
  EXPECT_FALSE(signin_prefs()
                   .GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id)
                   .has_value());
  signin_prefs().ClearCrossDeviceHistoryPromoLastDismissedTime(gaia_id);

  // Also test depth-2 malformed intermediate: "CrossDevicePromoPrefs" is a
  // valid dict, but "history" is a non-dict.
  {
    ScopedDictPrefUpdate update(&pref_service(), kSigninAccountPrefs);
    base::DictValue* account_dict = update->EnsureDict(gaia_id.ToString());
    base::DictValue* cross_device_dict =
        account_dict->EnsureDict("CrossDevicePromoPrefs");
    cross_device_dict->Set("history", "not_a_dict");
  }
  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoShownCount(gaia_id), 0);
  EXPECT_FALSE(
      signin_prefs().GetCrossDeviceHistoryPromoShownAfterDismissal(gaia_id));
  EXPECT_FALSE(signin_prefs()
                   .GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id)
                   .has_value());
  signin_prefs().ClearCrossDeviceHistoryPromoLastDismissedTime(gaia_id);

  // Writes should overwrite malformed intermediates with dictionaries.
  signin_prefs().IncrementAvatarButtonHistorySyncPromoShownCount(gaia_id);
  signin_prefs().SetCrossDeviceHistoryPromoShownCount(gaia_id, 5);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            1);
  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoShownCount(gaia_id), 5);
}

// Corrupted on-disk data, level 1: the accounts pref itself is not a dict.
// Reads must return default values, clears must be a no-op, and the first write
// must replace the corrupted value with a proper dict holding only the written
// account.
TEST_F(SigninPrefsTest,
       CorruptedAccountsPrefIsIgnoredOnReadAndOverwrittenOnWrite) {
  const GaiaId gaia_id("gaia_id_corrupted");
  const base::Time time = base::Time::FromSecondsSinceUnixEpoch(1700000000);
  pref_service().SetUserPref(kSigninAccountPrefs, base::Value(42));
  ASSERT_EQ(*pref_service().GetUserPref(kSigninAccountPrefs), base::Value(42));

  // Reads are ignored: every value kind falls back to its default.
  EXPECT_FALSE(HasAccountPrefs(gaia_id));
  EXPECT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(gaia_id), 0);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            0);
  EXPECT_FALSE(signin_prefs().GetExtensionsExplicitBrowserSignin(gaia_id));
  EXPECT_FALSE(
      signin_prefs().GetCrossDeviceHistoryPromoShownAfterDismissal(gaia_id));
  EXPECT_FALSE(signin_prefs()
                   .GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id)
                   .has_value());
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionUserChoice(gaia_id),
            ChromeSigninUserChoice::kNoChoice);

  // Clears are a no-op and leave the corrupted value untouched.
  signin_prefs().ClearChromeSigninBubbleRepromptCount(gaia_id);
  signin_prefs().ClearCrossDeviceHistoryPromoLastDismissedTime(gaia_id);
  EXPECT_EQ(*pref_service().GetUserPref(kSigninAccountPrefs), base::Value(42));

  // A top-level write replaces the corrupted value with a dict that contains
  // only the written account and key.
  EXPECT_EQ(signin_prefs().IncrementChromeSigninBubbleRepromptCount(gaia_id),
            1);
  EXPECT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(gaia_id), 1);
  EXPECT_TRUE(HasAccountPrefs(gaia_id));
  base::DictValue expected_accounts;
  base::DictValue* expected_account =
      expected_accounts.EnsureDict(gaia_id.ToString());
  expected_account->Set("ChromeSigninInterceptionRepromptCount", 1);
  const base::Value* raw_accounts =
      pref_service().GetUserPref(kSigninAccountPrefs);
  ASSERT_TRUE(raw_accounts);
  ASSERT_TRUE(raw_accounts->is_dict());
  EXPECT_EQ(raw_accounts->GetDict(), expected_accounts);

  // Nested writes then build on the now valid dict.
  signin_prefs().IncrementAvatarButtonHistorySyncPromoShownCount(gaia_id);
  signin_prefs().SetCrossDeviceHistoryPromoLastDismissedTime(gaia_id, time);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            1);
  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id),
            time);
  expected_account->EnsureDict("AvatarButtonPromoCountDictionary")
      ->Set("AvatarButtonHistorySyncPromoShownCount", 1);
  expected_account->EnsureDict("CrossDevicePromoPrefs")
      ->EnsureDict("history")
      ->Set("last_dismissed_time", base::TimeToValue(time));
  EXPECT_EQ(pref_service().GetDict(kSigninAccountPrefs), expected_accounts);
}

// Corrupted on-disk data, level 2: an account entry is not a dict. Reads must
// return default values, clears must be a no-op, and the first write must
// replace the corrupted entry with a dict holding only the written key, without
// affecting other (valid) accounts.
TEST_F(SigninPrefsTest,
       CorruptedAccountEntryIsIgnoredOnReadAndOverwrittenOnWrite) {
  const GaiaId gaia_id("gaia_id_corrupted");
  const GaiaId valid_gaia_id("gaia_id_valid");
  const base::Time time = base::Time::FromSecondsSinceUnixEpoch(1700000000);
  base::DictValue expected_accounts;
  expected_accounts.Set(gaia_id.ToString(), "not_a_dict");
  expected_accounts.EnsureDict(valid_gaia_id.ToString())
      ->Set("ChromeSigninInterceptionRepromptCount", 7);
  {
    ScopedDictPrefUpdate update(&pref_service(), kSigninAccountPrefs);
    *update = expected_accounts.Clone();
  }
  ASSERT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(valid_gaia_id),
            7);

  // Reads are ignored: every value kind falls back to its default.
  EXPECT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(gaia_id), 0);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            0);
  EXPECT_FALSE(signin_prefs().GetExtensionsExplicitBrowserSignin(gaia_id));
  EXPECT_FALSE(
      signin_prefs().GetCrossDeviceHistoryPromoShownAfterDismissal(gaia_id));
  EXPECT_FALSE(signin_prefs()
                   .GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id)
                   .has_value());
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionUserChoice(gaia_id),
            ChromeSigninUserChoice::kNoChoice);

  // Clears are a no-op and leave the corrupted entry untouched.
  signin_prefs().ClearChromeSigninBubbleRepromptCount(gaia_id);
  signin_prefs().ClearCrossDeviceHistoryPromoLastDismissedTime(gaia_id);
  EXPECT_EQ(pref_service().GetDict(kSigninAccountPrefs), expected_accounts);

  // A nested write replaces the corrupted entry with a dict that contains only
  // the written key. The valid account is untouched.
  signin_prefs().IncrementAvatarButtonHistorySyncPromoShownCount(gaia_id);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            1);
  base::DictValue* expected_account =
      expected_accounts.EnsureDict(gaia_id.ToString());
  expected_account->EnsureDict("AvatarButtonPromoCountDictionary")
      ->Set("AvatarButtonHistorySyncPromoShownCount", 1);
  EXPECT_EQ(pref_service().GetDict(kSigninAccountPrefs), expected_accounts);
  EXPECT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(valid_gaia_id),
            7);

  // Further writes at other depths build on the now valid entry.
  signin_prefs().SetChromeSigninInterceptionUserChoice(
      gaia_id, ChromeSigninUserChoice::kSignin);
  signin_prefs().SetCrossDeviceHistoryPromoLastDismissedTime(gaia_id, time);
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionUserChoice(gaia_id),
            ChromeSigninUserChoice::kSignin);
  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id),
            time);
  expected_account->Set("ChromeSigninInterceptionUserChoice",
                        static_cast<int>(ChromeSigninUserChoice::kSignin));
  expected_account->EnsureDict("CrossDevicePromoPrefs")
      ->EnsureDict("history")
      ->Set("last_dismissed_time", base::TimeToValue(time));
  EXPECT_EQ(pref_service().GetDict(kSigninAccountPrefs), expected_accounts);
}

// Corrupted on-disk data, level 3: an intermediate dict inside an otherwise
// valid account entry is not a dict. Reads below it must return default values,
// clears must be a no-op, and a write below it must replace only that
// intermediate with a dict, preserving the valid sibling keys of the account.
TEST_F(SigninPrefsTest,
       CorruptedIntermediateDictIsIgnoredOnReadAndOverwrittenOnWrite) {
  const GaiaId gaia_id("gaia_id_corrupted");
  const base::Time time = base::Time::FromSecondsSinceUnixEpoch(1700000000);
  base::DictValue expected_accounts;
  base::DictValue* expected_account =
      expected_accounts.EnsureDict(gaia_id.ToString());
  expected_account->Set("ChromeSigninInterceptionUserChoice",
                        static_cast<int>(ChromeSigninUserChoice::kSignin));
  expected_account->Set("AvatarButtonPromoCountDictionary", "not_a_dict");
  expected_account->Set("CrossDevicePromoPrefs", 42);
  {
    ScopedDictPrefUpdate update(&pref_service(), kSigninAccountPrefs);
    *update = expected_accounts.Clone();
  }

  // Reads below the corrupted intermediates are ignored and fall back to their
  // defaults, while the valid sibling key is still readable.
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            0);
  EXPECT_FALSE(signin_prefs()
                   .GetAvatarButtonSigninPromoLastShownTime(gaia_id)
                   .has_value());
  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoShownCount(gaia_id), 0);
  EXPECT_FALSE(
      signin_prefs().GetCrossDeviceHistoryPromoShownAfterDismissal(gaia_id));
  EXPECT_FALSE(signin_prefs()
                   .GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id)
                   .has_value());
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionUserChoice(gaia_id),
            ChromeSigninUserChoice::kSignin);

  // Clears are a no-op and leave the corrupted intermediates untouched.
  signin_prefs().ClearCrossDeviceHistoryPromoLastDismissedTime(gaia_id);
  EXPECT_EQ(pref_service().GetDict(kSigninAccountPrefs), expected_accounts);

  // A write under `CrossDevicePromoPrefs` replaces only that intermediate with
  // a dict. The other corrupted intermediate and the sibling key are untouched.
  signin_prefs().SetCrossDeviceHistoryPromoShownCount(gaia_id, 5);
  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoShownCount(gaia_id), 5);
  expected_account->Remove("CrossDevicePromoPrefs");
  expected_account->EnsureDict("CrossDevicePromoPrefs")
      ->EnsureDict("history")
      ->Set("shown_count", 5);
  EXPECT_EQ(pref_service().GetDict(kSigninAccountPrefs), expected_accounts);

  // Likewise for `AvatarButtonPromoCountDictionary`.
  signin_prefs().IncrementAvatarButtonHistorySyncPromoShownCount(gaia_id);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            1);
  expected_account->Remove("AvatarButtonPromoCountDictionary");
  expected_account->EnsureDict("AvatarButtonPromoCountDictionary")
      ->Set("AvatarButtonHistorySyncPromoShownCount", 1);
  EXPECT_EQ(pref_service().GetDict(kSigninAccountPrefs), expected_accounts);

  // Depth-2 corruption: `CrossDevicePromoPrefs` is a valid dict but `history`
  // is not. Reads are ignored, and a write replaces `history` with a dict that
  // contains only the written key.
  {
    ScopedDictPrefUpdate update(&pref_service(), kSigninAccountPrefs);
    update->FindDict(gaia_id.ToString())
        ->FindDict("CrossDevicePromoPrefs")
        ->Set("history", "not_a_dict");
  }
  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoShownCount(gaia_id), 0);
  signin_prefs().SetCrossDeviceHistoryPromoLastDismissedTime(gaia_id, time);
  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id),
            time);
  base::DictValue* expected_cross_device =
      expected_account->FindDict("CrossDevicePromoPrefs");
  expected_cross_device->Remove("history");
  expected_cross_device->EnsureDict("history")->Set("last_dismissed_time",
                                                    base::TimeToValue(time));
  EXPECT_EQ(pref_service().GetDict(kSigninAccountPrefs), expected_accounts);
}

TEST_F(SigninPrefsTest, RegisteredPrefsAreNonEmptyAndValid) {
  auto pass_key = SigninPrefsRegistry::CreatePassKeyForTesting();
  base::span<const SigninPrefsRegistry::PrefDescriptor> prefs =
      SigninPrefsRegistry::GetAll(pass_key);
  EXPECT_FALSE(prefs.empty());

  for (const auto& pref : prefs) {
    EXPECT_FALSE(pref.key.empty());
    EXPECT_NE(pref.type, base::Value::Type::NONE);
    if (pref.is_timestamp) {
      EXPECT_EQ(pref.type, base::Value::Type::STRING);
    }
    EXPECT_EQ(SigninPrefsRegistry::Find(pass_key, pref.parent_keys(), pref.key),
              &pref);
  }

  EXPECT_EQ(SigninPrefsRegistry::Find(pass_key, {}, "UnknownPrefKey"), nullptr);
  EXPECT_NE(SigninPrefsRegistry::Find(
                pass_key, {}, signin::internal::kPasswordSignInPromoShownCount),
            nullptr);
  EXPECT_NE(SigninPrefsRegistry::Find(
                pass_key,
                {{signin::internal::kCrossDevicePromoPrefs,
                  signin::internal::kCrossDevicePromoHistoryDictKey}},
                signin::internal::kCrossDevicePromoShownCountKey),
            nullptr);
}

TEST_F(SigninPrefsTest, IsValidValueValidatesTypesAndTimestamps) {
  auto pass_key = SigninPrefsRegistry::CreatePassKeyForTesting();

  // Integer preference: accepts integers, rejects strings/booleans.
  EXPECT_TRUE(SigninPrefsRegistry::IsValidValue(
      pass_key, {}, signin::internal::kPasswordSignInPromoShownCount,
      base::Value(3)));
  EXPECT_FALSE(SigninPrefsRegistry::IsValidValue(
      pass_key, {}, signin::internal::kPasswordSignInPromoShownCount,
      base::Value("3")));
  EXPECT_FALSE(SigninPrefsRegistry::IsValidValue(
      pass_key, {}, signin::internal::kPasswordSignInPromoShownCount,
      base::Value(true)));

  // Boolean preference: accepts booleans, rejects integers.
  EXPECT_TRUE(SigninPrefsRegistry::IsValidValue(
      pass_key, {}, signin::internal::kExtensionsExplicitBrowserSigninEnabled,
      base::Value(true)));
  EXPECT_FALSE(SigninPrefsRegistry::IsValidValue(
      pass_key, {}, signin::internal::kExtensionsExplicitBrowserSigninEnabled,
      base::Value(1)));

  // Timestamp preference: accepts valid TimeToValue strings, rejects arbitrary
  // strings and integers.
  EXPECT_TRUE(SigninPrefsRegistry::IsValidValue(
      pass_key, {}, signin::internal::kChromeLastSignoutTime,
      base::TimeToValue(base::Time::Now())));
  EXPECT_FALSE(SigninPrefsRegistry::IsValidValue(
      pass_key, {}, signin::internal::kChromeLastSignoutTime,
      base::Value("not_a_timestamp")));
  EXPECT_FALSE(SigninPrefsRegistry::IsValidValue(
      pass_key, {}, signin::internal::kChromeLastSignoutTime,
      base::Value(12345)));

  // Nested preference under CrossDevicePromoPrefs.history.
  EXPECT_TRUE(SigninPrefsRegistry::IsValidValue(
      pass_key, signin::internal::kCrossDeviceHistoryPromoParents,
      signin::internal::kCrossDevicePromoShownCountKey, base::Value(2)));
  EXPECT_FALSE(SigninPrefsRegistry::IsValidValue(
      pass_key, signin::internal::kCrossDeviceHistoryPromoParents,
      signin::internal::kCrossDevicePromoShownCountKey, base::Value("2")));
  // Same leaf key at the top level is NOT registered.
  EXPECT_FALSE(SigninPrefsRegistry::IsValidValue(
      pass_key, {}, signin::internal::kCrossDevicePromoShownCountKey,
      base::Value(2)));

  // Dictionary containers cannot be set as leaf values.
  EXPECT_FALSE(SigninPrefsRegistry::IsValidValue(
      pass_key, {}, signin::internal::kCrossDevicePromoPrefs,
      base::Value(base::Value::Type::DICT)));
  EXPECT_FALSE(SigninPrefsRegistry::IsValidValue(
      pass_key, {}, signin::internal::kAvatarButtonPromoCountDictionary,
      base::Value(base::Value::Type::DICT)));
  EXPECT_FALSE(SigninPrefsRegistry::IsValidValue(
      pass_key, signin::internal::kCrossDevicePromoRootParents,
      signin::internal::kCrossDevicePromoHistoryDictKey,
      base::Value(base::Value::Type::DICT)));

  // Unknown preference key: always rejected.
  EXPECT_FALSE(SigninPrefsRegistry::IsValidValue(pass_key, {}, "UnknownPrefKey",
                                                 base::Value(1)));
}

namespace {

using PrefPath = std::pair<std::vector<std::string>, std::string>;

void VerifyAndCollectDictEntriesAreRegistered(
    const base::DictValue& dict,
    std::vector<std::string_view> parents,
    std::set<PrefPath>* observed_paths) {
  auto pass_key = SigninPrefsRegistry::CreatePassKeyForTesting();
  for (const auto [key, value] : dict) {
    observed_paths->insert(
        {std::vector<std::string>(parents.begin(), parents.end()), key});
    if (value.is_dict()) {
      const SigninPrefsRegistry::PrefDescriptor* pref_desc =
          SigninPrefsRegistry::Find(pass_key, parents, key);
      EXPECT_NE(pref_desc, nullptr) << "Unregistered dict: " << key;
      if (pref_desc) {
        EXPECT_EQ(pref_desc->type, base::Value::Type::DICT);
      }
      std::vector<std::string_view> child_parents = parents;
      child_parents.push_back(key);
      VerifyAndCollectDictEntriesAreRegistered(value.GetDict(), child_parents,
                                               observed_paths);
    } else {
      EXPECT_TRUE(
          SigninPrefsRegistry::IsValidValue(pass_key, parents, key, value))
          << "Unregistered or type-mismatched key: " << key;
    }
  }
}

}  // namespace

TEST_F(SigninPrefsTest, AllWrittenPrefsAreRegisteredWithMatchingType) {
  const GaiaId gaia_id("gaia_id_all_prefs");
  const base::Time now = base::Time::Now();

  // Exercise all setters/mutators.
  signin_prefs().SetChromeSigninInterceptionUserChoice(
      gaia_id, ChromeSigninUserChoice::kSignin);
  signin_prefs().SetAccountMetricsId(gaia_id, 7);
  signin_prefs().SetAccountMetricsIdCapped(gaia_id);
  signin_prefs().SetChromeLastSignoutTime(gaia_id, now);
  signin_prefs().SetChromeSigninInterceptionLastBubbleDeclineTime(gaia_id, now);
  signin_prefs().IncrementChromeSigninBubbleRepromptCount(gaia_id);
  signin_prefs().IncrementChromeSigninInterceptionDismissCount(gaia_id);
  signin_prefs().IncrementPasswordSigninPromoImpressionCount(gaia_id);
  signin_prefs().IncrementAddressSigninPromoImpressionCount(gaia_id);
  signin_prefs().IncrementBookmarkSigninPromoImpressionCount(gaia_id);
  signin_prefs().IncrementBookmarkManagerSigninPromoImpressionCount(gaia_id);
  signin_prefs().IncrementBookmarkManagerSigninPromoDismissCount(gaia_id);
  signin_prefs().IncrementSearchAIModeSigninPromoImpressionCount(gaia_id);
  signin_prefs().IncrementAutofillSigninPromoDismissCount(gaia_id);
  signin_prefs().IncrementSearchAIModeSigninPromoDismissCount(gaia_id);
  signin_prefs().SetExtensionsExplicitBrowserSignin(gaia_id, true);
  signin_prefs().SetBookmarksExplicitBrowserSignin(gaia_id, true);
  signin_prefs().SetPolicyDisclaimerLastRegistrationFailureTime(gaia_id, now);
  signin_prefs().SetSearchAIModeSigninPromoLastImpressionTime(gaia_id, now);
  signin_prefs().IncrementHistoryPageHistorySyncPromoShownCount(gaia_id);
  signin_prefs().SetHistoryPageHistorySyncPromoLastDismissedTimestamp(gaia_id,
                                                                      now);
  signin_prefs().SetHistoryPageHistorySyncPromoShownAfterDismissal(gaia_id);
  signin_prefs().IncrementBookmarkBatchUploadPromoDismissCountWithLastTime(
      gaia_id);
  signin_prefs().SetBatchUploadLastUploadRemainingLocalDataCount(gaia_id, 4);

  // AvatarButtonPromoCountDictionary prefs via typed SigninPrefs accessors.
  signin_prefs().IncrementAvatarButtonHistorySyncPromoShownCount(gaia_id);
  signin_prefs().IncrementAvatarButtonHistorySyncPromoUsedCount(gaia_id);
  signin_prefs().IncrementAvatarButtonBatchUploadPromoShownCount(gaia_id);
  signin_prefs().IncrementAvatarButtonBatchUploadPromoUsedCount(gaia_id);
  signin_prefs().IncrementAvatarButtonBatchUploadBookmarkPromoShownCount(
      gaia_id);
  signin_prefs().IncrementAvatarButtonBatchUploadBookmarkPromoUsedCount(
      gaia_id);
  signin_prefs()
      .IncrementAvatarButtonBatchUploadWindows10DepreciationPromoShownCount(
          gaia_id);
  signin_prefs()
      .IncrementAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount(
          gaia_id);
  signin_prefs().IncrementAvatarButtonSigninPromoShownCount(gaia_id);
  signin_prefs().IncrementAvatarButtonSigninPromoUsedCount(gaia_id);
  signin_prefs().SetAvatarButtonSigninPromoLastShownTime(gaia_id, now);

  // CrossDevicePromoPrefs.history prefs via typed SigninPrefs accessors.
  signin_prefs().SetCrossDeviceHistoryPromoShownCount(gaia_id, 2);
  signin_prefs().SetCrossDeviceHistoryPromoLastDismissedTime(gaia_id, now);
  signin_prefs().SetCrossDeviceHistoryPromoShownAfterDismissal(gaia_id, true);

  // Verify all typed getters return the expected values and succeed.
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionUserChoice(gaia_id),
            ChromeSigninUserChoice::kSignin);
  EXPECT_EQ(signin_prefs().GetAccountMetricsId(gaia_id), 7);
  EXPECT_TRUE(signin_prefs().IsAccountMetricsIdCapped(gaia_id));
  EXPECT_EQ(signin_prefs().GetChromeLastSignoutTime(gaia_id), now);
  EXPECT_EQ(
      signin_prefs().GetChromeSigninInterceptionLastBubbleDeclineTime(gaia_id),
      now);
  EXPECT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(gaia_id), 1);
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionDismissCount(gaia_id), 1);
  EXPECT_EQ(signin_prefs().GetPasswordSigninPromoImpressionCount(gaia_id), 1);
  EXPECT_EQ(signin_prefs().GetAddressSigninPromoImpressionCount(gaia_id), 1);
  EXPECT_EQ(signin_prefs().GetBookmarkSigninPromoImpressionCount(gaia_id), 1);
  EXPECT_EQ(
      signin_prefs().GetBookmarkManagerSigninPromoImpressionCount(gaia_id), 1);
  EXPECT_EQ(signin_prefs().GetBookmarkManagerSigninPromoDismissCount(gaia_id),
            1);
  EXPECT_EQ(signin_prefs().GetSearchAIModeSigninPromoImpressionCount(gaia_id),
            1);
  EXPECT_EQ(signin_prefs().GetAutofillSigninPromoDismissCount(gaia_id), 1);
  EXPECT_EQ(signin_prefs().GetSearchAIModeSigninPromoDismissCount(gaia_id), 1);
  EXPECT_TRUE(signin_prefs().GetExtensionsExplicitBrowserSignin(gaia_id));
  EXPECT_TRUE(signin_prefs().GetBookmarksExplicitBrowserSignin(gaia_id));
  EXPECT_EQ(
      signin_prefs().GetPolicyDisclaimerLastRegistrationFailureTime(gaia_id),
      now);
  EXPECT_EQ(
      signin_prefs().GetSearchAIModeSigninPromoLastImpressionTime(gaia_id),
      now);
  EXPECT_EQ(signin_prefs().GetHistoryPageHistorySyncPromoShownCount(gaia_id),
            1);
  EXPECT_EQ(
      signin_prefs().GetHistoryPageHistorySyncPromoLastDismissedTimestamp(
          gaia_id),
      now);
  EXPECT_TRUE(
      signin_prefs().GetHistoryPageHistorySyncPromoShownAfterDismissal(
          gaia_id));
  auto [bookmark_batch_dismiss_count, bookmark_batch_dismiss_time] =
      signin_prefs().GetBookmarkBatchUploadPromoDismissCountWithLastTime(
          gaia_id);
  EXPECT_EQ(bookmark_batch_dismiss_count, 1);
  EXPECT_TRUE(bookmark_batch_dismiss_time.has_value());
  EXPECT_EQ(
      signin_prefs().GetBatchUploadLastUploadRemainingLocalDataCount(gaia_id),
      4);

  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            1);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoUsedCount(gaia_id),
            1);
  EXPECT_EQ(signin_prefs().GetAvatarButtonBatchUploadPromoShownCount(gaia_id),
            1);
  EXPECT_EQ(signin_prefs().GetAvatarButtonBatchUploadPromoUsedCount(gaia_id),
            1);
  EXPECT_EQ(
      signin_prefs().GetAvatarButtonBatchUploadBookmarkPromoShownCount(gaia_id),
      1);
  EXPECT_EQ(
      signin_prefs().GetAvatarButtonBatchUploadBookmarkPromoUsedCount(gaia_id),
      1);
  EXPECT_EQ(
      signin_prefs()
          .GetAvatarButtonBatchUploadWindows10DepreciationPromoShownCount(
              gaia_id),
      1);
  EXPECT_EQ(
      signin_prefs()
          .GetAvatarButtonBatchUploadWindows10DepreciationPromoUsedCount(
              gaia_id),
      1);
  EXPECT_EQ(signin_prefs().GetAvatarButtonSigninPromoShownCount(gaia_id), 1);
  EXPECT_EQ(signin_prefs().GetAvatarButtonSigninPromoUsedCount(gaia_id), 1);
  EXPECT_EQ(signin_prefs().GetAvatarButtonSigninPromoLastShownTime(gaia_id),
            now);

  EXPECT_EQ(signin_prefs().GetCrossDeviceHistoryPromoShownCount(gaia_id), 2);
  EXPECT_EQ(
      signin_prefs().GetCrossDeviceHistoryPromoLastDismissedTime(gaia_id), now);
  EXPECT_TRUE(
      signin_prefs().GetCrossDeviceHistoryPromoShownAfterDismissal(gaia_id));

  SigninPrefsAccessor accessor(pref_service(),
                               SigninPrefsAccessor::CreatePassKeyForTesting());
  const base::DictValue* account_dict =
      accessor.GetAccountPrefsDict().FindDict(gaia_id.ToString());
  ASSERT_NE(account_dict, nullptr);
  std::set<PrefPath> observed_paths;
  VerifyAndCollectDictEntriesAreRegistered(*account_dict, /*parents=*/{},
                                           &observed_paths);

  auto pass_key = SigninPrefsRegistry::CreatePassKeyForTesting();
  std::set<PrefPath> registered_paths;
  for (const auto& pref : SigninPrefsRegistry::GetAll(pass_key)) {
    base::span<const std::string_view> parents = pref.parent_keys();
    registered_paths.insert(
        {std::vector<std::string>(parents.begin(), parents.end()),
         std::string(pref.key)});
  }
  EXPECT_EQ(observed_paths, registered_paths);
}

TEST_F(SigninPrefsTest, EmptyGaiaInterceptionUserChoice) {
  const GaiaId empty_gaia;
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionUserChoice(empty_gaia),
            ChromeSigninUserChoice::kNoChoice);
  EXPECT_FALSE(HasAccountPrefs(empty_gaia));

  signin_prefs().SetChromeSigninInterceptionUserChoice(
      empty_gaia, ChromeSigninUserChoice::kSignin);
  EXPECT_FALSE(HasAccountPrefs(empty_gaia));
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionUserChoice(empty_gaia),
            ChromeSigninUserChoice::kNoChoice);
}

TEST_F(SigninPrefsTest, MigrateObsoleteAccountPrefsWithCorruptedEntry) {
  const GaiaId valid_gaia("valid_gaia");
  const GaiaId other_valid_gaia("other_valid_gaia");
  signin_prefs().SetDeprecatedPrefForTesting(valid_gaia);
  signin_prefs().IncrementChromeSigninBubbleRepromptCount(valid_gaia);
  signin_prefs().SetChromeSigninInterceptionUserChoice(
      other_valid_gaia, ChromeSigninUserChoice::kSignin);
  ASSERT_TRUE(
      signin_prefs().GetDeprecatedPrefForTesting(valid_gaia).has_value());

  // Corrupt the accounts dict by adding non-dict entries.
  {
    ScopedDictPrefUpdate update(&pref_service(), kSigninAccountPrefs);
    update->Set("corrupted_account_not_a_dict", "corrupted_string_value");
    update->Set("corrupted_account_int", 12345);
  }
  ASSERT_EQ(pref_service().GetDict(kSigninAccountPrefs).size(), 4u);

  // MigrateObsoleteSigninPrefs should not crash on non-dict entries, should
  // remove them, and should clean up deprecated prefs from valid accounts
  // while keeping their other keys intact.
  signin_prefs().MigrateObsoleteSigninPrefs();
  EXPECT_FALSE(
      signin_prefs().GetDeprecatedPrefForTesting(valid_gaia).has_value());
  EXPECT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(valid_gaia), 1);
  EXPECT_EQ(
      signin_prefs().GetChromeSigninInterceptionUserChoice(other_valid_gaia),
      ChromeSigninUserChoice::kSignin);

  base::DictValue expected_accounts;
  expected_accounts.EnsureDict(valid_gaia.ToString())
      ->Set("ChromeSigninInterceptionRepromptCount", 1);
  expected_accounts.EnsureDict(other_valid_gaia.ToString())
      ->Set("ChromeSigninInterceptionUserChoice",
            static_cast<int>(ChromeSigninUserChoice::kSignin));
  EXPECT_EQ(pref_service().GetDict(kSigninAccountPrefs), expected_accounts);
}

TEST_F(SigninPrefsTest, SigninPrefsAccessorDirectOperations) {
  SigninPrefsAccessor accessor(pref_service(),
                               SigninPrefsAccessor::CreatePassKeyForTesting());
  const GaiaId gaia_id("accessor_test_gaia");

  base::MockRepeatingClosure mock_observer;
  SigninPrefs::ObserveSigninPrefsChanges(pref_change_registrar(),
                                         mock_observer.Get());

  // ClearPref returns false and does not notify when account or key does not
  // exist.
  EXPECT_CALL(mock_observer, Run()).Times(0);
  EXPECT_FALSE(accessor.ClearPref(
      gaia_id, signin::internal::kChromeSigninInterceptionDismissCount));
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // SetValue sets a registered leaf value.
  EXPECT_CALL(mock_observer, Run()).Times(1);
  accessor.SetValue(gaia_id,
                    signin::internal::kChromeSigninInterceptionDismissCount,
                    base::Value(42));
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_EQ(
      accessor.GetIntPref(
          gaia_id, signin::internal::kChromeSigninInterceptionDismissCount),
      42);

  // ClearPref removes the key and returns true.
  EXPECT_CALL(mock_observer, Run()).Times(1);
  EXPECT_TRUE(accessor.ClearPref(
      gaia_id, signin::internal::kChromeSigninInterceptionDismissCount));
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_EQ(
      accessor.GetIntPref(
          gaia_id, signin::internal::kChromeSigninInterceptionDismissCount),
      0);
  // Second ClearPref returns false and does not notify.
  EXPECT_CALL(mock_observer, Run()).Times(0);
  EXPECT_FALSE(accessor.ClearPref(
      gaia_id, signin::internal::kChromeSigninInterceptionDismissCount));
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // SetValue with a valid timestamp string.
  const base::Time now = base::Time::Now();
  EXPECT_CALL(mock_observer, Run()).Times(1);
  accessor.SetValue(gaia_id, signin::internal::kChromeLastSignoutTime,
                    base::TimeToValue(now));
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_EQ(
      accessor.GetTimePref(gaia_id, signin::internal::kChromeLastSignoutTime),
      now);

  // Test IncrementIntPref clamping at INT_MAX.
  EXPECT_CALL(mock_observer, Run()).Times(2);
  accessor.SetIntPref(gaia_id,
                      signin::internal::kChromeSigninInterceptionDismissCount,
                      std::numeric_limits<int>::max());
  EXPECT_EQ(
      accessor.IncrementIntPref(
          gaia_id, signin::internal::kChromeSigninInterceptionDismissCount),
      std::numeric_limits<int>::max());
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_EQ(
      accessor.GetIntPref(
          gaia_id, signin::internal::kChromeSigninInterceptionDismissCount),
      std::numeric_limits<int>::max());

  // Test nested pref access (CrossDevice history promo) and clearing a DICT
  // subtree.
  EXPECT_CALL(mock_observer, Run()).Times(3);
  accessor.SetValue(gaia_id, signin::internal::kCrossDevicePromoShownCountKey,
                    base::Value(5),
                    signin::internal::kCrossDeviceHistoryPromoParents);
  EXPECT_EQ(accessor.GetIntPref(
                gaia_id, signin::internal::kCrossDevicePromoShownCountKey,
                signin::internal::kCrossDeviceHistoryPromoParents),
            5);
  EXPECT_TRUE(accessor.ClearPref(
      gaia_id, signin::internal::kCrossDevicePromoShownCountKey,
      signin::internal::kCrossDeviceHistoryPromoParents));
  EXPECT_EQ(accessor.GetIntPref(
                gaia_id, signin::internal::kCrossDevicePromoShownCountKey,
                signin::internal::kCrossDeviceHistoryPromoParents),
            0);
  EXPECT_TRUE(
      accessor.ClearPref(gaia_id, signin::internal::kCrossDevicePromoPrefs));
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
}

// Accessor-level counterpart of
// `CorruptedAccountsPrefIsIgnoredOnReadAndOverwrittenOnWrite`: the accounts
// pref itself is not a dict. Reads return defaults, clears are a no-op, and the
// first write replaces the corrupted value with a proper dict holding only the
// written account.
TEST_F(SigninPrefsTest,
       AccessorCorruptedAccountsPrefIgnoredOnReadOverwrittenOnWrite) {
  SigninPrefsAccessor accessor(pref_service(),
                               SigninPrefsAccessor::CreatePassKeyForTesting());
  const GaiaId gaia_id("gaia_id_corrupted");
  const base::Time time = base::Time::FromSecondsSinceUnixEpoch(1700000000);
  pref_service().SetUserPref(kSigninAccountPrefs, base::Value(42));
  ASSERT_EQ(*pref_service().GetUserPref(kSigninAccountPrefs), base::Value(42));

  // Reads are ignored: the accounts dict appears empty and every value kind
  // falls back to its default.
  EXPECT_TRUE(accessor.GetAccountPrefsDict().empty());
  EXPECT_FALSE(accessor.HasAccountPrefs(gaia_id));
  EXPECT_EQ(
      accessor.MaybeGetIntPref(
          gaia_id, signin::internal::kChromeSigninInterceptionRepromptCount),
      std::nullopt);
  EXPECT_EQ(
      accessor.GetIntPref(
          gaia_id, signin::internal::kAvatarButtonHistorySyncPromoShownCount,
          signin::internal::kAvatarButtonPromoParents),
      0);
  EXPECT_FALSE(accessor.GetBooleanPref(
      gaia_id, signin::internal::kExtensionsExplicitBrowserSigninEnabled));
  EXPECT_EQ(
      accessor.GetTimePref(
          gaia_id, signin::internal::kCrossDevicePromoLastDismissedTimeKey,
          signin::internal::kCrossDeviceHistoryPromoParents),
      std::nullopt);

  // Clears are a no-op, report nothing removed, and leave the corrupted value
  // untouched.
  EXPECT_FALSE(accessor.ClearPref(
      gaia_id, signin::internal::kChromeSigninInterceptionRepromptCount));
  EXPECT_FALSE(accessor.ClearPref(
      gaia_id, signin::internal::kCrossDevicePromoLastDismissedTimeKey,
      signin::internal::kCrossDeviceHistoryPromoParents));
  EXPECT_EQ(*pref_service().GetUserPref(kSigninAccountPrefs), base::Value(42));

  // A top-level write replaces the corrupted value with a dict that contains
  // only the written account and key.
  accessor.SetValue(gaia_id,
                    signin::internal::kChromeSigninInterceptionRepromptCount,
                    base::Value(3));
  EXPECT_EQ(
      accessor.MaybeGetIntPref(
          gaia_id, signin::internal::kChromeSigninInterceptionRepromptCount),
      3);
  EXPECT_TRUE(accessor.HasAccountPrefs(gaia_id));
  base::DictValue expected_accounts;
  base::DictValue* expected_account =
      expected_accounts.EnsureDict(gaia_id.ToString());
  expected_account->Set(
      signin::internal::kChromeSigninInterceptionRepromptCount, 3);
  const base::Value* raw_accounts =
      pref_service().GetUserPref(kSigninAccountPrefs);
  ASSERT_TRUE(raw_accounts);
  ASSERT_TRUE(raw_accounts->is_dict());
  EXPECT_EQ(raw_accounts->GetDict(), expected_accounts);
  EXPECT_EQ(accessor.GetAccountPrefsDict(), expected_accounts);

  // Nested writes then build on the now valid dict.
  EXPECT_EQ(
      accessor.IncrementIntPref(
          gaia_id, signin::internal::kAvatarButtonHistorySyncPromoShownCount,
          signin::internal::kAvatarButtonPromoParents),
      1);
  accessor.SetTimePref(gaia_id,
                       signin::internal::kCrossDevicePromoLastDismissedTimeKey,
                       time, signin::internal::kCrossDeviceHistoryPromoParents);
  EXPECT_EQ(
      accessor.GetTimePref(
          gaia_id, signin::internal::kCrossDevicePromoLastDismissedTimeKey,
          signin::internal::kCrossDeviceHistoryPromoParents),
      time);
  expected_account
      ->EnsureDict(signin::internal::kAvatarButtonPromoCountDictionary)
      ->Set(signin::internal::kAvatarButtonHistorySyncPromoShownCount, 1);
  expected_account->EnsureDict(signin::internal::kCrossDevicePromoPrefs)
      ->EnsureDict(signin::internal::kCrossDevicePromoHistoryDictKey)
      ->Set(signin::internal::kCrossDevicePromoLastDismissedTimeKey,
            base::TimeToValue(time));
  EXPECT_EQ(accessor.GetAccountPrefsDict(), expected_accounts);
}

// Accessor-level counterpart of
// `CorruptedAccountEntryIsIgnoredOnReadAndOverwrittenOnWrite`: an account entry
// is not a dict. Reads return defaults, clears are a no-op, and the first write
// replaces the corrupted entry with a dict holding only the written key,
// without affecting other (valid) accounts.
TEST_F(SigninPrefsTest,
       AccessorCorruptedAccountEntryIgnoredOnReadOverwrittenOnWrite) {
  SigninPrefsAccessor accessor(pref_service(),
                               SigninPrefsAccessor::CreatePassKeyForTesting());
  const GaiaId gaia_id("gaia_id_corrupted");
  const GaiaId valid_gaia_id("gaia_id_valid");
  const base::Time time = base::Time::FromSecondsSinceUnixEpoch(1700000000);
  base::DictValue expected_accounts;
  expected_accounts.Set(gaia_id.ToString(), "not_a_dict");
  expected_accounts.EnsureDict(valid_gaia_id.ToString())
      ->Set(signin::internal::kChromeSigninInterceptionRepromptCount, 7);
  {
    ScopedDictPrefUpdate update(&pref_service(), kSigninAccountPrefs);
    *update = expected_accounts.Clone();
  }
  ASSERT_EQ(accessor.GetAccountPrefsDict(), expected_accounts);

  // Reads are ignored: every value kind falls back to its default.
  EXPECT_EQ(
      accessor.MaybeGetIntPref(
          gaia_id, signin::internal::kChromeSigninInterceptionRepromptCount),
      std::nullopt);
  EXPECT_EQ(
      accessor.GetIntPref(
          gaia_id, signin::internal::kAvatarButtonHistorySyncPromoShownCount,
          signin::internal::kAvatarButtonPromoParents),
      0);
  EXPECT_FALSE(accessor.GetBooleanPref(
      gaia_id, signin::internal::kExtensionsExplicitBrowserSigninEnabled));
  EXPECT_EQ(
      accessor.GetTimePref(gaia_id, signin::internal::kChromeLastSignoutTime),
      std::nullopt);
  EXPECT_EQ(
      accessor.GetTimePref(
          gaia_id, signin::internal::kCrossDevicePromoLastDismissedTimeKey,
          signin::internal::kCrossDeviceHistoryPromoParents),
      std::nullopt);

  // Clears are a no-op, report nothing removed, and leave the corrupted entry
  // untouched.
  EXPECT_FALSE(accessor.ClearPref(
      gaia_id, signin::internal::kChromeSigninInterceptionRepromptCount));
  EXPECT_FALSE(accessor.ClearPref(
      gaia_id, signin::internal::kCrossDevicePromoLastDismissedTimeKey,
      signin::internal::kCrossDeviceHistoryPromoParents));
  EXPECT_EQ(accessor.GetAccountPrefsDict(), expected_accounts);

  // A nested write replaces the corrupted entry with a dict that contains only
  // the written key. The valid account is untouched.
  EXPECT_EQ(
      accessor.IncrementIntPref(
          gaia_id, signin::internal::kAvatarButtonHistorySyncPromoShownCount,
          signin::internal::kAvatarButtonPromoParents),
      1);
  base::DictValue* expected_account =
      expected_accounts.EnsureDict(gaia_id.ToString());
  expected_account
      ->EnsureDict(signin::internal::kAvatarButtonPromoCountDictionary)
      ->Set(signin::internal::kAvatarButtonHistorySyncPromoShownCount, 1);
  EXPECT_EQ(accessor.GetAccountPrefsDict(), expected_accounts);
  EXPECT_EQ(accessor.MaybeGetIntPref(
                valid_gaia_id,
                signin::internal::kChromeSigninInterceptionRepromptCount),
            7);

  // Further writes at other depths build on the now valid entry.
  accessor.SetBooleanPref(
      gaia_id, signin::internal::kExtensionsExplicitBrowserSigninEnabled, true);
  accessor.SetValue(gaia_id,
                    signin::internal::kCrossDevicePromoLastDismissedTimeKey,
                    base::TimeToValue(time),
                    signin::internal::kCrossDeviceHistoryPromoParents);
  EXPECT_TRUE(accessor.GetBooleanPref(
      gaia_id, signin::internal::kExtensionsExplicitBrowserSigninEnabled));
  EXPECT_EQ(
      accessor.GetTimePref(
          gaia_id, signin::internal::kCrossDevicePromoLastDismissedTimeKey,
          signin::internal::kCrossDeviceHistoryPromoParents),
      time);
  expected_account->Set(
      signin::internal::kExtensionsExplicitBrowserSigninEnabled, true);
  expected_account->EnsureDict(signin::internal::kCrossDevicePromoPrefs)
      ->EnsureDict(signin::internal::kCrossDevicePromoHistoryDictKey)
      ->Set(signin::internal::kCrossDevicePromoLastDismissedTimeKey,
            base::TimeToValue(time));
  EXPECT_EQ(accessor.GetAccountPrefsDict(), expected_accounts);
}

// Accessor-level counterpart of
// `CorruptedIntermediateDictIsIgnoredOnReadAndOverwrittenOnWrite`: an
// intermediate dict inside an otherwise valid account entry is not a dict.
// Reads below it return defaults, clears are a no-op, and a write below it
// replaces only that intermediate with a dict, preserving the valid sibling
// keys of the account.
TEST_F(SigninPrefsTest,
       AccessorCorruptedIntermediateDictIgnoredOnReadOverwrittenOnWrite) {
  SigninPrefsAccessor accessor(pref_service(),
                               SigninPrefsAccessor::CreatePassKeyForTesting());
  const GaiaId gaia_id("gaia_id_corrupted");
  const base::Time time = base::Time::FromSecondsSinceUnixEpoch(1700000000);
  base::DictValue expected_accounts;
  base::DictValue* expected_account =
      expected_accounts.EnsureDict(gaia_id.ToString());
  expected_account->Set(signin::internal::kChromeSigninInterceptionUserChoice,
                        static_cast<int>(ChromeSigninUserChoice::kSignin));
  expected_account->Set(signin::internal::kAvatarButtonPromoCountDictionary,
                        "not_a_dict");
  expected_account->Set(signin::internal::kCrossDevicePromoPrefs, 42);
  {
    ScopedDictPrefUpdate update(&pref_service(), kSigninAccountPrefs);
    *update = expected_accounts.Clone();
  }
  ASSERT_EQ(accessor.GetAccountPrefsDict(), expected_accounts);

  // Reads below the corrupted intermediates are ignored and fall back to their
  // defaults, while the valid sibling key is still readable.
  EXPECT_EQ(
      accessor.MaybeGetIntPref(
          gaia_id, signin::internal::kAvatarButtonHistorySyncPromoShownCount,
          signin::internal::kAvatarButtonPromoParents),
      std::nullopt);
  EXPECT_EQ(
      accessor.GetTimePref(
          gaia_id, signin::internal::kAvatarButtonSigninPromoLastShownTime,
          signin::internal::kAvatarButtonPromoParents),
      std::nullopt);
  EXPECT_EQ(accessor.GetIntPref(
                gaia_id, signin::internal::kCrossDevicePromoShownCountKey,
                signin::internal::kCrossDeviceHistoryPromoParents),
            0);
  EXPECT_FALSE(accessor.GetBooleanPref(
      gaia_id, signin::internal::kCrossDevicePromoShownAfterDismissalKey,
      signin::internal::kCrossDeviceHistoryPromoParents));
  EXPECT_EQ(
      accessor.GetTimePref(
          gaia_id, signin::internal::kCrossDevicePromoLastDismissedTimeKey,
          signin::internal::kCrossDeviceHistoryPromoParents),
      std::nullopt);
  EXPECT_EQ(accessor.MaybeGetIntPref(
                gaia_id, signin::internal::kChromeSigninInterceptionUserChoice),
            static_cast<int>(ChromeSigninUserChoice::kSignin));

  // Clears below the corrupted intermediates are a no-op, report nothing
  // removed, and leave the corrupted intermediates untouched.
  EXPECT_FALSE(accessor.ClearPref(
      gaia_id, signin::internal::kAvatarButtonHistorySyncPromoShownCount,
      signin::internal::kAvatarButtonPromoParents));
  EXPECT_FALSE(accessor.ClearPref(
      gaia_id, signin::internal::kCrossDevicePromoLastDismissedTimeKey,
      signin::internal::kCrossDeviceHistoryPromoParents));
  EXPECT_EQ(accessor.GetAccountPrefsDict(), expected_accounts);

  // A write under `CrossDevicePromoPrefs` replaces only that intermediate with
  // a dict. The other corrupted intermediate and the sibling key are untouched.
  accessor.SetIntPref(gaia_id, signin::internal::kCrossDevicePromoShownCountKey,
                      5, signin::internal::kCrossDeviceHistoryPromoParents);
  EXPECT_EQ(accessor.GetIntPref(
                gaia_id, signin::internal::kCrossDevicePromoShownCountKey,
                signin::internal::kCrossDeviceHistoryPromoParents),
            5);
  expected_account->Remove(signin::internal::kCrossDevicePromoPrefs);
  expected_account->EnsureDict(signin::internal::kCrossDevicePromoPrefs)
      ->EnsureDict(signin::internal::kCrossDevicePromoHistoryDictKey)
      ->Set(signin::internal::kCrossDevicePromoShownCountKey, 5);
  EXPECT_EQ(accessor.GetAccountPrefsDict(), expected_accounts);

  // Likewise for `AvatarButtonPromoCountDictionary`.
  EXPECT_EQ(
      accessor.IncrementIntPref(
          gaia_id, signin::internal::kAvatarButtonHistorySyncPromoShownCount,
          signin::internal::kAvatarButtonPromoParents),
      1);
  expected_account->Remove(signin::internal::kAvatarButtonPromoCountDictionary);
  expected_account
      ->EnsureDict(signin::internal::kAvatarButtonPromoCountDictionary)
      ->Set(signin::internal::kAvatarButtonHistorySyncPromoShownCount, 1);
  EXPECT_EQ(accessor.GetAccountPrefsDict(), expected_accounts);

  // Depth-2 corruption: `CrossDevicePromoPrefs` is a valid dict but `history`
  // is not. Reads are ignored, and a write replaces `history` with a dict that
  // contains only the written key.
  {
    ScopedDictPrefUpdate update(&pref_service(), kSigninAccountPrefs);
    update->FindDict(gaia_id.ToString())
        ->FindDict(signin::internal::kCrossDevicePromoPrefs)
        ->Set(signin::internal::kCrossDevicePromoHistoryDictKey, "not_a_dict");
  }
  EXPECT_EQ(accessor.GetIntPref(
                gaia_id, signin::internal::kCrossDevicePromoShownCountKey,
                signin::internal::kCrossDeviceHistoryPromoParents),
            0);
  accessor.SetTimePref(gaia_id,
                       signin::internal::kCrossDevicePromoLastDismissedTimeKey,
                       time, signin::internal::kCrossDeviceHistoryPromoParents);
  EXPECT_EQ(
      accessor.GetTimePref(
          gaia_id, signin::internal::kCrossDevicePromoLastDismissedTimeKey,
          signin::internal::kCrossDeviceHistoryPromoParents),
      time);
  base::DictValue* expected_cross_device =
      expected_account->FindDict(signin::internal::kCrossDevicePromoPrefs);
  expected_cross_device->Remove(
      signin::internal::kCrossDevicePromoHistoryDictKey);
  expected_cross_device
      ->EnsureDict(signin::internal::kCrossDevicePromoHistoryDictKey)
      ->Set(signin::internal::kCrossDevicePromoLastDismissedTimeKey,
            base::TimeToValue(time));
  EXPECT_EQ(accessor.GetAccountPrefsDict(), expected_accounts);
}

// One assertion per distinct `CHECK` path: every `EXPECT_CHECK_DEATH` spawns a
// child process, so each additional assertion costs several seconds.
TEST_F(SigninPrefsTest, SigninPrefsAccessorCheckFailures) {
  SigninPrefsAccessor accessor(pref_service(),
                               SigninPrefsAccessor::CreatePassKeyForTesting());
  const GaiaId gaia_id("accessor_death_test_gaia");

  // Unregistered key, on the typed path and on `ClearPref`.
  EXPECT_CHECK_DEATH(accessor.SetIntPref(gaia_id, "UnregisteredPrefKey", 1));
  EXPECT_CHECK_DEATH(accessor.ClearPref(gaia_id, "UnregisteredPrefKey"));

  // Registered leaf key under the wrong parents: `kAccountMetricsId` is only
  // registered at the top level of the account dictionary.
  EXPECT_CHECK_DEATH(
      accessor.SetIntPref(gaia_id, signin::internal::kAccountMetricsId, 1,
                          signin::internal::kCrossDeviceHistoryPromoParents));

  // Type mismatch: `kAccountMetricsId` is registered as INTEGER.
  EXPECT_CHECK_DEATH(
      accessor.GetBooleanPref(gaia_id, signin::internal::kAccountMetricsId));

  // SetValue with a malformed timestamp string.
  EXPECT_CHECK_DEATH(accessor.SetValue(gaia_id,
                                       signin::internal::kChromeLastSignoutTime,
                                       base::Value("not_a_valid_timestamp")));

  // Empty GaiaId.
  EXPECT_CHECK_DEATH(accessor.SetIntPref(
      GaiaId(), signin::internal::kChromeSigninInterceptionDismissCount, 1));
}
