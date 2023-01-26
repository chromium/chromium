// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"

#include <memory>
#include <string>

#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/mock_input_event_activation_protector.h"
#include "url/gurl.h"

using LoginState = content::IdentityRequestAccount::LoginState;
using SignInMode = content::IdentityRequestAccount::SignInMode;

namespace {

constexpr char kRpEtldPlusOne[] = "rp-example.com";
constexpr char kIdpEtldPlusOne[] = "idp-example.com";

// Mock AccountSelectionBubbleViewInterface which tracks state.
class TestBubbleView : public AccountSelectionBubbleViewInterface {
 public:
  TestBubbleView() = default;
  ~TestBubbleView() override = default;

  TestBubbleView(const TestBubbleView&) = delete;
  TestBubbleView& operator=(const TestBubbleView&) = delete;

  void ShowAccountPicker(
      const std::vector<IdentityProviderDisplayData>& idp_data_list,
      bool show_back_button) override {
    show_back_button_ = show_back_button;
    show_verifying_sheet_ = false;

    account_ids_.clear();
    for (content::IdentityRequestAccount account : idp_data_list[0].accounts) {
      account_ids_.push_back(account.id);
    }
  }

  void ShowVerifyingSheet(const content::IdentityRequestAccount& account,
                          const IdentityProviderDisplayData& idp_data,
                          const std::u16string& title) override {
    show_verifying_sheet_ = true;
    account_ids_ = {account.id};
  }

  void ShowSingleAccountConfirmDialog(
      const std::u16string& rp_for_display,
      const content::IdentityRequestAccount& account,
      const IdentityProviderDisplayData& idp_data) override {
    show_back_button_ = true;
    show_verifying_sheet_ = false;
    account_ids_ = {account.id};
  }

  void ShowFailureDialog(const std::u16string& rp_for_display,
                         const std::u16string& idp_for_display) override {}

  bool show_back_button_{false};
  bool show_verifying_sheet_{false};
  std::vector<std::string> account_ids_;
};

// Test FedCmAccountSelectionView which uses TestBubbleView.
class TestFedCmAccountSelectionView : public FedCmAccountSelectionView {
 public:
  TestFedCmAccountSelectionView(Delegate* delegate,
                                views::Widget* widget,
                                TestBubbleView* bubble_view)
      : FedCmAccountSelectionView(delegate),
        widget_(widget),
        bubble_view_(bubble_view) {}

  TestFedCmAccountSelectionView(const TestFedCmAccountSelectionView&) = delete;
  TestFedCmAccountSelectionView& operator=(
      const TestFedCmAccountSelectionView&) = delete;

 protected:
  views::Widget* CreateBubble(Browser* browser,
                              const std::u16string& rp_etld_plus_one,
                              const absl::optional<std::u16string>& idp_title,
                              blink::mojom::RpContext rp_context) override {
    return widget_;
  }

  AccountSelectionBubbleViewInterface* GetBubbleView() override {
    return bubble_view_;
  }

 private:
  base::raw_ptr<views::Widget> widget_;
  base::raw_ptr<TestBubbleView> bubble_view_;
};

// Mock AccountSelectionView::Delegate.
class MockAccountSelectionViewDelegate : public AccountSelectionView::Delegate {
 public:
  explicit MockAccountSelectionViewDelegate(content::WebContents* web_contents)
      : web_contents_(web_contents) {}
  ~MockAccountSelectionViewDelegate() override = default;

  MockAccountSelectionViewDelegate(const MockAccountSelectionViewDelegate&) =
      delete;
  MockAccountSelectionViewDelegate& operator=(
      const MockAccountSelectionViewDelegate&) = delete;

  MOCK_METHOD2(OnAccountSelected,
               void(const GURL&, const content::IdentityRequestAccount&));
  MOCK_METHOD1(OnDismiss,
               void(content::IdentityRequestDialogController::DismissReason));
  MOCK_METHOD0(GetNativeView, gfx::NativeView());

  content::WebContents* GetWebContents() override { return web_contents_; }

 private:
  base::raw_ptr<content::WebContents> web_contents_;
};

}  // namespace

class FedCmAccountSelectionViewDesktopTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    delegate_ = std::make_unique<MockAccountSelectionViewDelegate>(
        test_web_contents_.get());

    widget_.reset(CreateTestWidget().release());
    bubble_view_ = std::make_unique<TestBubbleView>();
  }

  IdentityProviderDisplayData CreateIdentityProviderDisplayData(
      const std::vector<std::pair<std::string, LoginState>>& account_infos) {
    std::vector<content::IdentityRequestAccount> accounts;
    for (const auto& account_info : account_infos) {
      accounts.emplace_back(account_info.first, "", "", "", GURL::EmptyGURL(),
                            account_info.second);
    }
    return IdentityProviderDisplayData(u"", content::IdentityProviderMetadata(),
                                       content::ClientMetadata(GURL(), GURL()),
                                       std::move(accounts));
  }

  std::unique_ptr<TestFedCmAccountSelectionView> CreateAndShow(
      const std::vector<content::IdentityRequestAccount>& accounts,
      SignInMode mode) {
    auto controller = std::make_unique<TestFedCmAccountSelectionView>(
        delegate_.get(), widget_.get(), bubble_view_.get());

    controller->Show(
        kRpEtldPlusOne,
        {{kIdpEtldPlusOne, accounts, content::IdentityProviderMetadata(),
          content::ClientMetadata(GURL(), GURL()),
          blink::mojom::RpContext::kSignIn}},
        mode);

    // In tests, use a MockInputEventActivationProtector that will not block any
    // input. This can be overridden by a specific test.
    auto input_protector =
        std::make_unique<views::MockInputEventActivationProtector>();
    ON_CALL(*input_protector, IsPossiblyUnintendedInteraction)
        .WillByDefault(testing::Return(false));
    controller->SetInputEventActivationProtectorForTesting(
        std::move(input_protector));
    return controller;
  }

  ui::MouseEvent CreateMouseEvent() {
    return ui::MouseEvent(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                          base::TimeTicks(), ui::EF_LEFT_MOUSE_BUTTON, 0);
  }

 protected:
  TestingProfile profile_;

  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories_;

  std::unique_ptr<content::WebContents> test_web_contents_;
  views::ViewsTestBase::WidgetAutoclosePtr widget_;
  std::unique_ptr<TestBubbleView> bubble_view_;
  std::unique_ptr<AccountSelectionView::Delegate> delegate_;
};

TEST_F(FedCmAccountSelectionViewDesktopTest, SingleAccountFlow) {
  const char kAccountId[] = "account_id";
  IdentityProviderDisplayData idp_data =
      CreateIdentityProviderDisplayData({{kAccountId, LoginState::kSignUp}});
  const std::vector<Account>& accounts = idp_data.accounts;
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts, SignInMode::kExplicit);
  AccountSelectionBubbleView::Observer* observer =
      static_cast<AccountSelectionBubbleView::Observer*>(controller.get());

  EXPECT_FALSE(bubble_view_->show_back_button_);
  EXPECT_FALSE(bubble_view_->show_verifying_sheet_);
  EXPECT_THAT(bubble_view_->account_ids_, testing::ElementsAre(kAccountId));

  observer->OnAccountSelected(accounts[0], idp_data, /*auto_signin=*/false,
                              CreateMouseEvent());
  EXPECT_TRUE(bubble_view_->show_verifying_sheet_);
  EXPECT_THAT(bubble_view_->account_ids_, testing::ElementsAre(kAccountId));
}

TEST_F(FedCmAccountSelectionViewDesktopTest, MultipleAccountFlowReturning) {
  const char kAccountId1[] = "account_id1";
  const char kAccountId2[] = "account_id2";
  IdentityProviderDisplayData idp_data = CreateIdentityProviderDisplayData(
      {{kAccountId1, LoginState::kSignIn}, {kAccountId2, LoginState::kSignIn}});
  const std::vector<Account>& accounts = idp_data.accounts;
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts, SignInMode::kExplicit);
  AccountSelectionBubbleView::Observer* observer =
      static_cast<AccountSelectionBubbleView::Observer*>(controller.get());

  EXPECT_FALSE(bubble_view_->show_back_button_);
  EXPECT_FALSE(bubble_view_->show_verifying_sheet_);
  EXPECT_THAT(bubble_view_->account_ids_,
              testing::ElementsAre(kAccountId1, kAccountId2));

  observer->OnAccountSelected(accounts[0], idp_data, /*auto_signin=*/false,
                              CreateMouseEvent());
  EXPECT_TRUE(bubble_view_->show_verifying_sheet_);
  EXPECT_THAT(bubble_view_->account_ids_, testing::ElementsAre(kAccountId1));
}

TEST_F(FedCmAccountSelectionViewDesktopTest, MultipleAccountFlowBack) {
  const char kAccountId1[] = "account_id1";
  const char kAccountId2[] = "account_id2";
  IdentityProviderDisplayData idp_data = CreateIdentityProviderDisplayData({
      {kAccountId1, LoginState::kSignUp},
      {kAccountId2, LoginState::kSignUp},
  });
  const std::vector<Account>& accounts = idp_data.accounts;
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts, SignInMode::kExplicit);
  AccountSelectionBubbleView::Observer* observer =
      static_cast<AccountSelectionBubbleView::Observer*>(controller.get());

  EXPECT_FALSE(bubble_view_->show_back_button_);
  EXPECT_FALSE(bubble_view_->show_verifying_sheet_);
  EXPECT_THAT(bubble_view_->account_ids_,
              testing::ElementsAre(kAccountId1, kAccountId2));

  observer->OnAccountSelected(accounts[0], idp_data, /*auto_signin=*/false,
                              CreateMouseEvent());
  EXPECT_TRUE(bubble_view_->show_back_button_);
  EXPECT_FALSE(bubble_view_->show_verifying_sheet_);
  EXPECT_THAT(bubble_view_->account_ids_, testing::ElementsAre(kAccountId1));

  observer->OnBackButtonClicked();
  EXPECT_FALSE(bubble_view_->show_back_button_);
  EXPECT_FALSE(bubble_view_->show_verifying_sheet_);
  EXPECT_THAT(bubble_view_->account_ids_,
              testing::ElementsAre(kAccountId1, kAccountId2));

  observer->OnAccountSelected(accounts[1], idp_data, /*auto_signin=*/false,
                              CreateMouseEvent());
  EXPECT_TRUE(bubble_view_->show_back_button_);
  EXPECT_FALSE(bubble_view_->show_verifying_sheet_);
  EXPECT_THAT(bubble_view_->account_ids_, testing::ElementsAre(kAccountId2));

  observer->OnAccountSelected(accounts[1], idp_data, /*auto_signin=*/false,
                              CreateMouseEvent());
  EXPECT_TRUE(bubble_view_->show_verifying_sheet_);
  EXPECT_THAT(bubble_view_->account_ids_, testing::ElementsAre(kAccountId2));
}

TEST_F(FedCmAccountSelectionViewDesktopTest, AutoSigninSingleAccountFlow) {
  const char kAccountId[] = "account_id";
  IdentityProviderDisplayData idp_data =
      CreateIdentityProviderDisplayData({{kAccountId, LoginState::kSignIn}});
  const std::vector<Account>& accounts = idp_data.accounts;
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts, SignInMode::kAuto);

  EXPECT_FALSE(bubble_view_->show_back_button_);
  EXPECT_TRUE(bubble_view_->show_verifying_sheet_);
  EXPECT_THAT(bubble_view_->account_ids_, testing::ElementsAre(kAccountId));
}

TEST_F(FedCmAccountSelectionViewDesktopTest,
       AutoSigninMultipleAccountsOneReturning) {
  const char kAccountId1[] = "account_id1";
  const char kAccountId2[] = "account_id2";
  IdentityProviderDisplayData idp_data = CreateIdentityProviderDisplayData(
      {{kAccountId1, LoginState::kSignIn}, {kAccountId2, LoginState::kSignUp}});
  const std::vector<Account>& accounts = idp_data.accounts;
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts, SignInMode::kAuto);

  EXPECT_FALSE(bubble_view_->show_back_button_);
  EXPECT_TRUE(bubble_view_->show_verifying_sheet_);
  EXPECT_THAT(bubble_view_->account_ids_, testing::ElementsAre(kAccountId1));
}

namespace {

// AccountSelectionViewDelegate which deletes the FedCmAccountSelectionView in
// OnAccountSelected().
class ViewDeletingAccountSelectionViewDelegate
    : public MockAccountSelectionViewDelegate {
 public:
  explicit ViewDeletingAccountSelectionViewDelegate(
      content::WebContents* web_contents)
      : MockAccountSelectionViewDelegate(web_contents) {}
  ~ViewDeletingAccountSelectionViewDelegate() override = default;

  ViewDeletingAccountSelectionViewDelegate(
      const ViewDeletingAccountSelectionViewDelegate&) = delete;
  ViewDeletingAccountSelectionViewDelegate& operator=(
      const ViewDeletingAccountSelectionViewDelegate&) = delete;

  void SetView(std::unique_ptr<FedCmAccountSelectionView> view) {
    view_ = std::move(view);
  }

  void OnAccountSelected(const GURL&,
                         const content::IdentityRequestAccount&) override {
    view_.reset();
  }

 private:
  std::unique_ptr<FedCmAccountSelectionView> view_;
};

}  // namespace

TEST_F(FedCmAccountSelectionViewDesktopTest, AccountSelectedDeletesView) {
  delegate_ = std::make_unique<ViewDeletingAccountSelectionViewDelegate>(
      test_web_contents_.get());
  ViewDeletingAccountSelectionViewDelegate* view_deleting_delegate =
      static_cast<ViewDeletingAccountSelectionViewDelegate*>(delegate_.get());

  const char kAccountId1[] = "account_id1";
  IdentityProviderDisplayData idp_data = CreateIdentityProviderDisplayData({
      {kAccountId1, LoginState::kSignIn},
  });
  const std::vector<Account>& accounts = idp_data.accounts;

  AccountSelectionBubbleView::Observer* observer = nullptr;
  {
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShow(accounts, SignInMode::kExplicit);
    observer =
        static_cast<AccountSelectionBubbleView::Observer*>(controller.get());
    view_deleting_delegate->SetView(std::move(controller));
  }

  // Destroys FedCmAccountSelectionView. Should not cause crash.
  observer->OnAccountSelected(accounts[0], idp_data, /*auto_signin=*/false,
                              CreateMouseEvent());
}

TEST_F(FedCmAccountSelectionViewDesktopTest, ClickProtection) {
  const char kAccountId[] = "account_id";
  IdentityProviderDisplayData idp_data =
      CreateIdentityProviderDisplayData({{kAccountId, LoginState::kSignUp}});
  const std::vector<Account>& accounts = idp_data.accounts;
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts, SignInMode::kExplicit);
  AccountSelectionBubbleView::Observer* observer =
      static_cast<AccountSelectionBubbleView::Observer*>(controller.get());

  // Use a mock input protector to more easily test. The protector rejects the
  // first input and accepts any subsequent input.
  auto input_protector =
      std::make_unique<views::MockInputEventActivationProtector>();
  EXPECT_CALL(*input_protector, IsPossiblyUnintendedInteraction)
      .WillOnce(testing::Return(true))
      .WillRepeatedly(testing::Return(false));
  controller->SetInputEventActivationProtectorForTesting(
      std::move(input_protector));

  observer->OnAccountSelected(accounts[0], idp_data, /*auto_signin=*/false,
                              CreateMouseEvent());
  // Nothing should change after first account selected.
  EXPECT_FALSE(bubble_view_->show_back_button_);
  EXPECT_FALSE(bubble_view_->show_verifying_sheet_);
  EXPECT_THAT(bubble_view_->account_ids_, testing::ElementsAre(kAccountId));

  observer->OnAccountSelected(accounts[0], idp_data, /*auto_signin=*/false,
                              CreateMouseEvent());
  // Should show verifying sheet after first account selected.
  EXPECT_TRUE(bubble_view_->show_verifying_sheet_);
  EXPECT_THAT(bubble_view_->account_ids_, testing::ElementsAre(kAccountId));
}
