// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/save_update_bubble_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/metrics/histogram_samples.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_clock.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/common/credential_manager_types.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/account_info.h"
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
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

constexpr ukm::SourceId kTestSourceId = 0x1234;

constexpr char kSiteOrigin[] = "http://example.com/login";
constexpr char kUsername[] = "Admin";
constexpr char kUsernameExisting[] = "User";
constexpr char kUsernameNew[] = "User585";
constexpr char kPassword[] = "AdminPass";
constexpr char kPasswordEdited[] = "asDfjkl;";
constexpr char kUIDismissalReasonGeneralMetric[] =
    "PasswordManager.UIDismissalReason";
constexpr char kUIDismissalReasonSaveMetric[] =
    "PasswordManager.SaveUIDismissalReason";
constexpr char kUIDismissalReasonUpdateMetric[] =
    "PasswordManager.UpdateUIDismissalReason";

}  // namespace

class SaveUpdateBubbleControllerTest : public ::testing::Test {
 public:
  SaveUpdateBubbleControllerTest() {
    // If kEnablePasswordsAccountStorage is enabled, then
    // SaveUpdateWithAccountStoreBubbleController is used instead of this class.
    feature_list_.InitAndDisableFeature(
        password_manager::features::kEnablePasswordsAccountStorage);
  }
  ~SaveUpdateBubbleControllerTest() override = default;

  void SetUp() override {
    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
    ON_CALL(*mock_delegate_, GetPasswordFormMetricsRecorder())
        .WillByDefault(Return(nullptr));
    PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
        profile(),
        base::BindRepeating(
            &password_manager::BuildPasswordStore<
                content::BrowserContext,
                testing::StrictMock<password_manager::MockPasswordStore>>));
    pending_password_.url = GURL(kSiteOrigin);
    pending_password_.signon_realm = kSiteOrigin;
    pending_password_.username_value = base::ASCIIToUTF16(kUsername);
    pending_password_.password_value = base::ASCIIToUTF16(kPassword);
  }

  void TearDown() override {
    // Reset the delegate first. It can happen if the user closes the tab.
    mock_delegate_.reset();
    controller_.reset();
  }

  PrefService* prefs() { return profile_.GetPrefs(); }

  TestingProfile* profile() { return &profile_; }

  password_manager::MockPasswordStore* GetStore() {
    return static_cast<password_manager::MockPasswordStore*>(
        PasswordStoreFactory::GetInstance()
            ->GetForProfile(profile(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }

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
  base::test::ScopedFeatureList feature_list_;
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  std::unique_ptr<SaveUpdateBubbleController> controller_;
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
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
  if (state == password_manager::ui::PENDING_PASSWORD_STATE)
    histogram = kUIDismissalReasonSaveMetric;
  else if (state == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE)
    histogram = kUIDismissalReasonUpdateMetric;
  DestroyModelAndVerifyControllerExpectations();
  histogram_tester.ExpectUniqueSample(histogram, dismissal_reason, 1);
}

// static
password_manager::InteractionsStats
SaveUpdateBubbleControllerTest::GetTestStats() {
  password_manager::InteractionsStats result;
  result.origin_domain = GURL(kSiteOrigin).GetOrigin();
  result.username_value = base::ASCIIToUTF16(kUsername);
  result.dismissal_count = 5;
  result.update_time = base::Time::FromTimeT(1);
  return result;
}

std::vector<std::unique_ptr<password_manager::PasswordForm>>
SaveUpdateBubbleControllerTest::GetCurrentForms() const {
  password_manager::PasswordForm form(pending_password());
  form.username_value = base::ASCIIToUTF16(kUsernameExisting);
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

// Tests that the controller reads the value of
// ArePasswordsRevealedWhenBubbleIsOpened() before invoking OnBubbleShown()
// since the latter resets the value returned by the former. (crbug.com/1049085)
TEST_F(SaveUpdateBubbleControllerTest,
       ArePasswordsRevealedWhenBubbleIsOpenedBeforeOnBubbleShown) {
  {
    testing::InSequence s;
    EXPECT_CALL(*delegate(), ArePasswordsRevealedWhenBubbleIsOpened());
    EXPECT_CALL(*delegate(), OnBubbleShown());
  }
  PretendPasswordWaiting();
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
  EXPECT_CALL(*GetStore(), AddSiteStatsImpl(stats));
  EXPECT_CALL(*delegate(), OnNoInteraction());
  EXPECT_CALL(*delegate(), SavePassword(_, _)).Times(0);
  EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
  DestroyModelExpectReason(
      password_manager::metrics_util::NO_DIRECT_INTERACTION);
}

TEST_F(SaveUpdateBubbleControllerTest, ClickSave) {
  PretendPasswordWaiting();

  EXPECT_TRUE(controller()->enable_editing());
  EXPECT_FALSE(controller()->IsCurrentStateUpdate());

  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*delegate(), OnPasswordsRevealed()).Times(0);
  EXPECT_CALL(*delegate(), SavePassword(pending_password().username_value,
                                        pending_password().password_value));
  EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
  EXPECT_CALL(*delegate(), OnNopeUpdateClicked()).Times(0);
  controller()->OnSaveClicked();
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_ACCEPT);
}

TEST_F(SaveUpdateBubbleControllerTest, ClickSaveInUpdateState) {
  PretendUpdatePasswordWaiting();

  // Edit username, now it's a new credential.
  controller()->OnCredentialEdited(base::ASCIIToUTF16(kUsernameNew),
                                   base::ASCIIToUTF16(kPasswordEdited));
  EXPECT_FALSE(controller()->IsCurrentStateUpdate());

  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*delegate(), SavePassword(base::ASCIIToUTF16(kUsernameNew),
                                        base::ASCIIToUTF16(kPasswordEdited)));
  EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
  EXPECT_CALL(*delegate(), OnNopeUpdateClicked()).Times(0);
  controller()->OnSaveClicked();
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_ACCEPT);
}

TEST_F(SaveUpdateBubbleControllerTest, ClickNever) {
  PretendPasswordWaiting();

  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*delegate(), SavePassword(_, _)).Times(0);
  EXPECT_CALL(*delegate(), NeverSavePassword());
  controller()->OnNeverForThisSiteClicked();
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            controller()->state());
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_NEVER);
}

TEST_F(SaveUpdateBubbleControllerTest, ClickUpdate) {
  PretendUpdatePasswordWaiting();

  EXPECT_TRUE(controller()->enable_editing());
  EXPECT_TRUE(controller()->IsCurrentStateUpdate());

  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
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
  controller()->OnCredentialEdited(base::ASCIIToUTF16(kUsernameExisting),
                                   base::ASCIIToUTF16(kPasswordEdited));
  EXPECT_TRUE(controller()->IsCurrentStateUpdate());

  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*delegate(), SavePassword(base::ASCIIToUTF16(kUsernameExisting),
                                        base::ASCIIToUTF16(kPasswordEdited)));
  EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
  EXPECT_CALL(*delegate(), OnNopeUpdateClicked()).Times(0);
  controller()->OnSaveClicked();
  DestroyModelExpectReason(password_manager::metrics_util::CLICKED_ACCEPT);
}

TEST_F(SaveUpdateBubbleControllerTest, GetInitialUsername_MatchedUsername) {
  PretendUpdatePasswordWaiting();
  EXPECT_EQ(base::UTF8ToUTF16(kUsername),
            controller()->pending_password().username_value);
}

TEST_F(SaveUpdateBubbleControllerTest, EditCredential) {
  PretendPasswordWaiting();
  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));

  const std::u16string kExpectedUsername = u"new_username";
  const std::u16string kExpectedPassword = u"new_password";

  controller()->OnCredentialEdited(kExpectedUsername, kExpectedPassword);
  EXPECT_EQ(kExpectedUsername, controller()->pending_password().username_value);
  EXPECT_EQ(kExpectedPassword, controller()->pending_password().password_value);
  EXPECT_CALL(*delegate(), SavePassword(kExpectedUsername, kExpectedPassword));
  EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
  controller()->OnSaveClicked();
  DestroyModelAndVerifyControllerExpectations();
}

TEST_F(SaveUpdateBubbleControllerTest, SuppressSignInPromo) {
  prefs()->SetBoolean(password_manager::prefs::kSignInPasswordPromoRevive,
                      true);
  prefs()->SetBoolean(password_manager::prefs::kWasSignInPasswordPromoClicked,
                      true);
  PretendPasswordWaiting();
  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*delegate(), SavePassword(pending_password().username_value,
                                        pending_password().password_value));
  controller()->OnSaveClicked();

  EXPECT_FALSE(controller()->ReplaceToShowPromotionIfNeeded());
  DestroyModelAndVerifyControllerExpectations();
}

TEST_F(SaveUpdateBubbleControllerTest, SignInPromoOK) {
  PretendPasswordWaiting();
  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*delegate(), SavePassword(pending_password().username_value,
                                        pending_password().password_value));
  controller()->OnSaveClicked();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  EXPECT_FALSE(controller()->ReplaceToShowPromotionIfNeeded());
#else
  EXPECT_TRUE(controller()->ReplaceToShowPromotionIfNeeded());
#endif
}

#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(SaveUpdateBubbleControllerTest, SignInPromoCancel) {
  base::HistogramTester histogram_tester;
  PretendPasswordWaiting();
  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*delegate(), SavePassword(pending_password().username_value,
                                        pending_password().password_value));
  controller()->OnSaveClicked();

  EXPECT_TRUE(controller()->ReplaceToShowPromotionIfNeeded());
  DestroyModelAndVerifyControllerExpectations();
  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonSaveMetric,
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
}

TEST_F(SaveUpdateBubbleControllerTest, SignInPromoDismiss) {
  base::HistogramTester histogram_tester;
  PretendPasswordWaiting();
  EXPECT_CALL(*GetStore(), RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
  EXPECT_CALL(*delegate(), SavePassword(pending_password().username_value,
                                        pending_password().password_value));
  controller()->OnSaveClicked();

  EXPECT_TRUE(controller()->ReplaceToShowPromotionIfNeeded());
  DestroyModelAndVerifyControllerExpectations();
  histogram_tester.ExpectUniqueSample(
      kUIDismissalReasonSaveMetric,
      password_manager::metrics_util::CLICKED_ACCEPT, 1);
  EXPECT_FALSE(prefs()->GetBoolean(
      password_manager::prefs::kWasSignInPasswordPromoClicked));
}
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)

// Verify that URL keyed metrics are properly recorded.
TEST_F(SaveUpdateBubbleControllerTest, RecordUKMs) {
  using BubbleDismissalReason =
      password_manager::PasswordFormMetricsRecorder::BubbleDismissalReason;
  using BubbleTrigger =
      password_manager::PasswordFormMetricsRecorder::BubbleTrigger;
  using password_manager::metrics_util::CredentialSourceType;
  using UkmEntry = ukm::builders::PasswordForm;

  // |credential_management_api| defines whether credentials originate from the
  // credential management API.
  for (const bool credential_management_api : {false, true}) {
    // |update| defines whether this is an update or a save bubble.
    for (const bool update : {false, true}) {
      for (const auto interaction :
           {BubbleDismissalReason::kAccepted, BubbleDismissalReason::kDeclined,
            BubbleDismissalReason::kIgnored}) {
        SCOPED_TRACE(testing::Message()
                     << "update = " << update
                     << ", interaction = " << static_cast<int64_t>(interaction)
                     << ", credential management api ="
                     << credential_management_api);
        ukm::TestAutoSetUkmRecorder test_ukm_recorder;
        {
          // Setup metrics recorder
          auto recorder = base::MakeRefCounted<
              password_manager::PasswordFormMetricsRecorder>(
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

          if (update)
            PretendUpdatePasswordWaiting();
          else
            PretendPasswordWaiting();

          if (interaction == BubbleDismissalReason::kAccepted) {
            EXPECT_CALL(*GetStore(),
                        RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
            EXPECT_CALL(*delegate(),
                        SavePassword(pending_password().username_value,
                                     pending_password().password_value));
            controller()->OnSaveClicked();
          } else if (interaction == BubbleDismissalReason::kDeclined &&
                     update) {
            EXPECT_CALL(*delegate(), SavePassword(_, _)).Times(0);
            controller()->OnNopeUpdateClicked();
          } else if (interaction == BubbleDismissalReason::kDeclined &&
                     !update) {
            EXPECT_CALL(*GetStore(),
                        RemoveSiteStatsImpl(GURL(kSiteOrigin).GetOrigin()));
            EXPECT_CALL(*delegate(), SavePassword(_, _)).Times(0);
            EXPECT_CALL(*delegate(), NeverSavePassword());
            controller()->OnNeverForThisSiteClicked();
          } else if (interaction == BubbleDismissalReason::kIgnored && update) {
            EXPECT_CALL(*delegate(), SavePassword(_, _)).Times(0);
            EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
          } else if (interaction == BubbleDismissalReason::kIgnored &&
                     !update) {
            EXPECT_CALL(*GetStore(), AddSiteStatsImpl(testing::_));
            EXPECT_CALL(*delegate(), OnNoInteraction());
            EXPECT_CALL(*delegate(), SavePassword(_, _)).Times(0);
            EXPECT_CALL(*delegate(), NeverSavePassword()).Times(0);
          } else {
            NOTREACHED();
          }
          DestroyModelAndVerifyControllerExpectations();
        }

        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(delegate()));
        // Flush async calls on password store.
        base::RunLoop().RunUntilIdle();
        ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(GetStore()));

        // Verify metrics.
        const auto& entries =
            test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
        EXPECT_EQ(1u, entries.size());
        for (const auto* entry : entries) {
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
    }
  }
}

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
       PasswordBubbleControllerBase::DisplayReason::kAutomatic))
    SUCCEED();

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
  EXPECT_CALL(*delegate(), ArePasswordsRevealedWhenBubbleIsOpened())
      .WillOnce(Return(false));
  EXPECT_CALL(*delegate(), BubbleIsManualFallbackForSaving())
      .WillRepeatedly(Return(is_manual_fallback_for_saving));

  PretendPasswordWaiting(display_reason);
  bool reauth_expected = form_has_autofilled_value;
  if (!reauth_expected) {
    reauth_expected =
        !is_manual_fallback_for_saving &&
        display_reason ==
            PasswordBubbleControllerBase::DisplayReason::kUserAction;
  }
  EXPECT_EQ(reauth_expected,
            controller()->password_revealing_requires_reauth());

  // delegate()->AuthenticateUser() is called only when reauth is expected.
  EXPECT_CALL(*delegate(), AuthenticateUser())
      .Times(reauth_expected)
      .WillOnce(Return(!does_os_support_user_auth));

  if (reauth_expected) {
    EXPECT_EQ(controller()->RevealPasswords(), !does_os_support_user_auth);
  } else {
    EXPECT_TRUE(controller()->RevealPasswords());
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

TEST_F(SaveUpdateBubbleControllerTest, EyeIcon_BubbleReopenedAfterAuth) {
  // Checks re-authentication is not needed if the bubble is opened right after
  // successful authentication.
  pending_password().form_has_autofilled_value = true;
  // After successful authentication this value is set to true.
  EXPECT_CALL(*delegate(), ArePasswordsRevealedWhenBubbleIsOpened())
      .WillOnce(Return(true));
  PretendPasswordWaiting(
      PasswordBubbleControllerBase::DisplayReason::kUserAction);

  EXPECT_FALSE(controller()->password_revealing_requires_reauth());
  EXPECT_TRUE(controller()->RevealPasswords());
}

TEST_F(SaveUpdateBubbleControllerTest, PasswordsRevealedReported) {
  PretendPasswordWaiting();

  EXPECT_CALL(*delegate(), OnPasswordsRevealed());
  EXPECT_TRUE(controller()->RevealPasswords());
}

TEST_F(SaveUpdateBubbleControllerTest, PasswordsRevealedReportedAfterReauth) {
  // The bubble is opened after reauthentication and the passwords are revealed.
  pending_password().form_has_autofilled_value = true;
  // After successful authentication this value is set to true.
  EXPECT_CALL(*delegate(), ArePasswordsRevealedWhenBubbleIsOpened())
      .WillOnce(Return(true));
  EXPECT_CALL(*delegate(), OnPasswordsRevealed());
  PretendPasswordWaiting(
      PasswordBubbleControllerBase::DisplayReason::kUserAction);
}

TEST_F(SaveUpdateBubbleControllerTest, DisableEditing) {
  EXPECT_CALL(*delegate(), BubbleIsManualFallbackForSaving())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*delegate(), GetCredentialSource())
      .WillOnce(Return(password_manager::metrics_util::CredentialSourceType::
                           kCredentialManagementAPI));
  PretendPasswordWaiting();
  EXPECT_FALSE(controller()->enable_editing());
}
