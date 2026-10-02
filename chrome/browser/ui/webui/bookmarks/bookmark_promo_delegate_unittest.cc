// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/bookmarks/bookmark_promo_delegate.h"

#include <memory>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service.h"
#include "chrome/browser/profiles/batch_upload/batch_upload_service_test_helper.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/signin/core/browser/test_account_preview_data_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_prefs.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/features.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/service/sync_prefs.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

class BookmarkPromoDelegateTestBase : public testing::Test {
 public:
  BookmarkPromoDelegateTestBase() {
    SigninPrefs::RegisterProfilePrefs(pref_service_.registry());
    syncer::SyncPrefs::RegisterProfilePrefs(pref_service_.registry());
    pref_service_.registry()->RegisterBooleanPref(prefs::kSigninAllowed, true);
  }

 protected:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  sync_preferences::TestingPrefServiceSyncable pref_service_;
  signin::IdentityTestEnvironment identity_test_env_;
  syncer::TestSyncService sync_service_;
};

class BatchUploadPromoDelegateTest : public BookmarkPromoDelegateTestBase {
 public:
  BatchUploadPromoDelegateTest() {
    std::vector<base::test::FeatureRef> enabled_features = {
        syncer::kReplaceSyncPromosWithSignInPromos};
#if !BUILDFLAG(IS_CHROMEOS)
    enabled_features.push_back(syncer::kUnoPhase2FollowUp);
#endif
    feature_list_.InitWithFeatures(enabled_features, {});
    batch_upload_service_ = batch_upload_test_helper_.CreateBatchUploadService(
        identity_test_env_.identity_manager(), &pref_service_);
  }

  std::unique_ptr<BatchUploadPromoDelegate> CreateDelegate() {
    return std::make_unique<BatchUploadPromoDelegate>(
        &pref_service_, identity_test_env_.identity_manager(), &sync_service_,
        batch_upload_service_.get());
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  BatchUploadServiceTestHelper batch_upload_test_helper_;
  std::unique_ptr<BatchUploadService> batch_upload_service_;
};

TEST_F(BatchUploadPromoDelegateTest, CanShowAndGetPromoDataWithLocalBookmarks) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);
  batch_upload_test_helper_.SetReturnDescriptions(syncer::BOOKMARKS, 2);

  auto delegate = CreateDelegate();
  EXPECT_TRUE(delegate->CanShowPromo());

  base::test::TestFuture<BookmarkPromoData> future;
  delegate->GetPromoData(future.GetCallback());
  const BookmarkPromoData& data = future.Get();
  EXPECT_TRUE(data.can_show);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BATCH_UPLOAD_PROMO_TITLE),
            data.promo_title);
  EXPECT_EQ(
      l10n_util::GetStringUTF16(IDS_BATCH_UPLOAD_PROMO_TITLE_OK_BUTTON_LABEL),
      data.action_button_text);
}

TEST_F(BatchUploadPromoDelegateTest, NotShownWithoutLocalBookmarks) {
  identity_test_env_.MakePrimaryAccountAvailable("user@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  auto delegate = CreateDelegate();
  EXPECT_TRUE(delegate->CanShowPromo());

  base::test::TestFuture<BookmarkPromoData> future;
  delegate->GetPromoData(future.GetCallback());
  EXPECT_FALSE(future.Get().can_show);
}

TEST_F(BatchUploadPromoDelegateTest, NotShownWhenSignedOut) {
  batch_upload_test_helper_.SetReturnDescriptions(syncer::BOOKMARKS, 2);

  auto delegate = CreateDelegate();
  EXPECT_FALSE(delegate->CanShowPromo());
}

TEST_F(BatchUploadPromoDelegateTest, DismissCountAndCooldown) {
  AccountInfo account = identity_test_env_.MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  batch_upload_test_helper_.SetReturnDescriptions(syncer::BOOKMARKS, 2);

  auto delegate = CreateDelegate();
  EXPECT_TRUE(delegate->CanShowPromo());

  // Dismissing enters the 7-day cooldown period.
  delegate->OnPromoDismissed();
  EXPECT_FALSE(delegate->CanShowPromo());

  // After 8 days, the promo can be shown again (up to 3 dismissals).
  task_environment_.FastForwardBy(base::Days(8));
  EXPECT_TRUE(delegate->CanShowPromo());

  // Exceeding 3 dismissals permanently disables the promo.
  for (int i = 0; i < 3; ++i) {
    delegate->OnPromoDismissed();
    task_environment_.FastForwardBy(base::Days(8));
  }
  EXPECT_FALSE(delegate->CanShowPromo());
}

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
class AccountAwareSignInPromoDelegateTest
    : public BookmarkPromoDelegateTestBase {
 public:
  std::unique_ptr<AccountAwareSignInPromoDelegate> CreateDelegate() {
    return std::make_unique<AccountAwareSignInPromoDelegate>(
        &pref_service_, identity_test_env_.identity_manager(), &sync_service_,
        &preview_service_);
  }

  AccountInfo SetUpWebSignedInPreferredAccount(bool include_bookmarks) {
    AccountInfo account_info =
        identity_test_env_.MakeAccountAvailable("camille.c.walsh@gmail.com");
    identity_test_env_.SetCookieAccounts(
        {{std::string(account_info.GetEmail()), account_info.GetGaiaId()}});
    account_info = signin::WithGeneratedUserInfo(account_info, "Camille");
    identity_test_env_.UpdateAccountInfoForAccount(account_info);

    signin::AccountPreviewDataService::AccountPreviewPreference pref;
    pref.gaia_id = account_info.GetGaiaId();
    if (include_bookmarks) {
      pref.preferred_data_types = {
          {.data_type = syncer::BOOKMARKS,
           .quartile = signin::SyncDataQuartile::kAboveQ3}};
    } else {
      pref.preferred_data_types = {
          {.data_type = syncer::PASSWORDS,
           .quartile = signin::SyncDataQuartile::kAboveQ3}};
    }
    preview_service_.SetPreferredAccountForPromo(pref);
    return account_info;
  }

 protected:
  base::test::ScopedFeatureList feature_list_{
      switches::kEnableAccountPreviewPreferredAccountFollowup};
  signin::TestAccountPreviewDataService preview_service_;
};

TEST_F(AccountAwareSignInPromoDelegateTest,
       CanShowAndGetPromoDataWithPreferredAccountBookmarks) {
  SetUpWebSignedInPreferredAccount(/*include_bookmarks=*/true);

  auto delegate = CreateDelegate();
  EXPECT_TRUE(delegate->CanShowPromo());

  base::test::TestFuture<BookmarkPromoData> future;
  delegate->GetPromoData(future.GetCallback());
  const BookmarkPromoData& data = future.Get();
  EXPECT_TRUE(data.can_show);
  EXPECT_EQ(l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_SIGNIN_PROMO_TITLE),
            data.promo_title);
  EXPECT_EQ(
      l10n_util::GetStringFUTF16(IDS_BOOKMARK_MANAGER_SIGNIN_PROMO_SUBTITLE,
                                 u"camille.c.walsh@gmail.com"),
      data.promo_subtitle);
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_PROFILES_DICE_WEB_ONLY_SIGNIN_BUTTON,
                                       u"Camille"),
            data.action_button_text);
  EXPECT_FALSE(data.promo_avatar_url.empty());
}

TEST_F(AccountAwareSignInPromoDelegateTest,
       GetPromoDataTitleWithOtherDeviceFormFactor) {
  SetUpWebSignedInPreferredAccount(/*include_bookmarks=*/true);
  auto pref = *preview_service_.GetPreferredAccountForPromo();
  pref.other_device_info = signin::PreferredDeviceInfo{
      .form_factor =
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE,
      .enabled_data_types = {syncer::BOOKMARKS},
  };
  preview_service_.SetPreferredAccountForPromo(pref);

  auto delegate = CreateDelegate();
  base::test::TestFuture<BookmarkPromoData> future;
  delegate->GetPromoData(future.GetCallback());
  const BookmarkPromoData& data = future.Get();
  EXPECT_TRUE(data.can_show);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_BOOKMARK_MANAGER_SIGNIN_PROMO_TITLE_WITH_DEVICE,
                l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_PHONE)),
            data.promo_title);
}

TEST_F(AccountAwareSignInPromoDelegateTest, NotShownWhenSignedIn) {
  SetUpWebSignedInPreferredAccount(/*include_bookmarks=*/true);
  identity_test_env_.MakePrimaryAccountAvailable("camille.c.walsh@gmail.com",
                                                 signin::ConsentLevel::kSignin);

  auto delegate = CreateDelegate();
  EXPECT_FALSE(delegate->CanShowPromo());
}

TEST_F(AccountAwareSignInPromoDelegateTest,
       NotShownWhenPreferredAccountHasNoBookmarks) {
  SetUpWebSignedInPreferredAccount(/*include_bookmarks=*/false);

  auto delegate = CreateDelegate();
  EXPECT_FALSE(delegate->CanShowPromo());
}

TEST_F(AccountAwareSignInPromoDelegateTest, MaxShownCountLimit) {
  base::HistogramTester histogram_tester;
  AccountInfo account_info =
      SetUpWebSignedInPreferredAccount(/*include_bookmarks=*/true);
  SigninPrefs signin_prefs(pref_service_);

  for (int i = 0; i < 5; ++i) {
    auto delegate = CreateDelegate();
    EXPECT_TRUE(delegate->CanShowPromo());
    base::test::TestFuture<BookmarkPromoData> future;
    delegate->GetPromoData(future.GetCallback());
    EXPECT_TRUE(future.Get().can_show);
    delegate->OnPromoShown();
    EXPECT_EQ(i + 1, signin_prefs.GetBookmarkManagerSigninPromoImpressionCount(
                         account_info.GetGaiaId()));
    histogram_tester.ExpectUniqueSample(
        "Signin.SignIn.Offered", signin_metrics::AccessPoint::kBookmarkManager,
        i + 1);
    histogram_tester.ExpectUniqueSample(
        "Signin.SignIn.Offered.WithDefault",
        signin_metrics::AccessPoint::kBookmarkManager, i + 1);
  }

  auto sixth_delegate = CreateDelegate();
  EXPECT_FALSE(sixth_delegate->CanShowPromo());
}

TEST_F(AccountAwareSignInPromoDelegateTest, MaxDismissCountLimit) {
  AccountInfo account_info =
      SetUpWebSignedInPreferredAccount(/*include_bookmarks=*/true);
  SigninPrefs signin_prefs(pref_service_);

  for (int i = 0; i < 2; ++i) {
    auto delegate = CreateDelegate();
    EXPECT_TRUE(delegate->CanShowPromo());
    delegate->OnPromoDismissed();
    EXPECT_FALSE(delegate->CanShowPromo());
    EXPECT_EQ(i + 1, signin_prefs.GetBookmarkManagerSigninPromoDismissCount(
                         account_info.GetGaiaId()));
  }

  auto third_delegate = CreateDelegate();
  EXPECT_FALSE(third_delegate->CanShowPromo());
}
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)

}  // namespace
