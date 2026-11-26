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

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/json/values_util.h"
#include "base/test/gtest_util.h"
#include "base/test/mock_callback.h"
#include "base/time/time.h"
#include "base/values.h"
#include "build/build_config.h"
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
constexpr char kMultipleAccountsMetadataList[] =
    "signin.multiple_accounts_metadata_list";

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

TEST_F(SigninPrefsTest, ChromeLastSigninTime) {
  const GaiaId gaia_id("gaia_id");

  ASSERT_FALSE(HasAccountPrefs(gaia_id));
  EXPECT_FALSE(signin_prefs().GetChromeLastSigninTime(gaia_id).has_value());
  signin_prefs().ClearAllChromeLastSigninTime();
  EXPECT_FALSE(HasAccountPrefs(gaia_id));

  const base::Time last_signin_time = base::Time::Now();
  signin_prefs().SetChromeLastSigninTime(gaia_id, last_signin_time);

  EXPECT_TRUE(HasAccountPrefs(gaia_id));
  EXPECT_EQ(signin_prefs().GetChromeLastSigninTime(gaia_id), last_signin_time);

  // Creating the main dict through setting a different pref should still return
  // the default value - no time.
  const GaiaId gaia_id2("gaia_id2");
  signin_prefs().SetChromeSigninInterceptionUserChoice(
      gaia_id2, ChromeSigninUserChoice::kSignin);
  ASSERT_TRUE(HasAccountPrefs(gaia_id2));
  EXPECT_FALSE(signin_prefs().GetChromeLastSigninTime(gaia_id2).has_value());

  signin_prefs().SetChromeLastSigninTime(gaia_id2, last_signin_time);
  signin_prefs().ClearAllChromeLastSigninTime();
  EXPECT_FALSE(signin_prefs().GetChromeLastSigninTime(gaia_id).has_value());
  EXPECT_FALSE(signin_prefs().GetChromeLastSigninTime(gaia_id2).has_value());
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

#if BUILDFLAG(IS_ANDROID)
TEST_F(SigninPrefsTest, DeviceSignalsPermanentConsentReceived) {
  const GaiaId gaia_id_1("gaia_id_1");
  const GaiaId gaia_id_2("gaia_id_2");

  // Defaults to false and reading does not create the account dictionary.
  EXPECT_FALSE(
      signin_prefs().GetDeviceSignalsPermanentConsentReceived(gaia_id_1));
  EXPECT_FALSE(HasAccountPrefs(gaia_id_1));

  signin_prefs().SetDeviceSignalsPermanentConsentReceived(gaia_id_1, true);
  EXPECT_TRUE(
      signin_prefs().GetDeviceSignalsPermanentConsentReceived(gaia_id_1));
  EXPECT_FALSE(
      signin_prefs().GetDeviceSignalsPermanentConsentReceived(gaia_id_2));

  signin_prefs().SetDeviceSignalsPermanentConsentReceived(gaia_id_1, false);
  EXPECT_FALSE(
      signin_prefs().GetDeviceSignalsPermanentConsentReceived(gaia_id_1));

  // The consent is removed together with the account prefs.
  signin_prefs().SetDeviceSignalsPermanentConsentReceived(gaia_id_1, true);
  signin_prefs().SetDeviceSignalsPermanentConsentReceived(gaia_id_2, true);
  EXPECT_EQ(signin_prefs().RemoveAllAccountPrefsExcept({gaia_id_2}), 1u);
  EXPECT_FALSE(
      signin_prefs().GetDeviceSignalsPermanentConsentReceived(gaia_id_1));
  EXPECT_TRUE(
      signin_prefs().GetDeviceSignalsPermanentConsentReceived(gaia_id_2));
}
#endif  // BUILDFLAG(IS_ANDROID)

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

TEST_F(SigninPrefsTest, CrossDevicePromoState) {
  using Surface = SigninPrefs::CrossDevicePromoSurface;
  using State = SigninPrefs::CrossDevicePromoState;
  const GaiaId gaia_id("gaia_id_1");
  const base::Time now = base::Time::Now();

  // Each surface is stored independently under its own sub-dictionary.
  const struct {
    Surface surface;
    std::string_view dict_key;
    State state;
  } kTestCases[] = {
      {Surface::kHistory, "history", {.shown_count = 1}},
      {Surface::kBookmarkSave,
       "bookmark_save",
       {.shown_count = 2, .dismissed_count = 1, .last_dismissed_time = now}},
      {Surface::kPasswordSave, "password_save", {.shown_count = 3}},
      {Surface::kAddressSave, "address_save", {.dismissed_count = 2}},
      {Surface::kPaymentSave,
       "payment_save",
       {.shown_count = 5, .last_dismissed_time = now}},
  };
  static_assert(Surface::kMaxValue == Surface::kPaymentSave,
                "Add new surfaces to the list above.");

  for (const auto& test_case : kTestCases) {
    EXPECT_EQ(
        signin_prefs().GetCrossDevicePromoState(gaia_id, test_case.surface),
        State());
  }
  for (const auto& test_case : kTestCases) {
    signin_prefs().SetCrossDevicePromoState(gaia_id, test_case.surface,
                                            test_case.state);
  }

  const base::DictValue* raw_cross_device_dict =
      pref_service()
          .GetDict(kSigninAccountPrefs)
          .FindDict(gaia_id.ToString())
          ->FindDict("CrossDevicePromoPrefs");
  ASSERT_TRUE(raw_cross_device_dict);
  for (const auto& test_case : kTestCases) {
    SCOPED_TRACE(test_case.dict_key);
    EXPECT_EQ(
        signin_prefs().GetCrossDevicePromoState(gaia_id, test_case.surface),
        test_case.state);

    base::DictValue expected_dict;
    expected_dict.Set("shown_count", test_case.state.shown_count);
    expected_dict.Set("dismissed_count", test_case.state.dismissed_count);
    if (test_case.state.last_dismissed_time) {
      expected_dict.Set(
          "last_dismissed_time",
          base::TimeToValue(*test_case.state.last_dismissed_time));
    }
    const base::DictValue* raw_dict =
        raw_cross_device_dict->FindDict(test_case.dict_key);
    ASSERT_TRUE(raw_dict);
    EXPECT_EQ(*raw_dict, expected_dict);
  }

  // Writing a state without `last_dismissed_time` clears it.
  signin_prefs().SetCrossDevicePromoState(gaia_id, Surface::kBookmarkSave,
                                          {.shown_count = 2});
  EXPECT_EQ(
      signin_prefs().GetCrossDevicePromoState(gaia_id, Surface::kBookmarkSave),
      State({.shown_count = 2}));
}

// `CrossDevicePromoPrefs.history.shown_after_dismissal` was replaced by
// `dismissed_count`: users who reached the previous limit (shown once after a
// dismissal) stay capped, and a past dismissal still counts.
TEST_F(SigninPrefsTest, MigrateCrossDeviceHistoryPromoShownAfterDismissal) {
  const GaiaId capped_gaia_id("capped_gaia_id");
  const GaiaId dismissed_gaia_id("dismissed_gaia_id");
  const GaiaId shown_gaia_id("shown_gaia_id");
  const GaiaId migrated_gaia_id("migrated_gaia_id");
  const base::Time time = base::Time::FromSecondsSinceUnixEpoch(1700000000);
  {
    ScopedDictPrefUpdate update(&pref_service(), kSigninAccountPrefs);
    base::DictValue* capped_history =
        update->EnsureDict(capped_gaia_id.ToString())
            ->EnsureDict("CrossDevicePromoPrefs")
            ->EnsureDict("history");
    capped_history->Set("shown_count", 2);
    capped_history->Set("last_dismissed_time", base::TimeToValue(time));
    capped_history->Set("shown_after_dismissal", true);

    base::DictValue* dismissed_history =
        update->EnsureDict(dismissed_gaia_id.ToString())
            ->EnsureDict("CrossDevicePromoPrefs")
            ->EnsureDict("history");
    dismissed_history->Set("shown_count", 1);
    dismissed_history->Set("last_dismissed_time", base::TimeToValue(time));
    dismissed_history->Set("shown_after_dismissal", false);

    base::DictValue* shown_history =
        update->EnsureDict(shown_gaia_id.ToString())
            ->EnsureDict("CrossDevicePromoPrefs")
            ->EnsureDict("history");
    shown_history->Set("shown_count", 3);
    shown_history->Set("shown_after_dismissal", false);

    // Already migrated data with a higher count is left as is.
    base::DictValue* migrated_history =
        update->EnsureDict(migrated_gaia_id.ToString())
            ->EnsureDict("CrossDevicePromoPrefs")
            ->EnsureDict("history");
    migrated_history->Set("dismissed_count", 3);
    migrated_history->Set("shown_after_dismissal", true);
  }

  signin_prefs().MigrateObsoleteSigninPrefs();

  using State = SigninPrefs::CrossDevicePromoState;
  constexpr auto kHistory = SigninPrefs::CrossDevicePromoSurface::kHistory;
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(capped_gaia_id, kHistory),
            State({.shown_count = 2,
                   .dismissed_count = 2,
                   .last_dismissed_time = time}));
  EXPECT_EQ(
      signin_prefs().GetCrossDevicePromoState(dismissed_gaia_id, kHistory),
      State({.shown_count = 1,
             .dismissed_count = 1,
             .last_dismissed_time = time}));
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(shown_gaia_id, kHistory),
            State({.shown_count = 3}));
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(migrated_gaia_id, kHistory),
            State({.dismissed_count = 3}));

  // The deprecated key is removed from all accounts.
  for (const GaiaId& gaia_id :
       {capped_gaia_id, dismissed_gaia_id, shown_gaia_id, migrated_gaia_id}) {
    const base::DictValue* raw_history = pref_service()
                                             .GetDict(kSigninAccountPrefs)
                                             .FindDict(gaia_id.ToString())
                                             ->FindDict("CrossDevicePromoPrefs")
                                             ->FindDict("history");
    ASSERT_TRUE(raw_history);
    EXPECT_FALSE(raw_history->contains("shown_after_dismissal"));
  }
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
  cross_device_history_dict.Set("dismissed_count", 1);
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

  EXPECT_EQ(
      signin_prefs().GetCrossDevicePromoState(
          gaia_id, SigninPrefs::CrossDevicePromoSurface::kHistory),
      SigninPrefs::CrossDevicePromoState({.shown_count = 11,
                                          .dismissed_count = 1,
                                          .last_dismissed_time = test_time}));

  // Now verify the reverse: mutating through typed accessors updates only the
  // targeted leaf keys at the exact expected paths while preserving all sibling
  // keys (including unknown keys under `history`).
  signin_prefs().IncrementAvatarButtonHistorySyncPromoShownCount(gaia_id);
  signin_prefs().SetCrossDevicePromoState(
      gaia_id, SigninPrefs::CrossDevicePromoSurface::kHistory,
      {.shown_count = 12, .dismissed_count = 1});

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

  // Reading nested promo prefs for a non-existent account must not create an
  // account dictionary entry or notify pref observers.
  EXPECT_CALL(mock_observer, Run()).Times(0);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            0);
  EXPECT_FALSE(signin_prefs()
                   .GetAvatarButtonSigninPromoLastShownTime(gaia_id)
                   .has_value());
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(
                gaia_id, SigninPrefs::CrossDevicePromoSurface::kHistory),
            SigninPrefs::CrossDevicePromoState());
  EXPECT_FALSE(HasAccountPrefs(gaia_id));
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Each write operation notifies pref observers once and creates the account
  // entry.
  EXPECT_CALL(mock_observer, Run()).Times(1);
  signin_prefs().IncrementAvatarButtonHistorySyncPromoShownCount(gaia_id);
  EXPECT_TRUE(HasAccountPrefs(gaia_id));
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Subsequent reads on an existing account still do not notify.
  EXPECT_CALL(mock_observer, Run()).Times(0);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            1);
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(
                gaia_id, SigninPrefs::CrossDevicePromoSurface::kHistory),
            SigninPrefs::CrossDevicePromoState());
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
  constexpr auto kHistory = SigninPrefs::CrossDevicePromoSurface::kHistory;
  ASSERT_EQ(signin_prefs()
                .GetCrossDevicePromoState(gaia_id, kHistory)
                .last_dismissed_time,
            time);

  SigninPrefsAccessor accessor(pref_service(),
                               SigninPrefsAccessor::CreatePassKeyForTesting());
  EXPECT_FALSE(accessor.ClearPref(
      gaia_id, signin::internal::kCrossDevicePromoLastDismissedTimeKey,
      signin::internal::kCrossDeviceHistoryPromoParents));

  EXPECT_EQ(signin_prefs()
                .GetCrossDevicePromoState(gaia_id, kHistory)
                .last_dismissed_time,
            time);
  EXPECT_FALSE(pref_service().GetUserPrefValue(kSigninAccountPrefs));
}

// Corrupted on-disk data: the accounts pref itself is not a dict. Clearing must
// be a no-op and must not assert (`PrefService::GetUserPrefValue()` would).
TEST_F(SigninPrefsTest, ClearPrefIgnoresNonDictAccountsPref) {
  const GaiaId gaia_id("gaia_id_corrupted");
  pref_service().SetUserPref(kSigninAccountPrefs, base::Value(42));

  SigninPrefsAccessor accessor(pref_service(),
                               SigninPrefsAccessor::CreatePassKeyForTesting());
  EXPECT_FALSE(accessor.ClearPref(
      gaia_id, signin::internal::kCrossDevicePromoLastDismissedTimeKey,
      signin::internal::kCrossDeviceHistoryPromoParents));
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

  // Reads should return default values without crashing.
  constexpr auto kHistory = SigninPrefs::CrossDevicePromoSurface::kHistory;
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            0);
  EXPECT_FALSE(signin_prefs()
                   .GetAvatarButtonSigninPromoLastShownTime(gaia_id)
                   .has_value());
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(gaia_id, kHistory),
            SigninPrefs::CrossDevicePromoState());

  // Also test depth-2 malformed intermediate: "CrossDevicePromoPrefs" is a
  // valid dict, but "history" is a non-dict.
  {
    ScopedDictPrefUpdate update(&pref_service(), kSigninAccountPrefs);
    base::DictValue* account_dict = update->EnsureDict(gaia_id.ToString());
    base::DictValue* cross_device_dict =
        account_dict->EnsureDict("CrossDevicePromoPrefs");
    cross_device_dict->Set("history", "not_a_dict");
  }
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(gaia_id, kHistory),
            SigninPrefs::CrossDevicePromoState());

  // Writes should overwrite malformed intermediates with dictionaries.
  signin_prefs().IncrementAvatarButtonHistorySyncPromoShownCount(gaia_id);
  signin_prefs().SetCrossDevicePromoState(gaia_id, kHistory,
                                          {.shown_count = 5});
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            1);
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(gaia_id, kHistory),
            SigninPrefs::CrossDevicePromoState({.shown_count = 5}));
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
  constexpr auto kHistory = SigninPrefs::CrossDevicePromoSurface::kHistory;
  EXPECT_FALSE(HasAccountPrefs(gaia_id));
  EXPECT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(gaia_id), 0);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            0);
  EXPECT_FALSE(signin_prefs().GetExtensionsExplicitBrowserSignin(gaia_id));
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(gaia_id, kHistory),
            SigninPrefs::CrossDevicePromoState());
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionUserChoice(gaia_id),
            ChromeSigninUserChoice::kNoChoice);

  // Clears are a no-op and leave the corrupted value untouched.
  signin_prefs().ClearChromeSigninBubbleRepromptCount(gaia_id);
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
  signin_prefs().SetCrossDevicePromoState(gaia_id, kHistory,
                                          {.last_dismissed_time = time});
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            1);
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(gaia_id, kHistory),
            SigninPrefs::CrossDevicePromoState({.last_dismissed_time = time}));
  expected_account->EnsureDict("AvatarButtonPromoCountDictionary")
      ->Set("AvatarButtonHistorySyncPromoShownCount", 1);
  base::DictValue* expected_history =
      expected_account->EnsureDict("CrossDevicePromoPrefs")
          ->EnsureDict("history");
  expected_history->Set("shown_count", 0);
  expected_history->Set("dismissed_count", 0);
  expected_history->Set("last_dismissed_time", base::TimeToValue(time));
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
  constexpr auto kHistory = SigninPrefs::CrossDevicePromoSurface::kHistory;
  EXPECT_EQ(signin_prefs().GetChromeSigninBubbleRepromptCount(gaia_id), 0);
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            0);
  EXPECT_FALSE(signin_prefs().GetExtensionsExplicitBrowserSignin(gaia_id));
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(gaia_id, kHistory),
            SigninPrefs::CrossDevicePromoState());
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionUserChoice(gaia_id),
            ChromeSigninUserChoice::kNoChoice);

  // Clears are a no-op and leave the corrupted entry untouched.
  signin_prefs().ClearChromeSigninBubbleRepromptCount(gaia_id);
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
  signin_prefs().SetCrossDevicePromoState(gaia_id, kHistory,
                                          {.last_dismissed_time = time});
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionUserChoice(gaia_id),
            ChromeSigninUserChoice::kSignin);
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(gaia_id, kHistory),
            SigninPrefs::CrossDevicePromoState({.last_dismissed_time = time}));
  expected_account->Set("ChromeSigninInterceptionUserChoice",
                        static_cast<int>(ChromeSigninUserChoice::kSignin));
  base::DictValue* expected_history =
      expected_account->EnsureDict("CrossDevicePromoPrefs")
          ->EnsureDict("history");
  expected_history->Set("shown_count", 0);
  expected_history->Set("dismissed_count", 0);
  expected_history->Set("last_dismissed_time", base::TimeToValue(time));
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
  constexpr auto kHistory = SigninPrefs::CrossDevicePromoSurface::kHistory;
  EXPECT_EQ(signin_prefs().GetAvatarButtonHistorySyncPromoShownCount(gaia_id),
            0);
  EXPECT_FALSE(signin_prefs()
                   .GetAvatarButtonSigninPromoLastShownTime(gaia_id)
                   .has_value());
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(gaia_id, kHistory),
            SigninPrefs::CrossDevicePromoState());
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionUserChoice(gaia_id),
            ChromeSigninUserChoice::kSignin);
  EXPECT_EQ(pref_service().GetDict(kSigninAccountPrefs), expected_accounts);

  // A write under `CrossDevicePromoPrefs` replaces only that intermediate with
  // a dict. The other corrupted intermediate and the sibling key are untouched.
  signin_prefs().SetCrossDevicePromoState(gaia_id, kHistory,
                                          {.shown_count = 5});
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(gaia_id, kHistory),
            SigninPrefs::CrossDevicePromoState({.shown_count = 5}));
  expected_account->Remove("CrossDevicePromoPrefs");
  base::DictValue* expected_history =
      expected_account->EnsureDict("CrossDevicePromoPrefs")
          ->EnsureDict("history");
  expected_history->Set("shown_count", 5);
  expected_history->Set("dismissed_count", 0);
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
  // contains only the written keys.
  {
    ScopedDictPrefUpdate update(&pref_service(), kSigninAccountPrefs);
    update->FindDict(gaia_id.ToString())
        ->FindDict("CrossDevicePromoPrefs")
        ->Set("history", "not_a_dict");
  }
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(gaia_id, kHistory),
            SigninPrefs::CrossDevicePromoState());
  signin_prefs().SetCrossDevicePromoState(gaia_id, kHistory,
                                          {.last_dismissed_time = time});
  EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(gaia_id, kHistory),
            SigninPrefs::CrossDevicePromoState({.last_dismissed_time = time}));
  expected_history =
      expected_account->FindDict("CrossDevicePromoPrefs")->FindDict("history");
  expected_history->Set("shown_count", 0);
  expected_history->Set("last_dismissed_time", base::TimeToValue(time));
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

  // Multi-account prefs live in a separate registry so single-account
  // consumers (`GetAll()`, `Find()`) never surface them.
  EXPECT_EQ(SigninPrefsRegistry::Find(
                pass_key, {},
                signin::internal::kAccountSwitchingPromoExplicitDismissalCount),
            nullptr);
  EXPECT_EQ(SigninPrefsRegistry::Find(
                pass_key, {},
                signin::internal::kAccountSwitchingPromoSuppressedBySwitch),
            nullptr);

  base::span<const SigninPrefsRegistry::PrefDescriptor> multi_prefs =
      SigninPrefsRegistry::GetAllMultipleAccounts(pass_key);
  EXPECT_FALSE(multi_prefs.empty());
  for (const auto& pref : multi_prefs) {
    EXPECT_FALSE(pref.key.empty());
    EXPECT_NE(pref.type, base::Value::Type::NONE);
    EXPECT_EQ(SigninPrefsRegistry::FindMultipleAccounts(
                  pass_key, pref.parent_keys(), pref.key),
              &pref);
  }
  EXPECT_EQ(SigninPrefsRegistry::FindMultipleAccounts(
                pass_key, {}, signin::internal::kPasswordSignInPromoShownCount),
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

  // Unknown preference key or multi-account key: always rejected on the
  // single-account validator.
  EXPECT_FALSE(SigninPrefsRegistry::IsValidValue(pass_key, {}, "UnknownPrefKey",
                                                 base::Value(1)));
  EXPECT_FALSE(SigninPrefsRegistry::IsValidValue(
      pass_key, {},
      signin::internal::kAccountSwitchingPromoExplicitDismissalCount,
      base::Value(1)));
  EXPECT_FALSE(SigninPrefsRegistry::IsValidValue(
      pass_key, {}, signin::internal::kAccountSwitchingPromoSuppressedBySwitch,
      base::Value(true)));

  EXPECT_TRUE(SigninPrefsRegistry::IsValidMultipleAccountsValue(
      pass_key, {},
      signin::internal::kAccountSwitchingPromoExplicitDismissalCount,
      base::Value(1)));
  EXPECT_FALSE(SigninPrefsRegistry::IsValidMultipleAccountsValue(
      pass_key, {},
      signin::internal::kAccountSwitchingPromoExplicitDismissalCount,
      base::Value(true)));
  EXPECT_FALSE(SigninPrefsRegistry::IsValidMultipleAccountsValue(
      pass_key, {}, signin::internal::kPasswordSignInPromoShownCount,
      base::Value(1)));
}

namespace {

using PrefPath = std::pair<std::vector<std::string>, std::string>;

void VerifyAndCollectDictEntriesAreRegistered(
    const base::DictValue& dict,
    std::vector<std::string_view> parents,
    bool is_multiple_accounts,
    std::set<PrefPath>* observed_paths) {
  auto pass_key = SigninPrefsRegistry::CreatePassKeyForTesting();
  for (const auto [key, value] : dict) {
    observed_paths->insert(
        {std::vector<std::string>(parents.begin(), parents.end()), key});
    const SigninPrefsRegistry::PrefDescriptor* pref_desc =
        is_multiple_accounts
            ? SigninPrefsRegistry::FindMultipleAccounts(pass_key, parents, key)
            : SigninPrefsRegistry::Find(pass_key, parents, key);
    if (value.is_dict()) {
      EXPECT_NE(pref_desc, nullptr) << "Unregistered dict: " << key;
      if (pref_desc) {
        EXPECT_EQ(pref_desc->type, base::Value::Type::DICT);
      }
      std::vector<std::string_view> child_parents = parents;
      child_parents.push_back(key);
      VerifyAndCollectDictEntriesAreRegistered(
          value.GetDict(), child_parents, is_multiple_accounts, observed_paths);
    } else {
      const bool valid =
          is_multiple_accounts
              ? SigninPrefsRegistry::IsValidMultipleAccountsValue(
                    pass_key, parents, key, value)
              : SigninPrefsRegistry::IsValidValue(pass_key, parents, key,
                                                  value);
      EXPECT_TRUE(valid) << "Unregistered or type-mismatched key: " << key;
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
  signin_prefs().SetChromeLastSigninTime(gaia_id, now);
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
#if BUILDFLAG(IS_ANDROID)
  signin_prefs().SetDeviceSignalsPermanentConsentReceived(gaia_id, true);
#endif  // BUILDFLAG(IS_ANDROID)
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

  // Multi-account AccountSwitchingPromo prefs via typed SigninPrefs accessors.
  const GaiaId second_gaia_id("gaia_id_all_prefs_second");
  signin_prefs().IncrementAccountSwitchingPromoExplicitDismissalCount(
      gaia_id, second_gaia_id);
  signin_prefs().SetAccountSwitchingPromoSuppressedBySwitch(gaia_id,
                                                            second_gaia_id);

  // CrossDevicePromoPrefs prefs via typed SigninPrefs accessors, for every
  // surface.
  constexpr auto kAllCrossDevicePromoSurfaces =
      std::to_array<SigninPrefs::CrossDevicePromoSurface>({
          SigninPrefs::CrossDevicePromoSurface::kHistory,
          SigninPrefs::CrossDevicePromoSurface::kBookmarkSave,
          SigninPrefs::CrossDevicePromoSurface::kPasswordSave,
          SigninPrefs::CrossDevicePromoSurface::kAddressSave,
          SigninPrefs::CrossDevicePromoSurface::kPaymentSave,
      });
  static_assert(SigninPrefs::CrossDevicePromoSurface::kMaxValue ==
                    SigninPrefs::CrossDevicePromoSurface::kPaymentSave,
                "Add new surfaces to the list above.");
  const SigninPrefs::CrossDevicePromoState cross_device_promo_state = {
      .shown_count = 2, .dismissed_count = 1, .last_dismissed_time = now};
  for (SigninPrefs::CrossDevicePromoSurface surface :
       kAllCrossDevicePromoSurfaces) {
    signin_prefs().SetCrossDevicePromoState(gaia_id, surface,
                                            cross_device_promo_state);
  }

  // Verify all typed getters return the expected values and succeed.
  EXPECT_EQ(signin_prefs().GetChromeSigninInterceptionUserChoice(gaia_id),
            ChromeSigninUserChoice::kSignin);
  EXPECT_EQ(signin_prefs().GetAccountMetricsId(gaia_id), 7);
  EXPECT_TRUE(signin_prefs().IsAccountMetricsIdCapped(gaia_id));
  EXPECT_EQ(signin_prefs().GetChromeLastSigninTime(gaia_id), now);
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
#if BUILDFLAG(IS_ANDROID)
  EXPECT_TRUE(signin_prefs().GetDeviceSignalsPermanentConsentReceived(gaia_id));
#endif  // BUILDFLAG(IS_ANDROID)
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

  EXPECT_EQ(signin_prefs().GetAccountSwitchingPromoExplicitDismissalCount(
                second_gaia_id, gaia_id),
            1);
  EXPECT_TRUE(signin_prefs().IsAccountSwitchingPromoSuppressedBySwitch(
      second_gaia_id, gaia_id));

  for (SigninPrefs::CrossDevicePromoSurface surface :
       kAllCrossDevicePromoSurfaces) {
    EXPECT_EQ(signin_prefs().GetCrossDevicePromoState(gaia_id, surface),
              cross_device_promo_state);
  }

  SigninPrefsAccessor accessor(pref_service(),
                               SigninPrefsAccessor::CreatePassKeyForTesting());
  const base::DictValue* account_dict =
      accessor.GetAccountPrefsDict().FindDict(gaia_id.ToString());
  ASSERT_NE(account_dict, nullptr);
  std::set<PrefPath> observed_single_paths;
  VerifyAndCollectDictEntriesAreRegistered(*account_dict, /*parents=*/{},
                                           /*is_multiple_accounts=*/false,
                                           &observed_single_paths);

  ASSERT_EQ(accessor.GetMultipleAccountsPrefsList().size(), 1u);
  const base::DictValue* multi_account_prefs =
      accessor.GetMultipleAccountsPrefsList()[0].GetDict().FindDict("prefs");
  ASSERT_NE(multi_account_prefs, nullptr);
  std::set<PrefPath> observed_multi_paths;
  VerifyAndCollectDictEntriesAreRegistered(*multi_account_prefs, /*parents=*/{},
                                           /*is_multiple_accounts=*/true,
                                           &observed_multi_paths);

  auto collect_registered =
      [](base::span<const SigninPrefsRegistry::PrefDescriptor> prefs) {
        std::set<PrefPath> paths;
        for (const auto& pref : prefs) {
          base::span<const std::string_view> parents = pref.parent_keys();
          paths.insert(
              {std::vector<std::string>(parents.begin(), parents.end()),
               std::string(pref.key)});
        }
        return paths;
      };

  auto pass_key = SigninPrefsRegistry::CreatePassKeyForTesting();
  EXPECT_EQ(observed_single_paths,
            collect_registered(SigninPrefsRegistry::GetAll(pass_key)));
  EXPECT_EQ(observed_multi_paths,
            collect_registered(
                SigninPrefsRegistry::GetAllMultipleAccounts(pass_key)));
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

  // Test ClearPrefForAllAccounts removes the key across multiple accounts in a
  // single ScopedDictPrefUpdate (notifying observers only once).
  const GaiaId gaia_id2("accessor_test_gaia_2");
  EXPECT_CALL(mock_observer, Run()).Times(2);
  accessor.SetTimePref(gaia_id, signin::internal::kChromeLastSigninTime, now);
  accessor.SetTimePref(gaia_id2, signin::internal::kChromeLastSigninTime, now);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_CALL(mock_observer, Run()).Times(1);
  EXPECT_EQ(
      accessor.ClearPrefForAllAccounts(signin::internal::kChromeLastSigninTime),
      2u);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_FALSE(
      accessor.GetTimePref(gaia_id, signin::internal::kChromeLastSigninTime)
          .has_value());
  EXPECT_FALSE(
      accessor.GetTimePref(gaia_id2, signin::internal::kChromeLastSigninTime)
          .has_value());

  // Subsequent ClearPrefForAllAccounts when no account has the key is a no-op
  // and does not notify observers.
  EXPECT_CALL(mock_observer, Run()).Times(0);
  EXPECT_EQ(
      accessor.ClearPrefForAllAccounts(signin::internal::kChromeLastSigninTime),
      0u);
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
  EXPECT_EQ(accessor.GetIntPref(
                gaia_id, signin::internal::kCrossDevicePromoDismissedCountKey,
                signin::internal::kCrossDeviceHistoryPromoParents),
            0);
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

TEST_F(SigninPrefsTest, MultipleAccountsPrefsAndRemovalCleanup) {
  const GaiaId account_a("gaia_a");
  const GaiaId account_b("gaia_b");
  const GaiaId account_c("gaia_c");
  const GaiaId account_d("gaia_d");

  // Lazy read returns defaults without creating list entries.
  EXPECT_EQ(signin_prefs().GetAccountSwitchingPromoExplicitDismissalCount(
                account_a, account_b),
            0);
  EXPECT_FALSE(signin_prefs().IsAccountSwitchingPromoSuppressedBySwitch(
      account_a, account_b));
  EXPECT_TRUE(pref_service().GetList(kMultipleAccountsMetadataList).empty());

  // Updating `{B, A}` then `{A, B}` resolves to the same canonical sorted
  // entry instead of creating duplicates.
  signin_prefs().SetChromeLastSigninTime(account_a, base::Time::Now());
  EXPECT_EQ(signin_prefs().IncrementAccountSwitchingPromoExplicitDismissalCount(
                account_b, account_a),
            1);
  EXPECT_EQ(signin_prefs().IncrementAccountSwitchingPromoExplicitDismissalCount(
                account_a, account_b),
            2);
  signin_prefs().SetAccountSwitchingPromoSuppressedBySwitch(account_c,
                                                            account_a);
  signin_prefs().SetAccountSwitchingPromoSuppressedBySwitch(account_d,
                                                            account_c);

  // Pair state is symmetric across argument order, scoped to `{A, B}`, and
  // isolated from `signin.accounts_metadata_dict`.
  EXPECT_EQ(signin_prefs().GetAccountSwitchingPromoExplicitDismissalCount(
                account_a, account_b),
            2);
  EXPECT_EQ(signin_prefs().GetAccountSwitchingPromoExplicitDismissalCount(
                account_b, account_a),
            2);
  EXPECT_FALSE(signin_prefs().IsAccountSwitchingPromoSuppressedBySwitch(
      account_a, account_b));
  EXPECT_TRUE(signin_prefs().IsAccountSwitchingPromoSuppressedBySwitch(
      account_a, account_c));
  EXPECT_TRUE(signin_prefs().IsAccountSwitchingPromoSuppressedBySwitch(
      account_c, account_a));
  EXPECT_FALSE(HasAccountPrefs(account_b));
  EXPECT_FALSE(HasAccountPrefs(account_c));
  ASSERT_EQ(pref_service().GetList(kMultipleAccountsMetadataList).size(), 3u);

  // Verify the canonical sorted `gaia_ids` list on disk even though `(B, A)`
  // was passed first.
  const base::DictValue& first_entry =
      pref_service().GetList(kMultipleAccountsMetadataList)[0].GetDict();
  EXPECT_FALSE(first_entry.contains("primary_gaia_id"));
  EXPECT_FALSE(first_entry.contains("target_gaia_ids"));
  const base::ListValue* first_gaia_ids = first_entry.FindList("gaia_ids");
  ASSERT_NE(first_gaia_ids, nullptr);
  ASSERT_EQ(first_gaia_ids->size(), 2u);
  EXPECT_EQ((*first_gaia_ids)[0].GetString(), account_a.ToString());
  EXPECT_EQ((*first_gaia_ids)[1].GetString(), account_b.ToString());

  // Verify unordered 3-account sets canonicalize to a single sorted entry.
  SigninPrefsAccessor accessor(pref_service(),
                               SigninPrefsAccessor::CreatePassKeyForTesting());
  EXPECT_EQ(accessor.IncrementMultipleAccountsIntPref(
                {account_c, account_a, account_b},
                signin::internal::kAccountSwitchingPromoExplicitDismissalCount),
            1);
  EXPECT_EQ(accessor.IncrementMultipleAccountsIntPref(
                {account_b, account_c, account_a},
                signin::internal::kAccountSwitchingPromoExplicitDismissalCount),
            2);
  EXPECT_EQ(accessor.GetMultipleAccountsIntPref(
                {account_a, account_b, account_c},
                signin::internal::kAccountSwitchingPromoExplicitDismissalCount),
            2);
  ASSERT_EQ(pref_service().GetList(kMultipleAccountsMetadataList).size(), 4u);
  const base::ListValue* triple_gaia_ids =
      pref_service()
          .GetList(kMultipleAccountsMetadataList)[3]
          .GetDict()
          .FindList("gaia_ids");
  ASSERT_NE(triple_gaia_ids, nullptr);
  ASSERT_EQ(triple_gaia_ids->size(), 3u);
  EXPECT_EQ((*triple_gaia_ids)[0].GetString(), account_a.ToString());
  EXPECT_EQ((*triple_gaia_ids)[1].GetString(), account_b.ToString());
  EXPECT_EQ((*triple_gaia_ids)[2].GetString(), account_c.ToString());

  // Append a malformed entry and an entry missing a `prefs` dict (even with
  // kept accounts) to verify cleanup drops invalid entries consistently.
  {
    ScopedListPrefUpdate update(&pref_service(), kMultipleAccountsMetadataList);
    update->Append(base::Value("malformed_entry"));

    base::DictValue missing_prefs_entry;
    base::ListValue kept_gaia_ids;
    kept_gaia_ids.Append(account_a.ToString());
    kept_gaia_ids.Append(account_c.ToString());
    missing_prefs_entry.Set("gaia_ids", std::move(kept_gaia_ids));
    update->Append(std::move(missing_prefs_entry));
  }
  ASSERT_EQ(pref_service().GetList(kMultipleAccountsMetadataList).size(), 6u);

  base::MockRepeatingClosure mock_observer;
  SigninPrefs::ObserveSigninPrefsChanges(pref_change_registrar(),
                                         mock_observer.Get());

  // Removing account B prunes `{A, B}`, `{A, B, C}`, and both invalid entries
  // in one update without touching A's single-account prefs or
  // `signin.accounts_metadata_dict`.
  EXPECT_CALL(mock_observer, Run()).Times(1);
  EXPECT_EQ(signin_prefs().RemoveAllAccountPrefsExcept(
                {account_a, account_c, account_d}),
            0u);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  EXPECT_EQ(signin_prefs().GetAccountSwitchingPromoExplicitDismissalCount(
                account_a, account_b),
            0);
  EXPECT_EQ(pref_service().GetList(kMultipleAccountsMetadataList).size(), 2u);
  EXPECT_TRUE(HasAccountPrefs(account_a));
  EXPECT_TRUE(signin_prefs().GetChromeLastSigninTime(account_a).has_value());
  EXPECT_TRUE(signin_prefs().IsAccountSwitchingPromoSuppressedBySwitch(
      account_a, account_c));
  EXPECT_TRUE(signin_prefs().IsAccountSwitchingPromoSuppressedBySwitch(
      account_c, account_d));

  // Subsequent cleanup with the same active set is a no-op and does not notify.
  EXPECT_CALL(mock_observer, Run()).Times(0);
  EXPECT_EQ(signin_prefs().RemoveAllAccountPrefsExcept(
                {account_a, account_c, account_d}),
            0u);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);

  // Removing account D prunes `{C, D}` while keeping `{A, C}`.
  EXPECT_CALL(mock_observer, Run()).Times(1);
  EXPECT_EQ(signin_prefs().RemoveAllAccountPrefsExcept({account_a, account_c}),
            0u);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_FALSE(signin_prefs().IsAccountSwitchingPromoSuppressedBySwitch(
      account_c, account_d));
  EXPECT_TRUE(signin_prefs().IsAccountSwitchingPromoSuppressedBySwitch(
      account_a, account_c));
  EXPECT_EQ(pref_service().GetList(kMultipleAccountsMetadataList).size(), 1u);

  // Removing account A deletes A's single-account dict and `{A, C}`, firing an
  // update per modified root pref (`signin.accounts_metadata_dict` and
  // `signin.multiple_accounts_metadata_list`).
  EXPECT_CALL(mock_observer, Run()).Times(2);
  EXPECT_EQ(signin_prefs().RemoveAllAccountPrefsExcept({account_c}), 1u);
  testing::Mock::VerifyAndClearExpectations(&mock_observer);
  EXPECT_FALSE(HasAccountPrefs(account_a));
  EXPECT_FALSE(signin_prefs().IsAccountSwitchingPromoSuppressedBySwitch(
      account_a, account_c));
  EXPECT_TRUE(pref_service().GetList(kMultipleAccountsMetadataList).empty());
}

TEST_F(SigninPrefsTest, MigrateObsoleteMultipleAccountsPrefs) {
  const GaiaId account_a("gaia_a");
  const GaiaId account_b("gaia_b");
  const GaiaId account_c("gaia_c");
  const GaiaId account_d("gaia_d");
  signin_prefs().SetAccountSwitchingPromoSuppressedBySwitch(account_a,
                                                            account_b);

  // Inject a deprecated key into the valid entry's `prefs` dict, an entry
  // containing only a deprecated key, an unsorted `gaia_ids` entry, a
  // single-account `gaia_ids` entry, and a malformed non-dict entry.
  {
    ScopedListPrefUpdate update(&pref_service(), kMultipleAccountsMetadataList);
    base::DictValue* valid_prefs = (*update)[0].GetDict().FindDict("prefs");
    ASSERT_NE(valid_prefs, nullptr);
    valid_prefs->Set(signin::internal::kDeprecatedTestingPref, 123);

    base::DictValue deprecated_only_entry;
    base::ListValue valid_gaia_ids;
    valid_gaia_ids.Append(account_c.ToString());
    valid_gaia_ids.Append(account_d.ToString());
    deprecated_only_entry.Set("gaia_ids", std::move(valid_gaia_ids));
    base::DictValue deprecated_prefs;
    deprecated_prefs.Set(signin::internal::kDeprecatedTestingPref, 456);
    deprecated_only_entry.Set("prefs", std::move(deprecated_prefs));
    update->Append(std::move(deprecated_only_entry));

    base::DictValue unsorted_entry;
    base::ListValue unsorted_gaia_ids;
    unsorted_gaia_ids.Append(account_b.ToString());
    unsorted_gaia_ids.Append(account_a.ToString());
    unsorted_entry.Set("gaia_ids", std::move(unsorted_gaia_ids));
    base::DictValue unsorted_prefs;
    unsorted_prefs.Set(
        signin::internal::kAccountSwitchingPromoSuppressedBySwitch, true);
    unsorted_entry.Set("prefs", std::move(unsorted_prefs));
    update->Append(std::move(unsorted_entry));

    base::DictValue too_few_entry;
    base::ListValue single_gaia_id;
    single_gaia_id.Append(account_a.ToString());
    too_few_entry.Set("gaia_ids", std::move(single_gaia_id));
    base::DictValue too_few_prefs;
    too_few_prefs.Set(
        signin::internal::kAccountSwitchingPromoSuppressedBySwitch, true);
    too_few_entry.Set("prefs", std::move(too_few_prefs));
    update->Append(std::move(too_few_entry));

    update->Append(base::Value("malformed_entry"));
  }
  ASSERT_EQ(pref_service().GetList(kMultipleAccountsMetadataList).size(), 5u);

  signin_prefs().MigrateObsoleteSigninPrefs();

  const base::ListValue& migrated_list =
      pref_service().GetList(kMultipleAccountsMetadataList);
  ASSERT_EQ(migrated_list.size(), 1u);
  EXPECT_TRUE(signin_prefs().IsAccountSwitchingPromoSuppressedBySwitch(
      account_b, account_a));
  const base::DictValue* remaining_prefs =
      migrated_list[0].GetDict().FindDict("prefs");
  ASSERT_NE(remaining_prefs, nullptr);
  EXPECT_FALSE(
      remaining_prefs->contains(signin::internal::kDeprecatedTestingPref));

  // Re-running multi-account migration when all entries are valid and contain
  // no deprecated keys must be a side-effect-free no-op.
  base::MockRepeatingClosure mock_observer;
  SigninPrefs::ObserveSigninPrefsChanges(pref_change_registrar(),
                                         mock_observer.Get());
  EXPECT_CALL(mock_observer, Run()).Times(0);
  SigninPrefsAccessor(pref_service(),
                      SigninPrefsAccessor::CreatePassKeyForTesting())
      .MigrateObsoleteMultipleAccountsPrefs();
}

TEST_F(SigninPrefsTest, MultipleAccountsCheckFailures) {
  const GaiaId account_a("gaia_a");
  const GaiaId account_b("gaia_b");
  SigninPrefsAccessor accessor(pref_service(),
                               SigninPrefsAccessor::CreatePassKeyForTesting());

  // Empty GaiaIds, fewer than two distinct GaiaIds, or duplicate inputs.
  EXPECT_CHECK_DEATH(
      signin_prefs().IncrementAccountSwitchingPromoExplicitDismissalCount(
          GaiaId(), account_a));
  EXPECT_CHECK_DEATH(
      signin_prefs().GetAccountSwitchingPromoExplicitDismissalCount(account_a,
                                                                    GaiaId()));
  EXPECT_CHECK_DEATH(signin_prefs().SetAccountSwitchingPromoSuppressedBySwitch(
      account_a, account_a));
  EXPECT_CHECK_DEATH(accessor.GetMultipleAccountsIntPref(
      /*gaia_ids=*/{},
      signin::internal::kAccountSwitchingPromoExplicitDismissalCount));
  EXPECT_CHECK_DEATH(accessor.IncrementMultipleAccountsIntPref(
      {account_b},
      signin::internal::kAccountSwitchingPromoExplicitDismissalCount));
  EXPECT_CHECK_DEATH(accessor.IncrementMultipleAccountsIntPref(
      {account_b, account_b},
      signin::internal::kAccountSwitchingPromoExplicitDismissalCount));

  // Cross-scope misuse is rejected.
  EXPECT_CHECK_DEATH(accessor.IncrementMultipleAccountsIntPref(
      {account_a, account_b},
      signin::internal::kChromeSigninInterceptionDismissCount));
  EXPECT_CHECK_DEATH(accessor.IncrementIntPref(
      account_a,
      signin::internal::kAccountSwitchingPromoExplicitDismissalCount));
  EXPECT_CHECK_DEATH(accessor.ClearPref(
      account_a, signin::internal::kAccountSwitchingPromoSuppressedBySwitch));
  EXPECT_CHECK_DEATH(accessor.ClearPrefForAllAccounts(
      signin::internal::kAccountSwitchingPromoSuppressedBySwitch));
}
