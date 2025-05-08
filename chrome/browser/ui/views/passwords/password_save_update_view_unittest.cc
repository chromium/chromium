// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/passwords/password_save_update_view.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/passwords/password_bubble_view_test_base.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/mock_password_feature_manager.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/browser/password_store/mock_password_store_interface.h"
#include "components/sync/test/test_sync_service.h"
#include "content/public/test/navigation_simulator.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/mojom/dialog_button.mojom.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/editable_combobox/editable_password_combobox.h"
#include "ui/views/interaction/interaction_test_util_views.h"

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
  static std::u16string SaveButtonCaption() {
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON);
  }
  static std::u16string NeverButtonCaption() {
    return l10n_util::GetStringUTF16(
        IDS_PASSWORD_MANAGER_BUBBLE_BLOCKLIST_BUTTON);
  }
  static std::u16string NotNowButtonCaption() {
    return l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_CANCEL_BUTTON);
  }

  password_manager::PasswordForm pending_password_;

 private:
  raw_ptr<PasswordSaveUpdateView> view_ = nullptr;
  std::vector<std::unique_ptr<password_manager::PasswordForm>> current_forms_;
};

PasswordSaveUpdateViewTest::PasswordSaveUpdateViewTest() {
  ON_CALL(*feature_manager_mock(), IsAccountStorageEnabled)
      .WillByDefault(Return(true));
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
  ASSERT_TRUE(view()->GetOkButton());
  EXPECT_EQ(view()->GetOkButton()->GetText(), SaveButtonCaption());
  ASSERT_TRUE(view()->GetCancelButton());
  EXPECT_EQ(view()->GetCancelButton()->GetText(), NeverButtonCaption());
  EXPECT_FALSE(view()->extra_view_for_testing());
}

TEST_F(PasswordSaveUpdateViewTest, NeverButtonClicked) {
  CreateViewAndShow();
  EXPECT_CALL(*model_delegate_mock(), NeverSavePassword);
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      view()->GetCancelButton());
}

TEST_F(PasswordSaveUpdateViewTest, ShouldSelectAccountStoreByDefault) {
  ON_CALL(*feature_manager_mock(), IsAccountStorageEnabled)
      .WillByDefault(Return(true));

  SimulateSignIn();
  CreateViewAndShow();
}

TEST_F(PasswordSaveUpdateViewTest, ShouldSelectProfileStoreByDefault) {
  ON_CALL(*feature_manager_mock(), IsAccountStorageEnabled)
      .WillByDefault(Return(false));

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

class PasswordSaveThreeButtonDialogViewTest
    : public PasswordSaveUpdateViewTest {
 public:
  PasswordSaveThreeButtonDialogViewTest();
  ~PasswordSaveThreeButtonDialogViewTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

PasswordSaveThreeButtonDialogViewTest::PasswordSaveThreeButtonDialogViewTest() {
  // Enable an experiment to have the Cancel button simply dismiss the dialog,
  // and have a third button to explicitly never save the password.
  scoped_feature_list_.InitWithFeatureState(
      features::kThreeButtonPasswordSaveDialog, true);
}

TEST_F(PasswordSaveThreeButtonDialogViewTest, ThreeButtonLayout) {
  CreateViewAndShow();
  ASSERT_TRUE(view()->GetOkButton());
  EXPECT_EQ(view()->GetOkButton()->GetText(), SaveButtonCaption());
  ASSERT_TRUE(view()->GetCancelButton());
  EXPECT_EQ(view()->GetCancelButton()->GetText(), NotNowButtonCaption());
  ASSERT_TRUE(view()->extra_view_for_testing());
  EXPECT_EQ(view()->extra_view_for_testing()->GetText(), NeverButtonCaption());
}

TEST_F(PasswordSaveThreeButtonDialogViewTest, ThreeButtonLayoutNotNowClicked) {
  CreateViewAndShow();
  EXPECT_CALL(*model_delegate_mock(), OnNotNowClicked);
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      view()->GetCancelButton());
}

TEST_F(PasswordSaveThreeButtonDialogViewTest, ThreeButtonLayoutNeverClicked) {
  CreateViewAndShow();
  EXPECT_CALL(*model_delegate_mock(), NeverSavePassword);
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      view()->extra_view_for_testing());
}
