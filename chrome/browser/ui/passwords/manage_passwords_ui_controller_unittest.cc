// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
#include "base/test/gmock_move_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/ui/hats/mock_trust_safety_sentiment_service.h"
#include "chrome/browser/ui/hats/trust_safety_sentiment_service_factory.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_controller.h"
#include "chrome/browser/ui/passwords/credential_manager_dialog_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/password_manager/core/browser/features/password_features.h"
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
  MOCK_METHOD(void, ControllerGone, (), (override));
};

class TestManagePasswordsIconView : public ManagePasswordsIconView {
 public:
  void SetState(password_manager::ui::State state) override { state_ = state; }
  password_manager::ui::State state() { return state_; }

 private:
  password_manager::ui::State state_;
};

class TestPasswordManagerClient
    : public password_manager::StubPasswordManagerClient {
 public:
  TestPasswordManagerClient()
      : mock_profile_store_(new MockPasswordStoreInterface()) {}

  MOCK_METHOD(void,
              TriggerReauthForPrimaryAccount,
              (signin_metrics::ReauthAccessPoint,
               base::OnceCallback<void(ReauthSucceeded)>),
              (override));

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
  MOCK_METHOD(CredentialLeakPrompt*,
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
  void HidePasswordBubble() override;

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

void TestManagePasswordsUIController::HidePasswordBubble() {
  opened_automatic_bubble_ = false;
  if (std::exchange(opened_automatic_bubble_, false) &&
      !web_contents()->IsBeingDestroyed()) {
    OnBubbleHidden();
  }
}

password_manager::PasswordForm BuildFormFromLoginAndURL(
    const std::string& username,
    const std::string& password,
    const std::string& url) {
  password_manager::PasswordForm form;
  form.username_value = base::ASCIIToUTF16(username);
  form.password_value = base::ASCIIToUTF16(password);
  form.url = GURL(url);
  form.signon_realm = form.url.DeprecatedGetOriginAsURL().spec();
  return form;
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
      .Times(AtMost(2))
      .WillRepeatedly(Return(best_matches));
  EXPECT_CALL(*form_manager, GetFederatedMatches())
      .Times(AtMost(2))
      .WillRepeatedly(
          Return(base::span<const password_manager::PasswordForm>()));
  EXPECT_CALL(*form_manager, GetURL())
      .Times(AtMost(2))
      .WillRepeatedly(ReturnRef(password_form->url));
  EXPECT_CALL(*form_manager, IsBlocklisted())
      .Times(AtMost(1))
      .WillOnce(Return(is_blocklisted));
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

}  // namespace

class ManagePasswordsUIControllerTest : public ChromeRenderViewHostTestHarness {
 public:
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

TEST_F(ManagePasswordsUIControllerTest, DefaultState) {
  EXPECT_TRUE(controller()->GetOrigin().opaque());

  ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordAutofilled) {
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

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmitted) {
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

TEST_F(ManagePasswordsUIControllerTest, BlocklistedFormPasswordSubmitted) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(
      best_matches, &submitted_form(), /*is_blocklisted=*/true);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_FALSE(controller()->opened_automatic_bubble());

  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmittedBubbleSuppressed) {
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

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmittedBubbleNotSuppressed) {
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

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmittedBubbleCancelled) {
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

TEST_F(ManagePasswordsUIControllerTest, DefaultStoreChanged) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  EXPECT_CALL(*client().GetPasswordFeatureManager(),
              ShouldChangeDefaultPasswordStore)
      .WillOnce(Return(true));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());

  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  EXPECT_EQ(url::Origin::Create(test_local_form().url),
            controller()->GetOrigin());

  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSWORD_STORE_CHANGED_BUBBLE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest,
       BlocklistedFormPasswordSubmittedDoesNotGetAutomaticWarning) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(
      best_matches, &submitted_form(), /*is_blocklisted=*/true);
  EXPECT_CALL(*client().GetPasswordFeatureManager(),
              ShouldChangeDefaultPasswordStore)
      .WillOnce(Return(true));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_FALSE(controller()->opened_automatic_bubble());

  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSWORD_STORE_CHANGED_BUBBLE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest,
       IfSaveBubbleIsSuppressedNoAutomaticWarning) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  std::vector<password_manager::InteractionsStats> stats = {
      {.origin_domain = submitted_form().url.DeprecatedGetOriginAsURL(),
       .username_value = submitted_form().username_value,
       .dismissal_count = kGreatDissmisalCount}};
  EXPECT_CALL(*client().GetPasswordFeatureManager(),
              ShouldChangeDefaultPasswordStore)
      .WillOnce(Return(true));
  EXPECT_CALL(*test_form_manager, GetInteractionsStats)
      .WillRepeatedly(Return(stats));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_FALSE(controller()->opened_automatic_bubble());

  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSWORD_STORE_CHANGED_BUBBLE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSaved) {
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
}

TEST_F(ManagePasswordsUIControllerTest, PhishedPasswordUpdated) {
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

TEST_F(ManagePasswordsUIControllerTest, PasswordSavedUKMRecording) {
  using UkmEntry = ukm::builders::PasswordForm;
  const struct {
    // Whether to simulate editing the username or picking a different password.
    bool edit_username;
    bool change_password;
    // The UMA sample expected for PasswordManager.EditsInSaveBubble.
    base::HistogramBase::Sample expected_uma_sample;
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

TEST_F(ManagePasswordsUIControllerTest,
       PasswordSavedInAccountStoreWhenReauthSucceeds) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  MockPasswordFormManagerForUI* test_form_manager_ptr = test_form_manager.get();

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  // The user hasn't opted in, so a reauth flow will be triggered.
  base::OnceCallback<void(ReauthSucceeded)> reauth_callback;
  EXPECT_CALL(client(),
              TriggerReauthForPrimaryAccount(
                  signin_metrics::ReauthAccessPoint::kPasswordSaveBubble, _))
      .WillOnce(MoveArg<1>(&reauth_callback));

  // The user clicks save which will invoke the reauth flow.
  controller()->AuthenticateUserForAccountStoreOptInAndSavePassword(
      submitted_form().username_value, submitted_form().password_value);

  // The bubble gets hidden after the user clicks on save.
  controller()->OnBubbleHidden();

  // Simulate a successful reauth which will cause the password to be saved.
  EXPECT_CALL(*test_form_manager_ptr, Save());
  std::move(reauth_callback).Run(ReauthSucceeded(true));

  // We should be now in the manage state and no other bubble should be opened
  // automatically.
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest,
       PasswordNotSavedInAccountStoreWhenReauthFails) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*test_form_manager, Save()).Times(0);

  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  // The user hasn't opted in, so a reauth flow will be triggered.
  base::OnceCallback<void(ReauthSucceeded)> reauth_callback;
  EXPECT_CALL(client(),
              TriggerReauthForPrimaryAccount(
                  signin_metrics::ReauthAccessPoint::kPasswordSaveBubble, _))
      .WillOnce(MoveArg<1>(&reauth_callback));

  {
    // Unsuccessful reauth should change the default store to profile store and
    // opt out of account storage. OptOutOfAccountStorageAndClearSettings()
    // clears the store pref, so SetDefaultPasswordStore() should come second.
    testing::InSequence in_sequence;
    EXPECT_CALL(*client().GetPasswordFeatureManager(),
                OptOutOfAccountStorageAndClearSettings());
    EXPECT_CALL(*client().GetPasswordFeatureManager(),
                SetDefaultPasswordStore(
                    password_manager::PasswordForm::Store::kProfileStore));
  }

  // The user clicks save which will invoke the reauth flow.
  controller()->AuthenticateUserForAccountStoreOptInAndSavePassword(
      submitted_form().username_value, submitted_form().password_value);

  // The bubble gets hidden after the user clicks on save.
  controller()->OnBubbleHidden();

  // Simulate an unsuccessful reauth which will cause the bubble to be open
  // again.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  std::move(reauth_callback).Run(ReauthSucceeded(false));

  // The bubble should have been opened again.
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
  EXPECT_TRUE(controller()->opened_automatic_bubble());
}

TEST_F(ManagePasswordsUIControllerTest, PasswordBlocklisted) {
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

TEST_F(ManagePasswordsUIControllerTest,
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

TEST_F(ManagePasswordsUIControllerTest, NormalNavigations) {
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

TEST_F(ManagePasswordsUIControllerTest, NormalNavigationsClosedBubble) {
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

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmittedToNonWebbyURL) {
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

TEST_F(ManagePasswordsUIControllerTest,
       OnBiometricAuthTransitionWhenStateInactive) {
  ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
  controller()->OnBiometricAuthenticationForFilling(profile()->GetPrefs());
  ASSERT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
}

TEST_F(ManagePasswordsUIControllerTest, BlocklistedElsewhere) {
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

TEST_F(ManagePasswordsUIControllerTest, AutomaticPasswordSave) {
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

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialLocal) {
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

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialLocalButFederated) {
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

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialCancel) {
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

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialPrefetch) {
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

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialPSL) {
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

TEST_F(ManagePasswordsUIControllerTest,
       PromptSaveBubbleAfterDefaultStoreChanged) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  EXPECT_CALL(*client().GetPasswordFeatureManager(),
              ShouldChangeDefaultPasswordStore)
      .WillOnce(Return(true));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility()).Times(2);

  // Change the pwm ui state to PASSWORD_STORE_CHANGED_BUBBLE_STATE.
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  EXPECT_EQ(url::Origin::Create(test_local_form().url),
            controller()->GetOrigin());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSWORD_STORE_CHANGED_BUBBLE_STATE);

  controller()->PromptSaveBubbleAfterDefaultStoreChanged();
  EXPECT_FALSE(controller()->opened_automatic_bubble());
  EXPECT_EQ(url::Origin::Create(test_local_form().url),
            controller()->GetOrigin());
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest,
       DefaultStoreChangedBubbleClosedAndKeyIconPressedManually) {
  std::vector<PasswordForm> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  EXPECT_CALL(*client().GetPasswordFeatureManager(),
              ShouldChangeDefaultPasswordStore)
      .WillOnce(Return(true));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility()).Times(2);

  // Change the pwm ui state to PASSWORD_STORE_CHANGED_BUBBLE_STATE.
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  EXPECT_EQ(url::Origin::Create(test_local_form().url),
            controller()->GetOrigin());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSWORD_STORE_CHANGED_BUBBLE_STATE);

  controller()->OnBubbleHidden();
  EXPECT_FALSE(controller()->opened_automatic_bubble());
  EXPECT_EQ(url::Origin::Create(test_local_form().url),
            controller()->GetOrigin());
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, AutoSignin) {
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

TEST_F(ManagePasswordsUIControllerTest, AutoSigninFirstRun) {
  EXPECT_CALL(*controller(), CreateAutoSigninPrompt(_))
      .WillOnce(Return(&dialog_prompt()));
  EXPECT_CALL(dialog_prompt(), ShowAutoSigninPrompt());
  controller()->OnPromptEnableAutoSignin();

  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
  EXPECT_CALL(dialog_prompt(), ControllerGone());
}

TEST_F(ManagePasswordsUIControllerTest, AutoSigninFirstRunAfterAutofill) {
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

TEST_F(ManagePasswordsUIControllerTest, AutoSigninFirstRunAfterNavigation) {
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

TEST_F(ManagePasswordsUIControllerTest, AutofillDuringAutoSignin) {
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

TEST_F(ManagePasswordsUIControllerTest, InactiveOnPSLMatched) {
  std::u16string kTestUsername = u"test_username";
  std::vector<PasswordForm> forms;
  PasswordForm psl_matched_test_form(test_local_form());
  psl_matched_test_form.match_type = PasswordForm::MatchType::kPSL;
  forms.push_back(psl_matched_test_form);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordAutofilled(
      forms, url::Origin::Create(forms.front().url), {});

  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
}

TEST_F(ManagePasswordsUIControllerTest, UpdatePasswordSubmitted) {
  std::vector<PasswordForm> best_matches = {test_local_form()};
  auto test_form_manager =
      CreateFormManagerWithBestMatches(best_matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));
  ExpectIconAndControllerStateIs(
      password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordUpdated) {
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

TEST_F(ManagePasswordsUIControllerTest, SavePendingStatePasswordAutofilled) {
  TestNotChangingStateOnAutofill(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, UpdatePendingStatePasswordAutofilled) {
  TestNotChangingStateOnAutofill(
      password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, ConfirmationStatePasswordAutofilled) {
  TestNotChangingStateOnAutofill(password_manager::ui::SAVE_CONFIRMATION_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, OpenBubbleTwice) {
  // Open the autosignin bubble.
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(test_local_form()));
  controller()->OnAutoSignin(std::move(local_credentials),
                             url::Origin::Create(test_local_form().url));
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, controller()->GetState());
  // The delegate used by the bubble for communicating with the controller.
  base::WeakPtr<PasswordsModelDelegate> proxy_delegate =
      controller()->GetModelDelegateProxy();

  // Open the bubble again.
  local_credentials.emplace_back(new PasswordForm(test_local_form()));
  controller()->OnAutoSignin(std::move(local_credentials),
                             url::Origin::Create(test_local_form().url));
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, controller()->GetState());
  // Check the delegate is destroyed. Thus, the first bubble has no way to mess
  // up with the controller's state.
  EXPECT_FALSE(proxy_delegate);
}

TEST_F(ManagePasswordsUIControllerTest, ManualFallbackForSaving_UseFallback) {
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
TEST_F(ManagePasswordsUIControllerTest,
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
// goes away if no passwords were persited before the manual fallback.
TEST_F(ManagePasswordsUIControllerTest,
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

TEST_F(ManagePasswordsUIControllerTest,
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

TEST_F(ManagePasswordsUIControllerTest,
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

TEST_F(ManagePasswordsUIControllerTest,
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

TEST_F(ManagePasswordsUIControllerTest,
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

TEST_F(ManagePasswordsUIControllerTest,
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

TEST_F(ManagePasswordsUIControllerTest,
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

TEST_F(ManagePasswordsUIControllerTest,
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

TEST_F(ManagePasswordsUIControllerTest,
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
TEST_F(ManagePasswordsUIControllerTest, PasswordDetails_IsntShownIfAuthFailed) {
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

TEST_F(ManagePasswordsUIControllerTest, AutofillDuringSignInPromo) {
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

TEST_F(ManagePasswordsUIControllerTest, SaveBubbleAfterLeakCheck) {
  std::vector<PasswordForm> matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(matches, &submitted_form());
  MockPasswordFormManagerForUI* form_manager_ptr = test_form_manager.get();
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_TRUE(controller()->opened_automatic_bubble());

  // Leak detection dialog hides the bubble.
  PasswordLeakDialogMock dialog_prompt;
  CredentialLeakDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*controller(), CreateCredentialLeakPrompt)
      .WillOnce(DoAll(SaveArg<0>(&dialog_controller), Return(&dialog_prompt)));
  EXPECT_CALL(dialog_prompt, ShowCredentialLeakPrompt);
  controller()->OnCredentialLeak(
      password_manager::CreateLeakType(password_manager::IsSaved(false),
                                       password_manager::IsReused(false),
                                       password_manager::IsSyncing(false)),
      GURL(kExampleUrl), kExampleUsername);
  // The bubble is gone.
  EXPECT_FALSE(controller()->opened_automatic_bubble());

  // After closing the lead check dialog, the blocklisting will be checked again
  // to decide whether to reopen the save prompt.
  EXPECT_CALL(*form_manager_ptr, IsBlocklisted()).WillOnce(Return(false));
  EXPECT_CALL(*form_manager_ptr, GetInteractionsStats())
      .WillOnce(
          Return(base::span<const password_manager::InteractionsStats>()));

  // Close the dialog.
  EXPECT_CALL(dialog_prompt, ControllerGone);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  dialog_controller->OnAcceptDialog();

  // The save bubble is back.
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest,
       NoSaveBubbleAfterLeakCheckForBlocklistedWebsites) {
  std::vector<PasswordForm> matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(
      matches, &submitted_form(), /*is_blocklisted=*/true);
  MockPasswordFormManagerForUI* form_manager_ptr = test_form_manager.get();
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_FALSE(controller()->opened_automatic_bubble());

  // Leak detection dialog hides the bubble.
  PasswordLeakDialogMock dialog_prompt;
  CredentialLeakDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*controller(), CreateCredentialLeakPrompt)
      .WillOnce(DoAll(SaveArg<0>(&dialog_controller), Return(&dialog_prompt)));
  EXPECT_CALL(dialog_prompt, ShowCredentialLeakPrompt);
  controller()->OnCredentialLeak(
      password_manager::CreateLeakType(password_manager::IsSaved(false),
                                       password_manager::IsReused(false),
                                       password_manager::IsSyncing(false)),
      GURL(kExampleUrl), kExampleUsername);
  // The bubble is gone.
  EXPECT_FALSE(controller()->opened_automatic_bubble());

  // After closing the lead check dialog, the blocklisting will be checked again
  // to decide whether to reopen the save prompt.
  EXPECT_CALL(*form_manager_ptr, IsBlocklisted()).WillOnce(Return(true));

  // Close the dialog.
  EXPECT_CALL(dialog_prompt, ControllerGone);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  dialog_controller->OnAcceptDialog();

  // The save bubble should not be opened because the website is blocklisted.
  EXPECT_FALSE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, UpdateBubbleAfterLeakCheck) {
  std::vector<PasswordForm> matches = {test_local_form()};
  auto test_form_manager =
      CreateFormManagerWithBestMatches(matches, &submitted_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));
  EXPECT_TRUE(controller()->opened_automatic_bubble());

  // Leak detection dialog hides the bubble.
  PasswordLeakDialogMock dialog_prompt;
  CredentialLeakDialogController* dialog_controller = nullptr;
  EXPECT_CALL(*controller(), CreateCredentialLeakPrompt)
      .WillOnce(DoAll(SaveArg<0>(&dialog_controller), Return(&dialog_prompt)));
  EXPECT_CALL(dialog_prompt, ShowCredentialLeakPrompt);
  controller()->OnCredentialLeak(
      password_manager::CreateLeakType(password_manager::IsSaved(true),
                                       password_manager::IsReused(false),
                                       password_manager::IsSyncing(false)),
      GURL(kExampleUrl), kExampleUsername);
  // The bubble is gone.
  EXPECT_FALSE(controller()->opened_automatic_bubble());

  // Close the dialog.
  EXPECT_CALL(dialog_prompt, ControllerGone);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  dialog_controller->OnAcceptDialog();

  // The update bubble is back.
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest,
       NotifyUnsyncedCredentialsWillBeDeleted) {
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  std::vector<password_manager::PasswordForm> credentials(2);
  credentials[0] =
      BuildFormFromLoginAndURL("user1", "password1", "http://a.com");
  credentials[1] =
      BuildFormFromLoginAndURL("user2", "password2", "http://b.com");

  controller()->NotifyUnsyncedCredentialsWillBeDeleted(credentials);

  EXPECT_EQ(controller()->GetUnsyncedCredentials(), credentials);
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::WILL_DELETE_UNSYNCED_ACCOUNT_PASSWORDS_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, SaveUnsyncedCredentialsInProfileStore) {
  std::vector<password_manager::PasswordForm> credentials = {
      BuildFormFromLoginAndURL("user1", "password1", "http://a.com"),
      BuildFormFromLoginAndURL("user2", "password2", "http://b.com")};

  // Set expectations on the store.
  MockPasswordStoreInterface* profile_store =
      client().GetProfilePasswordStore();
  EXPECT_CALL(*profile_store,
              AddLogin(MatchesLoginAndURL(credentials[0].username_value,
                                          credentials[0].password_value,
                                          credentials[0].url),
                       _));
  EXPECT_CALL(*profile_store,
              AddLogin(MatchesLoginAndURL(credentials[1].username_value,
                                          credentials[1].password_value,
                                          credentials[1].url),
                       _));

  // Save.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->SaveUnsyncedCredentialsInProfileStore(credentials);

  // Check the credentials are gone and the bubble is closed.
  EXPECT_TRUE(controller()->GetUnsyncedCredentials().empty());
  EXPECT_FALSE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, DiscardUnsyncedCredentials) {
  // Setup state with unsynced credentials.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  std::vector<password_manager::PasswordForm> credentials = {
      BuildFormFromLoginAndURL("user", "password", "http://a.com")};
  controller()->NotifyUnsyncedCredentialsWillBeDeleted(std::move(credentials));

  // No save should happen on the profile store.
  MockPasswordStoreInterface* profile_store =
      client().GetProfilePasswordStore();
  EXPECT_CALL(*profile_store, AddLogin).Times(0);

  // Discard.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->DiscardUnsyncedCredentials();

  // Check the credentials are gone and the bubble is closed.
  EXPECT_TRUE(controller()->GetUnsyncedCredentials().empty());
  EXPECT_FALSE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, OpenBubbleForMovableForm) {
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

TEST_F(ManagePasswordsUIControllerTest, OpenMoveBubbleFromManagementBubble) {
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

TEST_F(ManagePasswordsUIControllerTest, CloseMoveBubble) {
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

TEST_F(ManagePasswordsUIControllerTest, OpenSafeStateBubble) {
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

TEST_F(ManagePasswordsUIControllerTest, OpenMoreToFixBubble) {
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

TEST_F(ManagePasswordsUIControllerTest, NoMoreToFixBubbleIfPromoStillOpen) {
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

TEST_F(ManagePasswordsUIControllerTest, UsernameAdded) {
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

TEST_F(ManagePasswordsUIControllerTest, IsDeviceAuthenticatorObtained) {
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

TEST_F(ManagePasswordsUIControllerTest, PasskeySavedWithoutGpmPinCreation) {
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasskeySaved(/*gpm_pin_created=*/false);
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSKEY_SAVED_CONFIRMATION_STATE);
  EXPECT_FALSE(controller()->GpmPinCreatedDuringRecentPasskeyCreation());
}

TEST_F(ManagePasswordsUIControllerTest, PasskeySavedWithGpmPinCreation) {
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasskeySaved(/*gpm_pin_created=*/true);
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSKEY_SAVED_CONFIRMATION_STATE);
  EXPECT_TRUE(controller()->GpmPinCreatedDuringRecentPasskeyCreation());
}

TEST_F(ManagePasswordsUIControllerTest, InvalidPasskeyDeleted) {
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasskeyDeleted();
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSKEY_DELETED_CONFIRMATION_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, OpenPasskeyUpdatedBubble) {
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasskeyUpdated();
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSKEY_UPDATED_CONFIRMATION_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, OpenPasskeyNotAcceptedBubble) {
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasskeyNotAccepted();
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PASSKEY_NOT_ACCEPTED_STATE);
}

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(ManagePasswordsUIControllerTest,
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
TEST_F(ManagePasswordsUIControllerTest,
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
TEST_F(ManagePasswordsUIControllerTest,
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
TEST_F(
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
TEST_F(ManagePasswordsUIControllerTest,
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
TEST_F(ManagePasswordsUIControllerTest,
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

TEST_F(ManagePasswordsUIControllerTest,
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

TEST_F(ManagePasswordsUIControllerTest, BiometricActivationConfirmation) {
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

TEST_F(ManagePasswordsUIControllerTest,
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

TEST_F(ManagePasswordsUIControllerTest,
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

TEST_F(ManagePasswordsUIControllerTest, AuthenticationCancledOnPageChange) {
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

TEST_F(ManagePasswordsUIControllerTest, OnBiometricAuthBeforeFillingDeclined) {
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
TEST_F(ManagePasswordsUIControllerTest, OnKeychainErrorShouldShowBubble) {
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

TEST_F(ManagePasswordsUIControllerTest, OnKeychainErrorShouldNotShowBubble) {
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

class ManagePasswordsUIControllerWithBrowserTest
    : public BrowserWithTestWindowTest {
 public:
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
  BrowserWithTestWindowTest::SetUp();
  AddTab(browser(), GURL(kExampleUrl));
  ManagePasswordsUIController::CreateForWebContents(web_contents());
  controller()->set_client(&client_);
}

TEST_F(ManagePasswordsUIControllerWithBrowserTest,
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
  controller()->OnPasswordAutofilled(
      forms, url::Origin::Create(forms.front().url), {});

  ASSERT_EQ(2u, controller()->GetCurrentForms().size());
  EXPECT_EQ(controller()->GetState(),
            password_manager::ui::NOTIFY_RECEIVED_SHARED_CREDENTIALS);
  EXPECT_TRUE(controller()->IsAutomaticallyOpeningBubble());
  // All interactions with the bubble will close it and invoke OnBubbleHidden().
  controller()->OnBubbleHidden();
  // The bubble should transition to the manage state upon any interaction.
  EXPECT_EQ(controller()->GetState(), password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerWithBrowserTest,
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
}
