// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_ui_controller.h"

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "build/build_config.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
#include "chrome/browser/password_manager/password_change_delegate.h"
#include "chrome/browser/password_manager/password_change_delegate_mock.h"
#include "chrome/browser/password_manager/password_change_service_factory.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/survey_config.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_controller.h"
#include "chrome/browser/ui/passwords/credential_manager_dialog_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/browser/ui/views/frame/test_with_browser_view.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/affiliations/core/browser/mock_affiliation_service.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/leak_detection_dialog_utils.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/move_password_to_account_store_helper.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/interactions_stats.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/undo_password_change_controller.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_utils.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
#include "components/device_reauth/mock_device_authenticator.h"
#endif

using password_manager::MockPasswordFormManagerForUI;
using password_manager::MockPasswordStoreInterface;
using password_manager::PasswordForm;
using ReauthSucceeded =
    password_manager::PasswordManagerClient::ReauthSucceeded;
using InsecureType = password_manager::InsecureType;
using password_manager::InsecurityMetadata;
using password_manager::PasswordForm;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::AtMost;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Exactly;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;

namespace views {
class Widget;
}
namespace {

MATCHER_P3(MatchesLoginAndURL,
           username,
           password,
           url,
           "matches username, password and url") {
  return arg.username_value == username && arg.password_value == password &&
         arg.url == url;
}

// A random URL.
constexpr char kExampleUrl[] = "http://example.com";
constexpr char16_t kExampleUsername[] = u"Bob";
constexpr char16_t kExamplePassword[] = u"pass";

constexpr char kExampleRpId[] = "example.com";

// Number of dismissals that for sure suppresses the bubble.
constexpr int kGreatDissmisalCount = 10;

class CredentialManagementDialogPromptMock : public AccountChooserPrompt,
                                             public AutoSigninFirstRunPrompt {
 public:
  MOCK_METHOD(void, ShowAccountChooser, (), (override));
  MOCK_METHOD(void, ShowAutoSigninPrompt, (), (override));
  MOCK_METHOD(void, ControllerGone, (), (override));
};

class PasswordLeakDialogMock : public CredentialLeakPrompt {
 public:
  MOCK_METHOD(void, ShowCredentialLeakPrompt, (), (override));
  MOCK_METHOD(views::Widget*, GetWidgetForTesting, (), (override));
};

class TestManagePasswordsIconView : public ManagePasswordsIconView {
 public:
  void SetState(password_manager::ui::State state,
                bool is_blocklisted) override {
    state_ = state;
    is_blocklisted_ = is_blocklisted;
  }
  password_manager::ui::State state() { return state_; }

 private:
  password_manager::ui::State state_;
  bool is_blocklisted_ = false;
};

class TestPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  TestPasswordManagerClient()
      : mock_profile_store_(new MockPasswordStoreInterface()) {}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  MOCK_METHOD(std::unique_ptr<device_reauth::DeviceAuthenticator>,
              GetDeviceAuthenticator,
              (),
              (override));
#endif

  MockPasswordStoreInterface* GetProfilePasswordStore() const override {
    return mock_profile_store_.get();
  }

 private:
  scoped_refptr<MockPasswordStoreInterface> mock_profile_store_;
};

// This subclass is used to disable some code paths which are not essential for
// testing.
class TestManagePasswordsUIController : public ManagePasswordsUIController {
 public:
  TestManagePasswordsUIController(
      content::WebContents* contents,
      password_manager::PasswordManagerClient* client);

  bool opened_automatic_bubble() const { return opened_automatic_bubble_; }

  MOCK_METHOD(AccountChooserPrompt*,
              CreateAccountChooser,
              (CredentialManagerDialogController*),
              (override));
  MOCK_METHOD(AutoSigninFirstRunPrompt*,
              CreateAutoSigninPrompt,
              (CredentialManagerDialogController*),
              (override));
  MOCK_METHOD(std::unique_ptr<CredentialLeakPrompt>,
              CreateCredentialLeakPrompt,
              (CredentialLeakDialogController*),
              (override));
  MOCK_METHOD(bool, HasBrowserWindow, (), (override, const));
  MOCK_METHOD(void, OnUpdateBubbleAndIconVisibility, (), ());
  MOCK_METHOD(
      std::unique_ptr<password_manager::MovePasswordToAccountStoreHelper>,
      CreateMovePasswordToAccountStoreHelper,
      (const password_manager::PasswordForm& form,
       password_manager::metrics_util::MoveToAccountStoreTrigger trigger,
       base::OnceCallback<void()>),
      (override));

 private:
  void UpdateBubbleAndIconVisibility() override;
  void HideBubble(bool initiated_by_bubble_manager) override;

  bool opened_automatic_bubble_ = false;
};

TestManagePasswordsUIController::TestManagePasswordsUIController(
    content::WebContents* contents,
    password_manager::PasswordManagerClient* client)
    : ManagePasswordsUIController(contents) {
  // Do not silently replace an existing ManagePasswordsUIController because it
  // unregisters itself in WebContentsDestroyed().
  EXPECT_FALSE(contents->GetUserData(UserDataKey()));
  contents->SetUserData(UserDataKey(), base::WrapUnique(this));
  set_client(client);
}

void TestManagePasswordsUIController::UpdateBubbleAndIconVisibility() {
  opened_automatic_bubble_ = IsAutomaticallyOpeningBubble();
  ManagePasswordsUIController::UpdateBubbleAndIconVisibility();
  OnUpdateBubbleAndIconVisibility();
  TestManagePasswordsIconView view;
  UpdateIconAndBubbleState(&view);
  if (opened_automatic_bubble_) {
    OnBubbleShown();
  }
}

void TestManagePasswordsUIController::HideBubble(
    bool initiated_by_bubble_manager) {
  opened_automatic_bubble_ = false;
  if (std::exchange(opened_automatic_bubble_, false) &&
      !web_contents()->IsBeingDestroyed()) {
    OnBubbleHidden();
  }
}

password_manager::PasswordForm CreateInsecureCredential(PasswordForm form) {
  form.password_issues.insert(
      {InsecureType::kLeaked,
       InsecurityMetadata(
           base::Time(), password_manager::IsMuted(false),
           password_manager::TriggerBackendNotification(false))});
  return form;
}

std::unique_ptr<MockPasswordFormManagerForUI> CreateFormManagerWithBestMatches(
    const std::vector<PasswordForm>& best_matches,
    const PasswordForm* password_form,
    bool is_blocklisted = false) {
  auto form_manager =
      std::make_unique<testing::StrictMock<MockPasswordFormManagerForUI>>();
  EXPECT_CALL(*form_manager, GetBestMatches())
      .Times(AtMost(3))
      .WillRepeatedly(Return(best_matches));
  EXPECT_CALL(*form_manager, GetFederatedMatches())
      .Times(AtMost(2))
      .WillRepeatedly(
          Return(base::span<const password_manager::PasswordForm>()));
  EXPECT_CALL(*form_manager, GetURL())
      .Times(AtMost(2))
      .WillRepeatedly(ReturnRef(password_form->url));
  EXPECT_CALL(*form_manager, IsBlocklisted())
      .WillRepeatedly(Return(is_blocklisted));
  EXPECT_CALL(*form_manager, GetInteractionsStats())
      .Times(AtMost(1))
      .WillOnce(
          Return(base::span<const password_manager::InteractionsStats>()));
  EXPECT_CALL(*form_manager, GetInsecureCredentials())
      .Times(AtMost(1))
      .WillOnce(Return(base::span<const password_manager::PasswordForm>()));
  EXPECT_CALL(*form_manager, GetPendingCredentials())
      .WillRepeatedly(ReturnRef(*password_form));
  EXPECT_CALL(*form_manager, GetMetricsRecorder())
      .WillRepeatedly(Return(nullptr));
  return form_manager;
}

class MockPasswordChangeService : public ChromePasswordChangeService {
 public:
  MockPasswordChangeService()
      : ChromePasswordChangeService(/*pref_service*/ nullptr,
                                    /*affiliation_service=*/nullptr,
                                    /*optimization_keyed_service=*/nullptr,
                                    /*settings_service=*/nullptr,
                                    /*feature_manager=*/nullptr,
                                    /*log_router*/ nullptr) {}

  MOCK_METHOD(void,
              OfferPasswordChangeUi,
              (password_manager::PasswordForm, content::WebContents*),
              (override));
  MOCK_METHOD(PasswordChangeDelegate*,
              GetPasswordChangeDelegate,
              (content::WebContents*),
              (override));
};

password_manager::PasswordForm CreatePasswordForm(
    const std::string& url,
    const std::u16string& username,
    const std::u16string& password) {
  PasswordForm password_form;
  password_form.url = GURL(url);
  password_form.signon_realm = GURL(url).GetWithEmptyPath().spec();
  password_form.username_value = username;
  password_form.password_value = password;
  return password_form;
}

password_manager::PasswordForm CreateSignUpForm(
    const std::string& url,
    const std::u16string& username,
    const std::u16string& password) {
  PasswordForm password_form = CreatePasswordForm(url, username, password);

  // HasNewPasswordElement() && HasUsernameElement()
  password_form.new_password_element_renderer_id = autofill::FieldRendererId(1);
  password_form.username_element_renderer_id = autofill::FieldRendererId(2);

  return password_form;
}

}  // namespace

class ManagePasswordsUIControllerTest : public base::test::WithFeatureOverride,
                                        public ChromeRenderViewHostTestHarness {
 public:
  ManagePasswordsUIControllerTest()
      : base::test::WithFeatureOverride(
            autofill::features::kAutofillShowBubblesBasedOnPriorities),
        ChromeRenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

  void SetUp() override;

  TestPasswordManagerClient& client() { return client_; }
  PasswordForm& test_local_form() { return test_local_form_; }
  PasswordForm& test_federated_form() { return test_federated_form_; }
  PasswordForm& submitted_form() { return submitted_form_; }
  CredentialManagementDialogPromptMock& dialog_prompt() {
    return dialog_prompt_;
  }

  TestManagePasswordsUIController* controller() {
    return static_cast<TestManagePasswordsUIController*>(
        ManagePasswordsUIController::FromWebContents(web_contents()));
  }

  void ExpectIconStateIs(password_manager::ui::State state);
  void ExpectIconAndControllerStateIs(password_manager::ui::State state);

  // Tests that the state is not changed when the password is autofilled.
  void TestNotChangingStateOnAutofill(password_manager::ui::State state);

  void WaitForPasswordStore();

 private:
  TestPasswordManagerClient client_;

  PasswordForm test_local_form_;
  PasswordForm test_federated_form_;
  PasswordForm submitted_form_;
  CredentialManagementDialogPromptMock dialog_prompt_;
};

void ManagePasswordsUIControllerTest::SetUp() {
  ChromeRenderViewHostTestHarness::SetUp();

  // Create the test UIController here so that it's bound to
  // |test_web_contents_|, and will be retrieved correctly via
  // ManagePasswordsUIController::FromWebContents in |controller()|.
  new ::testing::NiceMock<TestManagePasswordsUIController>(web_contents(),
                                                           &client_);

  test_local_form_.url = GURL("http://example.com/login");
  test_local_form_.signon_realm =
      test_local_form_.url.DeprecatedGetOriginAsURL().spec();
  test_local_form_.username_value = u"username";
  test_local_form_.username_element = u"username_element";
  test_local_form_.password_value = u"12345";
  test_local_form_.password_element = u"password_element";
  test_local_form_.match_type = PasswordForm::MatchType::kExact;

  test_federated_form_.url = GURL("http://example.com/login");
  test_federated_form_.signon_realm =
      test_federated_form_.url.DeprecatedGetOriginAsURL().spec();
  test_federated_form_.username_value = u"username";
  test_federated_form_.federation_origin =
      url::SchemeHostPort(GURL("https://federation.test/"));
  test_federated_form_.match_type = PasswordForm::MatchType::kExact;

  submitted_form_ = test_local_form_;
  submitted_form_.username_value = u"submitted_username";
  submitted_form_.password_value = u"pass12345";

  // We need to be on a "webby" URL for most tests.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             GURL(kExampleUrl));
}

void ManagePasswordsUIControllerTest::ExpectIconStateIs(
    password_manager::ui::State state) {
  TestManagePasswordsIconView view;
  controller()->UpdateIconAndBubbleState(&view);
  EXPECT_EQ(state, view.state());
}

void ManagePasswordsUIControllerTest::ExpectIconAndControllerStateIs(
    password_manager::ui::State state) {
  ExpectIconStateIs(state);
  EXPECT_EQ(state, controller()->GetState());
}

void ManagePasswordsUIControllerTest::TestNotChangingStateOnAutofill(
    password_manager::ui::State state) {
  DCHECK(state == password_manager::ui::PENDING_PASSWORD_STATE ||
         state == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
         state == password_manager::ui::SAVE_CONFIRMATION_STATE);

  // Set the bubble state to |state|.
  std::vector<PasswordForm> best_matches;
  std::unique_ptr<MockPasswordFormManagerForUI> test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  if (state == password_manager::ui::PENDING_PASSWORD_STATE) {
    controller()->OnPasswordSubmitted(std::move(test_form_manager));
  } else if (state == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE) {
    best_matches.push_back(test_local_form());
    controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));
  } else {  // password_manager::ui::SAVE_CONFIRMATION_STATE
    controller()->OnAutomaticPasswordSave(std::move(test_form_manager),
                                          /*is_update_confirmation=*/false);
  }
  ASSERT_EQ(state, controller()->GetState());

  // Autofill happens.
  std::vector<PasswordForm> forms = {test_local_form()};
  controller()->OnPasswordAutofilled(
      forms, url::Origin::Create(forms.front().url), {});

  // State shouldn't changed.
  ExpectIconAndControllerStateIs(state);
}

void ManagePasswordsUIControllerTest::WaitForPasswordStore() {
  task_environment()->RunUntilIdle();
}

TEST_P(ManagePasswordsUIControllerTest, DefaultState) {
  EXPECT_TRUE(controller()->GetOrigin().opaque());

  ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, PasswordAutofilled) {
  std::vector<PasswordForm> forms = {test_local_form()};
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordAutofilled(
      forms, url::Origin::Create(forms.front().url), {});

  EXPECT_EQ(url::Origin::Create(test_local_form().url),
            controller()->GetOrigin());
  ASSERT_EQ(1u, controller()->GetCurrentForms().size());
  EXPECT_EQ(test_local_form().username_value,
            controller()->GetCurrentForms()[0]->username_value);

  // Controller should store a separate copy of the form as it doesn't own it.
  EXPECT_NE(&test_local_form(), controller()->GetCurrentForms()[0].get());

  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, PasswordSubmitted) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  EXPECT_EQ(url::Origin::Create(test_local_form().url),
            controller()->GetOrigin());

  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, BlocklistedFormPasswordSubmitted) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(
      best_matches, &submitted_form(), /*is_blocklisted=*/true);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_FALSE(controller()->opened_automatic_bubble());

  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, PasswordSubmittedBubbleSuppressed) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  std::vector<password_manager::InteractionsStats> stats(1);
  stats[0].origin_domain = submitted_form().url.DeprecatedGetOriginAsURL();
  stats[0].username_value = submitted_form().username_value;
  stats[0].dismissal_count = kGreatDissmisalCount;
  EXPECT_CALL(*test_form_manager, GetInteractionsStats)
      .WillRepeatedly(Return(stats));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_FALSE(controller()->opened_automatic_bubble());
  ASSERT_TRUE(controller()->GetCurrentInteractionStats());
  EXPECT_EQ(stats[0], *controller()->GetCurrentInteractionStats());

  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, PasswordSubmittedBubbleNotSuppressed) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  std::vector<password_manager::InteractionsStats> stats(1);
  stats[0].origin_domain = submitted_form().url.DeprecatedGetOriginAsURL();
  stats[0].username_value = u"not my username";
  stats[0].dismissal_count = kGreatDissmisalCount;
  EXPECT_CALL(*test_form_manager, GetInteractionsStats)
      .WillRepeatedly(Return(stats));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  EXPECT_FALSE(controller()->GetCurrentInteractionStats());

  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, PasswordSubmittedBubbleCancelled) {
  // Test on the real controller.
  std::unique_ptr<content::WebContents> web_content(CreateTestWebContents());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_content.get(),
                                                             GURL(kExampleUrl));
  ManagePasswordsUIController::CreateForWebContents(web_content.get());
  ManagePasswordsUIController* controller =
      ManagePasswordsUIController::FromWebContents(web_content.get());
  controller->set_client(&client());

  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  // The bubble is ready to open but the tab is inactive. Therefore, we don't
  // call UpdateIconAndBubbleState here.
  controller->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_TRUE(controller->IsAutomaticallyOpeningBubble());

  // The tab navigated in background. Because the controller's state has changed
  // the bubble shouldn't pop up anymore.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_content.get(), GURL("http://google.com"));
  EXPECT_FALSE(controller->IsAutomaticallyOpeningBubble());
}

TEST_P(ManagePasswordsUIControllerTest, PasswordSaved) {
  auto* mock_sentiment_service_ = static_cast<MockTrustSafetySentimentService*>(
      TrustSafetySentimentServiceFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              profile(),
              base::BindRepeating(&BuildMockTrustSafetySentimentService)));
  EXPECT_CALL(*mock_sentiment_service_, SavedPassword());

  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*test_form_manager, Save());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  base::HistogramTester histogram_tester;
  controller()->SavePassword(submitted_form().username_value,
                             submitted_form().password_value);
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
  histogram_tester.ExpectTotalCount(
      "PasswordManager.PasswordChangeRecoveryFlow", 0);
}

TEST_P(ManagePasswordsUIControllerTest, BackupPasswordSaved) {
  using UkmEntry = ukm::builders::PasswordManager_ChangeRecovery;
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  auto* mock_sentiment_service_ = static_cast<MockTrustSafetySentimentService*>(
      TrustSafetySentimentServiceFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              profile(),
              base::BindRepeating(&BuildMockTrustSafetySentimentService)));
  EXPECT_CALL(*mock_sentiment_service_, SavedPassword());
  MockHatsService* mock_hats_service = static_cast<MockHatsService*>(
      HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
          profile(), base::BindRepeating(&BuildMockHatsService)));
  EXPECT_CALL(*mock_hats_service, CanShowAnySurvey)
      .WillRepeatedly(Return(true));
  const std::u16string backup_password = u"backup";
  PasswordForm submitted_form;
  submitted_form.username_value = kExampleUsername;
  submitted_form.password_value = backup_password;
  PasswordForm stored_matching_form;
  stored_matching_form.username_value = kExampleUsername;
  stored_matching_form.password_value = kExamplePassword;
  stored_matching_form.SetPasswordBackupNote(backup_password);
  stored_matching_form.type =
      password_manager::PasswordForm::Type::kChangeSubmission;
  auto test_form_manager = CreateFormManagerWithBestMatches(
      /*best_matches=*/{stored_matching_form}, &submitted_form);

  EXPECT_CALL(*test_form_manager, Save());
  controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));

  EXPECT_CALL(*mock_hats_service, LaunchDelayedSurveyForWebContents(
                                      kHatsSurveyTriggerPasswordChangeDelayed,
                                      _, _, _, _, _, _, _, _, _));
  controller()->SavePassword(submitted_form.username_value,
                             submitted_form.password_value);
  // Advance the clock to trigger the delayed survey task and wait until it
  // actually launches.
  task_environment()->AdvanceClock(base::Seconds(2));
  task_environment()->RunUntilIdle();

  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChangeRecoveryFlow",
      password_manager::metrics_util::PasswordChangeRecoveryFlowState::
          kPrimaryPasswordUpdated,
      1);
  auto ukm_entries = test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
  EXPECT_EQ(1u, ukm_entries.size());
  ukm::TestUkmRecorder::ExpectEntryMetric(
      ukm_entries[0], UkmEntry::kPasswordChangeRecoveryFlowName,
      static_cast<int>(
          password_manager::metrics_util::PasswordChangeRecoveryFlowState::
              kPrimaryPasswordUpdated));
}

TEST_P(ManagePasswordsUIControllerTest, PhishedPasswordUpdated) {
  auto* mock_sentiment_service = static_cast<MockTrustSafetySentimentService*>(
      TrustSafetySentimentServiceFactory::GetInstance()
          ->SetTestingFactoryAndUse(
              profile(),
              base::BindRepeating(&BuildMockTrustSafetySentimentService)));

  submitted_form().password_issues.insert(
      {InsecureType::kPhished, InsecurityMetadata()});
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*test_form_manager, Save());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_CALL(*mock_sentiment_service, PhishedPasswordUpdateFinished());
  controller()->SavePassword(submitted_form().username_value,
                             submitted_form().password_value);
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, PasswordSavedUKMRecording) {
  using UkmEntry = ukm::builders::PasswordForm;
  const struct {
    // Whether to simulate editing the username or picking a different password.
    bool edit_username;
    bool change_password;
    // The UMA sample expected for PasswordManager.EditsInSaveBubble.
    base::HistogramBase::Sample32 expected_uma_sample;
  } kTests[] = {
      {false, false, 0}, {true, false, 1}, {false, true, 2}, {true, true, 3}};

  for (const auto& test : kTests) {
    SCOPED_TRACE(testing::Message()
                 << "edit_username = " << test.edit_username
                 << ", change_password = " << test.change_password);
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder test_ukm_recorder;

    // Setup metrics recorder.
    ukm::SourceId source_id = test_ukm_recorder.GetNewSourceID();
    auto recorder =
        base::MakeRefCounted<password_manager::PasswordFormMetricsRecorder>(
            true /*is_main_frame_secure*/, source_id, /*pref_service=*/nullptr);

    // Exercise controller.
    std::vector<PasswordForm> best_matches;
    auto test_form_manager =
        CreateFormManagerWithBestMatches(best_matches, &submitted_form());
    EXPECT_CALL(*test_form_manager, GetMetricsRecorder)
        .WillRepeatedly(Return(recorder.get()));
    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
    EXPECT_CALL(*test_form_manager,
                OnUpdateUsernameFromPrompt(std::u16string(u"other_username")))
        .Times(test.edit_username);
    EXPECT_CALL(*test_form_manager,
                OnUpdatePasswordFromPrompt(std::u16string(u"other_pwd")))
        .Times(test.change_password);
    EXPECT_CALL(*test_form_manager, Save());
    controller()->OnPasswordSubmitted(std::move(test_form_manager));

    controller()->SavePassword(
        test.edit_username ? u"other_username"
                           : submitted_form().username_value,
        test.change_password ? u"other_pwd" : submitted_form().password_value);
    ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);

    // Fake navigation so that the old form manager gets destroyed and
    // reports its metrics. Need to close the bubble, otherwise the bubble
    // state is retained on navigation, and the PasswordFormManager is not
    // destroyed.
    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
    controller()->OnBubbleHidden();
    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL(kExampleUrl));

    recorder = nullptr;
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(controller()));

    // Verify metrics.
    const auto& entries =
        test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const ukm::mojom::UkmEntry* entry : entries) {
      EXPECT_EQ(source_id, entry->source_id);

      if (test.edit_username) {
        test_ukm_recorder.ExpectEntryMetric(
            entry, UkmEntry::kUser_Action_EditedUsernameInBubbleName, 1u);
      } else {
        EXPECT_FALSE(test_ukm_recorder.EntryHasMetric(
            entry, UkmEntry::kUser_Action_EditedUsernameInBubbleName));
      }

      if (test.change_password) {
        test_ukm_recorder.ExpectEntryMetric(
            entry, UkmEntry::kUser_Action_SelectedDifferentPasswordInBubbleName,
            1u);
      } else {
        EXPECT_FALSE(test_ukm_recorder.EntryHasMetric(
            entry,
            UkmEntry::kUser_Action_SelectedDifferentPasswordInBubbleName));
      }
    }

    histogram_tester.ExpectUniqueSample("PasswordManager.EditsInSaveBubble",
                                        test.expected_uma_sample, 1);
  }
}

TEST_P(ManagePasswordsUIControllerTest, PasswordBlocklisted) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*test_form_manager, OnNeverClicked());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->NeverSavePassword();
  ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest,
       PasswordBlocklistedWithExistingCredentials) {
  std::vector<PasswordForm> best_matches = {test_local_form()};
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*test_form_manager, OnNeverClicked());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->NeverSavePassword();
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, NormalNavigations) {
  std::vector<PasswordForm> best_matches = {test_local_form()};
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);

  // Fake-navigate. We expect the bubble's state to persist so a user reasonably
  // has been able to interact with the bubble. This happens on
  // `accounts.google.com`, for instance.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             GURL(kExampleUrl));
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, NormalNavigationsClosedBubble) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*test_form_manager, Save());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  controller()->SavePassword(submitted_form().username_value,
                             submitted_form().password_value);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnBubbleHidden();
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);

  // Fake-navigate. There is no bubble, reset the state.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             GURL(kExampleUrl));
  ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, PasswordSubmittedToNonWebbyURL) {
  // Navigate to a non-webby URL, then see what happens!
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      web_contents(), GURL("chrome://sign-in"));

  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_TRUE(controller()->GetOrigin().opaque());

  ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest,
       OnBiometricAuthTransitionWhenStateInactive) {
  ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
  controller()->OnBiometricAuthenticationForFilling(profile()->GetPrefs());
  ASSERT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
}

TEST_P(ManagePasswordsUIControllerTest, BlocklistedElsewhere) {
  std::u16string kTestUsername = u"test_username";
  std::vector<PasswordForm> forms;
  forms.push_back(test_local_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordAutofilled(
      forms, url::Origin::Create(forms.front().url), {});

  test_local_form().blocked_by_user = true;
  password_manager::PasswordStoreChange change(
      password_manager::PasswordStoreChange::ADD, test_local_form());
  password_manager::PasswordStoreChangeList list(1, change);
  controller()->OnLoginsChanged(/*unused source store:*/ nullptr, list);

  EXPECT_EQ(url::Origin::Create(test_local_form().url),
            controller()->GetOrigin());

  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, AutomaticPasswordSave) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnAutomaticPasswordSave(std::move(test_form_manager),
                                        /*is_update_confirmation=*/false);
  EXPECT_EQ(password_manager::ui::SAVE_CONFIRMATION_STATE,
            controller()->GetState());

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnBubbleHidden();
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, ChooseCredentialLocal) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(test_local_form()));
  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  CredentialManagerDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*controller(), CreateAccountChooser(_))
      .WillOnce(
          DoAll(SaveArg<0>(&dialog_controller), Return(&dialog_prompt())));
  EXPECT_CALL(dialog_prompt(), ShowAccountChooser());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*controller(), HasBrowserWindow()).WillOnce(Return(true));
  base::MockCallback<ManagePasswordsState::CredentialsCallback> choose_callback;
  EXPECT_TRUE(controller()->OnChooseCredentials(std::move(local_credentials),
                                                origin, choose_callback.Get()));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());
  EXPECT_EQ(origin, controller()->GetOrigin());
  EXPECT_THAT(controller()->GetCurrentForms(),
              ElementsAre(Pointee(test_local_form())));
  ASSERT_THAT(dialog_controller->GetLocalForms(),
              ElementsAre(Pointee(test_local_form())));
  ExpectIconStateIs(password_manager::ui::INACTIVE_STATE);

  EXPECT_CALL(dialog_prompt(), ControllerGone());
  EXPECT_CALL(choose_callback, Run(Pointee(test_local_form())));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  dialog_controller->OnChooseCredentials(
      *dialog_controller->GetLocalForms()[0],
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());
}

TEST_P(ManagePasswordsUIControllerTest, ChooseCredentialLocalButFederated) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(test_federated_form()));
  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  CredentialManagerDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*controller(), CreateAccountChooser(_))
      .WillOnce(
          DoAll(SaveArg<0>(&dialog_controller), Return(&dialog_prompt())));
  EXPECT_CALL(dialog_prompt(), ShowAccountChooser());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*controller(), HasBrowserWindow()).WillOnce(Return(true));
  base::MockCallback<ManagePasswordsState::CredentialsCallback> choose_callback;
  EXPECT_TRUE(controller()->OnChooseCredentials(std::move(local_credentials),
                                                origin, choose_callback.Get()));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());
  EXPECT_EQ(origin, controller()->GetOrigin());
  EXPECT_THAT(controller()->GetCurrentForms(),
              ElementsAre(Pointee(test_federated_form())));
  ASSERT_THAT(dialog_controller->GetLocalForms(),
              ElementsAre(Pointee(test_federated_form())));
  ExpectIconStateIs(password_manager::ui::INACTIVE_STATE);

  EXPECT_CALL(dialog_prompt(), ControllerGone());
  EXPECT_CALL(choose_callback, Run(Pointee(test_federated_form())));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  dialog_controller->OnChooseCredentials(
      *dialog_controller->GetLocalForms()[0],
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());
}

TEST_P(ManagePasswordsUIControllerTest, ChooseCredentialCancel) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(test_local_form()));
  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  CredentialManagerDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*controller(), CreateAccountChooser(_))
      .WillOnce(
          DoAll(SaveArg<0>(&dialog_controller), Return(&dialog_prompt())));
  EXPECT_CALL(dialog_prompt(), ShowAccountChooser());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*controller(), HasBrowserWindow()).WillOnce(Return(true));
  base::MockCallback<ManagePasswordsState::CredentialsCallback> choose_callback;
  EXPECT_TRUE(controller()->OnChooseCredentials(std::move(local_credentials),
                                                origin, choose_callback.Get()));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());
  EXPECT_EQ(origin, controller()->GetOrigin());

  EXPECT_CALL(dialog_prompt(), ControllerGone()).Times(0);
  EXPECT_CALL(choose_callback, Run(nullptr));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  dialog_controller->OnCloseDialog();
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());
}

TEST_P(ManagePasswordsUIControllerTest, ChooseCredentialPrefetch) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(test_local_form()));
  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));

  // Simulate requesting a credential during prefetch. The tab has no associated
  // browser. Nothing should happen.
  EXPECT_CALL(*controller(), HasBrowserWindow()).WillOnce(Return(false));
  base::MockCallback<ManagePasswordsState::CredentialsCallback> choose_callback;
  EXPECT_FALSE(controller()->OnChooseCredentials(
      std::move(local_credentials), origin, choose_callback.Get()));
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
}

TEST_P(ManagePasswordsUIControllerTest, ChooseCredentialPSL) {
  test_local_form().match_type = PasswordForm::MatchType::kPSL;
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(test_local_form()));
  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  CredentialManagerDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*controller(), CreateAccountChooser(_))
      .WillOnce(
          DoAll(SaveArg<0>(&dialog_controller), Return(&dialog_prompt())));
  EXPECT_CALL(dialog_prompt(), ShowAccountChooser());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*controller(), HasBrowserWindow()).WillOnce(Return(true));
  base::MockCallback<ManagePasswordsState::CredentialsCallback> choose_callback;
  EXPECT_TRUE(controller()->OnChooseCredentials(std::move(local_credentials),
                                                origin, choose_callback.Get()));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());
  EXPECT_EQ(origin, controller()->GetOrigin());
  EXPECT_THAT(controller()->GetCurrentForms(), IsEmpty());
  ASSERT_THAT(dialog_controller->GetLocalForms(),
              ElementsAre(Pointee(test_local_form())));
  ExpectIconStateIs(password_manager::ui::INACTIVE_STATE);

  EXPECT_CALL(dialog_prompt(), ControllerGone());
  EXPECT_CALL(choose_callback, Run(Pointee(test_local_form())));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  dialog_controller->OnChooseCredentials(
      *dialog_controller->GetLocalForms()[0],
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());
  EXPECT_THAT(controller()->GetCurrentForms(), IsEmpty());
}

TEST_P(ManagePasswordsUIControllerTest, AutoSignin) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(test_local_form()));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnAutoSignin(std::move(local_credentials),
                             url::Origin::Create(test_local_form().url));
  EXPECT_EQ(url::Origin::Create(test_local_form().url),
            controller()->GetOrigin());
  ASSERT_FALSE(controller()->GetCurrentForms().empty());
  EXPECT_EQ(test_local_form(), *controller()->GetCurrentForms()[0]);
  ExpectIconAndControllerStateIs(password_manager::ui::AUTO_SIGNIN_STATE);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnBubbleHidden();
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, AutoSigninFirstRun) {
  EXPECT_CALL(*controller(), CreateAutoSigninPrompt(_))
      .WillOnce(Return(&dialog_prompt()));
  EXPECT_CALL(dialog_prompt(), ShowAutoSigninPrompt());
  controller()->OnPromptEnableAutoSignin();

  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
  EXPECT_CALL(dialog_prompt(), ControllerGone());
}

TEST_P(ManagePasswordsUIControllerTest, AutoSigninFirstRunAfterAutofill) {
  // Setup the managed state first.
  const PasswordForm* test_form_ptr = &test_local_form();
  const std::u16string kTestUsername = test_form_ptr->username_value;
  std::vector<PasswordForm> forms = {*test_form_ptr};
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordAutofilled(
      forms, url::Origin::Create(test_form_ptr->url), {});
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());

  // Pop up the autosignin promo. The state should stay intact.
  EXPECT_CALL(*controller(), CreateAutoSigninPrompt(_))
      .WillOnce(Return(&dialog_prompt()));
  EXPECT_CALL(dialog_prompt(), ShowAutoSigninPrompt());
  controller()->OnPromptEnableAutoSignin();

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());
  EXPECT_EQ(url::Origin::Create(test_form_ptr->url), controller()->GetOrigin());
  EXPECT_THAT(controller()->GetCurrentForms(),
              ElementsAre(Pointee(*test_form_ptr)));
  EXPECT_CALL(dialog_prompt(), ControllerGone());
}

TEST_P(ManagePasswordsUIControllerTest, AutoSigninFirstRunAfterNavigation) {
  // Pop up the autosignin promo.
  EXPECT_CALL(*controller(), CreateAutoSigninPrompt(_))
      .WillOnce(Return(&dialog_prompt()));
  EXPECT_CALL(dialog_prompt(), ShowAutoSigninPrompt());
  controller()->OnPromptEnableAutoSignin();

  // The dialog should survive any navigation.
  EXPECT_CALL(dialog_prompt(), ControllerGone()).Times(0);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents(),
                                                             GURL(kExampleUrl));
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&dialog_prompt()));
  EXPECT_CALL(dialog_prompt(), ControllerGone());
}

TEST_P(ManagePasswordsUIControllerTest, AutofillDuringAutoSignin) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(test_local_form()));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnAutoSignin(std::move(local_credentials),
                             url::Origin::Create(test_local_form().url));
  ExpectIconAndControllerStateIs(password_manager::ui::AUTO_SIGNIN_STATE);
  std::vector<PasswordForm> forms;
  std::u16string kTestUsername = test_local_form().username_value;
  forms.push_back(test_local_form());
  controller()->OnPasswordAutofilled(
      forms, url::Origin::Create(forms.front().url), {});

  ExpectIconAndControllerStateIs(password_manager::ui::AUTO_SIGNIN_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, ActiveOnPSLMatched) {
  std::u16string kTestUsername = u"test_username";
  std::vector<PasswordForm> forms;
  PasswordForm psl_matched_test_form(test_local_form());
  psl_matched_test_form.match_type = PasswordForm::MatchType::kPSL;
  forms.push_back(psl_matched_test_form);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordAutofilled(
      forms, url::Origin::Create(forms.front().url), {});

  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, UpdatePasswordSubmitted) {
  std::vector<PasswordForm> best_matches = {test_local_form()};
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));
  ExpectIconAndControllerStateIs(
      password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, PasswordUpdated) {
  std::vector<PasswordForm> best_matches = {test_local_form()};
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*test_form_manager, Save());
  controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));

  ExpectIconAndControllerStateIs(
      password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
  base::HistogramTester histogram_tester;
  controller()->SavePassword(submitted_form().username_value,
                             submitted_form().password_value);
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnBubbleHidden();
}

TEST_P(ManagePasswordsUIControllerTest, SavePendingStatePasswordAutofilled) {
  TestNotChangingStateOnAutofill(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, UpdatePendingStatePasswordAutofilled) {
  TestNotChangingStateOnAutofill(
      password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, ConfirmationStatePasswordAutofilled) {
  TestNotChangingStateOnAutofill(password_manager::ui::SAVE_CONFIRMATION_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, OpenBubbleTwice) {
  {
    // Open the autosignin bubble.
    std::vector<std::unique_ptr<PasswordForm>> local_credentials;
    local_credentials.emplace_back(new PasswordForm(test_local_form()));
    controller()->OnAutoSignin(std::move(local_credentials),
                               url::Origin::Create(test_local_form().url));
  }
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, controller()->GetState());
  // The delegate used by the bubble for communicating with the controller.
  base::WeakPtr<PasswordsModelDelegate> proxy_delegate =
      controller()->GetModelDelegateProxy();

  {
    // Open the bubble again.
    std::vector<std::unique_ptr<PasswordForm>> local_credentials;
    local_credentials.emplace_back(new PasswordForm(test_local_form()));
    controller()->OnAutoSignin(std::move(local_credentials),
                               url::Origin::Create(test_local_form().url));
  }
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, controller()->GetState());
  // Check the delegate is destroyed. Thus, the first bubble has no way to
  // mess up with the controller's state.
  EXPECT_FALSE(proxy_delegate);
}

TEST_P(ManagePasswordsUIControllerTest, ManualFallbackForSaving_UseFallback) {
  using UkmEntry = ukm::builders::PasswordForm;
  for (bool is_update : {false, true}) {
    SCOPED_TRACE(testing::Message("is_update = ") << is_update);
    base::HistogramTester histogram_tester;
    ukm::TestAutoSetUkmRecorder test_ukm_recorder;

    // Setup metrics recorder.
    ukm::SourceId source_id = test_ukm_recorder.GetNewSourceID();
    auto recorder =
        base::MakeRefCounted<password_manager::PasswordFormMetricsRecorder>(
            true /*is_main_frame_secure*/, source_id, /*pref_service=*/nullptr);
    std::vector<PasswordForm> matches = {test_local_form()};
    if (is_update) {
      matches.push_back(test_local_form());
    }
    auto test_form_manager =
        CreateFormManagerWithBestMatches(matches, &submitted_form());
    EXPECT_CALL(*test_form_manager, GetMetricsRecorder)
        .WillRepeatedly(Return(recorder.get()));
    EXPECT_CALL(*test_form_manager, Save());

    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility()).Times(3);
    controller()->OnShowManualFallbackForSaving(
        std::move(test_form_manager), false /* has_generated_password */,
        is_update);
    ExpectIconAndControllerStateIs(
        is_update ? password_manager::ui::PENDING_PASSWORD_UPDATE_STATE
                  : password_manager::ui::PENDING_PASSWORD_STATE);
    EXPECT_FALSE(controller()->opened_automatic_bubble());

    // A user clicks on omnibox icon, opens the bubble and press Save/Update.
    controller()->SavePassword(submitted_form().username_value,
                               submitted_form().password_value);

    // Fake navigation so that the old form manager gets destroyed and
    // reports its metrics. Need to close the bubble, otherwise the bubble
    // state is retained on navigation, and the PasswordFormManager is not
    // destroyed.
    controller()->OnBubbleHidden();
    content::NavigationSimulator::NavigateAndCommitFromBrowser(
        web_contents(), GURL(kExampleUrl));

    recorder = nullptr;
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(controller()));

    // Verify metrics.
    const auto& entries =
        test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    ASSERT_EQ(1u, entries.size());
    auto* entry = entries[0].get();
    EXPECT_EQ(source_id, entry->source_id);

    test_ukm_recorder.ExpectEntryMetric(
        entry, UkmEntry::kUser_Action_TriggeredManualFallbackForSavingName, 1u);
  }
}

// Verifies that after OnHideManualFallbackForSaving, the password manager icon
// goes into a state that allows managing existing passwords, if these existed
// before the manual fallback.
TEST_P(ManagePasswordsUIControllerTest,
       ManualFallbackForSaving_HideFallback_WithPreexistingPasswords) {
  for (bool is_update : {false, true}) {
    SCOPED_TRACE(testing::Message("is_update = ") << is_update);
    // Create password form manager with stored passwords.
    std::vector<PasswordForm> matches = {test_local_form()};
    auto test_form_manager =
        CreateFormManagerWithBestMatches(matches, &submitted_form());

    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
    controller()->OnShowManualFallbackForSaving(
        std::move(test_form_manager), false /* has_generated_password */,
        is_update);
    ExpectIconAndControllerStateIs(
        is_update ? password_manager::ui::PENDING_PASSWORD_UPDATE_STATE
                  : password_manager::ui::PENDING_PASSWORD_STATE);
    testing::Mock::VerifyAndClearExpectations(controller());
    EXPECT_FALSE(controller()->opened_automatic_bubble());

    // A user clears the password field. It hides the fallback.
    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
    controller()->OnHideManualFallbackForSaving();
    testing::Mock::VerifyAndClearExpectations(controller());

    ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
  }
}

// Verify that after OnHideManualFallbackForSaving, the password manager icon
// goes away if no passwords were persisted before the manual fallback.
TEST_P(ManagePasswordsUIControllerTest,
       ManualFallbackForSaving_HideFallback_WithoutPreexistingPasswords) {
  // Create password form manager without stored passwords.
  std::vector<PasswordForm> matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(matches, &submitted_form());

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnShowManualFallbackForSaving(
      std::move(test_form_manager), false /* has_generated_password */,
      false /* is_update */);
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
  testing::Mock::VerifyAndClearExpectations(controller());
  EXPECT_FALSE(controller()->opened_automatic_bubble());

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnHideManualFallbackForSaving();
  testing::Mock::VerifyAndClearExpectations(controller());

  ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest,
       ManualFallbackForSaving_HideFallback_Timeout) {
  for (bool enforce_navigation : {false, true}) {
    SCOPED_TRACE(testing::Message("enforce_navigation = ")
                 << enforce_navigation);
    ManagePasswordsUIController::set_save_fallback_timeout_in_seconds(0);

    std::vector<PasswordForm> matches;
    auto test_form_manager =
        CreateFormManagerWithBestMatches(matches, &submitted_form());

    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
    controller()->OnShowManualFallbackForSaving(
        std::move(test_form_manager), false /* has_generated_password */,
        false /* is_update */);
    ExpectIconAndControllerStateIs(
        password_manager::ui::PENDING_PASSWORD_STATE);
    testing::Mock::VerifyAndClearExpectations(controller());
    if (enforce_navigation) {
      // Fake-navigate. The fallback should persist.
      content::NavigationSimulator::NavigateAndCommitFromBrowser(
          web_contents(), GURL(kExampleUrl));
      ExpectIconAndControllerStateIs(
          password_manager::ui::PENDING_PASSWORD_STATE);
    }

    // As the timeout is zero, the fallback will be hidden right after show.
    // Visibility update confirms that hiding event happened.
    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
    content::RunAllTasksUntilIdle();

    EXPECT_FALSE(controller()->opened_automatic_bubble());
    ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
    testing::Mock::VerifyAndClearExpectations(controller());
  }
}

TEST_P(ManagePasswordsUIControllerTest,
       ManualFallbackForSaving_OpenBubbleBlocksFallbackHiding) {
  for (bool user_saved_password : {false, true}) {
    SCOPED_TRACE(testing::Message("user_saved_password = ")
                 << user_saved_password);

    ManagePasswordsUIController::set_save_fallback_timeout_in_seconds(0);
    std::vector<PasswordForm> matches;
    auto test_form_manager =
        CreateFormManagerWithBestMatches(matches, &submitted_form());

    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
    EXPECT_CALL(*test_form_manager, Save()).Times(user_saved_password);
    controller()->OnShowManualFallbackForSaving(
        std::move(test_form_manager), false /* has_generated_password */,
        false /* is_update */);
    ExpectIconAndControllerStateIs(
        password_manager::ui::PENDING_PASSWORD_STATE);
    testing::Mock::VerifyAndClearExpectations(controller());

    // A user opens the bubble.
    controller()->OnBubbleShown();

    // Fallback hiding is triggered by timeout but blocked because of open
    // bubble.
    content::RunAllTasksUntilIdle();
    ExpectIconAndControllerStateIs(
        password_manager::ui::PENDING_PASSWORD_STATE);

    if (user_saved_password) {
      controller()->SavePassword(submitted_form().username_value,
                                 submitted_form().password_value);
      ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
    } else {
      // A user closed the bubble. The fallback should be hidden after
      // navigation.
      controller()->OnBubbleHidden();
      EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
      content::NavigationSimulator::NavigateAndCommitFromBrowser(
          web_contents(), GURL(kExampleUrl));
      ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
    }
    testing::Mock::VerifyAndClearExpectations(controller());
  }
}

TEST_P(ManagePasswordsUIControllerTest,
       ManualFallbackForSavingFollowedByAutomaticBubble) {
  std::vector<PasswordForm> matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(matches, &submitted_form());
  PasswordForm pending = test_local_form();
  pending.username_value = u"manual_username";
  pending.password_value = u"manual_pass1234";
  EXPECT_CALL(*test_form_manager, GetPendingCredentials())
      .WillRepeatedly(ReturnRef(pending));

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnShowManualFallbackForSaving(
      std::move(test_form_manager), false /* has_generated_password */,
      false /* is_update */);
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
  testing::Mock::VerifyAndClearExpectations(controller());

  // A user opens the bubble.
  controller()->OnBubbleShown();

  // Automatic form submission detected.
  submitted_form().username_value = u"new_username";
  submitted_form().password_value = u"12345";
  test_form_manager =
      CreateFormManagerWithBestMatches(matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  // It should update the bubble already open.
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
  EXPECT_EQ(u"new_username", controller()->GetPendingPassword().username_value);
  EXPECT_EQ(u"12345", controller()->GetPendingPassword().password_value);
}

TEST_P(ManagePasswordsUIControllerTest,
       ManualFallbackForSaving_HideAutomaticBubble) {
  // Open the automatic bubble first.
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  ASSERT_TRUE(controller()->opened_automatic_bubble());
  EXPECT_EQ(url::Origin::Create(test_local_form().url),
            controller()->GetOrigin());

  // User navigates to another origin and types into a password form.
  test_local_form().url = GURL("http://nonexample.com/login");
  test_local_form().signon_realm = "http://nonexample.com/";
  test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &test_local_form());
  PasswordForm pending = test_local_form();
  pending.username_value = u"manual_username";
  pending.password_value = u"manual_pass1234";
  EXPECT_CALL(*test_form_manager, GetPendingCredentials())
      .WillRepeatedly(ReturnRef(pending));
  controller()->OnShowManualFallbackForSaving(
      std::move(test_form_manager), false /* has_generated_password */,
      false /* is_update */);
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);

  // The automatic bubble is gone. The controller is managing the new origin.
  EXPECT_FALSE(controller()->opened_automatic_bubble());
  EXPECT_EQ(url::Origin::Create(test_local_form().url),
            controller()->GetOrigin());
}

TEST_P(ManagePasswordsUIControllerTest,
       ManualFallbackForSaving_GeneratedPassword) {
  for (bool user_closed_bubble : {false, true}) {
    SCOPED_TRACE(testing::Message("user_closed_bubble = ")
                 << user_closed_bubble);
    std::vector<PasswordForm> matches = {test_local_form()};
    auto test_form_manager =
        CreateFormManagerWithBestMatches(matches, &submitted_form());
    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
    controller()->OnShowManualFallbackForSaving(
        std::move(test_form_manager), true /* has_generated_password */, false);
    ExpectIconAndControllerStateIs(
        password_manager::ui::SAVE_CONFIRMATION_STATE);
    EXPECT_FALSE(controller()->opened_automatic_bubble());

    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
    if (user_closed_bubble) {
      // A user opens the confirmation bubble and presses OK.
      controller()->OnBubbleHidden();
    } else {
      // The user removes the generated password. It hides the fallback.
      controller()->OnHideManualFallbackForSaving();
    }
    ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
    testing::Mock::VerifyAndClearExpectations(controller());
  }
}

TEST_P(ManagePasswordsUIControllerTest,
       PasswordDetails_OnShowPasswordIsInitialBubbleCredential) {
  std::unique_ptr<base::AutoReset<bool>> bypass_user_auth =
      controller()->BypassUserAuthtForTesting();
  password_manager::PasswordForm form;
  form.username_value = u"user";
  form.password_value = u"passw0rd";
  controller()->OnOpenPasswordDetailsBubble(form);

  EXPECT_EQ(
      controller()->GetManagePasswordsSingleCredentialDetailsModeCredential(),
      form);
  EXPECT_EQ(controller()->GetState(), password_manager::ui::MANAGE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest,
       PasswordDetails_BubbleIsInactiveAfterClosingPasswordDetails) {
  std::unique_ptr<base::AutoReset<bool>> bypass_user_auth =
      controller()->BypassUserAuthtForTesting();
  password_manager::PasswordForm details_form;
  details_form.username_value = u"user";
  details_form.password_value = u"passw0rd";
  controller()->OnOpenPasswordDetailsBubble(details_form);
  ASSERT_EQ(
      controller()->GetManagePasswordsSingleCredentialDetailsModeCredential(),
      details_form);

  controller()->OnBubbleHidden();

  EXPECT_EQ(
      controller()->GetManagePasswordsSingleCredentialDetailsModeCredential(),
      std::nullopt);
  EXPECT_EQ(controller()->GetState(), password_manager::ui::INACTIVE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest,
       PasswordDetails_BubbleSwitchesToListAfterClosingPasswordDetails) {
  std::unique_ptr<base::AutoReset<bool>> bypass_user_auth =
      controller()->BypassUserAuthtForTesting();
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(test_local_form()));
  controller()->OnAutoSignin(std::move(local_credentials),
                             url::Origin::Create(test_local_form().url));
  ASSERT_FALSE(controller()->GetCurrentForms().empty());

  password_manager::PasswordForm details_form;
  details_form.username_value = u"user";
  details_form.password_value = u"passw0rd";
  controller()->OnOpenPasswordDetailsBubble(details_form);
  ASSERT_EQ(
      controller()->GetManagePasswordsSingleCredentialDetailsModeCredential(),
      details_form);

  controller()->OnBubbleHidden();

  EXPECT_EQ(
      controller()->GetManagePasswordsSingleCredentialDetailsModeCredential(),
      std::nullopt);
  EXPECT_EQ(controller()->GetState(), password_manager::ui::MANAGE_STATE);
}

// The following test is being run on platforms that support device
// authentication, as on others the callback is stubbed to return `true`.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
TEST_P(ManagePasswordsUIControllerTest, PasswordDetails_IsntShownIfAuthFailed) {
  auto mock_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  EXPECT_CALL(*mock_authenticator, AuthenticateWithMessage)
      .WillOnce([](const std::u16string&,
                   device_reauth::DeviceAuthenticator::AuthenticateCallback
                       callback) { std::move(callback).Run(false); });
  EXPECT_CALL(client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(mock_authenticator))));

  password_manager::PasswordForm form;
  form.username_value = u"user";
  form.password_value = u"passw0rd";
  controller()->OnOpenPasswordDetailsBubble(form);

  EXPECT_EQ(
      controller()->GetManagePasswordsSingleCredentialDetailsModeCredential(),
      std::nullopt);
  EXPECT_EQ(controller()->GetState(), password_manager::ui::INACTIVE_STATE);
}
#endif

TEST_P(ManagePasswordsUIControllerTest, AutofillDuringSignInPromo) {
  std::vector<PasswordForm> matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*test_form_manager, Save());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  controller()->SavePassword(submitted_form().username_value,
                             submitted_form().password_value);
  // The state is 'Managed' but the bubble may still be on the screen showing
  // the sign-in promo.
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
  // The controller shouldn't force close the bubble if an autofill happened.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility()).Times(0);
  std::vector<PasswordForm> forms;
  forms.push_back(test_local_form());
  controller()->OnPasswordAutofilled(
      forms, url::Origin::Create(forms.front().url), {});

  // Once the bubble is closed the controller is reacting again.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnBubbleHidden();
}

TEST_P(ManagePasswordsUIControllerTest, SaveBubbleAfterLeakCheck) {
  std::vector<PasswordForm> matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(matches, &submitted_form());
  MockPasswordFormManagerForUI* form_manager_ptr = test_form_manager.get();
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_TRUE(controller()->opened_automatic_bubble());

  // Leak detection dialog hides the bubble.
  auto dialog_prompt = std::make_unique<PasswordLeakDialogMock>();
  CredentialLeakDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*dialog_prompt, ShowCredentialLeakPrompt);
  EXPECT_CALL(*controller(), CreateCredentialLeakPrompt)
      .WillOnce(DoAll(SaveArg<0>(&dialog_controller),
                      Return(std::move(dialog_prompt))));
  controller()->OnCredentialLeak(password_manager::LeakedPasswordDetails(
      password_manager::CreateLeakType(password_manager::IsSaved(false),
                                       password_manager::IsReused(false),
                                       password_manager::IsSyncing(false)),
      CreatePasswordForm(kExampleUrl, kExampleUsername, kExamplePassword),
      /*in_account_store=*/false));
  // The bubble is gone.
  EXPECT_FALSE(controller()->opened_automatic_bubble());

  // After closing the lead check dialog, the blocklisting will be checked again
  // to decide whether to reopen the save prompt.
  EXPECT_CALL(*form_manager_ptr, IsBlocklisted()).WillRepeatedly(Return(false));
  EXPECT_CALL(*form_manager_ptr, GetInteractionsStats())
      .WillOnce(
          Return(base::span<const password_manager::InteractionsStats>()));

  // Close the dialog.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  dialog_controller->OnAcceptDialog();

  // The save bubble is back.
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_P(ManagePasswordsUIControllerTest,
       NoSaveBubbleAfterLeakCheckForBlocklistedWebsites) {
  std::vector<PasswordForm> matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(
      matches, &submitted_form(), /*is_blocklisted=*/true);
  MockPasswordFormManagerForUI* form_manager_ptr = test_form_manager.get();
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_FALSE(controller()->opened_automatic_bubble());

  // Leak detection dialog hides the bubble.
  auto dialog_prompt = std::make_unique<PasswordLeakDialogMock>();
  CredentialLeakDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*dialog_prompt, ShowCredentialLeakPrompt);
  EXPECT_CALL(*controller(), CreateCredentialLeakPrompt)
      .WillOnce(DoAll(SaveArg<0>(&dialog_controller),
                      Return(std::move(dialog_prompt))));
  controller()->OnCredentialLeak(password_manager::LeakedPasswordDetails(
      password_manager::CreateLeakType(password_manager::IsSaved(false),
                                       password_manager::IsReused(false),
                                       password_manager::IsSyncing(false)),
      CreatePasswordForm(kExampleUrl, kExampleUsername, kExamplePassword),
      /*in_account_store=*/false));
  // The bubble is gone.
  EXPECT_FALSE(controller()->opened_automatic_bubble());

  // After closing the lead check dialog, the blocklisting will be checked again
  // to decide whether to reopen the save prompt.
  EXPECT_CALL(*form_manager_ptr, IsBlocklisted()).WillRepeatedly(Return(true));

  // Close the dialog.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  dialog_controller->OnAcceptDialog();

  // The save bubble should not be opened because the website is blocklisted.
  EXPECT_FALSE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, UpdateBubbleAfterLeakCheck) {
  std::vector<PasswordForm> matches = {test_local_form()};
  auto test_form_manager =
      CreateFormManagerWithBestMatches(matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));
  EXPECT_TRUE(controller()->opened_automatic_bubble());

  // Leak detection dialog hides the bubble.
  auto dialog_prompt = std::make_unique<PasswordLeakDialogMock>();
  auto* dialog_prompt_ptr = dialog_prompt.get();
  CredentialLeakDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*controller(), CreateCredentialLeakPrompt)
      .WillOnce(DoAll(SaveArg<0>(&dialog_controller),
                      Return(std::move(dialog_prompt))));
  EXPECT_CALL(*dialog_prompt_ptr, ShowCredentialLeakPrompt);
  controller()->OnCredentialLeak(password_manager::LeakedPasswordDetails(
      password_manager::CreateLeakType(password_manager::IsSaved(true),
                                       password_manager::IsReused(false),
                                       password_manager::IsSyncing(false)),
      CreatePasswordForm(kExampleUrl, kExampleUsername, kExamplePassword),
      /*in_account_store=*/false));
  // The bubble is gone.
  EXPECT_FALSE(controller()->opened_automatic_bubble());

  // Close the dialog.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  dialog_controller->OnAcceptDialog();

  // The update bubble is back.
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
}

// If the leaked password is the backup password of the login credentials, we
// should not offer password change and instead, we will show the old leak
// warning dialogue.
TEST_P(ManagePasswordsUIControllerTest,
       PasswordChangeDialogueIsSupressedForBackupPassword) {
  std::vector<PasswordForm> matches = {test_local_form()};
  auto test_form_manager =
      CreateFormManagerWithBestMatches(matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));
  EXPECT_TRUE(controller()->opened_automatic_bubble());

  // Leak detection dialog hides the bubble.
  auto dialog_prompt = std::make_unique<PasswordLeakDialogMock>();
  auto* dialog_prompt_ptr = dialog_prompt.get();
  CredentialLeakDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*controller(), CreateCredentialLeakPrompt)
      .WillOnce(DoAll(SaveArg<0>(&dialog_controller),
                      Return(std::move(dialog_prompt))));
  EXPECT_CALL(*dialog_prompt_ptr, ShowCredentialLeakPrompt);
  controller()->OnCredentialLeak(password_manager::LeakedPasswordDetails(
      password_manager::CreateLeakType(
          password_manager::IsSaved(true), password_manager::IsReused(false),
          password_manager::IsSyncing(false),
          password_manager::HasChangePasswordUrl(true),
          password_manager::IsSavedAsBackup(true)),
      CreatePasswordForm(kExampleUrl, kExampleUsername, kExamplePassword),
      /*in_account_store=*/false));
  // The bubble is gone.
  EXPECT_FALSE(controller()->opened_automatic_bubble());

  // Close the dialog.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  dialog_controller->OnAcceptDialog();

  // The update bubble is back.
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, OpenBubbleForMovableForm) {
  base::HistogramTester histogram_tester;

  std::vector<PasswordForm> matches = {test_local_form()};
  auto test_form_manager =
      CreateFormManagerWithBestMatches(matches, &submitted_form());

  // A submitted form triggers the move dialog.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility()).Times(2);
  controller()->OnShowMoveToAccountBubble(std::move(test_form_manager));
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::MOVE_CREDENTIAL_AFTER_LOG_IN_STATE);

  // A user confirms the move which closes the dialog.
  EXPECT_CALL(*controller(),
              CreateMovePasswordToAccountStoreHelper(
                  controller()->GetPendingPassword(),
                  password_manager::metrics_util::MoveToAccountStoreTrigger::
                      kSuccessfulLoginWithProfileStorePassword,
                  _));
  controller()->MovePasswordToAccountStore();
  EXPECT_FALSE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.AccountStorage.MoveToAccountStoreFlowOffered",
      password_manager::metrics_util::MoveToAccountStoreTrigger::
          kSuccessfulLoginWithProfileStorePassword,
      1);
}

TEST_P(ManagePasswordsUIControllerTest, OpenMoveBubbleFromManagementBubble) {
  const PasswordForm* test_form_ptr = &test_local_form();
  std::vector<PasswordForm> forms = {*test_form_ptr};
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordAutofilled(
      forms, url::Origin::Create(test_form_ptr->url), {});
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility()).Times(2);
  controller()->ShowMovePasswordBubble(test_local_form());
  EXPECT_FALSE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::MOVE_CREDENTIAL_FROM_MANAGE_BUBBLE_STATE);

  EXPECT_CALL(*controller(),
              CreateMovePasswordToAccountStoreHelper(
                  controller()->GetPendingPassword(),
                  password_manager::metrics_util::MoveToAccountStoreTrigger::
                      kExplicitlyTriggeredInPasswordsManagementBubble,
                  _));

  // A user confirms the move which closes the dialog.
  controller()->MovePasswordToAccountStore();
  EXPECT_FALSE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, CloseMoveBubble) {
  const PasswordForm* test_form_ptr = &test_local_form();
  std::vector<PasswordForm> forms = {*test_form_ptr};
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordAutofilled(
      forms, url::Origin::Create(test_form_ptr->url), {});
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility()).Times(2);
  controller()->ShowMovePasswordBubble(test_local_form());
  EXPECT_FALSE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::MOVE_CREDENTIAL_FROM_MANAGE_BUBBLE_STATE);

  // A user closes the dialog.
  controller()->OnBubbleHidden();
  EXPECT_FALSE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, OpenSafeStateBubble) {
  profile()->GetPrefs()->SetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::Minutes(1)).InSecondsFSinceUnixEpoch());
  submitted_form() = test_local_form();
  submitted_form().password_value = u"new_password";

  std::vector<PasswordForm> best_matches = {test_local_form()};
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  MockPasswordFormManagerForUI* test_form_manager_raw = test_form_manager.get();
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  EXPECT_CALL(*test_form_manager_raw, Save());
  PasswordForm credential = CreateInsecureCredential(test_local_form());
  // Pretend that the current credential was insecure but with the updated
  // password not anymore.
  EXPECT_CALL(*test_form_manager_raw, GetInsecureCredentials())
      .WillOnce(Return(std::vector<PasswordForm>{credential}));
  base::WeakPtr<password_manager::PasswordStoreConsumer> post_save_helper;

  EXPECT_CALL(*client().GetProfilePasswordStore(), GetAutofillableLogins)
      .WillOnce(testing::WithArg<0>(
          [&post_save_helper](auto consumer) { post_save_helper = consumer; }));
  controller()->SavePassword(submitted_form().username_value,
                             submitted_form().password_value);
  // The bubble gets hidden after the user clicks on save.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnBubbleHidden();

  std::vector<PasswordForm> results;
  results.push_back(submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  post_save_helper->OnGetPasswordStoreResultsOrErrorFrom(
      client().GetProfilePasswordStore(), std::move(results));
  WaitForPasswordStore();

  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSWORD_UPDATED_SAFE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, OpenMoreToFixBubble) {
  profile()->GetPrefs()->SetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::Minutes(1)).InSecondsFSinceUnixEpoch());
  submitted_form() = test_local_form();
  submitted_form().password_value = u"new_password";

  std::vector<PasswordForm> best_matches = {test_local_form()};
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  MockPasswordFormManagerForUI* test_form_manager_raw = test_form_manager.get();
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  EXPECT_CALL(*test_form_manager_raw, Save());
  // Pretend that the current credential was insecure.
  PasswordForm credential = CreateInsecureCredential(test_local_form());
  EXPECT_CALL(*test_form_manager_raw, GetInsecureCredentials())
      .WillOnce(Return(std::vector<PasswordForm>{credential}));

  base::WeakPtr<password_manager::PasswordStoreConsumer> post_save_helper;

  EXPECT_CALL(*client().GetProfilePasswordStore(), GetAutofillableLogins)
      .WillOnce(testing::WithArg<0>(
          [&post_save_helper](auto consumer) { post_save_helper = consumer; }));
  controller()->SavePassword(submitted_form().username_value,
                             submitted_form().password_value);
  // There are more insecure credentials to fix.
  std::vector<PasswordForm> expected_forms = {test_local_form(),
                                              submitted_form()};
  expected_forms.at(0).username_value = u"another username";
  expected_forms.at(0).password_issues.insert(
      {InsecureType::kLeaked, InsecurityMetadata()});
  expected_forms.at(1).password_issues.clear();
  // The bubble gets hidden after the user clicks on save.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnBubbleHidden();
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  post_save_helper->OnGetPasswordStoreResultsOrErrorFrom(
      client().GetProfilePasswordStore(), expected_forms);
  WaitForPasswordStore();

  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSWORD_UPDATED_MORE_TO_FIX);
}

TEST_P(ManagePasswordsUIControllerTest, NoMoreToFixBubbleIfPromoStillOpen) {
  profile()->GetPrefs()->SetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted,
      (base::Time::Now() - base::Minutes(1)).InSecondsFSinceUnixEpoch());
  submitted_form() = test_local_form();
  submitted_form().password_value = u"new_password";

  std::vector<PasswordForm> best_matches = {test_local_form()};
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  MockPasswordFormManagerForUI* test_form_manager_raw = test_form_manager.get();
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  EXPECT_CALL(*test_form_manager_raw, Save());
  PasswordForm credential = CreateInsecureCredential(test_local_form());
  // Pretend that the current credential was insecure.
  EXPECT_CALL(*test_form_manager_raw, GetInsecureCredentials())
      .WillOnce(Return(std::vector<PasswordForm>{credential}));
  controller()->SavePassword(submitted_form().username_value,
                             submitted_form().password_value);
  // The sign-in promo bubble stays open, the warning isn't shown.
  WaitForPasswordStore();

  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, UsernameAdded) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  MockPasswordFormManagerForUI* test_form_manager_raw = test_form_manager.get();
  controller()->OnAutomaticPasswordSave(std::move(test_form_manager),
                                        /*is_update_confirmation=*/false);

  EXPECT_CALL(*test_form_manager_raw, Save());
  EXPECT_CALL(*test_form_manager_raw,
              OnUpdateUsernameFromPrompt(Eq(kExampleUsername)));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnAddUsernameSaveClicked(kExampleUsername, submitted_form());

  EXPECT_TRUE(controller()->opened_automatic_bubble());
  EXPECT_EQ(password_manager::ui::SAVE_CONFIRMATION_STATE,
            controller()->GetState());
}

TEST_P(ManagePasswordsUIControllerTest, IsDeviceAuthenticatorObtained) {
  base::MockCallback<base::OnceCallback<void(bool)>> result_callback;
#if !BUILDFLAG(IS_MAC) && !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_CHROMEOS)
  EXPECT_CALL(result_callback, Run(/*success=*/true));
#else
  auto mock_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  EXPECT_CALL(*mock_authenticator, AuthenticateWithMessage);

  EXPECT_CALL(client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(mock_authenticator))));
#endif
  controller()->AuthenticateUserWithMessage(
      /*message=*/u"Do you want to enable this feature", result_callback.Get());
}

TEST_P(ManagePasswordsUIControllerTest, PasskeySavedWithoutGpmPinCreation) {
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasskeySaved(/*gpm_pin_created=*/false, kExampleRpId);
  EXPECT_EQ(controller()->PasskeyRpId(), kExampleRpId);
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSKEY_SAVED_CONFIRMATION_STATE);
  EXPECT_FALSE(controller()->GpmPinCreatedDuringRecentPasskeyCreation());
}

TEST_P(ManagePasswordsUIControllerTest, PasskeySavedWithGpmPinCreation) {
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasskeySaved(/*gpm_pin_created=*/true, kExampleRpId);
  EXPECT_EQ(controller()->PasskeyRpId(), kExampleRpId);
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSKEY_SAVED_CONFIRMATION_STATE);
  EXPECT_TRUE(controller()->GpmPinCreatedDuringRecentPasskeyCreation());
}

TEST_P(ManagePasswordsUIControllerTest, InvalidPasskeyDeleted) {
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasskeyDeleted();
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSKEY_DELETED_CONFIRMATION_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, OpenPasskeyUpdatedBubble) {
  std::string rp_id = "touhou.example.com";
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasskeyUpdated(rp_id);
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  EXPECT_EQ(controller()->PasskeyRpId(), rp_id);
  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSKEY_UPDATED_CONFIRMATION_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, OpenPasskeyNotAcceptedBubble) {
  std::string rp_id = "touhou.example.com";
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasskeyNotAccepted(rp_id);
  EXPECT_EQ(controller()->PasskeyRpId(), rp_id);
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSKEY_NOT_ACCEPTED_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, PasswordChangeFinishedSuccessfully) {
  PasswordChangeServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindLambdaForTesting([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
        return std::make_unique<MockPasswordChangeService>();
      }));

  auto* password_change_service = static_cast<MockPasswordChangeService*>(
      PasswordChangeServiceFactory::GetForProfile(profile()));

  // Assuming, the password form was just submitted and this is a new password.
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  ASSERT_EQ(controller()->GetState(),
            password_manager::ui::PENDING_PASSWORD_STATE);

  // Emulate password change flow has started.
  PasswordChangeDelegateMock mock_delegate;
  EXPECT_CALL(mock_delegate, GetCurrentState)
      .WillOnce(
          Return(PasswordChangeDelegate::State::kWaitingForChangePasswordForm));
  EXPECT_CALL(*password_change_service, GetPasswordChangeDelegate)
      .WillOnce(Return(&mock_delegate));
  ASSERT_EQ(controller()->GetState(), password_manager::ui::INACTIVE_STATE);

  // Password change flow has finished successfully. The state should change to
  // `MANAGE_STATE`.
  controller()->OnPasswordChangeFinishedSuccessfully();
  EXPECT_CALL(*password_change_service, GetPasswordChangeDelegate)
      .WillOnce(Return(nullptr));
  ASSERT_EQ(controller()->GetState(), password_manager::ui::MANAGE_STATE);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
TEST_P(ManagePasswordsUIControllerTest,
       ShouldShowBiometricAuthenticationForFillingPromo) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kHasUserInteractedWithBiometricAuthPromo, false);
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kBiometricAuthBeforeFillingPromoShownCounter, 0);
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnBiometricAuthenticationForFilling(profile()->GetPrefs());
  EXPECT_EQ(1, profile()->GetPrefs()->GetInteger(
                   password_manager::prefs::
                       kBiometricAuthBeforeFillingPromoShownCounter));
  ExpectIconAndControllerStateIs(
      password_manager::ui::BIOMETRIC_AUTHENTICATION_FOR_FILLING_STATE);
}

// Test if BiometricAuthForFilling promo is not shown if user interacted with
// the promo earlier.
TEST_P(ManagePasswordsUIControllerTest,
       ShouldNotShowBiometricAuthenticationForFillingPromoUserInteracted) {
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kHasUserInteractedWithBiometricAuthPromo, true);
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kBiometricAuthBeforeFillingPromoShownCounter, 1);
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility()).Times(0);
  controller()->OnBiometricAuthenticationForFilling(profile()->GetPrefs());
  EXPECT_EQ(1, profile()->GetPrefs()->GetInteger(
                   password_manager::prefs::
                       kBiometricAuthBeforeFillingPromoShownCounter));
}

// Test if BiometricAuthForFilling promo is not shown if User turned on the
// feature manually in settings.
TEST_P(ManagePasswordsUIControllerTest,
       ShouldNotShowBiometricAuthenticationForFillingPromoUserTurnedOnManualy) {
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kHasUserInteractedWithBiometricAuthPromo, false);
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kBiometricAuthBeforeFillingPromoShownCounter, 0);
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, true);

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility()).Times(0);
  controller()->OnBiometricAuthenticationForFilling(profile()->GetPrefs());
  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(
                   password_manager::prefs::
                       kBiometricAuthBeforeFillingPromoShownCounter));
}

// Test if BiometricAuthForFilling promo is not shown if User turned on the
// feature through promo.
TEST_P(
    ManagePasswordsUIControllerTest,
    ShouldNotShowBiometricAuthenticationForFillingPromoUserTurnedOnViaPromo) {
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kHasUserInteractedWithBiometricAuthPromo, true);
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kBiometricAuthBeforeFillingPromoShownCounter, 1);
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, true);

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility()).Times(0);
  controller()->OnBiometricAuthenticationForFilling(profile()->GetPrefs());
  EXPECT_EQ(1, profile()->GetPrefs()->GetInteger(
                   password_manager::prefs::
                       kBiometricAuthBeforeFillingPromoShownCounter));
}

// Test if BiometricAuthForFilling promo is not shown if User have seen promo
// more than
// `kMaxNumberOfTimesBiometricAuthForFillingPromoWillBeShown` times.
TEST_P(ManagePasswordsUIControllerTest,
       ShouldNotShowBiometricAuthenticationForFillingPromoCounterLimit) {
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kHasUserInteractedWithBiometricAuthPromo, false);
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kBiometricAuthBeforeFillingPromoShownCounter,
      kMaxNumberOfTimesBiometricAuthForFillingPromoWillBeShown + 1);
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility()).Times(0);
  controller()->OnBiometricAuthenticationForFilling(profile()->GetPrefs());
  EXPECT_EQ(kMaxNumberOfTimesBiometricAuthForFillingPromoWillBeShown + 1,
            profile()->GetPrefs()->GetInteger(
                password_manager::prefs::
                    kBiometricAuthBeforeFillingPromoShownCounter));
}

// On one specific tab BiometricAuthForFilling promo should be shown no more
// than once.
TEST_P(ManagePasswordsUIControllerTest,
       ShouldNotShowBiometricAuthenticationForFillingPromoTwiceOnTheSameTab) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kHasUserInteractedWithBiometricAuthPromo, false);
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kBiometricAuthBeforeFillingPromoShownCounter, 0);
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnBiometricAuthenticationForFilling(profile()->GetPrefs());
  ExpectIconAndControllerStateIs(
      password_manager::ui::BIOMETRIC_AUTHENTICATION_FOR_FILLING_STATE);

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility()).Times(0);
  controller()->OnBiometricAuthenticationForFilling(profile()->GetPrefs());
}

TEST_P(ManagePasswordsUIControllerTest,
       BiometricAuthPromoNotShowIfThereIsAnotherDialog) {
  // Show account chooser dialog.
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(
      std::make_unique<PasswordForm>(test_local_form()));
  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  CredentialManagerDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*controller(), CreateAccountChooser(_))
      .WillOnce(
          DoAll(SaveArg<0>(&dialog_controller), Return(&dialog_prompt())));
  EXPECT_CALL(dialog_prompt(), ShowAccountChooser());
  EXPECT_CALL(*controller(), HasBrowserWindow()).WillOnce(Return(true));
  base::MockCallback<ManagePasswordsState::CredentialsCallback> choose_callback;
  EXPECT_TRUE(controller()->OnChooseCredentials(std::move(local_credentials),
                                                origin, choose_callback.Get()));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());

  // Now try to show promo for biometric auth before filling.
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kHasUserInteractedWithBiometricAuthPromo, false);
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kBiometricAuthBeforeFillingPromoShownCounter, 0);
  profile()->GetPrefs()->SetBoolean(
      password_manager::prefs::kBiometricAuthenticationBeforeFilling, false);

  controller()->OnBiometricAuthenticationForFilling(profile()->GetPrefs());
  EXPECT_EQ(0, profile()->GetPrefs()->GetInteger(
                   password_manager::prefs::
                       kBiometricAuthBeforeFillingPromoShownCounter));

  // Verify that account chooser is still shown.
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());

  // Choose a credential to verify that there is no crash.
  EXPECT_CALL(choose_callback, Run(Pointee(test_local_form())));
  dialog_controller->OnChooseCredentials(
      *dialog_controller->GetLocalForms()[0],
      password_manager::CredentialType::CREDENTIAL_TYPE_PASSWORD);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());
}

TEST_P(ManagePasswordsUIControllerTest, BiometricActivationConfirmation) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  controller()->ShowBiometricActivationConfirmation();
  EXPECT_EQ(password_manager::ui::BIOMETRIC_AUTHENTICATION_CONFIRMATION_STATE,
            controller()->GetState());

  // After closing buble state switches automatically to MANAGE_STATE.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnBubbleHidden();
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest,
       BiometricActivationConfirmationNotShownOnTopOfAnotherDialog) {
  // Show account chooser dialog.
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(
      std::make_unique<PasswordForm>(test_local_form()));
  url::Origin origin = url::Origin::Create(GURL(kExampleUrl));
  CredentialManagerDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*controller(), CreateAccountChooser(_))
      .WillOnce(
          DoAll(SaveArg<0>(&dialog_controller), Return(&dialog_prompt())));
  EXPECT_CALL(dialog_prompt(), ShowAccountChooser());
  EXPECT_CALL(*controller(), HasBrowserWindow()).WillOnce(Return(true));
  base::MockCallback<ManagePasswordsState::CredentialsCallback> choose_callback;
  EXPECT_TRUE(controller()->OnChooseCredentials(std::move(local_credentials),
                                                origin, choose_callback.Get()));

  controller()->ShowBiometricActivationConfirmation();
  // Verify that account chooser is still shown.
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->GetState());
}

TEST_P(ManagePasswordsUIControllerTest,
       AuthenticateWithMessageTwiceCancelsFirstCall) {
  auto mock_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto* mock_authenticator_ptr = mock_authenticator.get();

  EXPECT_CALL(*mock_authenticator_ptr, AuthenticateWithMessage);
  EXPECT_CALL(client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(mock_authenticator))));

  controller()->AuthenticateUserWithMessage(/*message=*/u"", base::DoNothing());

  auto mock_authenticator2 =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();

  EXPECT_CALL(*mock_authenticator_ptr, Cancel);
  EXPECT_CALL(*mock_authenticator2.get(), AuthenticateWithMessage);
  EXPECT_CALL(client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(mock_authenticator2))));

  controller()->AuthenticateUserWithMessage(/*message=*/u"", base::DoNothing());
}

TEST_P(ManagePasswordsUIControllerTest, AuthenticationCancledOnPageChange) {
  base::MockCallback<base::OnceCallback<void(bool)>> result_callback;
  auto mock_authenticator =
      std::make_unique<device_reauth::MockDeviceAuthenticator>();
  auto* mock_authenticator_ptr = mock_authenticator.get();

  EXPECT_CALL(*mock_authenticator_ptr, AuthenticateWithMessage);
  EXPECT_CALL(client(), GetDeviceAuthenticator)
      .WillOnce(Return(testing::ByMove(std::move(mock_authenticator))));

  controller()->AuthenticateUserWithMessage(/*message=*/u"", base::DoNothing());

  EXPECT_CALL(*mock_authenticator_ptr, Cancel);

  static_cast<content::WebContentsObserver*>(controller())
      ->PrimaryPageChanged(controller()->GetWebContents()->GetPrimaryPage());
}

TEST_P(ManagePasswordsUIControllerTest, OnBiometricAuthBeforeFillingDeclined) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  controller()->OnBiometricAuthenticationForFilling(profile()->GetPrefs());

  ASSERT_EQ(password_manager::ui::BIOMETRIC_AUTHENTICATION_FOR_FILLING_STATE,
            controller()->GetState());

  controller()->OnBiometricAuthBeforeFillingDeclined();
  ASSERT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());
}

#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)

#if BUILDFLAG(IS_MAC)
TEST_P(ManagePasswordsUIControllerTest, OnKeychainErrorShouldShowBubble) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kRestartToGainAccessToKeychain);
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kRelaunchChromeBubbleDismissedCounter, 0);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnKeychainError();
  EXPECT_EQ(password_manager::ui::KEYCHAIN_ERROR_STATE,
            controller()->GetState());
}

TEST_P(ManagePasswordsUIControllerTest, OnKeychainErrorShouldNotShowBubble) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeature(
      password_manager::features::kRestartToGainAccessToKeychain);
  profile()->GetPrefs()->SetInteger(
      password_manager::prefs::kRelaunchChromeBubbleDismissedCounter, 4);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility()).Times(0);
  controller()->OnKeychainError();
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
}
#endif

// TODO(crbug.com/376283921): These tests should be turned into browser tests to avoid
// using `TestWithBrowserView`.
class ManagePasswordsUIControllerWithBrowserTest
    : public base::test::WithFeatureOverride,
      public TestWithBrowserView {
 public:
  explicit ManagePasswordsUIControllerWithBrowserTest(
      base::test::TaskEnvironment::TimeSource time_source =
          base::test::TaskEnvironment::TimeSource::SYSTEM_TIME)
      : base::test::WithFeatureOverride(
            autofill::features::kAutofillShowBubblesBasedOnPriorities),
        TestWithBrowserView(time_source) {}

  void SetUp() override;
  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  ManagePasswordsUIController* controller() {
    return ManagePasswordsUIController::FromWebContents(web_contents());
  }

 private:
  TestPasswordManagerClient client_;
};

void ManagePasswordsUIControllerWithBrowserTest::SetUp() {
  TestWithBrowserView::SetUp();
  AddTab(browser(), GURL(kExampleUrl));
  ManagePasswordsUIController::CreateForWebContents(web_contents());
  controller()->set_client(&client_);
}

TEST_P(ManagePasswordsUIControllerWithBrowserTest,
       OnAutofillingSharedPasswordNotNotifiedYet) {
  // Simulate two candidates in the dropdown menu where one of them is shared.
  PasswordForm non_shared_credentials;
  non_shared_credentials.url = GURL("http://example.com/login");
  non_shared_credentials.signon_realm = non_shared_credentials.url.spec();
  non_shared_credentials.username_value = u"username";
  non_shared_credentials.password_value = u"12345";
  non_shared_credentials.match_type = PasswordForm::MatchType::kExact;

  PasswordForm shared_credentials = non_shared_credentials;
  shared_credentials.type = PasswordForm::Type::kReceivedViaSharing;
  non_shared_credentials.username_value = u"username2";
  shared_credentials.sharing_notification_displayed = false;

  std::vector<PasswordForm> forms = {shared_credentials,
                                     non_shared_credentials};
  EXPECT_FALSE(controller()->IsShowingBubble());
  controller()->OnPasswordAutofilled(
      forms, url::Origin::Create(forms.front().url), {});

  ASSERT_EQ(2u, controller()->GetCurrentForms().size());
  EXPECT_EQ(controller()->GetState(),
            password_manager::ui::NOTIFY_RECEIVED_SHARED_CREDENTIALS);
  EXPECT_TRUE(controller()->IsShowingBubble());
  // All interactions with the bubble will close it and invoke OnBubbleHidden().
  controller()->OnBubbleHidden();
  // The bubble should transition to the manage state upon any interaction.
  EXPECT_EQ(controller()->GetState(), password_manager::ui::MANAGE_STATE);
  EXPECT_FALSE(controller()->IsShowingBubble());
}

TEST_P(ManagePasswordsUIControllerWithBrowserTest,
       OnAutofillingSharedPasswordNotifiedAlready) {
  // Simulate two candidates in the dropdown menu where one of them is shared,
  // while the user has been notified about the shared password already.
  PasswordForm non_shared_credentials;
  non_shared_credentials.url = GURL("http://example.com/login");
  non_shared_credentials.signon_realm = non_shared_credentials.url.spec();
  non_shared_credentials.username_value = u"username";
  non_shared_credentials.password_value = u"12345";
  non_shared_credentials.match_type = PasswordForm::MatchType::kExact;

  PasswordForm shared_credentials = non_shared_credentials;
  shared_credentials.type = PasswordForm::Type::kReceivedViaSharing;
  non_shared_credentials.username_value = u"username2";
  shared_credentials.sharing_notification_displayed = true;

  std::vector<PasswordForm> forms = {shared_credentials,
                                     non_shared_credentials};
  controller()->OnPasswordAutofilled(
      forms, url::Origin::Create(forms.front().url), {});

  ASSERT_EQ(2u, controller()->GetCurrentForms().size());
  // Shared password notification was displayed already, so the state should be
  // MANAGE_STATE.
  EXPECT_EQ(controller()->GetState(), password_manager::ui::MANAGE_STATE);
  EXPECT_FALSE(controller()->IsAutomaticallyOpeningBubble());
  EXPECT_FALSE(controller()->IsShowingBubble());
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    ManagePasswordsUIControllerWithBrowserTest);

TEST_P(ManagePasswordsUIControllerTest, ShowChangePasswordBubble) {
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->ShowChangePasswordBubble(kExampleUsername, kExamplePassword);
  EXPECT_EQ(controller()->PasswordChangeUsername(), kExampleUsername);
  EXPECT_EQ(controller()->PasswordChangeNewPassword(), kExamplePassword);
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(password_manager::ui::PASSWORD_CHANGE_STATE);

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnBubbleHidden();
  ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest,
       UpdatePasswordBubbleSuppressedDuringPasswordChange) {
  PasswordChangeServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindLambdaForTesting([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
        return std::make_unique<MockPasswordChangeService>();
      }));
  auto* password_change_service = static_cast<MockPasswordChangeService*>(
      PasswordChangeServiceFactory::GetForProfile(profile()));

  // Emulate password change flow has started.
  PasswordChangeDelegateMock mock_delegate;
  EXPECT_CALL(mock_delegate, GetCurrentState)
      .WillRepeatedly(
          Return(PasswordChangeDelegate::State::kWaitingForChangePasswordForm));
  EXPECT_CALL(*password_change_service, GetPasswordChangeDelegate)
      .WillRepeatedly(Return(&mock_delegate));
  ASSERT_EQ(controller()->GetState(), password_manager::ui::INACTIVE_STATE);

  // Simulate update password form submitted. Bubble and icon should not be
  // updated.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility).Times(0);
  std::vector<PasswordForm> best_matches;
  controller()->OnUpdatePasswordSubmitted(
      CreateFormManagerWithBestMatches(best_matches, &submitted_form()));
  EXPECT_EQ(controller()->GetState(), password_manager::ui::INACTIVE_STATE);
}

TEST_P(ManagePasswordsUIControllerTest, AutomatedPasswordChangeOffered) {
  base::HistogramTester histogram_tester;
  PasswordChangeServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindLambdaForTesting([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
        return std::make_unique<MockPasswordChangeService>();
      }));

  std::vector<PasswordForm> matches = {test_local_form()};
  auto test_form_manager =
      CreateFormManagerWithBestMatches(matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));

  auto* password_change_service = static_cast<MockPasswordChangeService*>(
      PasswordChangeServiceFactory::GetForProfile(profile()));
  EXPECT_CALL(*password_change_service, GetPasswordChangeDelegate)
      .WillRepeatedly(Return(nullptr));
  EXPECT_CALL(
      *password_change_service,
      OfferPasswordChangeUi(
          CreatePasswordForm(kExampleUrl, kExampleUsername, kExamplePassword),
          web_contents()));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());

  // The old leak check dialog is not offered.
  EXPECT_CALL(*controller(), CreateCredentialLeakPrompt).Times(0);

  controller()->OnCredentialLeak(password_manager::LeakedPasswordDetails(
      password_manager::CreateLeakType(
          password_manager::IsSaved(true), password_manager::IsReused(false),
          password_manager::IsSyncing(false),
          password_manager::HasChangePasswordUrl(true)),
      CreatePasswordForm(kExampleUrl, kExampleUsername, kExamplePassword),
      /*in_account_store=*/false));

  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordChange.UserHasPasswordSavedOnAPCLaunch", true,
      1);
}

TEST_P(ManagePasswordsUIControllerTest,
       AutomatedPasswordChangeNotOfferedForSignUpForm) {
  PasswordChangeServiceFactory::GetInstance()->SetTestingFactory(
      profile(),
      base::BindLambdaForTesting([](content::BrowserContext* context)
                                     -> std::unique_ptr<KeyedService> {
        return std::make_unique<MockPasswordChangeService>();
      }));

  std::vector<PasswordForm> matches = {test_local_form()};
  auto test_form_manager =
      CreateFormManagerWithBestMatches(matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));

  // Automated password change is not offered.
  auto* password_change_service = static_cast<MockPasswordChangeService*>(
      PasswordChangeServiceFactory::GetForProfile(profile()));
  EXPECT_CALL(*password_change_service, GetPasswordChangeDelegate).Times(0);
  EXPECT_CALL(*password_change_service, OfferPasswordChangeUi).Times(0);

  // The old leak detection dialog is displayed.
  auto dialog_prompt = std::make_unique<PasswordLeakDialogMock>();
  CredentialLeakDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*dialog_prompt, ShowCredentialLeakPrompt);
  EXPECT_CALL(*controller(), CreateCredentialLeakPrompt)
      .WillOnce(DoAll(SaveArg<0>(&dialog_controller),
                      Return(std::move(dialog_prompt))));

  controller()->OnCredentialLeak(password_manager::LeakedPasswordDetails(
      password_manager::CreateLeakType(
          password_manager::IsSaved(true), password_manager::IsReused(false),
          password_manager::IsSyncing(false),
          password_manager::HasChangePasswordUrl(true)),
      CreateSignUpForm(kExampleUrl, kExampleUsername, kExamplePassword),
      /*in_account_store=*/false));
}

INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(ManagePasswordsUIControllerTest);
