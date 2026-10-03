// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/password_manager/notification_cards/account_aware_password_signin_promo.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/signin/account_preview_data_service_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/signin/core/browser/account_preview_data_service.h"
#include "components/signin/core/browser/test_account_preview_data_service.h"
#include "components/signin/public/base/signin_pref_names.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/base/data_type.h"
#include "components/sync/protocol/sync_enums.pb.h"
#include "components/sync/test/test_sync_service.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

std::unique_ptr<KeyedService> CreateTestSyncService(
    content::BrowserContext* context) {
  auto sync_service = std::make_unique<syncer::TestSyncService>();
  sync_service->SetSignedOut();
  return sync_service;
}

std::unique_ptr<KeyedService> CreateTestAccountPreviewDataService(
    content::BrowserContext* context) {
  return std::make_unique<signin::TestAccountPreviewDataService>();
}

}  // namespace

class NotificationCardAccountAwarePasswordSigninPromoTest
    : public ChromeRenderViewHostTestHarness {
 public:
  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
            {TestingProfile::TestingFactory{
                 ChromeSigninClientFactory::GetInstance(),
                 base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                     &test_url_loader_factory_)},
             TestingProfile::TestingFactory{
                 SyncServiceFactory::GetInstance(),
                 base::BindRepeating(&CreateTestSyncService)},
             TestingProfile::TestingFactory{
                 AccountPreviewDataServiceFactory::GetInstance(),
                 base::BindRepeating(&CreateTestAccountPreviewDataService)}});
  }

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());
    identity_test_env_adaptor_->identity_test_env()->SetTestURLLoaderFactory(
        &test_url_loader_factory_);
    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
    preview_service_ = static_cast<signin::TestAccountPreviewDataService*>(
        AccountPreviewDataServiceFactory::GetForProfile(profile()));
  }

  void TearDown() override {
    preview_service_ = nullptr;
    sync_service_ = nullptr;
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  AccountInfo SetUpWebSignedInPreferredAccount(
      bool include_passwords,
      sync_pb::SyncEnums_DeviceFormFactor form_factor =
          sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_UNSPECIFIED) {
    AccountInfo account_info =
        identity_test_env_adaptor_->identity_test_env()->MakeAccountAvailable(
            "camille.c.walsh@gmail.com", {.set_cookie = true});
    account_info = signin::WithGeneratedUserInfo(account_info, "Camille");
    identity_test_env_adaptor_->identity_test_env()
        ->UpdateAccountInfoForAccount(account_info);

    signin::AccountPreviewDataService::AccountPreviewPreference pref;
    pref.gaia_id = account_info.GetGaiaId();
    pref.other_device_info.form_factor = form_factor;
    pref.other_device_info.enabled_data_types.Put(syncer::PASSWORDS);
    if (include_passwords) {
      pref.preferred_data_types = {
          {.data_type = syncer::PASSWORDS,
           .quartile = signin::SyncDataQuartile::kAboveQ3}};
    } else {
      pref.preferred_data_types = {
          {.data_type = syncer::BOOKMARKS,
           .quartile = signin::SyncDataQuartile::kAboveQ3}};
    }
    preview_service_->SetPreferredAccountForPromo(pref);
    return account_info;
  }

 protected:
  base::test::ScopedFeatureList feature_list_{
      switches::kEnableAccountPreviewPreferredAccountFollowup};
  mutable network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  raw_ptr<syncer::TestSyncService> sync_service_;
  raw_ptr<signin::TestAccountPreviewDataService> preview_service_;
};

TEST_F(NotificationCardAccountAwarePasswordSigninPromoTest,
       ShownAndStringsFormattedWithPreferredAccountPasswords) {
  SetUpWebSignedInPreferredAccount(/*include_passwords=*/true);

  AccountAwarePasswordSigninPromo promo(profile());
  EXPECT_EQ(promo.GetNotificationSeverity(),
            password_manager::NotificationSeverity::kHighPriorityPromo);
  EXPECT_TRUE(
      promo.ShouldShowCard(password_manager::NotificationCardPrefState{}));
  EXPECT_EQ(l10n_util::GetStringUTF16(
                IDS_PASSWORD_MANAGER_UI_SIGNIN_PROMO_CARD_TITLE),
            promo.GetTitle());
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_PASSWORD_MANAGER_UI_SIGNIN_PROMO_CARD_DESCRIPTION,
                u"camille.c.walsh@gmail.com"),
            promo.GetDescription());
  EXPECT_EQ(l10n_util::GetStringFUTF16(IDS_PROFILES_DICE_WEB_ONLY_SIGNIN_BUTTON,
                                       u"Camille"),
            promo.GetActionButtonText());
  EXPECT_FALSE(promo.GetActionButtonAvatarUrl().empty());
}

TEST_F(NotificationCardAccountAwarePasswordSigninPromoTest,
       TitleFormattedWithDeviceFormFactor) {
  AccountAwarePasswordSigninPromo promo(profile());

  SetUpWebSignedInPreferredAccount(
      /*include_passwords=*/true,
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_PHONE);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_PASSWORD_MANAGER_UI_SIGNIN_PROMO_CARD_TITLE_WITH_DEVICE,
                l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_PHONE)),
            promo.GetTitle());

  SetUpWebSignedInPreferredAccount(
      /*include_passwords=*/true,
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_TABLET);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_PASSWORD_MANAGER_UI_SIGNIN_PROMO_CARD_TITLE_WITH_DEVICE,
                l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_TABLET)),
            promo.GetTitle());

  SetUpWebSignedInPreferredAccount(
      /*include_passwords=*/true,
      sync_pb::SyncEnums_DeviceFormFactor_DEVICE_FORM_FACTOR_DESKTOP);
  EXPECT_EQ(l10n_util::GetStringFUTF16(
                IDS_PASSWORD_MANAGER_UI_SIGNIN_PROMO_CARD_TITLE_WITH_DEVICE,
                l10n_util::GetStringUTF16(IDS_ACCOUNT_PREVIEW_DEVICE_COMPUTER)),
            promo.GetTitle());
}

TEST_F(NotificationCardAccountAwarePasswordSigninPromoTest,
       NotShownWhenAlreadySignedIn) {
  SetUpWebSignedInPreferredAccount(/*include_passwords=*/true);
  identity_test_env_adaptor_->identity_test_env()->MakePrimaryAccountAvailable(
      "camille.c.walsh@gmail.com", signin::ConsentLevel::kSignin);

  AccountAwarePasswordSigninPromo promo(profile());
  EXPECT_FALSE(
      promo.ShouldShowCard(password_manager::NotificationCardPrefState{}));
}

TEST_F(NotificationCardAccountAwarePasswordSigninPromoTest,
       NotShownWhenPreferredAccountHasNoPasswords) {
  SetUpWebSignedInPreferredAccount(/*include_passwords=*/false);

  AccountAwarePasswordSigninPromo promo(profile());
  EXPECT_FALSE(
      promo.ShouldShowCard(password_manager::NotificationCardPrefState{}));
}

TEST_F(NotificationCardAccountAwarePasswordSigninPromoTest,
       NotShownWhenSigninDisallowed) {
  SetUpWebSignedInPreferredAccount(/*include_passwords=*/true);
  profile()->GetPrefs()->SetBoolean(prefs::kSigninAllowed, false);

  AccountAwarePasswordSigninPromo promo(profile());
  EXPECT_FALSE(
      promo.ShouldShowCard(password_manager::NotificationCardPrefState{}));
}

TEST_F(NotificationCardAccountAwarePasswordSigninPromoTest,
       ShownLimitAndDismissLimit) {
  SetUpWebSignedInPreferredAccount(/*include_passwords=*/true);

  AccountAwarePasswordSigninPromo promo(profile());
  for (int shown = 0;
       shown <
       password_manager::PasswordNotificationCardBase::kPromoDisplayLimit;
       ++shown) {
    EXPECT_TRUE(
        promo.ShouldShowCard(password_manager::NotificationCardPrefState{
            .number_of_times_shown = shown}));
  }
  EXPECT_FALSE(promo.ShouldShowCard(password_manager::NotificationCardPrefState{
      .number_of_times_shown =
          password_manager::PasswordNotificationCardBase::kPromoDisplayLimit}));

  EXPECT_FALSE(promo.ShouldShowCard(
      password_manager::NotificationCardPrefState{.was_dismissed = true}));
}
