// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/save_update_bubble_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/metrics/histogram_samples.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/test_signin_client_builder.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/features/password_manager_features_util.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/interactions_stats.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/password_store/mock_smart_bubble_stats_store.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/sync/test/test_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

constexpr ukm::SourceId kTestSourceId = 0x1234;

constexpr char kSiteOrigin[] = "http://example.com/login";
constexpr char16_t kUsername[] = u"Admin";
constexpr char16_t kUsernameExisting[] = u"User";
constexpr char16_t kUsernameNew[] = u"User585";
constexpr char16_t kPassword[] = u"AdminPass";
constexpr char16_t kPasswordEdited[] = u"asDfjkl;";
constexpr char kUIDismissalReasonGeneralMetric[] =
    "PasswordManager.UIDismissalReason";
constexpr char kUIDismissalReasonSaveMetric[] =
    "PasswordManager.SaveUIDismissalReason";
constexpr char kUIDismissalReasonUpdateMetric[] =
    "PasswordManager.UpdateUIDismissalReason";

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

void SetupAccountPasswordStore(syncer::TestSyncService* sync_service,
                               PrefService* pref_service) {
  sync_service->SetSignedIn(signin::ConsentLevel::kSignin);
  ASSERT_TRUE(password_manager::features_util::IsOptedInForAccountStorage(
      pref_service, sync_service));
}

}  // namespace

class SaveUpdateBubbleControllerTest : public ::testing::Test {
 public:
  SaveUpdateBubbleControllerTest() = default;
  ~SaveUpdateBubbleControllerTest() override = default;

  void SetUp() override {
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        ChromeSigninClientFactory::GetInstance(),
        base::BindRepeating(&signin::BuildTestSigninClient));
    profile_builder.AddTestingFactory(
        SyncServiceFactory::GetInstance(),
        base::BindRepeating(&BuildTestSyncService));
    profile_builder.AddTestingFactory(
        ProfilePasswordStoreFactory::GetInstance(),
        base::BindRepeating(
            &password_manager::BuildPasswordStoreInterface<
                content::BrowserContext,
                testing::StrictMock<
                    password_manager::MockPasswordStoreInterface>>));
    profile_ = profile_builder.Build();

    test_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);

    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
    ON_CALL(*mock_delegate_, GetPasswordFeatureManager())
        .WillByDefault(Return(&password_feature_manager_));
    ON_CALL(*mock_delegate_, GetPasswordFormMetricsRecorder())
        .WillByDefault(Return(nullptr));
    EXPECT_CALL(*GetStore(), GetSmartBubbleStatsStore)
        .WillRepeatedly(Return(&mock_smart_bubble_stats_store_));
    pending_password_.url = GURL(kSiteOrigin);
    pending_password_.signon_realm = kSiteOrigin;
    pending_password_.username_value = kUsername;
    pending_password_.password_value = kPassword;
  }

  void TearDown() override {
    // Reset the delegate first. It can happen if the user closes the tab.
    mock_delegate_.reset();
    controller_.reset();
  }

  PrefService* prefs() { return profile_->GetPrefs(); }

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

  password_manager::MockSmartBubbleStatsStore* mock_smart_bubble_stats_store() {
    return &mock_smart_bubble_stats_store_;
  }

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }

  password_manager::MockPasswordFeatureManager* password_feature_manager() {
    return &password_feature_manager_;
  }

  SaveUpdateBubbleController* controller() { return controller_.get(); }

  password_manager::PasswordForm& pending_password() {
    return pending_password_;
  }
  const password_manager::PasswordForm& pending_password() const {
    return pending_password_;
  }

  void SetUpWithState(password_manager::ui::State state,
                      PasswordBubbleControllerBase::DisplayReason reason);
  void PretendPasswordWaiting(
      PasswordBubbleControllerBase::DisplayReason reason =
          PasswordBubbleControllerBase::DisplayReason::kAutomatic);
  void PretendUpdatePasswordWaiting();

  void DestroyModelAndVerifyControllerExpectations();
  void DestroyModelExpectReason(
      password_manager::metrics_util::UIDismissalReason dismissal_reason);

  static password_manager::InteractionsStats GetTestStats();
  std::vector<std::unique_ptr<password_manager::PasswordForm>> GetCurrentForms()
      const;

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<SaveUpdateBubbleController> controller_;
  testing::NiceMock<password_manager::MockPasswordFeatureManager>
      password_feature_manager_;
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  testing::NiceMock<password_manager::MockSmartBubbleStatsStore>
      mock_smart_bubble_stats_store_;
  password_manager::PasswordForm pending_password_;
};

void SaveUpdateBubbleControllerTest::SetUpWithState(
    password_manager::ui::State state,
    PasswordBubbleControllerBase::DisplayReason reason) {
  url::Origin origin = url::Origin::Create(GURL(kSiteOrigin));
  EXPECT_CALL(*delegate(), GetOrigin()).WillOnce(Return(origin));
  EXPECT_CALL(*delegate(), GetState()).WillRepeatedly(Return(state));
  EXPECT_CALL(*delegate(), GetWebContents())
      .WillRepeatedly(Return(test_web_contents_.get()));
  controller_ = std::make_unique<SaveUpdateBubbleController>(
      mock_delegate_->AsWeakPtr(), reason);
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
  EXPECT_CALL(*delegate(), GetWebContents())
      .WillRepeatedly(Return(test_web_contents_.get()));
}

void SaveUpdateBubbleControllerTest::PretendPasswordWaiting(
    PasswordBubbleControllerBase::DisplayReason reason) {
  EXPECT_CALL(*delegate(), GetPendingPassword())
      .WillOnce(ReturnRef(pending_password()));
  password_manager::InteractionsStats stats = GetTestStats();
  EXPECT_CALL(*delegate(), GetCurrentInteractionStats())
      .WillOnce(Return(&stats));
  std::vector<std::unique_ptr<password_manager::PasswordForm>> forms =
      GetCurrentForms();
  EXPECT_CALL(*delegate(), GetCurrentForms()).WillOnce(ReturnRef(forms));
  SetUpWithState(password_manager::ui::PENDING_PASSWORD_STATE, reason);
}

void SaveUpdateBubbleControllerTest::PretendUpdatePasswordWaiting() {
  EXPECT_CALL(*delegate(), GetPendingPassword())
      .WillOnce(ReturnRef(pending_password()));
  std::vector<std::unique_ptr<password_manager::PasswordForm>> forms =
      GetCurrentForms();
  auto current_form =
      std::make_unique<password_manager::PasswordForm>(pending_password());
  current_form->password_value = u"old_password";
  forms.push_back(std::move(current_form));
  EXPECT_CALL(*delegate(), GetCurrentForms()).WillOnce(ReturnRef(forms));
  SetUpWithState(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
                 PasswordBubbleControllerBase::DisplayReason::kAutomatic);
}

void SaveUpdateBubbleControllerTest::
    DestroyModelAndVerifyControllerExpectations() {
  EXPECT_CALL(*delegate(), OnBubbleHidden());
  controller_->OnBubbleClosing();
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
  controller_.reset();
}

void SaveUpdateBubbleControllerTest::DestroyModelExpectReason(
    password_manager::metrics_util::UIDismissalReason dismissal_reason) {
  base::HistogramTester histogram_tester;
  password_manager::ui::State state = controller_->state();
  std::string histogram(kUIDismissalReasonGeneralMetric);
  if (state == password_manager::ui::PENDING_PASSWORD_STATE) {
    histogram = kUIDismissalReasonSaveMetric;
  } else if (state == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    histogram = kUIDismissalReasonUpdateMetric;
  }
  DestroyModelAndVerifyControllerExpectations();
  histogram_tester.ExpectUniqueSample(histogram, dismissal_reason, 1);
}

// static
password_manager::InteractionsStats
SaveUpdateBubbleControllerTest::GetTestStats() {
  password_manager::InteractionsStats result;
  result.origin_domain = GURL(kSiteOrigin).DeprecatedGetOriginAsURL();
  result.username_value = kUsername;
  result.dismissal_count = 5;
  result.update_time = base::Time::FromTimeT(1);
  return result;
}

std::vector<std::unique_ptr<password_manager::PasswordForm>>
SaveUpdateBubbleControllerTest::GetCurrentForms() const {
  password_manager::PasswordForm form(pending_password());
  form.username_value = kUsernameExisting;
  form.password_value = u"123456";

  password_manager::PasswordForm preferred_form(pending_password());
  preferred_form.username_value = u"preferred_username";
  preferred_form.password_value = u"654321";

  std::vector<std::unique_ptr<password_manager::PasswordForm>> forms;
  forms.push_back(std::make_unique<password_manager::PasswordForm>(form));
  forms.push_back(
      std::make_unique<password_manager::PasswordForm>(preferred_form));
  return forms;
}

TEST_F(SaveUpdateBubbleControllerTest, CloseWithoutInteraction) {
  PretendPasswordWaiting();

  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            controller()->state());
  base::SimpleTestClock clock;
  base::Time now = base::Time::Now();
  clock.SetNow(now);
  controller()->set_clock(&clock);
  password_manager::InteractionsStats stats = GetTestStats();
  stats.dismissal_count++;
  stats.update_time = now;
  EXPECT_CALL(*mock_smart_bubble_stats_store(), AddSiteStats(stats));
  EXPECT_CALL(*delegate(), OnNoInteraction());
  EXPECT_CALL(*delegate(), SavePassword(_, _)).Times(0);
  EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
  DestroyModelExpectReason(
      password_manager::metrics_util::NO_DIRECT_INTERACTION);
}

TEST_F(SaveUpdateBubbleControllerTest, ClickSaveInLocalStore) {
  ON_CALL(*password_feature_manager(), GetDefaultPasswordStore)
      .WillByDefault(
          Return(password_manager::PasswordForm::Store::kProfileStore));
  PretendPasswordWaiting();

  EXPECT_FALSE(controller()->IsCurrentStateUpdate());

  EXPECT_CALL(*mock_smart_bubble_stats_store(),
              RemoveSiteStats(GURL(kSiteOrigin).DeprecatedGetOriginAsURL()));
  EXPECT_CALL(*delegate(), OnPasswordsRevealed()).Times(0);
  EXPECT_CALL(*delegate(), SavePassword(pending_password().username_value,
                                        pending_password().password_value));
  EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
  EXPECT_CALL(*delegate(), OnNopeUpdateClicked()).Times(0);
  EXPECT_CALL(*delegate(), AuthenticateUserForAccountStoreOptInAndSavePassword)
      .Times(0);
  controller()->OnSaveClicked();
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_ACCEPT);
}

TEST_F(SaveUpdateBubbleControllerTest, ClickSaveInAccountStoreWhileOptedIn) {
  ON_CALL(*password_feature_manager(), GetDefaultPasswordStore)
      .WillByDefault(
          Return(password_manager::PasswordForm::Store::kAccountStore));
  ON_CALL(*password_feature_manager(), IsOptedInForAccountStorage)
      .WillByDefault(Return(true));
  PretendPasswordWaiting();

  EXPECT_FALSE(controller()->IsCurrentStateUpdate());
  EXPECT_FALSE(controller()->IsAccountStorageOptInRequiredBeforeSave());

  EXPECT_CALL(*mock_smart_bubble_stats_store(),
              RemoveSiteStats(GURL(kSiteOrigin).DeprecatedGetOriginAsURL()));
  EXPECT_CALL(*delegate(), OnPasswordsRevealed()).Times(0);
  EXPECT_CALL(*delegate(), SavePassword(pending_password().username_value,
                                        pending_password().password_value));
  EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
  EXPECT_CALL(*delegate(), OnNopeUpdateClicked()).Times(0);
  EXPECT_CALL(*delegate(), AuthenticateUserForAccountStoreOptInAndSavePassword)
      .Times(0);
  controller()->OnSaveClicked();
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_ACCEPT);
}

TEST_F(SaveUpdateBubbleControllerTest, ClickSaveInAccountStoreWhileNotOptedIn) {
  ON_CALL(*password_feature_manager(), GetDefaultPasswordStore)
      .WillByDefault(
          Return(password_manager::PasswordForm::Store::kAccountStore));
  ON_CALL(*password_feature_manager(), IsOptedInForAccountStorage)
      .WillByDefault(Return(false));
  PretendPasswordWaiting();

  EXPECT_FALSE(controller()->IsCurrentStateUpdate());
  EXPECT_TRUE(controller()->IsAccountStorageOptInRequiredBeforeSave());

  EXPECT_CALL(*mock_smart_bubble_stats_store(),
              RemoveSiteStats(GURL(kSiteOrigin).DeprecatedGetOriginAsURL()));
  EXPECT_CALL(*delegate(), SavePassword).Times(0);
  EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
  EXPECT_CALL(*delegate(), OnNopeUpdateClicked()).Times(0);
  EXPECT_CALL(*delegate(), AuthenticateUserForAccountStoreOptInAndSavePassword(
                               pending_password().username_value,
                               pending_password().password_value));
  controller()->OnSaveClicked();
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_ACCEPT);
}

TEST_F(SaveUpdateBubbleControllerTest, ClickUpdateWhileNotOptedIn) {
  // This is testing that updating a password should not trigger an account
  // store opt in flow even if the user isn't opted in.
  ON_CALL(*password_feature_manager(), GetDefaultPasswordStore)
      .WillByDefault(
          Return(password_manager::PasswordForm::Store::kAccountStore));
  ON_CALL(*password_feature_manager(), IsOptedInForAccountStorage)
      .WillByDefault(Return(false));
  PretendUpdatePasswordWaiting();

  EXPECT_TRUE(controller()->IsCurrentStateUpdate());
  EXPECT_FALSE(controller()->IsAccountStorageOptInRequiredBeforeSave());

  EXPECT_CALL(*mock_smart_bubble_stats_store(),
              RemoveSiteStats(GURL(kSiteOrigin).DeprecatedGetOriginAsURL()));
  EXPECT_CALL(*delegate(), SavePassword(pending_password().username_value,
                                        pending_password().password_value));
  EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
  EXPECT_CALL(*delegate(), OnNopeUpdateClicked()).Times(0);
  EXPECT_CALL(*delegate(), AuthenticateUserForAccountStoreOptInAndSavePassword)
      .Times(0);
  controller()->OnSaveClicked();
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_ACCEPT);
}

TEST_F(SaveUpdateBubbleControllerTest, ClickSaveInUpdateState) {
  PretendUpdatePasswordWaiting();

  // Edit username, now it's a new credential.
  controller()->OnCredentialEdited(kUsernameNew, kPasswordEdited);
  EXPECT_FALSE(controller()->IsCurrentStateUpdate());

  EXPECT_CALL(*mock_smart_bubble_stats_store(),
              RemoveSiteStats(GURL(kSiteOrigin).DeprecatedGetOriginAsURL()));
  EXPECT_CALL(*delegate(), SavePassword(Eq(kUsernameNew), Eq(kPasswordEdited)));
  EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
  EXPECT_CALL(*delegate(), OnNopeUpdateClicked()).Times(0);
  controller()->OnSaveClicked();
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_ACCEPT);
}

TEST_F(SaveUpdateBubbleControllerTest, ClickNever) {
  PretendPasswordWaiting();

  EXPECT_CALL(*mock_smart_bubble_stats_store(),
              RemoveSiteStats(GURL(kSiteOrigin).DeprecatedGetOriginAsURL()));
  EXPECT_CALL(*delegate(), SavePassword(_, _)).Times(0);
  EXPECT_CALL(*delegate(), NeverSavePassword());
  controller()->OnNeverForThisSiteClicked();
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            controller()->state());
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_NEVER);
}

TEST_F(SaveUpdateBubbleControllerTest, ClickUpdate) {
  PretendUpdatePasswordWaiting();

  EXPECT_TRUE(controller()->IsCurrentStateUpdate());

  EXPECT_CALL(*mock_smart_bubble_stats_store(),
              RemoveSiteStats(GURL(kSiteOrigin).DeprecatedGetOriginAsURL()));
  EXPECT_CALL(*delegate(), OnPasswordsRevealed()).Times(0);
  EXPECT_CALL(*delegate(), SavePassword(pending_password().username_value,
                                        pending_password().password_value));
  EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
  EXPECT_CALL(*delegate(), OnNopeUpdateClicked()).Times(0);
  controller()->OnSaveClicked();
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_ACCEPT);
}

TEST_F(SaveUpdateBubbleControllerTest, ClickUpdateInSaveState) {
  PretendPasswordWaiting();

  // Edit username, now it's an existing credential.
  controller()->OnCredentialEdited(kUsernameExisting, kPasswordEdited);
  EXPECT_TRUE(controller()->IsCurrentStateUpdate());

  EXPECT_CALL(*mock_smart_bubble_stats_store(),
              RemoveSiteStats(GURL(kSiteOrigin).DeprecatedGetOriginAsURL()));
  EXPECT_CALL(*delegate(),
              SavePassword(Eq(kUsernameExisting), Eq(kPasswordEdited)));
  EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
  EXPECT_CALL(*delegate(), OnNopeUpdateClicked()).Times(0);
  controller()->OnSaveClicked();
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_ACCEPT);
}

TEST_F(SaveUpdateBubbleControllerTest, GetInitialUsername_MatchedUsername) {
  PretendUpdatePasswordWaiting();
  EXPECT_EQ(kUsername, controller()->pending_password().username_value);
}

TEST_F(SaveUpdateBubbleControllerTest, ClickSaveWhenNoCredentialsExisted) {
  ASSERT_FALSE(prefs()->GetBoolean(
      password_manager::prefs::
          kAutofillableCredentialsProfileStoreLoginDatabase));
  ASSERT_FALSE(prefs()->GetBoolean(
      password_manager::prefs::
          kAutofillableCredentialsAccountStoreLoginDatabase));
  base::HistogramTester histogram_tester;
  PretendPasswordWaiting();

  EXPECT_FALSE(controller()->IsCurrentStateUpdate());
  controller()->OnSaveClicked();

  DestroyModelAndVerifyControllerExpectations();
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.SaveUIDismissalReason.UsersWithNoCredentials",
      static_cast<int>(password_manager::metrics_util::CLICKED_ACCEPT), 1);
}

TEST_F(SaveUpdateBubbleControllerTest, ClickSaveWhenCredentialsExisted) {
  prefs()->SetBoolean(password_manager::prefs::
                          kAutofillableCredentialsProfileStoreLoginDatabase,
                      true);
  base::HistogramTester histogram_tester;
  PretendPasswordWaiting();

  EXPECT_FALSE(controller()->IsCurrentStateUpdate());
  controller()->OnSaveClicked();

  DestroyModelAndVerifyControllerExpectations();
  histogram_tester.ExpectTotalCount(
      "PasswordManager.SaveUIDismissalReason.UsersWithNoCredentials", 0);
}

class SaveUpdateBubbleControllerUKMTest
    : public SaveUpdateBubbleControllerTest,
      public testing::WithParamInterface<
          std::tuple<bool /* whether from the credential management API*/,
                     bool /* is update bubble */,
                     password_manager::PasswordFormMetricsRecorder::
                         BubbleDismissalReason>> {};

// Verify that URL keyed metrics are properly recorded.
TEST_P(SaveUpdateBubbleControllerUKMTest, RecordUKMs) {
  using BubbleDismissalReason =
      password_manager::PasswordFormMetricsRecorder::BubbleDismissalReason;
  using BubbleTrigger =
      password_manager::PasswordFormMetricsRecorder::BubbleTrigger;
  using password_manager::metrics_util::CredentialSourceType;
  using UkmEntry = ukm::builders::PasswordForm;

  bool credential_management_api = std::get<0>(GetParam());
  bool update = std::get<1>(GetParam());
  BubbleDismissalReason interaction = std::get<2>(GetParam());

  SCOPED_TRACE(testing::Message()
               << "update = " << update
               << ", interaction = " << static_cast<int64_t>(interaction)
               << ", credential management api =" << credential_management_api);
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  {
    // Setup metrics recorder
    auto recorder =
        base::MakeRefCounted<password_manager::PasswordFormMetricsRecorder>(
            true /*is_main_frame_secure*/, kTestSourceId,
            /*pref_service=*/nullptr);

    // Exercise bubble.
    ON_CALL(*delegate(), GetPasswordFormMetricsRecorder())
        .WillByDefault(Return(recorder.get()));
    ON_CALL(*delegate(), GetCredentialSource())
        .WillByDefault(
            Return(credential_management_api
                       ? CredentialSourceType::kCredentialManagementAPI
                       : CredentialSourceType::kPasswordManager));

    if (update) {
      PretendUpdatePasswordWaiting();
    } else {
      PretendPasswordWaiting();
    }

    if (interaction == BubbleDismissalReason::kAccepted) {
      EXPECT_CALL(
          *mock_smart_bubble_stats_store(),
          RemoveSiteStats(GURL(kSiteOrigin).DeprecatedGetOriginAsURL()));
      EXPECT_CALL(*delegate(), SavePassword(pending_password().username_value,
                                            pending_password().password_value));
      controller()->OnSaveClicked();
    } else if (interaction == BubbleDismissalReason::kDeclined && update) {
      EXPECT_CALL(*delegate(), SavePassword(_, _)).Times(0);
      controller()->OnNoThanksClicked();
    } else if (interaction == BubbleDismissalReason::kDeclined && !update) {
      EXPECT_CALL(
          *mock_smart_bubble_stats_store(),
          RemoveSiteStats(GURL(kSiteOrigin).DeprecatedGetOriginAsURL()));
      EXPECT_CALL(*delegate(), SavePassword(_, _)).Times(0);
      EXPECT_CALL(*delegate(), NeverSavePassword());
      controller()->OnNeverForThisSiteClicked();
    } else if (interaction == BubbleDismissalReason::kIgnored && update) {
      EXPECT_CALL(*delegate(), SavePassword(_, _)).Times(0);
      EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
    } else if (interaction == BubbleDismissalReason::kIgnored && !update) {
      EXPECT_CALL(*mock_smart_bubble_stats_store(), AddSiteStats);
      EXPECT_CALL(*delegate(), OnNoInteraction());
      EXPECT_CALL(*delegate(), SavePassword(_, _)).Times(0);
      EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
    } else {
      NOTREACHED_IN_MIGRATION();
    }
    DestroyModelAndVerifyControllerExpectations();
  }

  // Flush async calls on password store.
  base::RunLoop().RunUntilIdle();

  // Verify metrics.
  const auto& entries =
      test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, entries.size());
  for (const ukm::mojom::UkmEntry* entry : entries) {
    EXPECT_EQ(kTestSourceId, entry->source_id);
    test_ukm_recorder.ExpectEntryMetric(
        entry,
        update ? UkmEntry::kUpdating_Prompt_ShownName
               : UkmEntry::kSaving_Prompt_ShownName,
        1);
    test_ukm_recorder.ExpectEntryMetric(
        entry,
        update ? UkmEntry::kUpdating_Prompt_TriggerName
               : UkmEntry::kSaving_Prompt_TriggerName,
        static_cast<int64_t>(
            credential_management_api
                ? BubbleTrigger::kCredentialManagementAPIAutomatic
                : BubbleTrigger::kPasswordManagerSuggestionAutomatic));
    test_ukm_recorder.ExpectEntryMetric(
        entry,
        update ? UkmEntry::kUpdating_Prompt_InteractionName
               : UkmEntry::kSaving_Prompt_InteractionName,
        static_cast<int64_t>(interaction));
  }
}

INSTANTIATE_TEST_SUITE_P(
    SaveUpdateBubbleController,
    SaveUpdateBubbleControllerUKMTest,
    testing::Combine(
        testing::Bool(),
        testing::Bool(),
        testing::Values(password_manager::PasswordFormMetricsRecorder::
                            BubbleDismissalReason::kAccepted,
                        password_manager::PasswordFormMetricsRecorder::
                            BubbleDismissalReason::kDeclined,
                        password_manager::PasswordFormMetricsRecorder::
                            BubbleDismissalReason::kIgnored)));

class SaveUpdateBubbleControllerPasswordRevealingTest
    : public SaveUpdateBubbleControllerTest,
      public testing::WithParamInterface<
          std::tuple<bool /*is manual fallback*/,
                     bool /*form has autofilled value*/,
                     bool /*does os support user authentication*/,
                     PasswordBubbleControllerBase::DisplayReason>> {};

TEST_P(SaveUpdateBubbleControllerPasswordRevealingTest,
       EyeIcon_ReauthForPasswordsRevealing) {
  bool is_manual_fallback_for_saving = std::get<0>(GetParam());
  bool form_has_autofilled_value = std::get<1>(GetParam());
  bool does_os_support_user_auth = std::get<2>(GetParam());
  PasswordBubbleControllerBase::DisplayReason display_reason =
      std::get<3>(GetParam());

  // That state is impossible.
  if (is_manual_fallback_for_saving &&
      (display_reason ==
       PasswordBubbleControllerBase::DisplayReason::kAutomatic)) {
    SUCCEED();
  }

  SCOPED_TRACE(
      testing::Message()
      << "is_manual_fallback_for_saving = " << is_manual_fallback_for_saving
      << " form_has_autofilled_value = " << form_has_autofilled_value
      << " display_reason = "
      << (display_reason ==
                  PasswordBubbleControllerBase::DisplayReason::kAutomatic
              ? "AUTOMATIC"
              : "USER_ACTION"));

  pending_password().form_has_autofilled_value = form_has_autofilled_value;
  EXPECT_CALL(*delegate(), BubbleIsManualFallbackForSaving())
      .WillRepeatedly(Return(is_manual_fallback_for_saving));

  PretendPasswordWaiting(display_reason);
  bool reauth_expected = false;
  if (display_reason ==
      PasswordBubbleControllerBase::DisplayReason::kUserAction) {
    reauth_expected =
        form_has_autofilled_value || !is_manual_fallback_for_saving;
  }
  EXPECT_EQ(reauth_expected,
            controller()->password_revealing_requires_reauth());

  if (reauth_expected) {
    EXPECT_CALL(*delegate(), AuthenticateUserWithMessage)
        .WillOnce(testing::WithArg<1>(testing::Invoke(
            [&](PasswordsModelDelegate::AvailabilityCallback callback) {
              std::move(callback).Run(!does_os_support_user_auth);
            })));
    base::MockCallback<PasswordsModelDelegate::AvailabilityCallback>
        mock_callback;
    EXPECT_CALL(mock_callback, Run(!does_os_support_user_auth));
    controller()->ShouldRevealPasswords(mock_callback.Get());
  } else {
    EXPECT_CALL(*delegate(), AuthenticateUserWithMessage).Times(0);
    base::MockCallback<PasswordsModelDelegate::AvailabilityCallback>
        mock_callback;
    EXPECT_CALL(mock_callback, Run(true));
    controller()->ShouldRevealPasswords(mock_callback.Get());
  }
}

INSTANTIATE_TEST_SUITE_P(
    SaveUpdateBubbleController,
    SaveUpdateBubbleControllerPasswordRevealingTest,
    testing::Combine(
        testing::Bool(),
        testing::Bool(),
        testing::Bool(),
        testing::Values(
            PasswordBubbleControllerBase::DisplayReason::kAutomatic,
            PasswordBubbleControllerBase::DisplayReason::kUserAction)));

TEST_F(SaveUpdateBubbleControllerTest, PasswordsRevealedReported) {
  PretendPasswordWaiting();

  EXPECT_CALL(*delegate(), OnPasswordsRevealed());
  base::MockCallback<PasswordsModelDelegate::AvailabilityCallback>
      mock_callback;
  EXPECT_CALL(mock_callback, Run(true));
  controller()->ShouldRevealPasswords(mock_callback.Get());
}

TEST_F(SaveUpdateBubbleControllerTest,
       UpdateAccountStoreAffectsTheAccountStore) {
  SetupAccountPasswordStore(sync_service(), prefs());
  EXPECT_CALL(*delegate(), GetPendingPassword())
      .WillOnce(ReturnRef(pending_password()));
  std::vector<std::unique_ptr<password_manager::PasswordForm>> forms;
  auto form =
      std::make_unique<password_manager::PasswordForm>(pending_password());
  form->password_value = u"old_password";
  form->in_store = password_manager::PasswordForm::Store::kAccountStore;
  forms.push_back(std::move(form));
  EXPECT_CALL(*delegate(), GetCurrentForms()).WillOnce(ReturnRef(forms));
  SetUpWithState(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
                 PasswordBubbleControllerBase::DisplayReason::kAutomatic);
  EXPECT_TRUE(
      controller()->IsCurrentStateAffectingPasswordsStoredInTheGoogleAccount());
}

TEST_F(SaveUpdateBubbleControllerTest,
       UpdateProfileStoreDoesnotAffectTheAccountStore) {
  SetupAccountPasswordStore(sync_service(), prefs());

  EXPECT_CALL(*delegate(), GetPendingPassword())
      .WillOnce(ReturnRef(pending_password()));
  std::vector<std::unique_ptr<password_manager::PasswordForm>> forms;
  auto form =
      std::make_unique<password_manager::PasswordForm>(pending_password());
  form->password_value = u"old_password";
  form->in_store = password_manager::PasswordForm::Store::kProfileStore;
  forms.push_back(std::move(form));
  EXPECT_CALL(*delegate(), GetCurrentForms()).WillOnce(ReturnRef(forms));
  SetUpWithState(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
                 PasswordBubbleControllerBase::DisplayReason::kAutomatic);
  EXPECT_FALSE(
      controller()->IsCurrentStateAffectingPasswordsStoredInTheGoogleAccount());
}

TEST_F(SaveUpdateBubbleControllerTest, UpdateBothStoresAffectsTheAccountStore) {
  SetupAccountPasswordStore(sync_service(), prefs());
  EXPECT_CALL(*delegate(), GetPendingPassword())
      .WillOnce(ReturnRef(pending_password()));

  std::vector<std::unique_ptr<password_manager::PasswordForm>> forms;
  auto profile_form =
      std::make_unique<password_manager::PasswordForm>(pending_password());
  profile_form->password_value = u"old_password";
  profile_form->in_store = password_manager::PasswordForm::Store::kProfileStore;
  forms.push_back(std::move(profile_form));

  auto account_form =
      std::make_unique<password_manager::PasswordForm>(pending_password());
  account_form->password_value = u"old_password";
  account_form->in_store = password_manager::PasswordForm::Store::kAccountStore;
  forms.push_back(std::move(account_form));

  EXPECT_CALL(*delegate(), GetCurrentForms()).WillOnce(ReturnRef(forms));
  SetUpWithState(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
                 PasswordBubbleControllerBase::DisplayReason::kAutomatic);
  EXPECT_TRUE(
      controller()->IsCurrentStateAffectingPasswordsStoredInTheGoogleAccount());
}

TEST_F(SaveUpdateBubbleControllerTest,
       SaveInAccountStoreAffectsTheAccountStore) {
  SetupAccountPasswordStore(sync_service(), prefs());
  ON_CALL(*password_feature_manager(), GetDefaultPasswordStore)
      .WillByDefault(
          Return(password_manager::PasswordForm::Store::kAccountStore));
  PretendPasswordWaiting();
  EXPECT_TRUE(
      controller()->IsCurrentStateAffectingPasswordsStoredInTheGoogleAccount());
}

TEST_F(SaveUpdateBubbleControllerTest,
       SaveInProfileStoreDoesntAffectTheAccountStore) {
  SetupAccountPasswordStore(sync_service(), prefs());
  ON_CALL(*password_feature_manager(), GetDefaultPasswordStore)
      .WillByDefault(
          Return(password_manager::PasswordForm::Store::kProfileStore));
  PretendPasswordWaiting();
  EXPECT_FALSE(
      controller()->IsCurrentStateAffectingPasswordsStoredInTheGoogleAccount());
}

TEST_F(SaveUpdateBubbleControllerTest, NullDelegate) {
  PasswordsModelDelegateMock delegate;
  EXPECT_CALL(delegate, GetPendingPassword())
      .WillOnce(ReturnRef(pending_password()));
  password_manager::InteractionsStats stats = GetTestStats();
  EXPECT_CALL(delegate, GetCurrentInteractionStats()).WillOnce(Return(&stats));
  std::vector<std::unique_ptr<password_manager::PasswordForm>> forms =
      GetCurrentForms();
  EXPECT_CALL(delegate, GetCurrentForms()).WillOnce(ReturnRef(forms));
  url::Origin origin = url::Origin::Create(GURL(kSiteOrigin));
  EXPECT_CALL(delegate, GetOrigin()).WillOnce(Return(origin));
  EXPECT_CALL(delegate, GetState())
      .WillRepeatedly(Return(password_manager::ui::PENDING_PASSWORD_STATE));
  EXPECT_CALL(delegate, GetWebContents()).WillRepeatedly(Return(nullptr));
  SaveUpdateBubbleController controller(
      delegate.AsWeakPtr(),
      PasswordBubbleControllerBase::DisplayReason::kAutomatic);

  controller.OnBubbleClosing();

  EXPECT_FALSE(
      controller.IsCurrentStateAffectingPasswordsStoredInTheGoogleAccount());
}

TEST_F(SaveUpdateBubbleControllerTest, ShowsUpdateEvenIfNoExistingCredential) {
  EXPECT_CALL(*delegate(), GetPendingPassword())
      .WillOnce(ReturnRef(pending_password()));
  std::vector<std::unique_ptr<password_manager::PasswordForm>> empty_list;

  // PSL matches aren't included in GetCurrentForms(), return empty list to
  // emulate this.
  EXPECT_CALL(*delegate(), GetCurrentForms()).WillOnce(ReturnRef(empty_list));
  SetUpWithState(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE,
                 PasswordBubbleControllerBase::DisplayReason::kAutomatic);

  EXPECT_TRUE(controller()->IsCurrentStateUpdate());
}
