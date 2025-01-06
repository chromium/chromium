// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/ui/autofill/autofill_bubble_signin_promo_controller.h"

#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/service/local_data_description.h"
#include "components/sync/test/mock_sync_service.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"

namespace {

std::unique_ptr<KeyedService> BuildMockSyncService(
    content::BrowserContext* context) {
  return std::make_unique<testing::NiceMock<syncer::MockSyncService>>();
}

}  // namespace

class AutofillBubbleSignInPromoControllerTest : public testing::Test {
 public:
  AutofillBubbleSignInPromoControllerTest() : id_("test_id") {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildMockSyncService));
    profile_ = profile_builder.Build();

    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
  }

  syncer::MockSyncService* sync_service_mock() {
    return static_cast<syncer::MockSyncService*>(
        SyncServiceFactory::GetForProfile(profile_.get()));
  }

  signin::IdentityManager* identity_manager() {
    return IdentityManagerFactory::GetForProfile(profile_.get());
  }

  std::vector<syncer::LocalDataItemModel::DataId> ids() { return {id_}; }
  content::WebContents& web_contents() { return *web_contents_.get(); }

 private:
  base::test::ScopedFeatureList feature_list_{
      switches::kImprovedSigninUIOnDesktop};
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler render_view_host_test_enabler_;
  syncer::LocalDataItemModel::DataId id_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
};

// TODO(crbug.com/388025216): Enable this test.
TEST_F(AutofillBubbleSignInPromoControllerTest,
       DISABLED_DataMigrationTriggeredWithExistingAccount) {
  auto autofill_bubble_controller =
      std::make_unique<autofill::AutofillBubbleSignInPromoController>(
          web_contents(),
          signin_metrics::AccessPoint::ACCESS_POINT_PASSWORD_BUBBLE, ids()[0]);

  // Simulate a sign in to the web.
  AccountInfo account_info = signin::MakeAccountAvailable(
      identity_manager(),
      signin::AccountAvailabilityOptionsBuilder()
          .WithAccessPoint(signin_metrics::AccessPoint::ACCESS_POINT_WEB_SIGNIN)
          .Build("test@gmail.com"));

  // Check that the data migration will be triggered.
  EXPECT_CALL(
      *sync_service_mock(),
      SelectTypeAndMigrateLocalDataItemsWhenActive(syncer::PASSWORDS, ids()))
      .Times(1);

  // This should sign in the user to Chrome directly, as there is already an
  // account on the web.
  autofill_bubble_controller->OnSignInToChromeClicked(account_info);

  // Check that the user was signed in to Chrome.
  EXPECT_TRUE(
      identity_manager()->HasPrimaryAccount(signin::ConsentLevel::kSignin));
}
