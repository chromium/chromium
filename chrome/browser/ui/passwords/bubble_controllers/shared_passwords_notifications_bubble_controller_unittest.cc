// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/shared_passwords_notifications_bubble_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/password_manager/password_manager_test_util.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_store/test_password_store.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Bucket;
using password_manager::PasswordForm;
using password_manager::metrics_util::
    SharedPasswordsNotificationBubbleInteractions;
using testing::Each;
using testing::Field;
using testing::Return;
using testing::ReturnRef;

namespace {

constexpr char kUrl[] = "http://example.com";

std::unique_ptr<PasswordForm> CreateUnnoitifiedSharedPasswordForm(
    const std::u16string& username) {
  auto shared_credentials = std::make_unique<PasswordForm>();
  shared_credentials->url = GURL(kUrl);
  shared_credentials->signon_realm = shared_credentials->url.spec();
  shared_credentials->username_value = username;
  shared_credentials->password_value = u"12345";
  shared_credentials->match_type = PasswordForm::MatchType::kExact;
  shared_credentials->type = PasswordForm::Type::kReceivedViaSharing;
  shared_credentials->sharing_notification_displayed = false;
  shared_credentials->sender_name = u"Sender Name";
  return shared_credentials;
}

}  // namespace

class SharedPasswordsNotificationBubbleControllerTest : public ::testing::Test {
 public:
  SharedPasswordsNotificationBubbleControllerTest() {
    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    mock_delegate_ =
        std::make_unique<testing::NiceMock<PasswordsModelDelegateMock>>();
    controller_ = std::make_unique<SharedPasswordsNotificationBubbleController>(
        mock_delegate_->AsWeakPtr());
  }
  ~SharedPasswordsNotificationBubbleControllerTest() override = default;

  void SetUp() override {
    store_ = CreateAndUseTestPasswordStore(&profile_);
    ON_CALL(*delegate(), GetCurrentForms)
        .WillByDefault(ReturnRef(current_forms_));
    ON_CALL(*delegate(), GetWebContents())
        .WillByDefault(Return(test_web_contents_.get()));
  }

  void TearDown() override {
    store_->ShutdownOnUIThread();
    testing::Test::TearDown();
  }

  void SetupSharedCredentialsInStore() {
    // Store two shared credentials that still need notifications.
    std::unique_ptr<password_manager::PasswordForm> shared_credentials1 =
        CreateUnnoitifiedSharedPasswordForm(u"username1");
    std::unique_ptr<password_manager::PasswordForm> shared_credentials2 =
        CreateUnnoitifiedSharedPasswordForm(u"username2");

    store_->AddLogin(*shared_credentials1);
    store_->AddLogin(*shared_credentials2);

    RunUntilIdle();

    current_forms().push_back(std::move(shared_credentials1));
    current_forms().push_back(std::move(shared_credentials2));
  }

  void RunUntilIdle() { task_environment_.RunUntilIdle(); }

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  SharedPasswordsNotificationBubbleController* controller() {
    return controller_.get();
  }
  password_manager::TestPasswordStore& store() { return *store_; }
  std::vector<std::unique_ptr<password_manager::PasswordForm>>&
  current_forms() {
    return current_forms_;
  }
  base::HistogramTester& histogram_tester() { return histogram_tester_; }

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> current_forms_;
  std::unique_ptr<PasswordsModelDelegateMock> mock_delegate_;
  std::unique_ptr<SharedPasswordsNotificationBubbleController> controller_;
  TestingProfile profile_;
  std::unique_ptr<content::WebContents> test_web_contents_;
  scoped_refptr<password_manager::TestPasswordStore> store_;
  base::HistogramTester histogram_tester_;
};

TEST_F(SharedPasswordsNotificationBubbleControllerTest, HasTitle) {
  EXPECT_FALSE(controller()->GetTitle().empty());
  histogram_tester().ExpectUniqueSample(
      "PasswordManager.SharedPasswordsNotificationBubble.UserAction",
      SharedPasswordsNotificationBubbleInteractions::kNotificationDisplayed, 1);
}

TEST_F(SharedPasswordsNotificationBubbleControllerTest,
       ShouldMarkCredentialsAsNotifiedUponClickingManagePassword) {
  SetupSharedCredentialsInStore();

  controller()->OnManagePasswordsClicked();

  RunUntilIdle();

  EXPECT_THAT(store().stored_passwords().at(GURL(kUrl).spec()),
              Each(Field(&PasswordForm::sharing_notification_displayed, true)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "PasswordManager.SharedPasswordsNotificationBubble.UserAction"),
      BucketsAre(Bucket(SharedPasswordsNotificationBubbleInteractions::
                            kNotificationDisplayed,
                        1),
                 Bucket(SharedPasswordsNotificationBubbleInteractions::
                            kManagePasswordsButtonClicked,
                        1)));
}

TEST_F(SharedPasswordsNotificationBubbleControllerTest,
       ShouldMarkCredentialsAsNotifiedUponClickingGotIt) {
  SetupSharedCredentialsInStore();

  controller()->OnAcknowledgeClicked();

  RunUntilIdle();

  EXPECT_THAT(store().stored_passwords().at(GURL(kUrl).spec()),
              Each(Field(&PasswordForm::sharing_notification_displayed, true)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "PasswordManager.SharedPasswordsNotificationBubble.UserAction"),
      BucketsAre(Bucket(SharedPasswordsNotificationBubbleInteractions::
                            kNotificationDisplayed,
                        1),
                 Bucket(SharedPasswordsNotificationBubbleInteractions::
                            kGotItButtonClicked,
                        1)));
}

TEST_F(SharedPasswordsNotificationBubbleControllerTest,
       ShouldMarkCredentialsAsNotifiedUponClickingCloseBubble) {
  SetupSharedCredentialsInStore();

  controller()->OnCloseBubbleClicked();

  RunUntilIdle();

  EXPECT_THAT(store().stored_passwords().at(GURL(kUrl).spec()),
              Each(Field(&PasswordForm::sharing_notification_displayed, true)));
  EXPECT_THAT(
      histogram_tester().GetAllSamples(
          "PasswordManager.SharedPasswordsNotificationBubble.UserAction"),
      BucketsAre(Bucket(SharedPasswordsNotificationBubbleInteractions::
                            kNotificationDisplayed,
                        1),
                 Bucket(SharedPasswordsNotificationBubbleInteractions::
                            kCloseButtonClicked,
                        1)));
}

TEST_F(SharedPasswordsNotificationBubbleControllerTest,
       ShouldComputeNotificationBodyTextForSingleSharedCredential) {
  current_forms().push_back(CreateUnnoitifiedSharedPasswordForm(u"username1"));

  EXPECT_NE(std::u16string(), controller()->GetNotificationBody());
  // For single shared credential, sender name is displayed in the notification.
  EXPECT_NE(gfx::Range(), controller()->GetSenderNameRange());
}

TEST_F(SharedPasswordsNotificationBubbleControllerTest,
       ShouldComputeNotificationBodyTextForMultipleSharedCredential) {
  current_forms().push_back(CreateUnnoitifiedSharedPasswordForm(u"username1"));
  current_forms().push_back(CreateUnnoitifiedSharedPasswordForm(u"username2"));

  EXPECT_NE(std::u16string(), controller()->GetNotificationBody());
  // For multiple shared credentials, sender name is *not* displayed in the
  // notification.
  EXPECT_EQ(gfx::Range(), controller()->GetSenderNameRange());
}
