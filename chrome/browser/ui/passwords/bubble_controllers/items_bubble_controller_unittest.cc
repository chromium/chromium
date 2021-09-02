// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/bubble_controllers/items_bubble_controller.h"

#include <memory>

#include "base/strings/utf_string_conversions.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/ui/passwords/passwords_model_delegate_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

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
  }

  ~ItemsBubbleControllerTest() override = default;

  static std::vector<std::unique_ptr<password_manager::PasswordForm>>
  GetCurrentForms();

  PasswordsModelDelegateMock* delegate() { return mock_delegate_.get(); }
  ItemsBubbleController* controller() { return controller_.get(); }
  TestingProfile* profile() { return &profile_; }

  password_manager::MockPasswordStore* GetStore() {
    return static_cast<password_manager::MockPasswordStore*>(
        PasswordStoreFactory::GetInstance()
            ->GetForProfile(profile(), ServiceAccessType::EXPLICIT_ACCESS)
            .get());
  }

  void Init();
  void DestroyController();

 private:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_enabler_;
  TestingProfile profile_;
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

  EXPECT_CALL(*GetStore(), AddLogin(form));

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
