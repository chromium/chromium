// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/mock_callback.h"
#include "build/build_config.h"
#include "chrome/browser/ui/passwords/credential_leak_dialog_controller.h"
#include "chrome/browser/ui/passwords/credential_manager_dialog_controller.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_view.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/browser/ui/passwords/password_dialog_prompts.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/mock_password_form_manager_for_ui.h"
#include "components/password_manager/core/browser/password_bubble_experiment.h"
#include "components/password_manager/core/browser/password_form_metrics_recorder.h"
#include "components/password_manager/core/browser/statistics_table.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "components/prefs/pref_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_utils.h"
#include "content/public/test/web_contents_tester.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill::PasswordForm;
using base::ASCIIToUTF16;
using password_manager::MockPasswordFormManagerForUI;
using ::testing::_;
using ::testing::AtMost;
using ::testing::Contains;
using ::testing::DoAll;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::Pointee;
using ::testing::Return;
using ::testing::ReturnRef;
using ::testing::SaveArg;

namespace {

// A random URL.
constexpr char kExampleUrl[] = "http://example.com";

// Number of dismissals that for sure suppresses the bubble.
constexpr int kGreatDissmisalCount = 10;

class CredentialManagementDialogPromptMock : public AccountChooserPrompt,
                                             public AutoSigninFirstRunPrompt {
 public:
  MOCK_METHOD0(ShowAccountChooser, void());
  MOCK_METHOD0(ShowAutoSigninPrompt, void());
  MOCK_METHOD0(ControllerGone, void());
};

class PasswordLeakDialogMock : public CredentialLeakPrompt {
 public:
  MOCK_METHOD0(ShowCredentialLeakPrompt, void());
  MOCK_METHOD0(ControllerGone, void());
};

class TestManagePasswordsIconView : public ManagePasswordsIconView {
 public:
  void SetState(password_manager::ui::State state) override { state_ = state; }
  password_manager::ui::State state() { return state_; }

 private:
  password_manager::ui::State state_;
};

// This sublass is used to disable some code paths which are not essential for
// testing.
class TestManagePasswordsUIController : public ManagePasswordsUIController {
 public:
  TestManagePasswordsUIController(
      content::WebContents* contents,
      password_manager::PasswordManagerClient* client);

  bool opened_bubble() const { return opened_bubble_; }
  bool opened_automatic_bubble() const { return opened_automatic_bubble_; }
  bool are_passwords_revealed_in_opened_bubble() const {
    return are_passwords_revealed_in_opened_bubble_;
  }

  MOCK_METHOD1(CreateAccountChooser,
               AccountChooserPrompt*(CredentialManagerDialogController*));
  MOCK_METHOD1(CreateAutoSigninPrompt,
               AutoSigninFirstRunPrompt*(CredentialManagerDialogController*));
  MOCK_METHOD1(CreateCredentialLeakPrompt,
               CredentialLeakPrompt*(CredentialLeakDialogController*));
  MOCK_CONST_METHOD0(HasBrowserWindow, bool());
  MOCK_METHOD0(OnUpdateBubbleAndIconVisibility, void());
  using ManagePasswordsUIController::DidFinishNavigation;

 private:
  void UpdateBubbleAndIconVisibility() override;
  void HidePasswordBubble() override;
  bool ShowAuthenticationDialog() override { return true; }

  bool opened_bubble_ = false;
  bool opened_automatic_bubble_ = false;
  bool are_passwords_revealed_in_opened_bubble_ = false;
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
  opened_bubble_ = ShouldBubblePopUp();
  opened_automatic_bubble_ = IsAutomaticallyOpeningBubble();
  ManagePasswordsUIController::UpdateBubbleAndIconVisibility();
  OnUpdateBubbleAndIconVisibility();
  TestManagePasswordsIconView view;
  UpdateIconAndBubbleState(&view);
  if (opened_bubble_) {
    are_passwords_revealed_in_opened_bubble_ =
        ArePasswordsRevealedWhenBubbleIsOpened();
    OnBubbleShown();
  }
}

void TestManagePasswordsUIController::HidePasswordBubble() {
  opened_automatic_bubble_ = false;
  are_passwords_revealed_in_opened_bubble_ = false;
  if (std::exchange(opened_bubble_, false) &&
      !web_contents()->IsBeingDestroyed()) {
    OnBubbleHidden();
  }
}

}  // namespace

class ManagePasswordsUIControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override;

  password_manager::StubPasswordManagerClient& client() { return client_; }
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

  std::unique_ptr<MockPasswordFormManagerForUI>
  CreateFormManagerWithBestMatches(
      const std::vector<const PasswordForm*>* best_matches,
      bool is_blacklisted = false);

  // Tests that the state is not changed when the password is autofilled.
  void TestNotChangingStateOnAutofill(password_manager::ui::State state);

 private:
  password_manager::StubPasswordManagerClient client_;

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
  new TestManagePasswordsUIController(web_contents(), &client_);

  test_local_form_.origin = GURL("http://example.com/login");
  test_local_form_.username_value = ASCIIToUTF16("username");
  test_local_form_.username_element = ASCIIToUTF16("username_element");
  test_local_form_.password_value = ASCIIToUTF16("12345");
  test_local_form_.password_element = ASCIIToUTF16("password_element");

  test_federated_form_.origin = GURL("http://example.com/login");
  test_federated_form_.username_value = ASCIIToUTF16("username");
  test_federated_form_.federation_origin =
      url::Origin::Create(GURL("https://federation.test/"));

  submitted_form_ = test_local_form_;
  submitted_form_.username_value = ASCIIToUTF16("submitted_username");
  submitted_form_.password_value = ASCIIToUTF16("pass12345");

  // We need to be on a "webby" URL for most tests.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL(kExampleUrl));
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

std::unique_ptr<MockPasswordFormManagerForUI>
ManagePasswordsUIControllerTest::CreateFormManagerWithBestMatches(
    const std::vector<const PasswordForm*>* best_matches,
    bool is_blacklisted) {
  auto form_manager =
      std::make_unique<testing::StrictMock<MockPasswordFormManagerForUI>>();
  EXPECT_CALL(*form_manager, GetBestMatches())
      .Times(AtMost(1))
      .WillOnce(ReturnRef(*best_matches));
  EXPECT_CALL(*form_manager, GetFederatedMatches())
      .Times(AtMost(1))
      .WillOnce(Return(std::vector<const autofill::PasswordForm*>()));
  EXPECT_CALL(*form_manager, GetOrigin())
      .Times(AtMost(1))
      .WillOnce(ReturnRef(test_local_form_.origin));
  EXPECT_CALL(*form_manager, IsBlacklisted())
      .Times(AtMost(1))
      .WillOnce(Return(is_blacklisted));
  EXPECT_CALL(*form_manager, GetInteractionsStats())
      .Times(AtMost(1))
      .WillOnce(
          Return(base::span<const password_manager::InteractionsStats>()));
  EXPECT_CALL(*form_manager, GetPendingCredentials())
      .WillRepeatedly(ReturnRef(submitted_form_));
  EXPECT_CALL(*form_manager, GetMetricsRecorder())
      .WillRepeatedly(Return(nullptr));
  return form_manager;
}

void ManagePasswordsUIControllerTest::TestNotChangingStateOnAutofill(
    password_manager::ui::State state) {
  DCHECK(state == password_manager::ui::PENDING_PASSWORD_STATE ||
         state == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE ||
         state == password_manager::ui::CONFIRMATION_STATE);

  // Set the bubble state to |state|.
  std::vector<const PasswordForm*> best_matches;
  std::unique_ptr<MockPasswordFormManagerForUI> test_form_manager =
      CreateFormManagerWithBestMatches(&best_matches);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  if (state == password_manager::ui::PENDING_PASSWORD_STATE)
    controller()->OnPasswordSubmitted(std::move(test_form_manager));
  else if (state == password_manager::ui::PENDING_PASSWORD_UPDATE_STATE)
    controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));
  else  // password_manager::ui::CONFIRMATION_STATE
    controller()->OnAutomaticPasswordSave(std::move(test_form_manager));
  ASSERT_EQ(state, controller()->GetState());

  // Autofill happens.
  std::vector<const PasswordForm*> forms = {&test_local_form()};
  controller()->OnPasswordAutofilled(forms, forms.front()->origin, nullptr);

  // State shouldn't changed.
  ExpectIconAndControllerStateIs(state);
}

TEST_F(ManagePasswordsUIControllerTest, DefaultState) {
  EXPECT_EQ(GURL::EmptyGURL(), controller()->GetOrigin());

  ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordAutofilled) {
  std::vector<const PasswordForm*> forms = {&test_local_form()};
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordAutofilled(forms, forms.front()->origin, nullptr);

  EXPECT_EQ(test_local_form().origin, controller()->GetOrigin());
  ASSERT_EQ(1u, controller()->GetCurrentForms().size());
  EXPECT_EQ(test_local_form().username_value,
            controller()->GetCurrentForms()[0]->username_value);

  // Controller should store a separate copy of the form as it doesn't own it.
  EXPECT_NE(&test_local_form(), controller()->GetCurrentForms()[0].get());

  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmitted) {
  std::vector<const PasswordForm*> best_matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&best_matches);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  EXPECT_EQ(test_local_form().origin, controller()->GetOrigin());

  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, BlacklistedFormPasswordSubmitted) {
  std::vector<const PasswordForm*> best_matches;
  auto test_form_manager =
      CreateFormManagerWithBestMatches(&best_matches, /*is_blacklisted=*/true);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_FALSE(controller()->opened_automatic_bubble());

  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmittedBubbleSuppressed) {
  std::vector<const PasswordForm*> best_matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&best_matches);
  std::vector<password_manager::InteractionsStats> stats(1);
  stats[0].origin_domain = submitted_form().origin.GetOrigin();
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
  std::vector<const PasswordForm*> best_matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&best_matches);
  std::vector<password_manager::InteractionsStats> stats(1);
  stats[0].origin_domain = submitted_form().origin.GetOrigin();
  stats[0].username_value = ASCIIToUTF16("not my username");
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
  content::WebContentsTester::For(web_content.get())
      ->NavigateAndCommit(GURL(kExampleUrl));
  ManagePasswordsUIController::CreateForWebContents(web_content.get());
  ManagePasswordsUIController* controller =
      ManagePasswordsUIController::FromWebContents(web_content.get());
  controller->set_client(&client());

  std::vector<const PasswordForm*> best_matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&best_matches);
  // The bubble is ready to open but the tab is inactive. Therefore, we don't
  // call UpdateIconAndBubbleState here.
  controller->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_TRUE(controller->IsAutomaticallyOpeningBubble());

  // The tab navigated in background. Because the controller's state has changed
  // the bubble shouldn't pop up anymore.
  content::WebContentsTester::For(web_content.get())
      ->NavigateAndCommit(GURL("http://google.com"));
  EXPECT_FALSE(controller->IsAutomaticallyOpeningBubble());
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSaved) {
  std::vector<const PasswordForm*> best_matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&best_matches);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*test_form_manager, Save());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  base::HistogramTester histogram_tester;
  controller()->SavePassword(submitted_form().username_value,
                             submitted_form().password_value);
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordSavedWithManualFallback", false, 1);
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
            true /*is_main_frame_secure*/, source_id);

    // Exercise controller.
    std::vector<const PasswordForm*> best_matches;
    auto test_form_manager = CreateFormManagerWithBestMatches(&best_matches);
    EXPECT_CALL(*test_form_manager, GetMetricsRecorder)
        .WillRepeatedly(Return(recorder.get()));
    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
    EXPECT_CALL(*test_form_manager,
                OnUpdateUsernameFromPrompt(ASCIIToUTF16("other_username")))
        .Times(test.edit_username);
    EXPECT_CALL(*test_form_manager,
                OnUpdatePasswordFromPrompt(ASCIIToUTF16("other_pwd")))
        .Times(test.change_password);
    EXPECT_CALL(*test_form_manager, Save());
    controller()->OnPasswordSubmitted(std::move(test_form_manager));

    controller()->SavePassword(
        test.edit_username ? base::UTF8ToUTF16("other_username")
                           : submitted_form().username_value,
        test.change_password ? base::UTF8ToUTF16("other_pwd")
                             : submitted_form().password_value);
    ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);

    // Fake navigation so that the old form manager gets destroyed and
    // reports its metrics. Need to close the bubble, otherwise the bubble
    // state is retained on navigation, and the PasswordFormManager is not
    // destroyed.
    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
    controller()->OnBubbleHidden();
    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
    content::MockNavigationHandle test_handle(web_contents());
    test_handle.set_has_committed(true);
    controller()->DidFinishNavigation(&test_handle);

    recorder = nullptr;
    ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(controller()));

    // Verify metrics.
    const auto& entries =
        test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    EXPECT_EQ(1u, entries.size());
    for (const auto* entry : entries) {
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

TEST_F(ManagePasswordsUIControllerTest, PasswordBlacklisted) {
  std::vector<const PasswordForm*> best_matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&best_matches);

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*test_form_manager, OnNeverClicked());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->NeverSavePassword();
  ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest,
       PasswordBlacklistedWithExistingCredentials) {
  std::vector<const PasswordForm*> best_matches = {&test_local_form()};
  auto test_form_manager = CreateFormManagerWithBestMatches(&best_matches);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*test_form_manager, OnNeverClicked());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->NeverSavePassword();
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, NormalNavigations) {
  std::vector<const PasswordForm*> best_matches = {&test_local_form()};
  auto test_form_manager = CreateFormManagerWithBestMatches(&best_matches);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);

  // Fake-navigate. We expect the bubble's state to persist so a user reasonably
  // has been able to interact with the bubble. This happens on
  // `accounts.google.com`, for instance.
  content::MockNavigationHandle test_handle(web_contents());
  test_handle.set_has_committed(true);
  controller()->DidFinishNavigation(&test_handle);
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, NormalNavigationsClosedBubble) {
  std::vector<const PasswordForm*> best_matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&best_matches);
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
  content::MockNavigationHandle test_handle(web_contents());
  test_handle.set_has_committed(true);
  controller()->DidFinishNavigation(&test_handle);
  ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmittedToNonWebbyURL) {
  // Navigate to a non-webby URL, then see what happens!
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("chrome://sign-in"));

  std::vector<const PasswordForm*> best_matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&best_matches);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_EQ(GURL::EmptyGURL(), controller()->GetOrigin());

  ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, BlacklistedElsewhere) {
  base::string16 kTestUsername = ASCIIToUTF16("test_username");
  std::vector<const PasswordForm*> forms;
  forms.push_back(&test_local_form());
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordAutofilled(forms, forms.front()->origin, nullptr);

  test_local_form().blacklisted_by_user = true;
  password_manager::PasswordStoreChange change(
      password_manager::PasswordStoreChange::ADD, test_local_form());
  password_manager::PasswordStoreChangeList list(1, change);
  controller()->OnLoginsChanged(list);

  EXPECT_EQ(test_local_form().origin, controller()->GetOrigin());

  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, AutomaticPasswordSave) {
  std::vector<const PasswordForm*> best_matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&best_matches);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnAutomaticPasswordSave(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::CONFIRMATION_STATE, controller()->GetState());

  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnBubbleHidden();
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialLocal) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(test_local_form()));
  GURL origin(kExampleUrl);
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
  GURL origin(kExampleUrl);
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
  GURL origin(kExampleUrl);
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
  GURL origin(kExampleUrl);

  // Simulate requesting a credential during prefetch. The tab has no associated
  // browser. Nothing should happen.
  EXPECT_CALL(*controller(), HasBrowserWindow()).WillOnce(Return(false));
  base::MockCallback<ManagePasswordsState::CredentialsCallback> choose_callback;
  EXPECT_FALSE(controller()->OnChooseCredentials(
      std::move(local_credentials), origin, choose_callback.Get()));
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
}

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialPSL) {
  test_local_form().is_public_suffix_match = true;
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(test_local_form()));
  GURL origin(kExampleUrl);
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

TEST_F(ManagePasswordsUIControllerTest, AutoSignin) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(test_local_form()));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnAutoSignin(std::move(local_credentials),
                             test_local_form().origin);
  EXPECT_EQ(test_local_form().origin, controller()->GetOrigin());
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
  const base::string16 kTestUsername = test_form_ptr->username_value;
  std::vector<const PasswordForm*> forms;
  forms.push_back(test_form_ptr);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordAutofilled(forms, test_form_ptr->origin, nullptr);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());

  // Pop up the autosignin promo. The state should stay intact.
  EXPECT_CALL(*controller(), CreateAutoSigninPrompt(_))
      .WillOnce(Return(&dialog_prompt()));
  EXPECT_CALL(dialog_prompt(), ShowAutoSigninPrompt());
  controller()->OnPromptEnableAutoSignin();

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->GetState());
  EXPECT_EQ(test_form_ptr->origin, controller()->GetOrigin());
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
  content::MockNavigationHandle test_handle(web_contents());
  test_handle.set_has_committed(true);
  controller()->DidFinishNavigation(&test_handle);
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(&dialog_prompt()));
  EXPECT_CALL(dialog_prompt(), ControllerGone());
}

TEST_F(ManagePasswordsUIControllerTest, AutofillDuringAutoSignin) {
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(test_local_form()));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnAutoSignin(std::move(local_credentials),
                             test_local_form().origin);
  ExpectIconAndControllerStateIs(password_manager::ui::AUTO_SIGNIN_STATE);
  std::vector<const PasswordForm*> forms;
  base::string16 kTestUsername = test_local_form().username_value;
  forms.push_back(&test_local_form());
  controller()->OnPasswordAutofilled(forms, forms.front()->origin, nullptr);

  ExpectIconAndControllerStateIs(password_manager::ui::AUTO_SIGNIN_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, InactiveOnPSLMatched) {
  base::string16 kTestUsername = ASCIIToUTF16("test_username");
  std::vector<const PasswordForm*> forms;
  PasswordForm psl_matched_test_form(test_local_form());
  psl_matched_test_form.is_public_suffix_match = true;
  forms.push_back(&psl_matched_test_form);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordAutofilled(forms, forms.front()->origin, nullptr);

  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->GetState());
}

TEST_F(ManagePasswordsUIControllerTest, UpdatePasswordSubmitted) {
  std::vector<const PasswordForm*> best_matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&best_matches);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));
  ExpectIconAndControllerStateIs(
      password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, PasswordUpdated) {
  std::vector<const PasswordForm*> best_matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&best_matches);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  EXPECT_CALL(*test_form_manager, Save());
  controller()->OnUpdatePasswordSubmitted(std::move(test_form_manager));

  ExpectIconAndControllerStateIs(
      password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
  base::HistogramTester histogram_tester;
  controller()->SavePassword(submitted_form().username_value,
                             submitted_form().password_value);
  ExpectIconAndControllerStateIs(password_manager::ui::MANAGE_STATE);
  histogram_tester.ExpectUniqueSample(
      "PasswordManager.PasswordSavedWithManualFallback", false, 1);
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
  TestNotChangingStateOnAutofill(password_manager::ui::CONFIRMATION_STATE);
}

TEST_F(ManagePasswordsUIControllerTest, OpenBubbleTwice) {
  // Open the autosignin bubble.
  std::vector<std::unique_ptr<PasswordForm>> local_credentials;
  local_credentials.emplace_back(new PasswordForm(test_local_form()));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnAutoSignin(std::move(local_credentials),
                             test_local_form().origin);
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, controller()->GetState());
  // The delegate used by the bubble for communicating with the controller.
  base::WeakPtr<PasswordsModelDelegate> proxy_delegate =
      controller()->GetModelDelegateProxy();

  // Open the bubble again.
  local_credentials.emplace_back(new PasswordForm(test_local_form()));
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnAutoSignin(std::move(local_credentials),
                             test_local_form().origin);
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
            true /*is_main_frame_secure*/, source_id);
    std::vector<const PasswordForm*> matches;
    auto test_form_manager = CreateFormManagerWithBestMatches(&matches);
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
    content::MockNavigationHandle test_handle(web_contents());
    test_handle.set_has_committed(true);
    controller()->DidFinishNavigation(&test_handle);

    recorder = nullptr;
    EXPECT_TRUE(testing::Mock::VerifyAndClearExpectations(controller()));

    // Verify metrics.
    const auto& entries =
        test_ukm_recorder.GetEntriesByName(UkmEntry::kEntryName);
    ASSERT_EQ(1u, entries.size());
    auto* entry = entries[0];
    EXPECT_EQ(source_id, entry->source_id);

    histogram_tester.ExpectUniqueSample(
        "PasswordManager.PasswordSavedWithManualFallback", true, 1);
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
    std::vector<const PasswordForm*> matches = {&test_local_form()};
    auto test_form_manager = CreateFormManagerWithBestMatches(&matches);

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
  std::vector<const PasswordForm*> matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&matches);

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

    std::vector<const PasswordForm*> matches;
    auto test_form_manager = CreateFormManagerWithBestMatches(&matches);

    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
    controller()->OnShowManualFallbackForSaving(
        std::move(test_form_manager), false /* has_generated_password */,
        false /* is_update */);
    ExpectIconAndControllerStateIs(
        password_manager::ui::PENDING_PASSWORD_STATE);
    testing::Mock::VerifyAndClearExpectations(controller());
    if (enforce_navigation) {
      // Fake-navigate. The fallback should persist.
      content::MockNavigationHandle test_handle(web_contents());
      test_handle.set_has_committed(true);
      controller()->DidFinishNavigation(&test_handle);
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
    std::vector<const PasswordForm*> matches;
    auto test_form_manager = CreateFormManagerWithBestMatches(&matches);

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
      content::MockNavigationHandle test_handle(web_contents());
      test_handle.set_has_committed(true);
      controller()->DidFinishNavigation(&test_handle);
      ExpectIconAndControllerStateIs(password_manager::ui::INACTIVE_STATE);
    }
    testing::Mock::VerifyAndClearExpectations(controller());
  }
}

TEST_F(ManagePasswordsUIControllerTest,
       ManualFallbackForSavingFollowedByAutomaticBubble) {
  std::vector<const PasswordForm*> matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&matches);
  PasswordForm pending = test_local_form();
  pending.username_value = ASCIIToUTF16("manual_username");
  pending.password_value = ASCIIToUTF16("manual_pass1234");
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
  test_form_manager = CreateFormManagerWithBestMatches(&matches);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility()).Times(0);
  controller()->OnPasswordSubmitted(std::move(test_form_manager));

  // It should have no effect as the bubble was already open.
  ExpectIconAndControllerStateIs(password_manager::ui::PENDING_PASSWORD_STATE);
  EXPECT_EQ(ASCIIToUTF16("manual_username"),
            controller()->GetPendingPassword().username_value);
  EXPECT_EQ(ASCIIToUTF16("manual_pass1234"),
            controller()->GetPendingPassword().password_value);
}

TEST_F(ManagePasswordsUIControllerTest,
       ManualFallbackForSaving_GeneratedPassword) {
  for (bool user_closed_bubble : {false, true}) {
    SCOPED_TRACE(testing::Message("user_closed_bubble = ")
                 << user_closed_bubble);
    std::vector<const PasswordForm*> matches = {&test_local_form()};
    auto test_form_manager = CreateFormManagerWithBestMatches(&matches);
    EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
    controller()->OnShowManualFallbackForSaving(
        std::move(test_form_manager), true /* has_generated_password */, false);
    ExpectIconAndControllerStateIs(password_manager::ui::CONFIRMATION_STATE);
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

TEST_F(ManagePasswordsUIControllerTest, AutofillDuringSignInPromo) {
  std::vector<const PasswordForm*> matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&matches);
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
  std::vector<const PasswordForm*> forms;
  forms.push_back(&test_local_form());
  controller()->OnPasswordAutofilled(forms, forms.front()->origin, nullptr);

  // Once the bubble is closed the controller is reacting again.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnBubbleHidden();
}

TEST_F(ManagePasswordsUIControllerTest, AuthenticateUserToRevealPasswords) {
  std::vector<const PasswordForm*> matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&matches);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  controller()->OnPasswordSubmitted(std::move(test_form_manager));
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            controller()->GetState());
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ASSERT_TRUE(testing::Mock::VerifyAndClearExpectations(controller()));

  // Simulate that re-auth is need to reveal passwords in the bubble.
  bool success = controller()->AuthenticateUser();
#if defined(OS_WIN) || defined(OS_MACOSX)
  EXPECT_FALSE(success);
  // Let the task posted in AuthenticateUser re-open the bubble.
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  content::RunAllPendingInMessageLoop();
  EXPECT_TRUE(controller()->opened_bubble());
  EXPECT_FALSE(controller()->opened_automatic_bubble());
  EXPECT_TRUE(controller()->are_passwords_revealed_in_opened_bubble());
  // Since the bubble is opened, this property is already cleared.
  EXPECT_FALSE(controller()->ArePasswordsRevealedWhenBubbleIsOpened());

  // Close the bubble.
  controller()->OnBubbleHidden();
#else
  EXPECT_TRUE(success);
#endif
}

TEST_F(ManagePasswordsUIControllerTest, UpdateBubbleAfterLeakCheck) {
  std::vector<const PasswordForm*> matches;
  auto test_form_manager = CreateFormManagerWithBestMatches(&matches);
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
                                       password_manager::IsReused(true),
                                       password_manager::IsSyncing(false)),
      GURL(kExampleUrl));
  // The bubble is gone.
  EXPECT_FALSE(controller()->opened_bubble());

  // Close the dialog.
  EXPECT_CALL(dialog_prompt, ControllerGone);
  EXPECT_CALL(*controller(), OnUpdateBubbleAndIconVisibility());
  dialog_controller->OnAcceptDialog();

  // The update bubble is back.
  EXPECT_TRUE(controller()->opened_automatic_bubble());
  ExpectIconAndControllerStateIs(
      password_manager::ui::PENDING_PASSWORD_UPDATE_STATE);
}
