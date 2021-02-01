// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_save_update_with_account_store_view.h"

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/sync/driver/test_sync_service.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/combobox/combobox.h"

using ::testing::Return;
using ::testing::ReturnRef;

namespace {

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

class PasswordSaveUpdateWithAccountStoreViewTest
    : public PasswordBubbleViewTestBase {
 public:
  PasswordSaveUpdateWithAccountStoreViewTest();
  ~PasswordSaveUpdateWithAccountStoreViewTest() override = default;

  void CreateViewAndShow();
  void SimulateSignIn();

  void TearDown() override {
    view_->GetWidget()->CloseWithReason(
        views::Widget::ClosedReason::kCloseButtonClicked);

    PasswordBubbleViewTestBase::TearDown();
  }

  PasswordSaveUpdateWithAccountStoreView* view() { return view_; }
  views::Combobox* account_picker() {
    return view_->DestinationDropdownForTesting();
  }

 protected:
  password_manager::PasswordForm pending_password_;

 private:
  base::test::ScopedFeatureList feature_list_;
  PasswordSaveUpdateWithAccountStoreView* view_;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> current_forms_;
};

PasswordSaveUpdateWithAccountStoreViewTest::
    PasswordSaveUpdateWithAccountStoreViewTest() {
  // If kEnablePasswordsAccountStorage is disabled, then
  // PasswordSaveUpdateView is used instead of
  // PasswordSaveUpdateWithAccountStoreView.
  feature_list_.InitAndEnableFeature(
      password_manager::features::kEnablePasswordsAccountStorage);

  ON_CALL(*feature_manager_mock(), GetDefaultPasswordStore)
      .WillByDefault(
          Return(password_manager::PasswordForm::Store::kAccountStore));
  ON_CALL(*model_delegate_mock(), GetOrigin)
      .WillByDefault(Return(url::Origin::Create(pending_password_.url)));
  ON_CALL(*model_delegate_mock(), GetState)
      .WillByDefault(Return(password_manager::ui::PENDING_PASSWORD_STATE));
  ON_CALL(*model_delegate_mock(), GetPendingPassword)
      .WillByDefault(ReturnRef(pending_password_));
  ON_CALL(*model_delegate_mock(), GetCurrentForms)
      .WillByDefault(ReturnRef(current_forms_));

  PasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(
          &password_manager::BuildPasswordStore<
              content::BrowserContext,
              testing::NiceMock<password_manager::MockPasswordStore>>));
  ProfileSyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildTestSyncService));
}

void PasswordSaveUpdateWithAccountStoreViewTest::CreateViewAndShow() {
  CreateAnchorViewAndShow();

  view_ = new PasswordSaveUpdateWithAccountStoreView(
      web_contents(), anchor_view(), LocationBarBubbleDelegateView::AUTOMATIC,
      /*promo_controller=*/nullptr);
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

void PasswordSaveUpdateWithAccountStoreViewTest::SimulateSignIn() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo info =
      signin::MakePrimaryAccountAvailable(identity_manager, "test@email.com");
}

TEST_F(PasswordSaveUpdateWithAccountStoreViewTest, HasTitleAndTwoButtons) {
  CreateViewAndShow();
  EXPECT_TRUE(view()->ShouldShowWindowTitle());
  EXPECT_TRUE(view()->GetOkButton());
  EXPECT_TRUE(view()->GetCancelButton());
}

TEST_F(PasswordSaveUpdateWithAccountStoreViewTest, ShouldNotShowAccountPicker) {
  ON_CALL(*feature_manager_mock(), ShouldShowAccountStorageBubbleUi)
      .WillByDefault(Return(false));
  CreateViewAndShow();
  EXPECT_FALSE(account_picker());
}

TEST_F(PasswordSaveUpdateWithAccountStoreViewTest, ShouldShowAccountPicker) {
  ON_CALL(*feature_manager_mock(), ShouldShowAccountStorageBubbleUi)
      .WillByDefault(Return(true));
  SimulateSignIn();
  CreateViewAndShow();
  ASSERT_TRUE(account_picker());
  EXPECT_EQ(0, account_picker()->GetSelectedIndex());
}

TEST_F(PasswordSaveUpdateWithAccountStoreViewTest,
       ShouldSelectAccountStoreByDefault) {
  ON_CALL(*feature_manager_mock(), ShouldShowAccountStorageBubbleUi)
      .WillByDefault(Return(true));
  ON_CALL(*feature_manager_mock(), GetDefaultPasswordStore)
      .WillByDefault(
          Return(password_manager::PasswordForm::Store::kAccountStore));

  SimulateSignIn();

  CreateViewAndShow();

  ASSERT_TRUE(account_picker());
  EXPECT_EQ(0, account_picker()->GetSelectedIndex());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_DESTINATION_DROPDOWN_SAVE_TO_ACCOUNT),
      account_picker()->GetTextForRow(account_picker()->GetSelectedIndex()));
}

TEST_F(PasswordSaveUpdateWithAccountStoreViewTest,
       ShouldSelectProfileStoreByDefault) {
  ON_CALL(*feature_manager_mock(), ShouldShowAccountStorageBubbleUi)
      .WillByDefault(Return(true));
  ON_CALL(*feature_manager_mock(), GetDefaultPasswordStore)
      .WillByDefault(
          Return(password_manager::PasswordForm::Store::kProfileStore));
  SimulateSignIn();
  CreateViewAndShow();
  ASSERT_TRUE(account_picker());
  EXPECT_EQ(1, account_picker()->GetSelectedIndex());
  EXPECT_EQ(
      l10n_util::GetStringUTF16(
          IDS_PASSWORD_MANAGER_DESTINATION_DROPDOWN_SAVE_TO_DEVICE),
      account_picker()->GetTextForRow(account_picker()->GetSelectedIndex()));
}

// This is a regression test for crbug.com/1093290
TEST_F(PasswordSaveUpdateWithAccountStoreViewTest,
       OnThemesChangedShouldNotCrashForFederatedCredentials) {
  GURL kURL("https://example.com");
  url::Origin kOrigin = url::Origin::Create(kURL);
  ON_CALL(*model_delegate_mock(), GetOrigin).WillByDefault(Return(kOrigin));
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      kURL, web_contents()->GetMainFrame());

  // Set the federation_origin to force a Federated Credentials bubble.
  pending_password_.federation_origin = kOrigin;

  CreateViewAndShow();
}
