// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_save_update_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/editable_combobox/editable_password_combobox.h"

using ::testing::Return;
using ::testing::ReturnRef;

namespace {

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

class PasswordSaveUpdateViewTest : public PasswordBubbleViewTestBase {
 public:
  PasswordSaveUpdateViewTest();
  ~PasswordSaveUpdateViewTest() override = default;

  void CreateViewAndShow();
  void SimulateSignIn();

  void TearDown() override {
    std::exchange(view_, nullptr)
        ->GetWidget()
        ->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);

    PasswordBubbleViewTestBase::TearDown();
  }

  PasswordSaveUpdateView* view() { return view_; }

 protected:
  password_manager::PasswordForm pending_password_;

 private:
  raw_ptr<PasswordSaveUpdateView> view_ = nullptr;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> current_forms_;
};

PasswordSaveUpdateViewTest::PasswordSaveUpdateViewTest() {
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

  ProfilePasswordStoreFactory::GetInstance()->SetTestingFactoryAndUse(
      profile(),
      base::BindRepeating(&password_manager::BuildPasswordStoreInterface<
                          content::BrowserContext,
                          testing::NiceMock<
                              password_manager::MockPasswordStoreInterface>>));
  SyncServiceFactory::GetInstance()->SetTestingFactory(
      profile(), base::BindRepeating(&BuildTestSyncService));
}

void PasswordSaveUpdateViewTest::CreateViewAndShow() {
  CreateAnchorViewAndShow();

  view_ = new PasswordSaveUpdateView(web_contents(), anchor_view(),
                                     LocationBarBubbleDelegateView::AUTOMATIC);
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

void PasswordSaveUpdateViewTest::SimulateSignIn() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager, "test@email.com", signin::ConsentLevel::kSync);
}

TEST_F(PasswordSaveUpdateViewTest, HasTitleAndTwoButtons) {
  CreateViewAndShow();
  EXPECT_TRUE(view()->ShouldShowWindowTitle());
  EXPECT_TRUE(view()->GetOkButton());
  EXPECT_TRUE(view()->GetCancelButton());
}

TEST_F(PasswordSaveUpdateViewTest, ShouldSelectAccountStoreByDefault) {
  ON_CALL(*feature_manager_mock(), ShouldShowAccountStorageBubbleUi)
      .WillByDefault(Return(true));
  ON_CALL(*feature_manager_mock(), GetDefaultPasswordStore)
      .WillByDefault(
          Return(password_manager::PasswordForm::Store::kAccountStore));

  SimulateSignIn();
  CreateViewAndShow();
}

TEST_F(PasswordSaveUpdateViewTest, ShouldSelectProfileStoreByDefault) {
  ON_CALL(*feature_manager_mock(), ShouldShowAccountStorageBubbleUi)
      .WillByDefault(Return(true));
  ON_CALL(*feature_manager_mock(), GetDefaultPasswordStore)
      .WillByDefault(
          Return(password_manager::PasswordForm::Store::kProfileStore));

  SimulateSignIn();
  CreateViewAndShow();
}

// This is a regression test for crbug.com/1093290
TEST_F(PasswordSaveUpdateViewTest,
       OnThemesChangedShouldNotCrashForFederatedCredentials) {
  GURL kURL("https://example.com");
  url::Origin kOrigin = url::Origin::Create(kURL);
  ON_CALL(*model_delegate_mock(), GetOrigin).WillByDefault(Return(kOrigin));
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      kURL, web_contents()->GetPrimaryMainFrame());

  // Set the federation_origin to force a Federated Credentials bubble.
  pending_password_.federation_origin = url::SchemeHostPort(kURL);
  pending_password_.match_type =
      password_manager::PasswordForm::MatchType::kExact;
  CreateViewAndShow();
}

// This is a regression test for crbug.com/1475021
TEST_F(PasswordSaveUpdateViewTest, SaveButtonIsDisabledWhenPasswordIsEmpty) {
  CreateViewAndShow();
  const PasswordSaveUpdateView* save_bubble =
      static_cast<const PasswordSaveUpdateView*>(view());
  const views::DialogDelegate* dialog_delegate = view();

  save_bubble->password_dropdown_for_testing()->SetText(u"password");
  EXPECT_TRUE(
      dialog_delegate->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  save_bubble->password_dropdown_for_testing()->SetText(u"");
  EXPECT_FALSE(
      dialog_delegate->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));

  save_bubble->password_dropdown_for_testing()->SetText(u"pass");
  EXPECT_TRUE(
      dialog_delegate->IsDialogButtonEnabled(ui::mojom::DialogButton::kOk));
}
