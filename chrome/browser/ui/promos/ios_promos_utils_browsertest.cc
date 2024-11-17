// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/promos/ios_promos_utils.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/promos/promos_types.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "content/public/test/browser_test.h"

class IOSPromosUtilsTest : public SyncTest {
 public:
  IOSPromosUtilsTest() : SyncTest(SINGLE_CLIENT) {
    scoped_iph_feature_list_.InitAndEnableFeatures(
        {feature_engagement::kIPHiOSPasswordPromoDesktopFeature,
         feature_engagement::kIPHiOSPaymentPromoDesktopFeature,
         feature_engagement::kIPHiOSAddressPromoDesktopFeature});
  }

  IOSPromosUtilsTest(const IOSPromosUtilsTest&) = delete;

  IOSPromosUtilsTest& operator=(const IOSPromosUtilsTest&) = delete;

  void SetupSyncForAccount() {
    ASSERT_TRUE(SetupClients());

    // Sign the profile in.
    ASSERT_TRUE(
        GetClient(0)->SignInPrimaryAccount(signin::ConsentLevel::kSignin));

    CoreAccountInfo current_info =
        IdentityManagerFactory::GetForProfile(GetProfile(0))
            ->GetPrimaryAccountInfo(signin::ConsentLevel::kSync);
    // Need to update hosted domain since it is not populated.
    AccountInfo account_info;
    account_info.account_id = current_info.account_id;
    account_info.gaia = current_info.gaia;
    account_info.email = current_info.email;
    account_info.hosted_domain = kNoHostedDomainFound;
    signin::UpdateAccountInfoForAccount(
        IdentityManagerFactory::GetForProfile(GetProfile(0)), account_info);

    ASSERT_TRUE(SetupSync());
  }

  SyncServiceImplHarness* sync_harness() {
    if (sync_harness_) {
      return sync_harness_.get();
    }

    SyncServiceFactory::GetAsSyncServiceImplForProfileForTesting(
        browser()->profile())
        ->OverrideNetworkForTest(
            fake_server::CreateFakeServerHttpPostProviderFactory(
                GetFakeServer()->AsWeakPtr()));
    sync_harness_ = SyncServiceImplHarness::Create(
        browser()->profile(), "user@example.com", "password",
        SyncServiceImplHarness::SigninType::FAKE_SIGNIN);
    return sync_harness_.get();
  }

  std::unique_ptr<SyncServiceImplHarness> sync_harness_;
  feature_engagement::test::ScopedIphFeatureList scoped_iph_feature_list_;
};

IN_PROC_BROWSER_TEST_F(IOSPromosUtilsTest, InvokeUi_passwords) {
  ASSERT_TRUE(sync_harness()->SetupSync());

  ios_promos_utils::VerifyIOSPromoEligibility(
      IOSPromoType::kPassword, browser()->profile(),
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar_button_provider());
}

IN_PROC_BROWSER_TEST_F(IOSPromosUtilsTest, InvokeUi_addresses) {
  ASSERT_TRUE(sync_harness()->SetupSync());

  ios_promos_utils::VerifyIOSPromoEligibility(
      IOSPromoType::kAddress, browser()->profile(),
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar_button_provider());
}

IN_PROC_BROWSER_TEST_F(IOSPromosUtilsTest, InvokeUi_payments) {
  ASSERT_TRUE(sync_harness()->SetupSync());

  ios_promos_utils::VerifyIOSPromoEligibility(
      IOSPromoType::kPayment, browser()->profile(),
      BrowserView::GetBrowserViewForBrowser(browser())
          ->toolbar_button_provider());
}
