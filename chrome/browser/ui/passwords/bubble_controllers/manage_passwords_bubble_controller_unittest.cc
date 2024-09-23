// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/manage_passwords_bubble_controller.h"

#include <memory>

#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
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

password_manager::PasswordForm CreateTestForm(int index = 1) {
  password_manager::PasswordForm form;
  form.url = GURL(kSiteOrigin);
  form.signon_realm = kSiteOrigin;
  form.username_value = u"User" + base::NumberToString16(index);
  form.password_value = u"Password" + base::NumberToString16(index);
  return form;
}

}  // namespace

class ManagePasswordsBubbleControllerTest : public ::testing::Test {
 public:
  ManagePasswordsBubbleControllerTest() {
    TestingProfile::Builder builder;
    builder.AddTestingFactories(IdentityTestEnvironmentProfileAdaptor::
                                    GetIdentityTestEnvironmentFactories());
    builder.AddTestingFactory(
        ProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStoreInterface<
                content::BrowserContext,
                testing::StrictMock<
                    password_manager::MockPasswordStoreInterface>>));
    builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(
            [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
              return std::make_unique<syncer::TestSyncService>();
            }));
    profile_ = builder.Build();
    test_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
    ON_CALL(*mock_delegate_, GetPasswordFormMetricsRecorder())
        .WillByDefault(Return(nullptr));
    ON_CALL(*mock_delegate_,
            GetManagePasswordsSingleCredentialDetailsModeCredential)
        .WillByDefault(::testing::ReturnRef(
            default_single_credential_details_mode_credential_));
  }

  ~ManagePasswordsBubbleControllerTest() override = default;

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  ManagePasswordsBubbleController* controller() { return controller_.get(); }
  TestingProfile* profile() { return profile_.get(); }
  syncer::TestSyncService* sync_service() {
    return static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
  }

  password_manager::MockPasswordStoreInterface* GetStore() {
    return static_cast<password_manager::MockPasswordStoreInterface*>(
        ProfilePasswordStoreFactory::GetInstance()
            ->GetForProfile(profile(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  void Init();
  const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
  GetCurrentForms() const;
  void DestroyController();

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  content::RenderViewHostTestEnabler rvh_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> current_forms_;
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<ManagePasswordsBubbleController> controller_;
  std::optional<password_manager::PasswordForm>
      default_single_credential_details_mode_credential_;
};

void ManagePasswordsBubbleControllerTest::Init() {
  current_forms_.push_back(
      std::make_unique<password_manager::PasswordForm>(CreateTestForm(1)));
  current_forms_.push_back(
      std::make_unique<password_manager::PasswordForm>(CreateTestForm(2)));

  ON_CALL(*delegate(), GetCurrentForms())
      .WillByDefault(ReturnRef(current_forms_));

  url::Origin origin = url::Origin::Create(GURL(kSiteOrigin));
  ON_CALL(*delegate(), GetOrigin()).WillByDefault(Return(origin));

  EXPECT_CALL(*delegate(), GetWebContents())
      .WillRepeatedly(Return(test_web_contents_.get()));

  EXPECT_CALL(*delegate(), OnBubbleShown());
  controller_ = std::make_unique<ManagePasswordsBubbleController>(
      mock_delegate_->AsWeakPtr());
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));

  EXPECT_CALL(*delegate(), GetWebContents())
      .WillRepeatedly(Return(test_web_contents_.get()));
}

const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
ManagePasswordsBubbleControllerTest::GetCurrentForms() const {
  return current_forms_;
}

void ManagePasswordsBubbleControllerTest::DestroyController() {
  controller_.reset();
}

TEST_F(ManagePasswordsBubbleControllerTest, OnManageClicked) {
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

TEST_F(ManagePasswordsBubbleControllerTest, ShouldReturnLocalCredentials) {
  Init();
  base::span<std::unique_ptr<password_manager::PasswordForm> const>
      credentials = controller()->GetCredentials();
  const std::vector<std::unique_ptr<password_manager::PasswordForm>>&
      expected_credentials =
          ManagePasswordsBubbleControllerTest::GetCurrentForms();
  EXPECT_EQ(credentials.size(), expected_credentials.size());
  for (size_t i = 0; i < credentials.size(); i++) {
    EXPECT_EQ(*credentials[i], *expected_credentials[i]);
  }
}

TEST_F(ManagePasswordsBubbleControllerTest, ShouldReturnPasswordSyncState) {
  Init();
  sync_service()->SetSignedIn(signin::ConsentLevel::kSignin);
  sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, false);

  EXPECT_EQ(controller()->GetPasswordSyncState(),
            ManagePasswordsBubbleController::SyncState::kNotActive);

  sync_service()->GetUserSettings()->SetSelectedType(
      syncer::UserSelectableType::kPasswords, true);
  ASSERT_FALSE(sync_service()->IsSyncFeatureEnabled());

  EXPECT_EQ(
      controller()->GetPasswordSyncState(),
      ManagePasswordsBubbleController::SyncState::kActiveWithAccountPasswords);

  sync_service()->SetSignedIn(signin::ConsentLevel::kSync);
  ASSERT_TRUE(sync_service()->IsSyncFeatureEnabled());

  EXPECT_EQ(controller()->GetPasswordSyncState(),
            ManagePasswordsBubbleController::SyncState::
                kActiveWithSyncFeatureEnabled);

  sync_service()->SetIsUsingExplicitPassphrase(true);
  EXPECT_EQ(controller()->GetPasswordSyncState(),
            ManagePasswordsBubbleController::SyncState::
                kActiveWithSyncFeatureEnabled);
}

TEST_F(ManagePasswordsBubbleControllerTest, ShouldGetPrimaryAccountEmail) {
  Init();
  // Simulate sign-in.
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  signin::MakePrimaryAccountAvailable(identity_manager, "test@email.com",
                                      signin::ConsentLevel::kSync);
  EXPECT_EQ(controller()->GetPrimaryAccountEmail(), u"test@email.com");
}

TEST_F(ManagePasswordsBubbleControllerTest, OnUpdatePasswordNote) {
  Init();

  password_manager::PasswordForm original_form = CreateTestForm();

  password_manager::PasswordForm updated_form = original_form;
  updated_form.SetNoteWithEmptyUniqueDisplayName(u"Important Note");

  password_manager::PasswordForm expected_updated_form = updated_form;

  EXPECT_CALL(*GetStore(), UpdateLogin(expected_updated_form, _));
  controller()->set_details_bubble_credential(original_form);
  controller()->UpdateDetailsBubbleCredentialInPasswordStore(updated_form);
  EXPECT_EQ(controller()->get_details_bubble_credential(), updated_form);
}

TEST_F(ManagePasswordsBubbleControllerTest, OnUpdateUsername) {
  Init();

  password_manager::PasswordForm original_form = CreateTestForm();
  original_form.username_value = std::u16string();

  original_form.password_issues.insert(
      {password_manager::InsecureType::kLeaked,
       password_manager::InsecurityMetadata()});
  original_form.password_issues.insert(
      {password_manager::InsecureType::kWeak,
       password_manager::InsecurityMetadata()});

  password_manager::PasswordForm updated_form = original_form;
  // Update the username, only the Weak password issue is still relevant.
  updated_form.username_value = u"uncommon username";

  password_manager::PasswordForm expected_updated_form = updated_form;
  expected_updated_form.password_issues.erase(
      password_manager::InsecureType::kLeaked);

  EXPECT_CALL(*GetStore(), UpdateLogin).Times(0);
  EXPECT_CALL(*GetStore(), UpdateLoginWithPrimaryKey(expected_updated_form,
                                                     original_form, _));
  controller()->set_details_bubble_credential(original_form);
  controller()->UpdateDetailsBubbleCredentialInPasswordStore(updated_form);
}

TEST_F(ManagePasswordsBubbleControllerTest, OnUpdateUsernameAndPasswordNote) {
  Init();

  password_manager::PasswordForm original_form = CreateTestForm();
  original_form.username_value = std::u16string();
  original_form.SetNoteWithEmptyUniqueDisplayName(u"Original Important Note");

  password_manager::PasswordForm updated_form = original_form;
  updated_form.username_value = u"uncommon username";
  updated_form.SetNoteWithEmptyUniqueDisplayName(u"Updated Important Note");

  password_manager::PasswordForm expected_updated_form = updated_form;

  EXPECT_CALL(*GetStore(), UpdateLogin).Times(0);
  EXPECT_CALL(*GetStore(), UpdateLoginWithPrimaryKey(expected_updated_form,
                                                     original_form, _));
  controller()->set_details_bubble_credential(original_form);
  controller()->UpdateDetailsBubbleCredentialInPasswordStore(updated_form);
}

TEST_F(ManagePasswordsBubbleControllerTest,
       ShouldChangeDetailsBubbleCredentialOnSuccessfulOsAuth) {
  // The time it takes the user to complete the authentication flow in seconds.
  const int kTimeToAuth = 10;
  base::HistogramTester histogram_tester;
  Init();
  password_manager::PasswordForm details_bubble_form = CreateTestForm();

  EXPECT_CALL(*delegate(), AuthenticateUserWithMessage)
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](PasswordsModelDelegate::AvailabilityCallback callback) {
            // Waiting for kTimeToAuth seconds to simulate the time user will
            // need to authenticate
            task_environment().FastForwardBy(base::Seconds(kTimeToAuth));
            // Respond with true to simulate a successful user reauth.
            std::move(callback).Run(true);
          })));
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;
  EXPECT_CALL(mock_callback, Run(true));
  controller()->AuthenticateUserAndDisplayDetailsOf(details_bubble_form,
                                                    mock_callback.Get());
  histogram_tester.ExpectUniqueTimeSample(
      "PasswordManager.ManagementBubble.AuthenticationTime2",
      base::Seconds(kTimeToAuth), 1);
  EXPECT_EQ(controller()->get_details_bubble_credential(), details_bubble_form);
}

TEST_F(ManagePasswordsBubbleControllerTest,
       ShouldNotChangeDetailsBubbleCredentialOnFailedOsAuth) {
  Init();
  password_manager::PasswordForm details_bubble_form = CreateTestForm();

  EXPECT_CALL(*delegate(), AuthenticateUserWithMessage)
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](PasswordsModelDelegate::AvailabilityCallback callback) {
            // Respond with false to simulate a failed user reauth.
            std::move(callback).Run(false);
          })));
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;
  EXPECT_CALL(mock_callback, Run(false));
  controller()->AuthenticateUserAndDisplayDetailsOf(details_bubble_form,
                                                    mock_callback.Get());
  EXPECT_FALSE(controller()->get_details_bubble_credential().has_value());
}

TEST_F(ManagePasswordsBubbleControllerTest, ShouldReturnWhetherUsernameExists) {
  Init();
  EXPECT_TRUE(controller()->UsernameExists(u"User1"));
  EXPECT_FALSE(controller()->UsernameExists(u"AnotherUsername"));
}

TEST_F(ManagePasswordsBubbleControllerTest, OpenMoveBubble) {
  base::HistogramTester histogram_tester;
  Init();
  password_manager::PasswordForm details_bubble_form = CreateTestForm();

  // Used to mock displaying the details view. This is needed to set the
  // selected_form value to the one that we expect.
  EXPECT_CALL(*delegate(), AuthenticateUserWithMessage)
      .WillOnce(testing::WithArg<1>(testing::Invoke(
          [&](PasswordsModelDelegate::AvailabilityCallback callback) {
            // Respond with true to simulate a successful user reauth.
            std::move(callback).Run(true);
          })));
  base::MockCallback<base::OnceCallback<void(bool)>> mock_callback;
  EXPECT_CALL(mock_callback, Run(true));
  controller()->AuthenticateUserAndDisplayDetailsOf(details_bubble_form,
                                                    mock_callback.Get());

  EXPECT_CALL(*delegate(), ShowMovePasswordBubble(details_bubble_form));
  controller()->OnMovePasswordLinkClicked();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordManagementBubble.UserAction",
      password_manager::metrics_util::PasswordManagementBubbleInteractions::
          kMovePasswordLinkClicked,
      1);
}
