// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/items_bubble_controller.h"

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

constexpr char kUIDismissalReasonGeneralMetric[] =
    "PasswordManager.UIDismissalReason";

constexpr char kSiteOrigin[] = "http://example.com/login/";

}  // namespace

class ItemsBubbleControllerTest : public ::testing::Test {
 public:
  ItemsBubbleControllerTest() {
    profile_ = IdentityTestEnvironmentProfileAdaptor::
        CreateProfileForIdentityTestEnvironment();
    test_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
    ON_CALL(*mock_delegate_, GetPasswordFormMetricsRecorder())
        .WillByDefault(Return(nullptr));

    PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(), base::BindRepeating(
                       &password_manager::BuildPasswordStoreInterface<
                           content::BrowserContext,
                           testing::StrictMock<
                               password_manager::MockPasswordStoreInterface>>));

    test_sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile(),
            base::BindRepeating(
                [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                  return std::make_unique<syncer::TestSyncService>();
                })));
  }

  ~ItemsBubbleControllerTest() override = default;

  static std::vector<std::unique_ptr<password_manager::PasswordForm>>
  GetCurrentForms();

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  ItemsBubbleController* controller() { return controller_.get(); }
  TestingProfile* profile() { return profile_.get(); }
  syncer::TestSyncService* sync_service() { return test_sync_service_; }

  password_manager::MockPasswordStoreInterface* GetStore() {
    return static_cast<password_manager::MockPasswordStoreInterface*>(
        PasswordStoreFactory::GetInstance()
            ->GetForProfile(profile(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  void Init();
  void DestroyController();

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  raw_ptr<syncer::TestSyncService> test_sync_service_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<ItemsBubbleController> controller_;
};

void ItemsBubbleControllerTest::Init() {
  std::vector<std::unique_ptr<password_manager::PasswordForm>> forms =
      GetCurrentForms();
  EXPECT_CALL(*delegate(), GetCurrentForms()).WillOnce(ReturnRef(forms));

  url::Origin origin = url::Origin::Create(GURL(kSiteOrigin));
  EXPECT_CALL(*delegate(), GetOrigin()).WillOnce(Return(origin));

  EXPECT_CALL(*delegate(), GetWebContents())
      .WillRepeatedly(Return(test_web_contents_.get()));

  EXPECT_CALL(*delegate(), OnBubbleShown());
  controller_ =
      std::make_unique<ItemsBubbleController>(mock_delegate_->AsWeakPtr());
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));

  EXPECT_CALL(*delegate(), GetWebContents())
      .WillRepeatedly(Return(test_web_contents_.get()));
}

void ItemsBubbleControllerTest::DestroyController() {
  controller_.reset();
}

// static
std::vector<std::unique_ptr<password_manager::PasswordForm>>
ItemsBubbleControllerTest::GetCurrentForms() {
  password_manager::PasswordForm form1;
  form1.url = GURL(kSiteOrigin);
  form1.signon_realm = kSiteOrigin;
  form1.username_value = u"User1";
  form1.password_value = u"123456";

  password_manager::PasswordForm form2;
  form2.url = GURL(kSiteOrigin);
  form2.signon_realm = kSiteOrigin;
  form2.username_value = u"User2";
  form2.password_value = u"654321";

  std::vector<std::unique_ptr<password_manager::PasswordForm>> forms;
  forms.push_back(std::make_unique<password_manager::PasswordForm>(form1));
  forms.push_back(std::make_unique<password_manager::PasswordForm>(form2));
  return forms;
}

TEST_F(ItemsBubbleControllerTest, OnManageClicked) {
  Init();

  EXPECT_CALL(
      *delegate(),
      NavigateToPasswordManagerSettingsPage(
          password_manager::ManagePasswordsReferrer::kManagePasswordsBubble));

  controller()->OnManageClicked(
      password_manager::ManagePasswordsReferrer::kManagePasswordsBubble);

  base::HistogramTester histogram_tester;

  DestroyController();

  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonGeneralMetric,
      password_manager::metrics_util::CLICKED_MANAGE, 1);
}

TEST_F(ItemsBubbleControllerTest, OnPasswordActionAddPassword) {
  Init();

  password_manager::PasswordForm form;
  form.url = GURL(kSiteOrigin);
  form.signon_realm = kSiteOrigin;
  form.username_value = u"User";
  form.password_value = u"123456";

  EXPECT_CALL(*GetStore(), AddLogin(form, _));

  controller()->OnPasswordAction(
      form, PasswordBubbleControllerBase::PasswordAction::kAddPassword);
}

TEST_F(ItemsBubbleControllerTest, OnPasswordActionRemovePassword) {
  Init();

  password_manager::PasswordForm form;
  form.url = GURL(kSiteOrigin);
  form.signon_realm = kSiteOrigin;
  form.username_value = u"User";
  form.password_value = u"123456";

  EXPECT_CALL(*GetStore(), RemoveLogin(form));

  controller()->OnPasswordAction(
      form, PasswordBubbleControllerBase::PasswordAction::kRemovePassword);
}

TEST_F(ItemsBubbleControllerTest, ShouldReturnLocalCredentials) {
  Init();
  std::vector<password_manager::PasswordForm> local_credentials =
      controller()->local_credentials();
  std::vector<std::unique_ptr<password_manager::PasswordForm>>
      expected_local_credentials = ItemsBubbleControllerTest::GetCurrentForms();
  EXPECT_EQ(local_credentials.size(), expected_local_credentials.size());
  for (size_t i = 0; i < local_credentials.size(); i++) {
    EXPECT_EQ(local_credentials[i], *expected_local_credentials[i]);
  }
}

TEST_F(ItemsBubbleControllerTest, ShouldReturnPasswordSyncState) {
  Init();
  CoreAccountInfo account;
  account.email = "account@gmail.com";
  account.gaia = "account";
  account.account_id = CoreAccountId::FromGaiaId(account.gaia);

  sync_service()->SetAccountInfo(account);
  sync_service()->SetHasSyncConsent(false);
  sync_service()->SetDisableReasons({});
  sync_service()->SetTransportState(
      syncer::SyncService::TransportState::ACTIVE);
  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet());

  EXPECT_EQ(controller()->GetPasswordSyncState(),
            password_manager::SyncState::kNotSyncing);

  sync_service()->GetUserSettings()->SetSelectedTypes(
      /*sync_everything=*/false,
      /*types=*/syncer::UserSelectableTypeSet(
          syncer::UserSelectableType::kPasswords));
  EXPECT_EQ(
      controller()->GetPasswordSyncState(),
      password_manager::SyncState::kAccountPasswordsActiveNormalEncryption);

  sync_service()->SetHasSyncConsent(true);
  EXPECT_EQ(controller()->GetPasswordSyncState(),
            password_manager::SyncState::kSyncingNormalEncryption);

  sync_service()->SetIsUsingExplicitPassphrase(true);
  EXPECT_EQ(controller()->GetPasswordSyncState(),
            password_manager::SyncState::kSyncingWithCustomPassphrase);
}

TEST_F(ItemsBubbleControllerTest, ShouldGetPrimaryAccountEmail) {
  Init();
  // Simulate sign-in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  signin::MakePrimaryAccountAvailable(identity_manager, "test@email.com",
                                      signin::ConsentLevel::kSync);
  EXPECT_EQ(controller()->GetPrimaryAccountEmail(), u"test@email.com");
}
