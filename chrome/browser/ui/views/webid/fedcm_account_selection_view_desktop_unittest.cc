// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"

#include <memory>
#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"
#include "chrome/browser/ui/views/webid/account_selection_view_test_base.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/constrained_window/constrained_window_views.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/test/mock_input_event_activation_protector.h"
#include "url/gurl.h"

using LoginState = content::IdentityRequestAccount::LoginState;
using SignInMode = content::IdentityRequestAccount::SignInMode;
using TokenError = content::IdentityCredentialTokenError;
using DismissReason = content::IdentityRequestDialogController::DismissReason;

namespace {

constexpr char kTopFrameEtldPlusOne[] = "top-frame-example.com";
constexpr char kIdpEtldPlusOne[] = "idp-example.com";
constexpr char kConfigUrl[] = "https://idp-example.com/fedcm.json";
constexpr char kLoginUrl[] = "https://idp-example.com/login";

constexpr char kAccountId1[] = "account_id1";
constexpr char kAccountId2[] = "account_id2";

// Mock AccountSelectionViewBase which tracks state.
class TestAccountSelectionView : public AccountSelectionViewBase {
 public:
  enum class SheetType {
    kAccountPicker,
    kConfirmAccount,
    kVerifying,
    kFailure,
    kError,
    kRequestPermission,
    kLoading,
    kSingleReturningAccount
  };

  explicit TestAccountSelectionView(views::Widget* dialog_widget)
      : dialog_widget_(dialog_widget) {}
  ~TestAccountSelectionView() override = default;

  TestAccountSelectionView(const TestAccountSelectionView&) = delete;
  TestAccountSelectionView& operator=(const TestAccountSelectionView&) = delete;

  void ShowMultiAccountPicker(
      const std::vector<IdentityRequestAccountPtr>& accounts,
      const std::vector<IdentityProviderDataPtr>& idp_list,
      bool show_back_button,
      bool is_choose_an_account) override {
    CHECK(!is_choose_an_account || show_back_button);
    show_back_button_ = show_back_button;
    is_choose_an_account_ = is_choose_an_account;
    sheet_type_ = SheetType::kAccountPicker;

    account_ids_.clear();
    for (const auto& account : accounts) {
      account_ids_.push_back(account->id);
    }
  }

  void ShowVerifyingSheet(const content::IdentityRequestAccount& account,
                          const std::u16string& title) override {
    sheet_type_ = SheetType::kVerifying;
    account_ids_ = {account.id};
    show_back_button_ = false;
    is_choose_an_account_ = false;
  }

  void ShowSingleAccountConfirmDialog(
      const content::IdentityRequestAccount& account,
      bool show_back_button) override {
    show_back_button_ = show_back_button;
    is_choose_an_account_ = false;
    sheet_type_ = SheetType::kConfirmAccount;
    account_ids_ = {account.id};
  }

  void ShowFailureDialog(
      const std::u16string& idp_for_display,
      const content::IdentityProviderMetadata& idp_metadata) override {
    sheet_type_ = SheetType::kFailure;
    account_ids_ = {};
    show_back_button_ = false;
    is_choose_an_account_ = false;
  }

  void ShowErrorDialog(const std::u16string& idp_for_display,
                       const content::IdentityProviderMetadata& idp_metadata,
                       const std::optional<TokenError>& error) override {
    sheet_type_ = SheetType::kError;
    account_ids_ = {};
    show_back_button_ = false;
    is_choose_an_account_ = false;
  }

  void ShowRequestPermissionDialog(
      const content::IdentityRequestAccount& account,
      const content::IdentityProviderData& idp_data) override {
    show_back_button_ = true;
    is_choose_an_account_ = false;
    sheet_type_ = SheetType::kRequestPermission;
    account_ids_ = {account.id};
  }

  void ShowSingleReturningAccountDialog(
      const std::vector<IdentityRequestAccountPtr>& accounts,
      const std::vector<IdentityProviderDataPtr>& idp_list) override {
    show_back_button_ = false;
    is_choose_an_account_ = false;
    sheet_type_ = SheetType::kSingleReturningAccount;
    CHECK(!accounts.empty());
    CHECK_EQ(accounts[0]->login_state.value_or(LoginState::kSignUp),
             LoginState::kSignIn);
    account_ids_ = {accounts[0]->id};
  }

  void ShowLoadingDialog() override {
    sheet_type_ = SheetType::kLoading;
    account_ids_ = {};
    show_back_button_ = false;
  }

  std::string GetDialogTitle() const override { return std::string(); }

  void InitDialogWidget() override {}
  void CloseDialog() override {}
  base::WeakPtr<views::Widget> GetDialogWidget() override {
    return dialog_widget_->GetWeakPtr();
  }
  void UpdateDialogPosition() override { dialog_position_updated_ = true; }
  bool CanFitInWebContents() override { return can_fit_in_web_contents_; }

  bool show_back_button_{false};
  bool is_choose_an_account_{false};
  bool dialog_position_updated_{false};
  bool can_fit_in_web_contents_{true};
  std::optional<SheetType> sheet_type_;
  std::vector<std::string> account_ids_;
  raw_ptr<views::Widget> dialog_widget_;
};

// Mock version of FedCmModalDialogView for injection during tests.
class MockFedCmModalDialogView : public FedCmModalDialogView {
 public:
  explicit MockFedCmModalDialogView(content::WebContents* web_contents,
                                    FedCmModalDialogView::Observer* observer)
      : FedCmModalDialogView(web_contents, observer) {}
  ~MockFedCmModalDialogView() override = default;

  MockFedCmModalDialogView(const MockFedCmModalDialogView&) = delete;
  MockFedCmModalDialogView& operator=(const MockFedCmModalDialogView&) = delete;

  MOCK_METHOD(content::WebContents*,
              ShowPopupWindow,
              (const GURL& url),
              (override));

  void ClosePopupWindow() override {
    FedCmModalDialogView::Observer* observer = GetObserverForTesting();
    if (observer) {
      observer->OnPopupWindowDestroyed();
    }
  }

  MOCK_METHOD(void, ResizeAndFocusPopupWindow, (), (override));
  MOCK_METHOD(void, SetCustomYPosition, (int y), (override));
  MOCK_METHOD(void,
              SetActiveModeSheetType,
              (AccountSelectionView::SheetType),
              (override));
};

// Test FedCmAccountSelectionView which uses TestAccountSelectionView.
class TestFedCmAccountSelectionView : public FedCmAccountSelectionView {
 public:
  TestFedCmAccountSelectionView(
      Delegate* delegate,
      TestAccountSelectionView* account_selection_view)
      : FedCmAccountSelectionView(delegate),
        account_selection_view_(account_selection_view) {
    auto input_protector =
        std::make_unique<views::MockInputEventActivationProtector>();
    ON_CALL(*input_protector, IsPossiblyUnintendedInteraction)
        .WillByDefault(testing::Return(false));
    SetInputEventActivationProtectorForTesting(std::move(input_protector));
  }

  TestFedCmAccountSelectionView(const TestFedCmAccountSelectionView&) = delete;
  TestFedCmAccountSelectionView& operator=(
      const TestFedCmAccountSelectionView&) = delete;

  blink::mojom::RpContext GetRpContext() { return rp_context_; }
  size_t num_dialogs_{0u};

  MOCK_METHOD(void, MaybeResetAccountSelectionView, (), (override));

 protected:
  AccountSelectionViewBase* CreateAccountSelectionView(
      const std::u16string& rp_for_display,
      const std::optional<std::u16string>& idp_title,
      blink::mojom::RpContext rp_context,
      blink::mojom::RpMode rp_mode,
      bool has_modal_support) override {
    ++num_dialogs_;
    rp_context_ = rp_context;
    if (rp_mode == blink::mojom::RpMode::kActive && has_modal_support) {
      dialog_type_ = FedCmAccountSelectionView::DialogType::MODAL;
    }
    return account_selection_view_;
  }

  FedCmAccountSelectionView::DialogType GetDialogType() override {
    return dialog_type_;
  }

 private:
  raw_ptr<TestAccountSelectionView> account_selection_view_;
  blink::mojom::RpContext rp_context_;
  FedCmAccountSelectionView::DialogType dialog_type_{
      FedCmAccountSelectionView::DialogType::BUBBLE};
};

// Stub AccountSelectionView::Delegate.
class StubAccountSelectionViewDelegate : public AccountSelectionView::Delegate {
 public:
  explicit StubAccountSelectionViewDelegate(content::WebContents* web_contents)
      : web_contents_(web_contents) {}
  ~StubAccountSelectionViewDelegate() override = default;

  StubAccountSelectionViewDelegate(const StubAccountSelectionViewDelegate&) =
      delete;
  StubAccountSelectionViewDelegate& operator=(
      const StubAccountSelectionViewDelegate&) = delete;

  void OnAccountSelected(const GURL&,
                         const content::IdentityRequestAccount&) override {}
  void OnDismiss(DismissReason dismiss_reason) override {
    dismiss_reason_ = dismiss_reason;
    if (on_dismiss_) {
      std::move(on_dismiss_).Run();
    }
  }
  void OnLoginToIdP(const GURL& idp_config_url,
                    const GURL& idp_login_url) override {}
  void OnMoreDetails() override {}
  void OnAccountsDisplayed() override {}
  gfx::NativeView GetNativeView() override { return gfx::NativeView(); }

  content::WebContents* GetWebContents() override { return web_contents_; }
  std::optional<DismissReason> GetDismissReason() { return dismiss_reason_; }

  void SetOnDismissClosure(base::OnceClosure on_dismiss) {
    on_dismiss_ = std::move(on_dismiss);
  }

 private:
  raw_ptr<content::WebContents> web_contents_;
  std::optional<DismissReason> dismiss_reason_;
  base::OnceClosure on_dismiss_;
};

}  // namespace

class FedCmAccountSelectionViewDesktopTest : public ChromeViewsTestBase {
 public:
  void SetUp() override {
    ChromeViewsTestBase::SetUp();

    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    delegate_ = std::make_unique<StubAccountSelectionViewDelegate>(
        test_web_contents_.get());

    // TODO(crbug.com/40232479) - We can probably clean this up and
    // get rid of the need for a WidgetAutoClosePtr when we switch to
    // CLIENT_OWNS_WIDGET.
    dialog_widget_.reset(
        CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET)
            .release());
    account_selection_view_ =
        std::make_unique<TestAccountSelectionView>(dialog_widget_.get());
    histogram_tester_ = std::make_unique<base::HistogramTester>();

    idp_data_ = CreateIdentityProviderData();
    accounts_ = {CreateAccount(idp_data_)};
    new_accounts_ = {CreateAccount(idp_data_)};
  }

  IdentityProviderDataPtr CreateIdentityProviderData(
      bool has_login_status_mismatch = false,
      const std::vector<content::IdentityRequestDialogDisclosureField>&
          disclosure_fields = kDefaultDisclosureFields) {
    return base::MakeRefCounted<content::IdentityProviderData>(
        /*idp_for_display=*/"", content::IdentityProviderMetadata(),
        content::ClientMetadata(GURL(), GURL(), GURL()),
        blink::mojom::RpContext::kSignIn, disclosure_fields,
        has_login_status_mismatch);
  }

  IdentityRequestAccountPtr CreateAccount(
      IdentityProviderDataPtr idp,
      LoginState idp_claimed_login_state = LoginState::kSignUp,
      LoginState browser_trusted_login_state = LoginState::kSignUp,
      std::string account_id = kAccountId1) {
    IdentityRequestAccountPtr account = base::MakeRefCounted<Account>(
        account_id, "", "", "", GURL(),
        /*login_hints=*/std::vector<std::string>(),
        /*domain_hints=*/std::vector<std::string>(),
        /*labels=*/std::vector<std::string>(),
        /*login_state=*/idp_claimed_login_state,
        /*browser_trusted_login_state=*/browser_trusted_login_state);
    account->identity_provider = std::move(idp);
    return account;
  }

  std::vector<IdentityRequestAccountPtr> CreateAccounts(
      const std::vector<std::pair<std::string, LoginState>>& account_infos,
      IdentityProviderDataPtr idp_data) {
    std::vector<IdentityRequestAccountPtr> accounts;
    for (const auto& account_info : account_infos) {
      accounts.emplace_back(base::MakeRefCounted<Account>(
          account_info.first, "", "", "", GURL(),
          /*login_hints=*/std::vector<std::string>(),
          /*domain_hints=*/std::vector<std::string>(),
          /*labels=*/std::vector<std::string>(),
          /*login_state=*/account_info.second,
          /*browser_trusted_login_state=*/account_info.second));
      accounts.back()->identity_provider = idp_data;
    }
    return accounts;
  }

  std::unique_ptr<TestFedCmAccountSelectionView> CreateAndShow(
      const std::vector<IdentityRequestAccountPtr>& accounts,
      SignInMode sign_in_mode,
      blink::mojom::RpMode rp_mode = blink::mojom::RpMode::kPassive) {
    auto controller = std::make_unique<TestFedCmAccountSelectionView>(
        delegate_.get(), account_selection_view_.get());
    Show(*controller, accounts, sign_in_mode, rp_mode);
    return controller;
  }

  void Show(TestFedCmAccountSelectionView& controller,
            const std::vector<IdentityRequestAccountPtr>& accounts,
            SignInMode sign_in_mode,
            blink::mojom::RpMode rp_mode,
            const std::vector<IdentityRequestAccountPtr>& new_accounts =
                std::vector<IdentityRequestAccountPtr>()) {
    controller.Show(kTopFrameEtldPlusOne, {idp_data_}, accounts, sign_in_mode,
                    rp_mode, new_accounts);
  }

  std::unique_ptr<TestFedCmAccountSelectionView> CreateAndShowMismatchDialog(
      blink::mojom::RpContext rp_context = blink::mojom::RpContext::kSignIn,
      blink::mojom::RpMode rp_mode = blink::mojom::RpMode::kPassive) {
    auto controller = std::make_unique<TestFedCmAccountSelectionView>(
        delegate_.get(), account_selection_view_.get());
    controller->ShowFailureDialog(kTopFrameEtldPlusOne, kIdpEtldPlusOne,
                                  rp_context, rp_mode,
                                  content::IdentityProviderMetadata());
    EXPECT_EQ(TestAccountSelectionView::SheetType::kFailure,
              account_selection_view_->sheet_type_);
    return controller;
  }

  std::unique_ptr<TestFedCmAccountSelectionView> CreateAndShowErrorDialog(
      blink::mojom::RpContext rp_context = blink::mojom::RpContext::kSignIn,
      blink::mojom::RpMode rp_mode = blink::mojom::RpMode::kPassive) {
    auto controller = std::make_unique<TestFedCmAccountSelectionView>(
        delegate_.get(), account_selection_view_.get());
    controller->ShowErrorDialog(
        kTopFrameEtldPlusOne, kIdpEtldPlusOne, rp_context, rp_mode,
        content::IdentityProviderMetadata(), /*error=*/std::nullopt);
    EXPECT_EQ(TestAccountSelectionView::SheetType::kError,
              account_selection_view_->sheet_type_);
    return controller;
  }

  std::unique_ptr<TestFedCmAccountSelectionView> CreateAndShowLoadingDialog(
      blink::mojom::RpContext rp_context = blink::mojom::RpContext::kSignIn,
      blink::mojom::RpMode rp_mode = blink::mojom::RpMode::kActive) {
    auto controller = std::make_unique<TestFedCmAccountSelectionView>(
        delegate_.get(), account_selection_view_.get());
    controller->ShowLoadingDialog(kTopFrameEtldPlusOne, kIdpEtldPlusOne,
                                  rp_context, rp_mode);
    EXPECT_EQ(TestAccountSelectionView::SheetType::kLoading,
              account_selection_view_->sheet_type_);
    return controller;
  }

  void CreateAndShowPopupWindow(TestFedCmAccountSelectionView& controller) {
    auto idp_signin_popup_window = std::make_unique<MockFedCmModalDialogView>(
        test_web_contents_.get(), &controller);
    EXPECT_CALL(*idp_signin_popup_window, ShowPopupWindow).Times(1);
    controller.SetIdpSigninPopupWindowForTesting(
        std::move(idp_signin_popup_window));

    controller.ShowModalDialog(GURL(u"https://example.com"),
                               blink::mojom::RpMode::kPassive);
  }

  std::unique_ptr<TestFedCmAccountSelectionView> CreateAndShowMultiIdp(
      const std::vector<IdentityProviderDataPtr>& idp_list,
      const std::vector<IdentityRequestAccountPtr>& accounts,
      SignInMode sign_in_mode,
      blink::mojom::RpMode rp_mode) {
    auto controller = std::make_unique<TestFedCmAccountSelectionView>(
        delegate_.get(), account_selection_view_.get());
    controller->Show(kTopFrameEtldPlusOne, idp_list, accounts, sign_in_mode,
                     rp_mode,
                     /*new_accounts=*/std::vector<IdentityRequestAccountPtr>());
    return controller;
  }

  std::unique_ptr<TestFedCmAccountSelectionView>
  CreateAndShowAccountsModalThroughPopupWindow(
      const std::vector<IdentityRequestAccountPtr>& all_accounts,
      const std::vector<IdentityRequestAccountPtr>& new_accounts) {
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowLoadingDialog();
    AccountSelectionViewBase::Observer* observer =
        static_cast<AccountSelectionViewBase::Observer*>(controller.get());

    // Emulate the login to IdP flow.
    observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl),
                           CreateMouseEvent());
    CreateAndShowPopupWindow(*controller);

    // Emulate user completing the sign-in flow and IdP prompts closing the
    // pop-up window and sending new accounts.
    controller->CloseModalDialog();

    Show(*controller, all_accounts, SignInMode::kExplicit,
         blink::mojom::RpMode::kActive, new_accounts);

    return controller;
  }

  std::unique_ptr<TestFedCmAccountSelectionView>
  CreateAndShowAccountsThroughUseAnotherAccount(
      const std::vector<std::pair<std::string, LoginState>>& old_account_infos,
      const std::vector<std::pair<std::string, LoginState>>& new_account_infos,
      blink::mojom::RpMode rp_mode = blink::mojom::RpMode::kPassive) {
    accounts_ = CreateAccounts(old_account_infos, idp_data_);
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShow(accounts_, SignInMode::kExplicit, rp_mode);
    AccountSelectionViewBase::Observer* observer =
        static_cast<AccountSelectionViewBase::Observer*>(controller.get());

    // Emulate the user clicking "use another account button".
    observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl),
                           CreateMouseEvent());
    CreateAndShowPopupWindow(*controller);

    // Emulate user completing the sign-in flow and IdP prompts closing the
    // pop-up window and sending new accounts.
    controller->CloseModalDialog();

    new_accounts_ = CreateAccounts(new_account_infos, idp_data_);

    std::vector<std::pair<std::string, LoginState>> combined_account_infos =
        old_account_infos;
    for (const auto& account_info : new_account_infos) {
      if (std::find(combined_account_infos.begin(),
                    combined_account_infos.end(),
                    account_info) != combined_account_infos.end()) {
        continue;
      }

      combined_account_infos.emplace_back(account_info);
    }

    const std::vector<IdentityRequestAccountPtr> combined_accounts =
        CreateAccounts(combined_account_infos, idp_data_);

    Show(*controller, combined_accounts, SignInMode::kExplicit, rp_mode,
         new_accounts_);

    return controller;
  }

  ui::MouseEvent CreateMouseEvent() {
    return ui::MouseEvent(ui::EventType::kMousePressed, gfx::Point(),
                          gfx::Point(), base::TimeTicks(),
                          ui::EF_LEFT_MOUSE_BUTTON, 0);
  }

 protected:
  TestingProfile profile_;

  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories_;

  std::unique_ptr<content::WebContents> test_web_contents_;
  views::ViewsTestBase::WidgetAutoclosePtr dialog_widget_;
  std::unique_ptr<TestAccountSelectionView> account_selection_view_;
  std::unique_ptr<StubAccountSelectionViewDelegate> delegate_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;

  IdentityProviderDataPtr idp_data_;
  std::vector<IdentityRequestAccountPtr> accounts_;
  std::vector<IdentityRequestAccountPtr> new_accounts_;
};

TEST_F(FedCmAccountSelectionViewDesktopTest, SingleAccountFlow) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts_, SignInMode::kExplicit);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
}

TEST_F(FedCmAccountSelectionViewDesktopTest, MultipleAccountFlowReturning) {
  accounts_ = CreateAccounts(
      {{kAccountId1, LoginState::kSignIn}, {kAccountId2, LoginState::kSignIn}},
      idp_data_);
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts_, SignInMode::kExplicit);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1, kAccountId2));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
}

TEST_F(FedCmAccountSelectionViewDesktopTest, MultipleAccountFlowBack) {
  accounts_ = CreateAccounts(
      {{kAccountId1, LoginState::kSignUp}, {kAccountId2, LoginState::kSignUp}},
      idp_data_);
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts_, SignInMode::kExplicit);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1, kAccountId2));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_TRUE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnBackButtonClicked();
  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1, kAccountId2));

  observer->OnAccountSelected(*accounts_[1], *idp_data_, CreateMouseEvent());
  EXPECT_TRUE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId2));

  observer->OnAccountSelected(*accounts_[1], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId2));
}

// Test transitioning from IdP sign-in status mismatch failure dialog to regular
// sign-in dialog.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       IdpSigninStatusMismatchDialogToSigninFlow) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMismatchDialog();
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_EQ(TestAccountSelectionView::SheetType::kFailure,
            account_selection_view_->sheet_type_);

  Show(*controller, accounts_, SignInMode::kExplicit,
       blink::mojom::RpMode::kPassive, new_accounts_);

  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  observer->OnAccountSelected(*new_accounts_[0], *idp_data_,
                              CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);

  EXPECT_EQ(1u, controller->num_dialogs_);
}

// Test transitioning from IdP sign-in status mismatch failure dialog to regular
// sign-in dialog while the dialog is hidden. This emulates a user signing
// into the IdP in a different tab.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       IdpSigninStatusMismatchDialogToSigninFlowHidden) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMismatchDialog();
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_EQ(TestAccountSelectionView::SheetType::kFailure,
            account_selection_view_->sheet_type_);


  // If the user switched tabs to sign-into the IdP, Show() may be called while
  // the associated FedCM tab is inactive. Show() should not show the
  // views::Widget in this case.
  controller->OnTabBackgrounded();
  Show(*controller, accounts_, SignInMode::kExplicit,
       blink::mojom::RpMode::kPassive, new_accounts_);
  EXPECT_FALSE(dialog_widget_->IsVisible());

  controller->OnTabForegrounded();
  EXPECT_TRUE(dialog_widget_->IsVisible());

  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);

  EXPECT_EQ(1u, controller->num_dialogs_);
}

TEST_F(FedCmAccountSelectionViewDesktopTest, AutoReauthnSingleAccountFlow) {
  accounts_[0]->browser_trusted_login_state = LoginState::kSignIn;
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts_, SignInMode::kAuto);

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
}

namespace {

// AccountSelectionViewDelegate which deletes the FedCmAccountSelectionView in
// OnAccountSelected().
class ViewDeletingAccountSelectionViewDelegate
    : public StubAccountSelectionViewDelegate {
 public:
  explicit ViewDeletingAccountSelectionViewDelegate(
      content::WebContents* web_contents)
      : StubAccountSelectionViewDelegate(web_contents) {}
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

  accounts_ = {
      CreateAccount(idp_data_, LoginState::kSignIn, LoginState::kSignIn)};

  AccountSelectionViewBase::Observer* observer = nullptr;
  {
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShow(accounts_, SignInMode::kExplicit);
    observer =
        static_cast<AccountSelectionViewBase::Observer*>(controller.get());
    view_deleting_delegate->SetView(std::move(controller));
  }

  // Destroys FedCmAccountSelectionView. Should not cause crash.
  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
}

TEST_F(FedCmAccountSelectionViewDesktopTest, ClickProtection) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts_, SignInMode::kExplicit);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  // Use a mock input protector to more easily test. The protector rejects the
  // first input and accepts any subsequent input.
  auto input_protector =
      std::make_unique<views::MockInputEventActivationProtector>();
  EXPECT_CALL(*input_protector, IsPossiblyUnintendedInteraction)
      .WillOnce(testing::Return(true))
      .WillRepeatedly(testing::Return(false));
  controller->SetInputEventActivationProtectorForTesting(
      std::move(input_protector));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  // Nothing should change after first account selected.
  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  // Should show verifying sheet after first account selected.
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
}

// Tests that when the auth re-authn dialog is closed, the relevant metric is
// recorded.
TEST_F(FedCmAccountSelectionViewDesktopTest, CloseAutoReauthnSheetMetric) {
  accounts_ = {
      CreateAccount(idp_data_, LoginState::kSignIn, LoginState::kSignIn)};
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts_, SignInMode::kAuto);
  histogram_tester_->ExpectTotalCount("Blink.FedCm.ClosedSheetType.Desktop", 0);

  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());
  observer->OnCloseButtonClicked(CreateMouseEvent());
  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.ClosedSheetType.Desktop",
      static_cast<int>(AccountSelectionView::SheetType::AUTO_REAUTHN), 1);
}

// Tests that when the mismatch dialog is closed through the close icon, the
// relevant metric is recorded.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       MismatchDialogDismissedByCloseIconMetric) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMismatchDialog();
  histogram_tester_->ExpectTotalCount(
      "Blink.FedCm.IdpSigninStatus.MismatchDialogResult", 0);

  // Emulate user clicking the close icon.
  dialog_widget_->CloseWithReason(
      views::Widget::ClosedReason::kCloseButtonClicked);
  controller->OnWidgetDestroying(dialog_widget_.get());

  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.IdpSigninStatus.MismatchDialogResult",
      static_cast<int>(FedCmAccountSelectionView::MismatchDialogResult::
                           kDismissedByCloseIcon),
      1);
}

// Tests that when the mismatch dialog is closed through means other than the
// close icon, the relevant metric is recorded.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       MismatchDialogDismissedForOtherReasonsMetric) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMismatchDialog();
  histogram_tester_->ExpectTotalCount(
      "Blink.FedCm.IdpSigninStatus.MismatchDialogResult", 0);

  // Emulate user closing the mismatch dialog for an unspecified reason.
  dialog_widget_->CloseWithReason(views::Widget::ClosedReason::kUnspecified);
  controller->OnWidgetDestroying(dialog_widget_.get());

  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.IdpSigninStatus.MismatchDialogResult",
      static_cast<int>(FedCmAccountSelectionView::MismatchDialogResult::
                           kDismissedForOtherReasons),
      1);
}

// Tests that when FedCmAccountSelectionView is destroyed while the mismatch
// dialog is open, the relevant metric is recorded.
TEST_F(FedCmAccountSelectionViewDesktopTest, MismatchDialogDestroyedMetric) {
  {
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowMismatchDialog();
    histogram_tester_->ExpectTotalCount(
        "Blink.FedCm.IdpSigninStatus.MismatchDialogResult", 0);
  }

  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.IdpSigninStatus.MismatchDialogResult",
      static_cast<int>(FedCmAccountSelectionView::MismatchDialogResult::
                           kDismissedForOtherReasons),
      1);
}

// Tests that when the continue button on the mismatch dialog is clicked, the
// relevant metric is recorded.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       MismatchDialogContinueClickedMetric) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMismatchDialog();
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());
  histogram_tester_->ExpectTotalCount(
      "Blink.FedCm.IdpSigninStatus.MismatchDialogResult", 0);

  // Emulate user clicking on "Continue" button in the mismatch dialog.
  observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl), CreateMouseEvent());
  CreateAndShowPopupWindow(*controller);

  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.IdpSigninStatus.MismatchDialogResult",
      static_cast<int>(
          FedCmAccountSelectionView::MismatchDialogResult::kContinued),
      1);
}

// Tests that when the continue button on the mismatch dialog is clicked and
// then FedCmAccountSelectionView is destroyed, we record only the metric for
// the continue button being clicked.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       MismatchDialogContinueClickedThenDestroyedMetric) {
  {
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowMismatchDialog();
    AccountSelectionViewBase::Observer* observer =
        static_cast<AccountSelectionViewBase::Observer*>(controller.get());
    histogram_tester_->ExpectTotalCount(
        "Blink.FedCm.IdpSigninStatus.MismatchDialogResult", 0);

    // Emulate user clicking on "Continue" button in the mismatch dialog.
    observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl),
                           CreateMouseEvent());
    CreateAndShowPopupWindow(*controller);
  }

  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.IdpSigninStatus.MismatchDialogResult",
      static_cast<int>(
          FedCmAccountSelectionView::MismatchDialogResult::kContinued),
      1);
}

// Test transitioning from IdP sign-in status mismatch dialog to regular sign-in
// dialog. This emulates a user signing into the IdP in a pop-up window and the
// pop-up window closes PRIOR to the mismatch dialog being updated to a regular
// sign-in dialog.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       IdpSigninStatusPopupClosedBeforeAccountsPopulated) {
  {
    // Trigger IdP sign-in status mismatch dialog.
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowMismatchDialog();
    AccountSelectionViewBase::Observer* observer =
        static_cast<AccountSelectionViewBase::Observer*>(controller.get());

    // Emulate user clicking on "Continue" button in the mismatch dialog.
    observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl),
                           CreateMouseEvent());
    CreateAndShowPopupWindow(*controller);

    // When pop-up window is shown, mismatch dialog should be hidden.
    EXPECT_FALSE(dialog_widget_->IsVisible());

    // Emulate user completing the sign-in flow and IdP prompts closing the
    // pop-up window.
    controller->CloseModalDialog();

    // Mismatch dialog should remain hidden because it has not been updated to
    // an accounts dialog yet.
    EXPECT_FALSE(dialog_widget_->IsVisible());

    histogram_tester_->ExpectTotalCount(
        "Blink.FedCm.IdpSigninStatus."
        "IdpClosePopupToBrowserShowAccountsDuration",
        0);
    histogram_tester_->ExpectTotalCount(
        "Blink.FedCm.IdpSigninStatus.PopupWindowResult", 0);

    // Emulate IdP sending the IdP sign-in status header which updates the
    // mismatch dialog to an accounts dialog.
    Show(*controller, accounts_, SignInMode::kExplicit,
         blink::mojom::RpMode::kPassive, new_accounts_);

    // Accounts dialog should now be visible. One account is logged in, so no
    // back button is shown.
    EXPECT_TRUE(dialog_widget_->IsVisible());
    EXPECT_FALSE(account_selection_view_->show_back_button_);
  }

  histogram_tester_->ExpectTotalCount(
      "Blink.FedCm.IdpSigninStatus.IdpClosePopupToBrowserShowAccountsDuration",
      1);
  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.IdpSigninStatus.PopupWindowResult",
      static_cast<int>(FedCmAccountSelectionView::PopupWindowResult::
                           kAccountsReceivedAndPopupClosedByIdp),
      1);
}

// Test transitioning from IdP sign-in status mismatch dialog to regular sign-in
// dialog. This emulates a user signing into the IdP in a pop-up window and the
// pop-up window closes AFTER the mismatch dialog has been updated to a regular
// sign-in dialog.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       IdpSigninStatusPopupClosedAfterAccountsPopulated) {
  {
    // Trigger IdP sign-in status mismatch dialog.
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowMismatchDialog();
    AccountSelectionViewBase::Observer* observer =
        static_cast<AccountSelectionViewBase::Observer*>(controller.get());

    // Emulate user clicking on "Continue" button in the mismatch dialog.
    observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl),
                           CreateMouseEvent());
    CreateAndShowPopupWindow(*controller);

    // When pop-up window is shown, mismatch dialog should be hidden.
    EXPECT_FALSE(dialog_widget_->IsVisible());

    // Emulate IdP sending the IdP sign-in status header which updates the
    // mismatch dialog to an accounts dialog.
    Show(*controller, accounts_, SignInMode::kExplicit,
         blink::mojom::RpMode::kPassive, new_accounts_);

    // Accounts dialog should remain hidden because the pop-up window has not
    // been closed yet.
    EXPECT_FALSE(dialog_widget_->IsVisible());

    histogram_tester_->ExpectTotalCount(
        "Blink.FedCm.IdpSigninStatus."
        "IdpClosePopupToBrowserShowAccountsDuration",
        0);
    histogram_tester_->ExpectTotalCount(
        "Blink.FedCm.IdpSigninStatus.PopupWindowResult", 0);

    // Emulate IdP closing the pop-up window.
    controller->CloseModalDialog();

    // Accounts dialog should now be visible.
    EXPECT_TRUE(dialog_widget_->IsVisible());
  }

  histogram_tester_->ExpectTotalCount(
      "Blink.FedCm.IdpSigninStatus."
      "IdpClosePopupToBrowserShowAccountsDuration",
      1);
  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.IdpSigninStatus.PopupWindowResult",
      static_cast<int>(FedCmAccountSelectionView::PopupWindowResult::
                           kAccountsReceivedAndPopupClosedByIdp),
      1);
}

// Test that when user opens a pop-up window to complete the IDP sign-in flow,
// we record the appropriate metric when accounts are received but
// IdentityProvider.close() is not called.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       IdpSigninStatusAccountsReceivedAndNoPopupClosedByIdpMetric) {
  {
    // Trigger IdP sign-in status mismatch dialog.
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowMismatchDialog();
    AccountSelectionViewBase::Observer* observer =
        static_cast<AccountSelectionViewBase::Observer*>(controller.get());

    // Emulate user clicking on "Continue" button in the mismatch dialog.
    observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl),
                           CreateMouseEvent());
    CreateAndShowPopupWindow(*controller);

    // Emulate IdP sending the IdP sign-in status header which updates the
    // failure dialog to an accounts dialog.
    Show(*controller, accounts_, SignInMode::kExplicit,
         blink::mojom::RpMode::kPassive, new_accounts_);

    histogram_tester_->ExpectTotalCount(
        "Blink.FedCm.IdpSigninStatus.PopupWindowResult", 0);
  }

  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.IdpSigninStatus.PopupWindowResult",
      static_cast<int>(FedCmAccountSelectionView::PopupWindowResult::
                           kAccountsReceivedAndPopupNotClosedByIdp),
      1);
}

// Test that when user opens a pop-up window to complete the IDP sign-in flow,
// we record the appropriate metric when accounts are not received but
// IdentityProvider.close() is called.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       IdpSigninStatusAccountsNotReceivedAndPopupClosedByIdpMetric) {
  {
    // Trigger IdP sign-in status mismatch dialog.
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowMismatchDialog();
    AccountSelectionViewBase::Observer* observer =
        static_cast<AccountSelectionViewBase::Observer*>(controller.get());

    // Emulate user clicking on "Continue" button in the mismatch dialog.
    observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl),
                           CreateMouseEvent());
    CreateAndShowPopupWindow(*controller);

    // Emulate IdentityProvider.close() being called in the pop-up window.
    controller->CloseModalDialog();

    histogram_tester_->ExpectTotalCount(
        "Blink.FedCm.IdpSigninStatus.PopupWindowResult", 0);
  }

  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.IdpSigninStatus.PopupWindowResult",
      static_cast<int>(FedCmAccountSelectionView::PopupWindowResult::
                           kAccountsNotReceivedAndPopupClosedByIdp),
      1);
}

// Test that when user opens a pop-up window to complete the IDP sign-in flow,
// we record the appropriate metric when accounts are not received and
// IdentityProvider.close() is not called.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       IdpSigninStatusAccountsNotReceivedAndNoPopupClosedByIdpMetric) {
  {
    // Trigger IdP sign-in status mismatch dialog.
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowMismatchDialog();
    AccountSelectionViewBase::Observer* observer =
        static_cast<AccountSelectionViewBase::Observer*>(controller.get());

    // Emulate user clicking on "Continue" button in the mismatch dialog.
    observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl),
                           CreateMouseEvent());
    CreateAndShowPopupWindow(*controller);

    histogram_tester_->ExpectTotalCount(
        "Blink.FedCm.IdpSigninStatus.PopupWindowResult", 0);
  }

  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.IdpSigninStatus.PopupWindowResult",
      static_cast<int>(FedCmAccountSelectionView::PopupWindowResult::
                           kAccountsNotReceivedAndPopupNotClosedByIdp),
      1);
}

// Test closing the IdP sign-in pop-up window through IdentityProvider.close()
// should not close the widget.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       IdpSigninStatusPopupClosedViaIdentityProviderClose) {
  // Trigger IdP sign-in status mismatch dialog.
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMismatchDialog();

  // Emulate user clicking on "Continue" button in the mismatch dialog.
  CreateAndShowPopupWindow(*controller);

  // Emulate IdentityProvider.close() being called in the pop-up window.
  controller->CloseModalDialog();

  // Widget should not be closed.
  EXPECT_FALSE(dialog_widget_->IsClosed());
}

// Test closing the IdP sign-in pop-up window through means other than
// IdentityProvider.close() should also close the widget.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       IdpSigninStatusPopupClosedViaPopupDestroyed) {
  // Trigger IdP sign-in status mismatch dialog.
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMismatchDialog();

  // Emulate user clicking on "Continue" button in the mismatch dialog.
  CreateAndShowPopupWindow(*controller);

  // Emulate user closing the pop-up window.
  controller->OnPopupWindowDestroyed();

  // Widget should be closed.
  EXPECT_TRUE(dialog_widget_->IsClosed());
}

// Test that the mismatch dialog can be shown again after the pop-up window is
// closed.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       IdpSigninStatusMismatchDialogReshown) {
  // Trigger IdP sign-in status mismatch dialog.
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMismatchDialog();

  // Emulate user clicking on "Continue" button in the mismatch dialog.
  CreateAndShowPopupWindow(*controller);

  // Mismatch dialog should be hidden because pop-up window is open.
  EXPECT_FALSE(dialog_widget_->IsVisible());

  // Emulate IdentityProvider.close() being called in the pop-up window.
  controller->CloseModalDialog();

  // Mismatch dialog should remain hidden because it has not been updated to an
  // accounts dialog yet.
  EXPECT_FALSE(dialog_widget_->IsVisible());

  // Emulate another mismatch so we need to show the mismatch dialog again.
  controller->ShowFailureDialog(
      kTopFrameEtldPlusOne, kIdpEtldPlusOne, blink::mojom::RpContext::kSignIn,
      blink::mojom::RpMode::kPassive, content::IdentityProviderMetadata());

  // Mismatch dialog is visible again.
  EXPECT_TRUE(dialog_widget_->IsVisible());
}

// Tests that RP context is properly set for the mismatch UI.
TEST_F(FedCmAccountSelectionViewDesktopTest, MismatchDialogWithRpContext) {
  {
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowMismatchDialog();
    EXPECT_EQ(controller->GetRpContext(), blink::mojom::RpContext::kSignIn);
  }
  {
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowMismatchDialog(blink::mojom::RpContext::kSignUp);
    EXPECT_EQ(controller->GetRpContext(), blink::mojom::RpContext::kSignUp);
  }
  {
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowMismatchDialog(blink::mojom::RpContext::kContinue);
    EXPECT_EQ(controller->GetRpContext(), blink::mojom::RpContext::kContinue);
  }
  {
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowMismatchDialog(blink::mojom::RpContext::kUse);
    EXPECT_EQ(controller->GetRpContext(), blink::mojom::RpContext::kUse);
  }
}

// Tests the following
// 1. pop-up window is closed
// 2. visibility changes to hidden e.g. user navigates to different tab
// 3. Show() is invoked
// 4. widget should remain hidden
// 5. visibility changes to visible e.g. user navigates back to same tab
// 6. widget should now be visible
TEST_F(FedCmAccountSelectionViewDesktopTest,
       BubbleWidgetAfterPopupRemainsHiddenAfterAccountsFetched) {
  // Trigger IdP sign-in status mismatch dialog.
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMismatchDialog();
  // Emulate user clicking on "Continue" button in the mismatch dialog.
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());
  observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl), CreateMouseEvent());
  CreateAndShowPopupWindow(*controller);

  // Emulate IdP closing the pop-up window.
  controller->CloseModalDialog();

  controller->OnTabBackgrounded();

  // Emulate IdP sending the IdP sign-in status header which updates the
  // mismatch dialog to an accounts dialog.
  Show(*controller, accounts_, SignInMode::kExplicit,
       blink::mojom::RpMode::kPassive, new_accounts_);
  EXPECT_FALSE(dialog_widget_->IsVisible());

  controller->OnTabForegrounded();
  EXPECT_TRUE(dialog_widget_->IsVisible());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
}

// Tests the following
// 1. pop-up window is closed
// 2. visibility changes to hidden e.g. user navigates to different tab
// 3. visibility changes to visible e.g. user navigates back to same tab
// 4. widget should remain hidden
// 5. Show() is invoked
// 6. widget should now be visible
TEST_F(FedCmAccountSelectionViewDesktopTest,
       BubbleWidgetAfterPopupRemainsHiddenBeforeAccountsFetched) {
  // Trigger IdP sign-in status mismatch dialog.
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMismatchDialog();
  // Emulate user clicking on "Continue" button in the mismatch dialog.
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());
  observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl), CreateMouseEvent());
  CreateAndShowPopupWindow(*controller);

  // Emulate IdP closing the pop-up window.
  controller->CloseModalDialog();

  // Switch to a different tab and then switch back to the same tab. The widget
  // should remain hidden because the mismatch dialog has not been updated into
  // an accounts dialog yet.
  controller->OnTabBackgrounded();
  EXPECT_FALSE(dialog_widget_->IsVisible());
  controller->OnTabForegrounded();
  EXPECT_FALSE(dialog_widget_->IsVisible());

  // Emulate IdP sending the IdP sign-in status header which updates the
  // mismatch dialog to an accounts dialog.
  Show(*controller, accounts_, SignInMode::kExplicit,
       blink::mojom::RpMode::kPassive, new_accounts_);

  // The widget should now be visible.
  EXPECT_TRUE(dialog_widget_->IsVisible());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
}

// Test transitioning from IdP sign-in status mismatch failure dialog to regular
// sign-in dialog where two accounts are logged in at once.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       IdpSigninStatusMismatchMultiAccount) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMismatchDialog();
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  accounts_ = CreateAccounts(
      {{kAccountId1, LoginState::kSignUp}, {kAccountId2, LoginState::kSignUp}},
      idp_data_);
  new_accounts_ = accounts_;

  Show(*controller, accounts_, SignInMode::kExplicit,
       blink::mojom::RpMode::kPassive, new_accounts_);

  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  // Should have only shown both accounts.
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1, kAccountId2));
  // There are no other accounts, so back button should not be present.
  EXPECT_FALSE(account_selection_view_->show_back_button_);

  observer->OnAccountSelected(*new_accounts_[0], *idp_data_,
                              CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_TRUE(account_selection_view_->show_back_button_);
  observer->OnAccountSelected(*new_accounts_[0], *idp_data_,
                              CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  EXPECT_EQ(1u, controller->num_dialogs_);
}

// Test the use another account flow, resulting in the new account being shown
// after logging in.
TEST_F(FedCmAccountSelectionViewDesktopTest, UseAnotherAccount) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowAccountsThroughUseAnotherAccount(
          /*old_account_infos=*/{{kAccountId1, LoginState::kSignUp}},
          /*new_account_infos=*/{{kAccountId2, LoginState::kSignUp}});
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  // Only the newly logged in account is shown in the latest dialog.
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId2));

  // Back button must be showing since there are multiple accounts.
  EXPECT_TRUE(account_selection_view_->show_back_button_);

  // Clicking the back button shows all the accounts.
  observer->OnBackButtonClicked();
  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1, kAccountId2));
}

// Test the use another account flow when signing into the same account that the
// user started with.
TEST_F(FedCmAccountSelectionViewDesktopTest, UseAnotherAccountForSameAccount) {
  CreateAndShowAccountsThroughUseAnotherAccount(
      /*old_account_infos=*/{{kAccountId1, LoginState::kSignUp}},
      /*new_account_infos=*/{{kAccountId1, LoginState::kSignUp}});

  // Only the newly logged in account is shown in the latest dialog.
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  // Back button must NOT be showing since there is only one account.
  EXPECT_FALSE(account_selection_view_->show_back_button_);
}

// Test the use another account flow, resulting in account chooser UI if it's a
// returning account.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       UseAnotherAccountForReturningAccount) {
  CreateAndShowAccountsThroughUseAnotherAccount(
      /*old_account_infos=*/{{kAccountId1, LoginState::kSignUp}},
      /*new_account_infos=*/{{kAccountId2, LoginState::kSignIn}});

  // The account chooser UI is NOT skipped.
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId2));
  EXPECT_TRUE(account_selection_view_->show_back_button_);
}

// Test the use another account flow in a modal, resulting in the new account
// being shown after logging in.
TEST_F(FedCmAccountSelectionViewDesktopTest, UseAnotherAccountModal) {
  CreateAndShowAccountsThroughUseAnotherAccount(
      /*old_account_infos=*/{{kAccountId1, LoginState::kSignUp}},
      /*new_account_infos=*/{{kAccountId2, LoginState::kSignUp}},
      /*rp_mode=*/blink::mojom::RpMode::kActive);

  // The permission UI is NOT skipped.
  EXPECT_EQ(TestAccountSelectionView::SheetType::kRequestPermission,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId2));
  EXPECT_TRUE(account_selection_view_->show_back_button_);
}

// Test the use another account flow in a modal when signing into the same
// account that the user started with.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       UseAnotherAccountModalForSameAccount) {
  CreateAndShowAccountsThroughUseAnotherAccount(
      /*old_account_infos=*/{{kAccountId1, LoginState::kSignUp}},
      /*new_account_infos=*/{{kAccountId1, LoginState::kSignUp}},
      /*rp_mode=*/blink::mojom::RpMode::kActive);

  // The permission UI is NOT skipped.
  EXPECT_EQ(TestAccountSelectionView::SheetType::kRequestPermission,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
}

// Test the use another account flow in a modal, resulting in no account chooser
// UI if it's a returning account.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       UseAnotherAccountModalForReturningAccount) {
  CreateAndShowAccountsThroughUseAnotherAccount(
      /*old_account_infos=*/{{kAccountId1, LoginState::kSignUp}},
      /*new_account_infos=*/{{kAccountId2, LoginState::kSignIn}},
      /*rp_mode=*/blink::mojom::RpMode::kActive);

  // The account chooser UI is skipped.
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId2));
  EXPECT_FALSE(account_selection_view_->show_back_button_);
}

// Test the logged-out LoginStatus flow in a modal, resulting in account
// chooser UI if it's a returning account.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       LoginStatusLoggedOutModalForReturningAccount) {
  accounts_ = {
      CreateAccount(idp_data_, LoginState::kSignIn, LoginState::kSignIn)};
  new_accounts_ = accounts_;

  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowAccountsModalThroughPopupWindow(accounts_, new_accounts_);

  // The account chooser UI is NOT skipped if user signed in from LOADING state.
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
  EXPECT_FALSE(account_selection_view_->show_back_button_);
}

// Test the logged-out LoginStatus flow in a modal, resulting in showing account
// chooser UI if it's a non-returning account.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       LoginStatusLoggedOutModalForNonReturningAccount) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowAccountsModalThroughPopupWindow(accounts_, new_accounts_);

  // The permission UI is NOT skipped.
  EXPECT_EQ(TestAccountSelectionView::SheetType::kRequestPermission,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
}

// Test the browser trusted login state controls whether to skip the account
// chooser UI when in conflict with login state.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       BrowserTrustedLoginStateTakesPrecedenceOverLoginState) {
  accounts_ = {
      CreateAccount(idp_data_, /*idp_claimed_login_state=*/LoginState::kSignIn,
                    /*browser_trusted_login_state=*/LoginState::kSignUp)};
  new_accounts_ = accounts_;

  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowAccountsModalThroughPopupWindow(accounts_, new_accounts_);

  // The account chooser UI is NOT skipped. Normally, this is permission UI but
  // because we do not want to show disclosure UI without disclosure text, we
  // show the account chooser UI instead.
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
  EXPECT_FALSE(account_selection_view_->show_back_button_);
}

// Test user triggering the use another account flow twice in a modal, without
// closing the pop-up from the first use another account flow.
TEST_F(FedCmAccountSelectionViewDesktopTest, UseAnotherAccountTwiceModal) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
      accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kActive);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  // Emulate the user clicking "use another account button".
  observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl), CreateMouseEvent());
  CreateAndShowPopupWindow(*controller);

  // Modal remains visible.
  EXPECT_TRUE(dialog_widget_->IsVisible());

  // Emulate the user clicking "use another account button" again. This should
  // not crash.
  observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl), CreateMouseEvent());
  CreateAndShowPopupWindow(*controller);
}

// Test user triggering the use another account flow twice in a modal, with
// closing the pop-up from the first use another account flow.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       UseAnotherAccountCloseThenReopenModal) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
      accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kActive);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  // Emulate the user clicking "use another account button".
  observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl), CreateMouseEvent());
  CreateAndShowPopupWindow(*controller);

  // Modal remains visible.
  EXPECT_TRUE(dialog_widget_->IsVisible());

  // Emulate user closing the pop-up window.
  controller->OnPopupWindowDestroyed();

  // Modal remains visible.
  EXPECT_TRUE(dialog_widget_->IsVisible());

  // Emulate the user clicking "use another account button" again. This should
  // not crash.
  observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl), CreateMouseEvent());
  CreateAndShowPopupWindow(*controller);
}

// Test user triggering the use another account flow then clicking on the cancel
// button in the modal without completing the use other account flow.
TEST_F(FedCmAccountSelectionViewDesktopTest, UseAnotherAccountThenCancel) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
      accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kActive);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  // Emulate the user clicking "use another account button".
  observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl), CreateMouseEvent());
  CreateAndShowPopupWindow(*controller);

  // Modal remains visible.
  EXPECT_TRUE(dialog_widget_->IsVisible());

  // Emulate the user clicking "cancel" button. This should close the widget.
  observer->OnCloseButtonClicked(CreateMouseEvent());
  EXPECT_TRUE(dialog_widget_->IsClosed());
}

// Tests that the error dialog can be shown.
TEST_F(FedCmAccountSelectionViewDesktopTest, ErrorDialogShown) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowErrorDialog();
  EXPECT_TRUE(dialog_widget_->IsVisible());
}

// Tests that RP context is properly set for the error dialog.
TEST_F(FedCmAccountSelectionViewDesktopTest, ErrorDialogWithRpContext) {
  {
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowErrorDialog();
    EXPECT_EQ(controller->GetRpContext(), blink::mojom::RpContext::kSignIn);
  }
  {
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowErrorDialog(blink::mojom::RpContext::kSignUp);
    EXPECT_EQ(controller->GetRpContext(), blink::mojom::RpContext::kSignUp);
  }
  {
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowErrorDialog(blink::mojom::RpContext::kContinue);
    EXPECT_EQ(controller->GetRpContext(), blink::mojom::RpContext::kContinue);
  }
  {
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowErrorDialog(blink::mojom::RpContext::kUse);
    EXPECT_EQ(controller->GetRpContext(), blink::mojom::RpContext::kUse);
  }
}

// Tests the flow for when the "got it" button on the error dialog is clicked.
TEST_F(FedCmAccountSelectionViewDesktopTest, ErrorDialogGotItClicked) {
  // Trigger error dialog.
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowErrorDialog();
  EXPECT_TRUE(dialog_widget_->IsVisible());

  // Emulate user clicking on "got it" button in the error dialog.
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());
  observer->OnGotIt(CreateMouseEvent());

  // Widget should be dismissed.
  StubAccountSelectionViewDelegate* delegate =
      static_cast<StubAccountSelectionViewDelegate*>(delegate_.get());
  EXPECT_EQ(delegate->GetDismissReason(), DismissReason::kGotItButton);
}

// Tests the flow for when the "more details" button on the error dialog is
// clicked.
TEST_F(FedCmAccountSelectionViewDesktopTest, ErrorDialogMoreDetailsClicked) {
  // Trigger error dialog.
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowErrorDialog();
  EXPECT_TRUE(dialog_widget_->IsVisible());

  // Emulate user clicking on "more details" button in the error dialog.
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());
  observer->OnMoreDetails(CreateMouseEvent());
  CreateAndShowPopupWindow(*controller);

  // Widget should be dismissed.
  StubAccountSelectionViewDelegate* delegate =
      static_cast<StubAccountSelectionViewDelegate*>(delegate_.get());
  EXPECT_EQ(delegate->GetDismissReason(), DismissReason::kMoreDetailsButton);
}

TEST_F(FedCmAccountSelectionViewDesktopTest, MultiIdpWithOneIdpMismatch) {
  std::vector<IdentityProviderDataPtr> idp_list = {
      CreateIdentityProviderData(),
      CreateIdentityProviderData(/*has_login_status_mismatch*/ true)};
  std::vector<IdentityRequestAccountPtr> accounts = {
      CreateAccount(idp_list[0])};
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMultiIdp(idp_list, accounts, SignInMode::kExplicit,
                            blink::mojom::RpMode::kPassive);

  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnAccountSelected(*accounts[0], *idp_list[0], CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
  histogram_tester_->ExpectTotalCount(
      "Blink.FedCm.ChooseAnAccountSelected.Desktop", 0);
  EXPECT_FALSE(account_selection_view_->is_choose_an_account_);
}

TEST_F(FedCmAccountSelectionViewDesktopTest,
       MultiIdpWithSingleReturningAccount) {
  std::vector<IdentityProviderDataPtr> idp_list = {
      CreateIdentityProviderData(), CreateIdentityProviderData(),
      CreateIdentityProviderData(/*has_login_status_mismatch=*/true)};
  std::vector<IdentityRequestAccountPtr> accounts = {
      CreateAccount(idp_list[0], LoginState::kSignIn, LoginState::kSignIn),
      CreateAccount(idp_list[1], LoginState::kSignUp, LoginState::kSignUp,
                    kAccountId2)};
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMultiIdp(idp_list, accounts, SignInMode::kExplicit,
                            blink::mojom::RpMode::kPassive);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_FALSE(account_selection_view_->is_choose_an_account_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kSingleReturningAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  // Simulate 'Choose an account' button being clicked.
  observer->OnChooseAnAccountClicked();
  EXPECT_TRUE(account_selection_view_->show_back_button_);
  EXPECT_TRUE(account_selection_view_->is_choose_an_account_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1, kAccountId2));

  // Simulate second account picked.
  observer->OnAccountSelected(*accounts[1], *idp_list[1], CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId2));
  EXPECT_FALSE(account_selection_view_->is_choose_an_account_);

  // Simulate 'back' clicked: now in 'choose an account'.
  observer->OnBackButtonClicked();
  EXPECT_TRUE(account_selection_view_->show_back_button_);
  EXPECT_TRUE(account_selection_view_->is_choose_an_account_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1, kAccountId2));

  // Simulate 'back' clicked again: now in 'single returning account'.
  observer->OnBackButtonClicked();
  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_FALSE(account_selection_view_->is_choose_an_account_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kSingleReturningAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  // Simulate account picked
  observer->OnAccountSelected(*accounts[0], *idp_list[0], CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
  histogram_tester_->ExpectUniqueSample(
      "Blink.FedCm.ChooseAnAccountSelected.Desktop", 1, 1);
}

// Tests that closing the dialog when the single returning account UI is shown
// does not cause a crash.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       MultiIdpWithSingleReturningAccountClose) {
  std::vector<IdentityProviderDataPtr> idp_list = {
      CreateIdentityProviderData(), CreateIdentityProviderData(),
      CreateIdentityProviderData(/*has_login_status_mismatch=*/true)};
  std::vector<IdentityRequestAccountPtr> accounts = {
      CreateAccount(idp_list[0], LoginState::kSignIn, LoginState::kSignIn),
      CreateAccount(idp_list[1], LoginState::kSignUp, LoginState::kSignUp,
                    kAccountId2)};
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMultiIdp(idp_list, accounts, SignInMode::kExplicit,
                            blink::mojom::RpMode::kPassive);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kSingleReturningAccount,
            account_selection_view_->sheet_type_);

  // Simulate the dialog being closed.
  observer->OnCloseButtonClicked(CreateMouseEvent());
}

// Tests that if a pop-up window is opened in active mode, closing the
// pop-up window triggers the dismiss callback.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       ActiveModePopupCloseTriggersDismissCallback) {
  // Initialize a controller but do not trigger any dialogs.
  auto controller = std::make_unique<TestFedCmAccountSelectionView>(
      delegate_.get(), account_selection_view_.get());
  EXPECT_FALSE(dialog_widget_->IsVisible());

  // Emulate user clicking on a button to sign in with an IDP via active mode.
  auto popup_window = std::make_unique<MockFedCmModalDialogView>(
      test_web_contents_.get(), controller.get());
  EXPECT_CALL(*popup_window, ShowPopupWindow).Times(1);
  controller->SetIdpSigninPopupWindowForTesting(std::move(popup_window));
  controller->ShowModalDialog(GURL(u"https://example.com"),
                              blink::mojom::RpMode::kActive);

  // Emulate user closing the pop-up window.
  controller->OnPopupWindowDestroyed();

  // Widget should be dismissed.
  StubAccountSelectionViewDelegate* delegate =
      static_cast<StubAccountSelectionViewDelegate*>(delegate_.get());
  EXPECT_EQ(delegate->GetDismissReason(), DismissReason::kOther);

  // Reset the widget explicitly since no widget was shown. Otherwise, the test
  // will complain that a widget is still open.
  dialog_widget_.reset();
}

TEST_F(FedCmAccountSelectionViewDesktopTest, MultiIdpMismatchAndShow) {
  std::vector<IdentityProviderDataPtr> idp_list = {
      CreateIdentityProviderData(),
      CreateIdentityProviderData(/*has_login_status_mismatch=*/true)};
  std::vector<IdentityRequestAccountPtr> accounts = {
      CreateAccount(idp_list[0])};
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMultiIdp(idp_list, accounts, SignInMode::kExplicit,
                            blink::mojom::RpMode::kPassive);

  // Emulate user clicking on "Continue" button in the mismatch dialog.
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());
  observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl), CreateMouseEvent());
  CreateAndShowPopupWindow(*controller);
  controller->CloseModalDialog();

  // The backend will pass the accounts reordered.
  std::vector<IdentityRequestAccountPtr> new_accounts = {CreateAccount(
      idp_list[1], LoginState::kSignUp, LoginState::kSignUp, kAccountId2)};
  std::vector<IdentityRequestAccountPtr> all_accounts = new_accounts;
  all_accounts.emplace_back(accounts[0]);

  Show(*controller, all_accounts, SignInMode::kExplicit,
       blink::mojom::RpMode::kActive, new_accounts);

  // Should show only the new account, with a back button for other account.
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId2));
  ASSERT_TRUE(account_selection_view_->show_back_button_);

  observer->OnBackButtonClicked();
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId2, kAccountId1));
  EXPECT_FALSE(account_selection_view_->show_back_button_);

  EXPECT_EQ(1u, controller->num_dialogs_);
}

// Tests that if a single account chooser is opened in active mode mode,
// selecting an account shows the request permission sheet. Then, confirming the
// account on the request permission sheet shows the verifying sheet.
TEST_F(FedCmAccountSelectionViewDesktopTest, SingleAccountFlowModal) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
      accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kActive);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kRequestPermission,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
}

// Tests that if a multiple account chooser is opened in active mode mode,
// selecting an account shows the request permission sheet. Then, confirming the
// account on the request permission sheet shows the verifying sheet.
TEST_F(FedCmAccountSelectionViewDesktopTest, MultipleAccountFlowModal) {
  accounts_ = CreateAccounts(
      {{kAccountId1, LoginState::kSignUp}, {kAccountId2, LoginState::kSignUp}},
      idp_data_);
  std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
      accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kActive);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1, kAccountId2));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kRequestPermission,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
}

// Tests that if a single account chooser is opened in active mode mode,
// selecting a returning account shows the verifying sheet.
TEST_F(FedCmAccountSelectionViewDesktopTest, SingleAccountFlowReturningModal) {
  accounts_ = {
      CreateAccount(idp_data_, LoginState::kSignIn, LoginState::kSignIn)};
  std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
      accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kActive);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
}

// Tests that if a multiple account chooser is opened in active mode mode,
// selecting a returning account shows the verifying sheet.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       MultipleAccountFlowReturningModal) {
  accounts_ = CreateAccounts(
      {{kAccountId1, LoginState::kSignIn}, {kAccountId2, LoginState::kSignUp}},
      idp_data_);
  std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
      accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kActive);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1, kAccountId2));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
}

// Tests that if a single account chooser is opened in active mode mode,
// clicking the back button in the request permission dialog returns the user to
// the single account chooser.
TEST_F(FedCmAccountSelectionViewDesktopTest, SingleAccountFlowBackModal) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
      accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kActive);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_TRUE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kRequestPermission,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnBackButtonClicked();
  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_TRUE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kRequestPermission,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
}

// Tests that if a multiple account chooser is opened in active mode mode,
// clicking the back button in the request permission dialog returns the user to
// the multiple account chooser.
TEST_F(FedCmAccountSelectionViewDesktopTest, MultipleAccountFlowBackModal) {
  accounts_ = CreateAccounts(
      {{kAccountId1, LoginState::kSignUp}, {kAccountId2, LoginState::kSignUp}},
      idp_data_);
  std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
      accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kActive);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1, kAccountId2));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_TRUE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kRequestPermission,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnBackButtonClicked();
  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1, kAccountId2));

  observer->OnAccountSelected(*accounts_[1], *idp_data_, CreateMouseEvent());
  EXPECT_TRUE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kRequestPermission,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId2));

  observer->OnAccountSelected(*accounts_[1], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId2));
}

// Tests that auto re-authn works in button mode.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       AutoReauthnSingleAccountFlowModal) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowLoadingDialog();

  EXPECT_CALL(*controller, MaybeResetAccountSelectionView).Times(0);
  accounts_ = {
      CreateAccount(idp_data_, LoginState::kSignIn, LoginState::kSignIn)};
  Show(*controller, accounts_, SignInMode::kAuto,
       blink::mojom::RpMode::kActive);

  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
}

// Tests that the user can dismiss the loading modal.
TEST_F(FedCmAccountSelectionViewDesktopTest, DismissLoadingModal) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowLoadingDialog();
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());
  observer->OnCloseButtonClicked(CreateMouseEvent());
}

// Tests that the loading modal is not hidden when a pop-up window is displayed.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       ActiveModeLoadingModalNotHiddenDuringLoginToIdP) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowLoadingDialog();
  EXPECT_TRUE(dialog_widget_->IsVisible());

  // Emulate user clicking on a button to sign in with an IDP via active mode.
  auto popup_window = std::make_unique<MockFedCmModalDialogView>(
      test_web_contents_.get(), controller.get());
  EXPECT_CALL(*popup_window, ShowPopupWindow).Times(1);
  controller->SetIdpSigninPopupWindowForTesting(std::move(popup_window));
  controller->ShowModalDialog(GURL(u"https://example.com"),
                              blink::mojom::RpMode::kActive);

  EXPECT_TRUE(dialog_widget_->IsVisible());
}

// Tests that opening an IDP sign-in pop-up during the loading modal, then
// closing the pop-up, does not crash. (This simulates the user triggering a
// active mode, then an IDP sign-in pop-up shows up because the user is logged
// out)
TEST_F(FedCmAccountSelectionViewDesktopTest,
       CloseIdpSigninPopupDuringLoadingState) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowLoadingDialog();
  delegate_->SetOnDismissClosure(
      base::BindOnce(&std::unique_ptr<TestFedCmAccountSelectionView>::reset,
                     base::Unretained(&controller), nullptr));

  CreateAndShowPopupWindow(*controller);
  // This should not crash.
  controller->CloseModalDialog();
}

// Tests that opening a popup after a verifying sheet, then closing the popup,
// notifies the observer.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       UserClosingPopupAfterVerifyingSheetShouldNotify) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts_, SignInMode::kExplicit);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);

  CreateAndShowPopupWindow(*controller);
  controller->popup_window_->ClosePopupWindow();
  EXPECT_EQ(delegate_->GetDismissReason(), DismissReason::kOther);
}

// Tests that opening a popup after a verifying sheet, then closing the popup
// programmatically, does not notify the observer.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       CodeClosingPopupAfterVerifyingSheetShouldNotNotify) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts_, SignInMode::kExplicit);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);

  CreateAndShowPopupWindow(*controller);
  controller->CloseModalDialog();
  EXPECT_FALSE(delegate_->GetDismissReason().has_value());
}

// Tests that if the dialog skips requesting permission, the verifying sheet is
// shown.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       SkipRequestPermissionShowsVerifying) {
  idp_data_->disclosure_fields = {};
  std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
      accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kPassive);
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_FALSE(account_selection_view_->show_back_button_);
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
}

// Tests that if IDP supports add account, the correct sheet type is shown
// depending on the number of accounts and the rp mode.
TEST_F(FedCmAccountSelectionViewDesktopTest, SupportAddAccount) {
  idp_data_->idp_metadata.supports_add_account = true;
  std::vector<IdentityRequestAccountPtr> multiple_accounts = CreateAccounts(
      {{kAccountId1, LoginState::kSignIn}, {kAccountId2, LoginState::kSignIn}},
      idp_data_);
  {
    // Single account passive mode.
    std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
        accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kPassive);
    EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
              account_selection_view_->sheet_type_);
  }
  {
    // Multiple account passive mode.
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShow(multiple_accounts, SignInMode::kExplicit,
                      blink::mojom::RpMode::kPassive);
    EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
              account_selection_view_->sheet_type_);
  }
  {
    // Single account active mode.
    std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
        accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kActive);
    EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
              account_selection_view_->sheet_type_);
  }
  {
    // Multiple account active mode.
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShow(multiple_accounts, SignInMode::kExplicit,
                      blink::mojom::RpMode::kActive);
    EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
              account_selection_view_->sheet_type_);
  }
}

// Tests that when adding accounts is supported, the back button is shown even
// if there is a single account after logging in.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       IdpSigninStatusMismatchToAccountChooserWithSupportAddAccount) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowMismatchDialog();
  AccountSelectionViewBase::Observer* observer =
      static_cast<AccountSelectionViewBase::Observer*>(controller.get());

  EXPECT_EQ(TestAccountSelectionView::SheetType::kFailure,
            account_selection_view_->sheet_type_);

  idp_data_->idp_metadata.supports_add_account = true;

  Show(*controller, accounts_, SignInMode::kExplicit,
       blink::mojom::RpMode::kPassive, new_accounts_);

  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  ASSERT_TRUE(account_selection_view_->show_back_button_);
  observer->OnBackButtonClicked();
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);

  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kConfirmAccount,
            account_selection_view_->sheet_type_);
  observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  EXPECT_EQ(TestAccountSelectionView::SheetType::kVerifying,
            account_selection_view_->sheet_type_);

  EXPECT_EQ(1u, controller->num_dialogs_);
}

// Tests that the correct account chooser result metrics are recorded.
TEST_F(FedCmAccountSelectionViewDesktopTest, AccountChooserResultMetric) {
  auto CheckForSampleAndReset(
      [&](FedCmAccountSelectionView::AccountChooserResult result) {
        histogram_tester_->ExpectUniqueSample(
            "Blink.FedCm.Button.AccountChooserResult", static_cast<int>(result),
            1);
        histogram_tester_ = std::make_unique<base::HistogramTester>();
      });

  // The AccountChooserResult metric is recorded in OnDismiss, therefore, we
  // check for the histogram after the TestFedCmAccountSelectionView goes out
  // of scope.
  {
    // User clicks on account row.
    std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
        accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kActive);
    AccountSelectionViewBase::Observer* observer =
        static_cast<AccountSelectionViewBase::Observer*>(controller.get());
    observer->OnAccountSelected(*accounts_[0], *idp_data_, CreateMouseEvent());
  }
  CheckForSampleAndReset(
      FedCmAccountSelectionView::AccountChooserResult::kAccountRow);

  {
    // User clicks on cancel button.
    std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
        accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kActive);
    AccountSelectionViewBase::Observer* observer =
        static_cast<AccountSelectionViewBase::Observer*>(controller.get());
    observer->OnCloseButtonClicked(CreateMouseEvent());
  }
  CheckForSampleAndReset(
      FedCmAccountSelectionView::AccountChooserResult::kCancelButton);

  {
    // User clicks on use other account button.
    std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
        accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kActive);
    AccountSelectionViewBase::Observer* observer =
        static_cast<AccountSelectionViewBase::Observer*>(controller.get());
    observer->OnLoginToIdP(GURL(kConfigUrl), GURL(kLoginUrl),
                           CreateMouseEvent());
  }
  CheckForSampleAndReset(
      FedCmAccountSelectionView::AccountChooserResult::kUseOtherAccountButton);

  {
    // User closes the tab or window.
    std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
        accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kActive);
  }
  CheckForSampleAndReset(
      FedCmAccountSelectionView::AccountChooserResult::kTabClosed);

  {
    // Returning user signing in via IDP sign-in pop-up when signed-out should
    // record a sample.
    std::vector<IdentityRequestAccountPtr> accounts = {
        CreateAccount(idp_data_, LoginState::kSignIn, LoginState::kSignIn)};
    std::vector<IdentityRequestAccountPtr> new_accounts = accounts;
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowAccountsModalThroughPopupWindow(accounts, new_accounts);
    // User is shown the account chooser.
    EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
              account_selection_view_->sheet_type_);
    AccountSelectionViewBase::Observer* observer =
        static_cast<AccountSelectionViewBase::Observer*>(controller.get());
    observer->OnAccountSelected(*new_accounts[0], *idp_data_,
                                CreateMouseEvent());
  }
  CheckForSampleAndReset(
      FedCmAccountSelectionView::AccountChooserResult::kAccountRow);

  {
    // Non-returning user signing in via IDP sign-in pop-up should not record a
    // sample.
    std::unique_ptr<TestFedCmAccountSelectionView> controller =
        CreateAndShowAccountsModalThroughPopupWindow(accounts_, new_accounts_);

    // User is shown the request permission dialog, skipping the account
    // chooser.
    EXPECT_EQ(TestAccountSelectionView::SheetType::kRequestPermission,
              account_selection_view_->sheet_type_);
  }
  histogram_tester_->ExpectTotalCount("Blink.FedCm.Button.AccountChooserResult",
                                      0);
  {
    // passive mode should not record a sample.
    std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
        accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kPassive);
  }
  histogram_tester_->ExpectTotalCount("Blink.FedCm.Button.AccountChooserResult",
                                      0);
}

// Tests that for active modes, going from an accounts dialog to an error dialog
// resets the account selection view. This is needed to switch from modal to
// bubble, since the error UI does not have a modal equivalent.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       AccountsToErrorActiveModeResetsView) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller = CreateAndShow(
      accounts_, SignInMode::kExplicit, blink::mojom::RpMode::kActive);

  EXPECT_CALL(*controller, MaybeResetAccountSelectionView).Times(1);
  controller->ShowErrorDialog(
      kTopFrameEtldPlusOne, kIdpEtldPlusOne, blink::mojom::RpContext::kSignIn,
      blink::mojom::RpMode::kActive, content::IdentityProviderMetadata(),
      /*error=*/std::nullopt);
}

// Tests that for active modes, going from a loading dialog to an accounts
// dialog does not reset the account selection view. This is important because
// the accounts dialog reuses the header from the loading dialog.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       LoadingToAccountsActiveModeReusesView) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowLoadingDialog();

  EXPECT_CALL(*controller, MaybeResetAccountSelectionView).Times(0);
  Show(*controller, accounts_, SignInMode::kExplicit,
       blink::mojom::RpMode::kActive);
}

// Tests that resizing web contents would update the dialog visibility depending
// on whether the dialog can fit within the web contents.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       ResizeWebContentsChangesDialogVisibility) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts_, SignInMode::kExplicit);
  EXPECT_TRUE(dialog_widget_->IsVisible());

  // Emulate that the web contents is too small to fit the dialog, hiding the
  // dialog.
  account_selection_view_->can_fit_in_web_contents_ = false;
  controller->PrimaryMainFrameWasResized(/*width_changed=*/true);
  EXPECT_FALSE(dialog_widget_->IsVisible());

  // Emulate that the web contents is big enough to fit the dialog, showing the
  // dialog.
  account_selection_view_->can_fit_in_web_contents_ = true;
  controller->PrimaryMainFrameWasResized(/*width_changed=*/true);
  EXPECT_TRUE(dialog_widget_->IsVisible());
}

// Tests that resizing web contents in different web contents visibility
// scenarios would update the dialog visibility correctly depending on whether
// the dialog can fit within the web contents or whether the web contents where
// the dialog is contained is visible.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       ResizeWebContentsWithWindowVisibilityChanges) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts_, SignInMode::kExplicit);

  // Emulate user changing tabs, hiding the dialog.
  controller->OnTabBackgrounded();
  EXPECT_FALSE(dialog_widget_->IsVisible());

  // Emulate user resizing the window, making the web contents too small to fit
  // the dialog. The dialog should remain hidden.
  account_selection_view_->can_fit_in_web_contents_ = false;
  controller->PrimaryMainFrameWasResized(/*width_changed=*/true);
  EXPECT_FALSE(dialog_widget_->IsVisible());

  // Emulate user changing back to the tab containing the dialog. The dialog
  // should remain hidden because the web contents is still too small to fit the
  // dialog.
  controller->OnTabForegrounded();
  EXPECT_FALSE(dialog_widget_->IsVisible());

  // Emulate user resizing the window, making the web contents is big enough to
  // fit the dialog. The dialog should now be visible.
  account_selection_view_->can_fit_in_web_contents_ = true;
  controller->PrimaryMainFrameWasResized(/*width_changed=*/true);
  EXPECT_TRUE(dialog_widget_->IsVisible());

  // Emulate user resizing the window, making the web contents too small to fit
  // the dialog. The dialog should be hidden.
  account_selection_view_->can_fit_in_web_contents_ = false;
  controller->PrimaryMainFrameWasResized(/*width_changed=*/false);
  EXPECT_FALSE(dialog_widget_->IsVisible());

  // Emulate user changing tabs, the dialog should remain hidden.
  controller->OnTabBackgrounded();
  EXPECT_FALSE(dialog_widget_->IsVisible());

  // Emulate user resizing the window, making the web contents big enough to fit
  // the dialog. The dialog should remain hidden because the user is on a
  // different tab.
  account_selection_view_->can_fit_in_web_contents_ = true;
  controller->PrimaryMainFrameWasResized(/*width_changed=*/false);
  EXPECT_FALSE(dialog_widget_->IsVisible());

  // Emulate user changing back to the tab containing the dialog. The dialog
  // should now be visible.
  controller->OnTabForegrounded();
  EXPECT_TRUE(dialog_widget_->IsVisible());
}

// Tests that changing visibility from hidden to visible, also updates the
// dialog position. This is needed in case the web contents was resized while
// hidden.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       VisibilityChangesUpdatesDialogPosition) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts_, SignInMode::kExplicit);

  // Emulate user changing tabs, hiding the dialog.
  controller->OnTabBackgrounded();
  EXPECT_FALSE(account_selection_view_->dialog_position_updated_);
  EXPECT_FALSE(dialog_widget_->IsVisible());

  // Emulate user changing back to the tab containing the dialog, updating the
  // dialog position.
  controller->OnTabForegrounded();
  EXPECT_TRUE(account_selection_view_->dialog_position_updated_);
  EXPECT_TRUE(dialog_widget_->IsVisible());
}

// Test that the fields API (request_permission={}) correctly hides the
// disclosure UI after logging in through the popup when logged out.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       RequestPermissionFalseAndNewIdpDataDisclosureText) {
  idp_data_->disclosure_fields = {};
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShowAccountsModalThroughPopupWindow(accounts_, new_accounts_);

  // The account chooser UI is NOT skipped if user signed in from LOADING state.
  EXPECT_EQ(TestAccountSelectionView::SheetType::kAccountPicker,
            account_selection_view_->sheet_type_);
  EXPECT_THAT(account_selection_view_->account_ids_,
              testing::ElementsAre(kAccountId1));
  EXPECT_FALSE(account_selection_view_->show_back_button_);
  // This should use the multi account picker, which does not show the
  // disclosure text.
  EXPECT_EQ(FedCmAccountSelectionView::State::MULTI_ACCOUNT_PICKER,
            controller->state_);
}

// Tests that a loading state pop-up opened during a active mode sets a custom Y
// position. This is so that the pop-up covers the loading modal dialog to
// direct user attention towards the pop-up.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       ActiveModeLoadingStatePopupSetsCustomYPosition) {
  auto controller = CreateAndShowLoadingDialog();

  // Open loading state pop-up and expect it to call `SetCustomYPosition`.
  auto popup_window = std::make_unique<MockFedCmModalDialogView>(
      test_web_contents_.get(), controller.get());
  EXPECT_CALL(*popup_window, ShowPopupWindow).Times(1);
  EXPECT_CALL(*popup_window, SetCustomYPosition).Times(1);
  controller->SetIdpSigninPopupWindowForTesting(std::move(popup_window));
  controller->ShowModalDialog(GURL(u"https://example.com"),
                              blink::mojom::RpMode::kActive);

  // Reset the widget explicitly since no widget was shown. Otherwise, the test
  // will complain that a widget is still open.
  dialog_widget_.reset();
}

// Tests that a use other account pop-up opened during a active mode does not
// set a custom Y position.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       ActiveModeUseOtherAccountPopupDoesNotSetCustomYPosition) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts_, SignInMode::kExplicit);

  // Open use other account pop-up and expect it to call `SetCustomYPosition`.
  auto popup_window = std::make_unique<MockFedCmModalDialogView>(
      test_web_contents_.get(), controller.get());
  EXPECT_CALL(*popup_window, ShowPopupWindow).Times(1);
  EXPECT_CALL(*popup_window, SetCustomYPosition).Times(0);
  controller->SetIdpSigninPopupWindowForTesting(std::move(popup_window));
  controller->ShowModalDialog(GURL(u"https://example.com"),
                              blink::mojom::RpMode::kActive);

  // Reset the widget explicitly since no widget was shown. Otherwise, the test
  // will complain that a widget is still open.
  dialog_widget_.reset();
}

// Tests that a loading state pop-up opened during a active mode should call
// `SetActiveModeSheetType`.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       ActiveModeLoadingStatePopupSetsActiveModeSheetType) {
  auto controller = CreateAndShowLoadingDialog();

  // Open loading state pop-up and expect it to call `SetActiveModeSheetType`.
  auto popup_window = std::make_unique<MockFedCmModalDialogView>(
      test_web_contents_.get(), controller.get());
  EXPECT_CALL(*popup_window, ShowPopupWindow).Times(1);
  EXPECT_CALL(*popup_window, SetActiveModeSheetType).Times(1);
  controller->SetIdpSigninPopupWindowForTesting(std::move(popup_window));
  controller->ShowModalDialog(GURL(u"https://example.com"),
                              blink::mojom::RpMode::kActive);

  // Reset the widget explicitly since no widget was shown. Otherwise, the test
  // will complain that a widget is still open.
  dialog_widget_.reset();
}

// Tests that a use other account pop-up opened during a active mode should call
// `SetActiveModeSheetType`.
TEST_F(FedCmAccountSelectionViewDesktopTest,
       ActiveModeUseOtherAccountPopupSetsActiveModeSheetType) {
  std::unique_ptr<TestFedCmAccountSelectionView> controller =
      CreateAndShow(accounts_, SignInMode::kExplicit);

  // Open use other account pop-up and expect it to call
  // `SetActiveModeSheetType`.
  auto popup_window = std::make_unique<MockFedCmModalDialogView>(
      test_web_contents_.get(), controller.get());
  EXPECT_CALL(*popup_window, ShowPopupWindow).Times(1);
  EXPECT_CALL(*popup_window, SetActiveModeSheetType).Times(1);
  controller->SetIdpSigninPopupWindowForTesting(std::move(popup_window));
  controller->ShowModalDialog(GURL(u"https://example.com"),
                              blink::mojom::RpMode::kActive);

  // Reset the widget explicitly since no widget was shown. Otherwise, the test
  // will complain that a widget is still open.
  dialog_widget_.reset();
}
