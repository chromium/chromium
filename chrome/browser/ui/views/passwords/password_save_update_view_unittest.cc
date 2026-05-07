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
#include "chrome/browser/password_manager/factories/profile_password_store_factory.h"
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
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/editable_combobox/editable_password_combobox.h"
#include "ui/views/interaction/interaction_test_util_views.h"
#include "ui/views/view_tracker.h"
#include "ui/views/view_utils.h"

using ::testing::Return;
using ::testing::ReturnRef;

namespace {

std::unique_ptr<KeyedService> BuildTestSyncService(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

class PasswordSaveUpdateViewFixtureBase : public PasswordBubbleViewTestBase {
 public:
  PasswordSaveUpdateViewFixtureBase();
  ~PasswordSaveUpdateViewFixtureBase() override = default;

  void TearDown() override {
    if (view_) {
      std::exchange(view_, nullptr)
          ->GetWidget()
          ->CloseWithReason(views::Widget::ClosedReason::kCloseButtonClicked);
    }
    PasswordBubbleViewTestBase::TearDown();
  }

  void CreateViewAndShow();

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
  std::vector<std::unique_ptr<password_manager::PasswordForm>> current_forms_;
  raw_ptr<PasswordSaveUpdateView> view_ = nullptr;
};

enum class PasswordSaveUpdateViewTestVariant {
  kTwoButton,
  kThreeButton,
};

class PasswordSaveUpdateViewTest
    : public PasswordSaveUpdateViewFixtureBase,
      public testing::WithParamInterface<PasswordSaveUpdateViewTestVariant> {
 public:
  PasswordSaveUpdateViewTest();
  ~PasswordSaveUpdateViewTest() override = default;

  void SimulateSignIn();

  views::MdTextButton* GetNeverButton() {
    return IsThreeButton() ? view()->extra_view_for_testing()
                           : view()->GetCancelButton();
  }
  views::MdTextButton* GetNotNowButton() {
    return IsThreeButton() ? view()->GetCancelButton() : nullptr;
  }

  bool IsThreeButton() {
    return GetParam() == PasswordSaveUpdateViewTestVariant::kThreeButton;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

PasswordSaveUpdateViewFixtureBase::PasswordSaveUpdateViewFixtureBase() {
  ON_CALL(*feature_manager_mock(), IsAccountStorageActive)
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

void PasswordSaveUpdateViewFixtureBase::CreateViewAndShow() {
  CreateAnchorViewAndShow();

  view_ = new PasswordSaveUpdateView(web_contents(),
                                     views::BubbleAnchor(anchor_view()),
                                     LocationBarBubbleDelegateView::AUTOMATIC);
  views::BubbleDialogDelegateView::CreateBubble(view_)->Show();
}

PasswordSaveUpdateViewTest::PasswordSaveUpdateViewTest() {
  switch (GetParam()) {
    case PasswordSaveUpdateViewTestVariant::kTwoButton:
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{},
          /*disabled_features=*/{
              features::kThreeButtonPasswordSaveDialog,
              features::kPasswordSaveUpdateDropdownMenuExperiment});
      break;
    case PasswordSaveUpdateViewTestVariant::kThreeButton:
      scoped_feature_list_.InitWithFeatures(
          /*enabled_features=*/{features::kThreeButtonPasswordSaveDialog},
          /*disabled_features=*/{
              features::kPasswordSaveUpdateDropdownMenuExperiment});
      break;
  }
}

void PasswordSaveUpdateViewTest::SimulateSignIn() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile());
  AccountInfo info = signin::MakePrimaryAccountAvailable(
      identity_manager, "test@email.com", signin::ConsentLevel::kSignin);
}

TEST_P(PasswordSaveUpdateViewTest, HasTitleAndButtons) {
  CreateViewAndShow();
  EXPECT_TRUE(view()->ShouldShowWindowTitle());
  ASSERT_TRUE(view()->GetOkButton());
  EXPECT_EQ(view()->GetOkButton()->GetText(), SaveButtonCaption());
  ASSERT_TRUE(GetNeverButton());
  EXPECT_EQ(GetNeverButton()->GetText(), NeverButtonCaption());
  if (IsThreeButton()) {
    ASSERT_TRUE(GetNotNowButton());
    EXPECT_EQ(GetNotNowButton()->GetText(), NotNowButtonCaption());
  } else {
    EXPECT_FALSE(view()->extra_view_for_testing());
  }
}

TEST_P(PasswordSaveUpdateViewTest, NeverButtonClicked) {
  CreateViewAndShow();
  EXPECT_CALL(*model_delegate_mock(), NeverSavePassword);
  views::test::InteractionTestUtilSimulatorViews::PressButton(GetNeverButton());
}

TEST_P(PasswordSaveUpdateViewTest, NotNowButtonClicked) {
  CreateViewAndShow();
  if (!IsThreeButton()) {
    GTEST_SKIP() << "NotNow button is only available in the 3-button layout";
  }
  EXPECT_CALL(*model_delegate_mock(), OnNotNowClicked);
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      GetNotNowButton());
}

TEST_P(PasswordSaveUpdateViewTest, ShouldSelectAccountStoreByDefault) {
  ON_CALL(*feature_manager_mock(), IsAccountStorageActive)
      .WillByDefault(Return(true));

  SimulateSignIn();
  CreateViewAndShow();
}

TEST_P(PasswordSaveUpdateViewTest, ShouldSelectProfileStoreByDefault) {
  ON_CALL(*feature_manager_mock(), IsAccountStorageActive)
      .WillByDefault(Return(false));

  SimulateSignIn();
  CreateViewAndShow();
}

// This is a regression test for crbug.com/40698799
TEST_P(PasswordSaveUpdateViewTest,
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

// This is a regression test for crbug.com/40927721
TEST_P(PasswordSaveUpdateViewTest, SaveButtonIsDisabledWhenPasswordIsEmpty) {
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

class PasswordDropdownExperimentTest
    : public PasswordSaveUpdateViewFixtureBase {
 public:
  PasswordDropdownExperimentTest();
  ~PasswordDropdownExperimentTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

PasswordDropdownExperimentTest::PasswordDropdownExperimentTest() {
  scoped_feature_list_.InitAndEnableFeature(
      features::kPasswordSaveUpdateDropdownMenuExperiment);
}

TEST_F(PasswordDropdownExperimentTest,
       HasNoSubtitleForSameDomainAndNoHostInTitle) {
  GURL kURL("http://example.com");
  pending_password_.url = kURL;

  content::NavigationSimulator::NavigateAndCommitFromDocument(
      kURL, web_contents()->GetPrimaryMainFrame());

  EXPECT_CALL(*model_delegate_mock(), GetOrigin())
      .WillRepeatedly(Return(url::Origin::Create(kURL)));

  CreateViewAndShow();
  EXPECT_EQ(view()->GetSubtitle(), std::u16string());
  EXPECT_EQ(view()->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD));
}

TEST_F(PasswordDropdownExperimentTest,
       HasSubtitleForDifferentDomainAndNoHostInTitle) {
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("http://different.com"), web_contents()->GetPrimaryMainFrame());

  pending_password_.url = GURL("http://example.com");

  EXPECT_CALL(*model_delegate_mock(), GetOrigin())
      .WillRepeatedly(Return(url::Origin::Create(GURL("http://example.com"))));

  CreateViewAndShow();
  EXPECT_EQ(view()->GetSubtitle(), u"example.com");
  EXPECT_EQ(view()->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_SAVE_PASSWORD));
}

TEST_F(PasswordDropdownExperimentTest, DropdownMenuExperimentButtons) {
  CreateViewAndShow();

  EXPECT_TRUE(view()->GetOkButtonForTesting());
  EXPECT_FALSE(view()->GetCancelButtonForTesting());

  views::View* split_button =
      view()->GetViewByID(PasswordSaveUpdateView::kSplitButton);
  EXPECT_TRUE(split_button);
}

TEST_F(PasswordDropdownExperimentTest, DropdownMenuExperimentUpdateBubble) {
  EXPECT_CALL(*model_delegate_mock(), GetState)
      .WillRepeatedly(
          Return(password_manager::ui::PENDING_PASSWORD_UPDATE_STATE));

  CreateViewAndShow();

  EXPECT_TRUE(view()->GetOkButtonForTesting());
  EXPECT_TRUE(view()->GetCancelButtonForTesting());

  EXPECT_FALSE(view()->GetViewByID(PasswordSaveUpdateView::kSplitButton));
}

TEST_F(PasswordDropdownExperimentTest, DropdownMenuExperimentMorphing) {
  // Add an existing credential.
  password_manager::PasswordForm existing_form;
  existing_form.username_value = u"existing_user";
  existing_form.password_value = u"password";
  current_forms_.push_back(
      std::make_unique<password_manager::PasswordForm>(existing_form));

  // Set pending password to a NEW user.
  pending_password_.username_value = u"new_user";
  pending_password_.password_value = u"password";

  // Start in Save state.
  ON_CALL(*model_delegate_mock(), GetState)
      .WillByDefault(Return(password_manager::ui::PENDING_PASSWORD_STATE));

  CreateViewAndShow();

  // Verify custom buttons are used and visible.
  views::View* split_button =
      view()->GetViewByID(PasswordSaveUpdateView::kSplitButton);
  views::View* dismiss_update_button =
      view()->GetViewByID(PasswordSaveUpdateView::kDismissUpdateButton);
  ASSERT_TRUE(split_button);
  EXPECT_TRUE(split_button->GetVisible());
  ASSERT_TRUE(dismiss_update_button);
  EXPECT_FALSE(dismiss_update_button->GetVisible());

  views::View* ok_button = view()->GetOkButtonForTesting();
  views::View* cancel_button = view()->GetCancelButtonForTesting();

  ASSERT_TRUE(ok_button);
  EXPECT_FALSE(cancel_button && cancel_button->GetVisible());

  // In Save state:
  EXPECT_TRUE(split_button->GetVisible());
  EXPECT_EQ(views::AsViewClass<views::MdTextButton>(ok_button)->GetText(),
            SaveButtonCaption());

  // Now simulate state change to Update by changing username to
  // "existing_user".
  view()->username_dropdown_for_testing()->SetText(u"existing_user");
  view()->TriggerOnContentChangedForTesting();

  // After morphing to Update:
  EXPECT_TRUE(dismiss_update_button->GetVisible());
  EXPECT_FALSE(split_button->GetVisible());
  EXPECT_TRUE(
      view()->GetViewByID(PasswordSaveUpdateView::kDismissUpdateButton) &&
      view()
          ->GetViewByID(PasswordSaveUpdateView::kDismissUpdateButton)
          ->GetVisible());
  EXPECT_EQ(
      views::AsViewClass<views::MdTextButton>(ok_button)->GetText(),
      l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SHORT_UPDATE_BUTTON));
}

TEST_F(PasswordDropdownExperimentTest,
       DropdownMenuExperimentInteraction_NotNow) {
  CreateViewAndShow();

  views::View* split_button =
      view()->GetViewByID(PasswordSaveUpdateView::kSplitButton);
  ASSERT_TRUE(split_button);
  views::View* not_now_button =
      split_button->GetViewByID(PasswordSaveUpdateView::kNotNowButton);
  ASSERT_TRUE(not_now_button);

  EXPECT_CALL(*model_delegate_mock(), OnNotNowClicked);
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      views::AsViewClass<views::Button>(not_now_button));
}

TEST_F(PasswordDropdownExperimentTest,
       DropdownMenuExperimentInteraction_SaveAndReplaceWithPromo) {
  pending_password_.password_value = u"password";

  CreateViewAndShow();

  views::View* split_button =
      view()->GetViewByID(PasswordSaveUpdateView::kSplitButton);
  ASSERT_TRUE(split_button);
  EXPECT_TRUE(split_button->GetVisible());

  EXPECT_CALL(*model_delegate_mock(), SavePassword);

  views::ViewTracker tracker(view());
  view()->AcceptDialog();

  // Verify that the extra view container is now hidden or deleted.
  if (tracker.view()) {
    views::View* container =
        view()->GetViewByID(PasswordSaveUpdateView::kCustomButtonRow);
    EXPECT_TRUE(!container || !container->GetVisible());
  }
}

TEST_F(PasswordDropdownExperimentTest, DropdownMenuExperimentInteraction_Save) {
  pending_password_.password_value = u"password";

  CreateViewAndShow();

  EXPECT_CALL(*model_delegate_mock(), SavePassword);
  view()->AcceptDialog();
}

TEST_F(PasswordDropdownExperimentTest,
       DropdownMenuExperimentInteraction_Never) {
  pending_password_.password_value = u"password";
  CreateViewAndShow();

  views::View* split_button =
      view()->GetViewByID(PasswordSaveUpdateView::kSplitButton);
  ASSERT_TRUE(split_button);
  views::View* ok_button = view()->GetOkButtonForTesting();
  ASSERT_TRUE(ok_button);
  ASSERT_FALSE(split_button->children().empty());

  views::View* caret_button =
      split_button->GetViewByID(PasswordSaveUpdateView::kCaretButton);
  ASSERT_TRUE(caret_button);

  views::test::InteractionTestUtilSimulatorViews::PressButton(
      views::AsViewClass<views::Button>(caret_button));

  ASSERT_TRUE(view()->MenuModelForTesting());
  EXPECT_CALL(*model_delegate_mock(), NeverSavePassword);
  view()->MenuModelForTesting()->ActivatedAt(0);
}

TEST_F(PasswordDropdownExperimentTest,
       DropdownMenuExperimentInteraction_CancelUpdatePassword) {
  // Add an existing credential.
  password_manager::PasswordForm existing_form;
  existing_form.username_value = u"existing_user";
  existing_form.password_value = u"password";
  current_forms_.push_back(
      std::make_unique<password_manager::PasswordForm>(existing_form));

  // Start in Save state.
  ON_CALL(*model_delegate_mock(), GetState)
      .WillByDefault(Return(password_manager::ui::PENDING_PASSWORD_STATE));

  CreateViewAndShow();

  // Morph to Update.
  view()->username_dropdown_for_testing()->SetText(u"existing_user");
  view()->TriggerOnContentChangedForTesting();

  views::View* cancel_button =
      view()->GetViewByID(PasswordSaveUpdateView::kDismissUpdateButton);
  ASSERT_TRUE(cancel_button);
  EXPECT_TRUE(cancel_button->GetVisible());

  EXPECT_CALL(*model_delegate_mock(), OnNotNowClicked);
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      views::AsViewClass<views::Button>(cancel_button));
}

TEST_F(PasswordDropdownExperimentTest, OkButtonDisabledWhenPasswordIsEmpty) {
  CreateViewAndShow();

  views::View* ok_button = view()->GetOkButtonForTesting();
  ASSERT_TRUE(ok_button);

  view()->password_dropdown_for_testing()->SetText(u"");
  view()->TriggerOnContentChangedForTesting();
  EXPECT_FALSE(ok_button->GetEnabled());
}

TEST_F(PasswordDropdownExperimentTest, OkButtonEnabledWhenPasswordIsNotEmpty) {
  CreateViewAndShow();

  views::View* ok_button = view()->GetOkButtonForTesting();
  ASSERT_TRUE(ok_button);

  view()->password_dropdown_for_testing()->SetText(u"password");
  view()->TriggerOnContentChangedForTesting();
  EXPECT_TRUE(ok_button->GetEnabled());
}

TEST_F(PasswordDropdownExperimentTest, DropdownMenuExperimentNonBlockingMenu) {
  pending_password_.password_value = u"password";
  CreateViewAndShow();

  views::View* split_button =
      view()->GetViewByID(PasswordSaveUpdateView::kSplitButton);
  ASSERT_TRUE(split_button);
  views::View* ok_button = view()->GetOkButtonForTesting();
  ASSERT_TRUE(ok_button);
  ASSERT_FALSE(split_button->children().empty());

  views::View* caret_button =
      split_button->GetViewByID(PasswordSaveUpdateView::kCaretButton);
  ASSERT_TRUE(caret_button);

  // 1. Open the menu by pressing the caret button.
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      views::AsViewClass<views::Button>(caret_button));

  // 2. Attempt to click the "Save" button while the menu is open.
  // If the menu is non-blocking, this should succeed and notify the delegate.
  EXPECT_CALL(*model_delegate_mock(), SavePassword);
  views::test::InteractionTestUtilSimulatorViews::PressButton(
      views::AsViewClass<views::Button>(ok_button));
}

TEST_F(PasswordDropdownExperimentTest, CustomButtonRowLayoutDetails) {
  CreateViewAndShow();

  views::View* button_row =
      view()->GetViewByID(PasswordSaveUpdateView::kCustomButtonRow);
  ASSERT_TRUE(button_row);

  // Check that it uses BoxLayout with horizontal orientation and kEnd
  // alignment.
  views::BoxLayoutView* box_layout_view =
      views::AsViewClass<views::BoxLayoutView>(button_row);
  ASSERT_TRUE(box_layout_view);

  EXPECT_EQ(box_layout_view->GetOrientation(),
            views::BoxLayout::Orientation::kHorizontal);
  EXPECT_EQ(box_layout_view->GetMainAxisAlignment(),
            views::BoxLayout::MainAxisAlignment::kEnd);

  // Verify the buttons exist as children of the row.
  views::View* ok_button = view()->GetOkButtonForTesting();
  views::View* split_button =
      view()->GetViewByID(PasswordSaveUpdateView::kSplitButton);
  ASSERT_TRUE(ok_button);
  ASSERT_TRUE(split_button);

  EXPECT_EQ(ok_button->parent(), button_row);
  EXPECT_EQ(split_button->parent(), button_row);
}

TEST_F(PasswordDropdownExperimentTest,
       FederatedCredentialsBubbleShouldNotCrash) {
  GURL kURL("https://example.com");
  url::Origin kOrigin = url::Origin::Create(kURL);
  EXPECT_CALL(*model_delegate_mock(), GetOrigin)
      .WillRepeatedly(Return(kOrigin));
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      kURL, web_contents()->GetPrimaryMainFrame());

  // Set the federation_origin to force a Federated Credentials bubble.
  pending_password_.federation_origin = url::SchemeHostPort(kURL);
  pending_password_.match_type =
      password_manager::PasswordForm::MatchType::kExact;

  CreateViewAndShow();
}

// These tests are parameterized to run in the original two-button layout,
// and the newer three-button variant. Three-button is now the default, but
// the original version still runs for a small percentage of users, so maintain
// test coverage on both variants.
INSTANTIATE_TEST_SUITE_P(
    All,
    PasswordSaveUpdateViewTest,
    testing::Values(PasswordSaveUpdateViewTestVariant::kTwoButton,
                    PasswordSaveUpdateViewTestVariant::kThreeButton),
    [](const testing::TestParamInfo<PasswordSaveUpdateViewTestVariant>& info) {
      switch (info.param) {
        case PasswordSaveUpdateViewTestVariant::kTwoButton:
          return "TwoButton";
        case PasswordSaveUpdateViewTestVariant::kThreeButton:
          return "ThreeButton";
      }
    });
