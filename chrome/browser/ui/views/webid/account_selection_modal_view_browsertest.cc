// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_modal_view.h"

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/webid/account_selection_view_test_base.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"

class AccountSelectionModalViewTest : public DialogBrowserTest,
                                      public AccountSelectionViewTestBase {
 public:
  AccountSelectionModalViewTest() {
    test_shared_url_loader_factory_ =
      base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
          &test_url_loader_factory_);
  }
  AccountSelectionModalViewTest(const AccountSelectionModalViewTest&) =
      delete;
  AccountSelectionModalViewTest& operator=(
      const AccountSelectionModalViewTest&) = delete;
  ~AccountSelectionModalViewTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    dialog_ = new AccountSelectionModalView(
        kTopFrameETLDPlusOne, kIdpETLDPlusOne,
        blink::mojom::RpContext::kSignIn,
        browser()->tab_strip_model()->GetActiveWebContents(),
        shared_url_loader_factory(), /*observer=*/nullptr,
        /*widget_observer=*/nullptr);
  }

 protected:
  void CreateAccountSelectionModal() {
    ShowUi("");
  }

  void CreateSingleAccountPicker(
      bool show_back_button,
      const content::IdentityRequestAccount& account,
      const content::IdentityProviderMetadata& idp_metadata,
      const std::string& terms_of_service_url,
      bool show_auto_reauthn_checkbox = false,
      bool exclude_iframe = true) {
    CreateAccountSelectionModal();
    IdentityProviderDisplayData idp_data(
        kIdpETLDPlusOne, idp_metadata,
        CreateTestClientMetadata(terms_of_service_url), {account},
        /*request_permission=*/true, /*has_login_status_mismatch=*/false);
    dialog_->ShowSingleAccountConfirmDialog(
        kTopFrameETLDPlusOne, /*iframe_for_display=*/std::nullopt, account,
        idp_data, show_back_button);
  }

  void CreateMultiAccountPicker(
      const std::vector<std::string>& account_suffixes,
      bool supports_add_account = false) {
    std::vector<content::IdentityRequestAccount> account_list =
        CreateTestIdentityRequestAccounts(
            account_suffixes,
            content::IdentityRequestAccount::LoginState::kSignUp);

    CreateAccountSelectionModal();
    std::vector<IdentityProviderDisplayData> idp_data;
    content::IdentityProviderMetadata metadata;
    metadata.supports_add_account = supports_add_account;
    idp_data.emplace_back(
        kIdpETLDPlusOne, metadata,
        CreateTestClientMetadata(/*terms_of_service_url=*/""), account_list,
        /*request_permission=*/true, /*has_login_status_mismatch=*/false);
    dialog_->ShowMultiAccountPicker(idp_data);
  }

  void CreateRequestPermissionDialog(
      const content::IdentityRequestAccount& account,
      const content::IdentityProviderMetadata& idp_metadata,
      const std::string& terms_of_service_url) {
    CreateAccountSelectionModal();
    IdentityProviderDisplayData idp_data(
        kIdpETLDPlusOne, idp_metadata,
        CreateTestClientMetadata(terms_of_service_url), {account},
        /*request_permission=*/true, /*has_login_status_mismatch=*/false);
    dialog_->ShowRequestPermissionDialog(kTopFrameETLDPlusOne, account,
                                         idp_data);
  }

  void ShowVerifyingSheet() {
    const std::string kAccountSuffix = "suffix";
    content::IdentityRequestAccount account(CreateTestIdentityRequestAccount(
        kAccountSuffix, content::IdentityRequestAccount::LoginState::kSignUp));
    IdentityProviderDisplayData idp_data(
        kIdpETLDPlusOne, content::IdentityProviderMetadata(),
        CreateTestClientMetadata(/*terms_of_service_url=*/""), {account},
        /*request_permission=*/true, /*has_login_status_mismatch=*/false);
    dialog_->ShowVerifyingSheet(account, idp_data, kTitleSignIn);
  }

  void CreateLoadingDialog() {
    CreateAccountSelectionModal();
    dialog_->ShowLoadingDialog();
  }

  void PerformHeaderChecks(views::View* header,
                           const std::u16string& expected_title,
                           const std::u16string& expected_body) {
    // Perform some basic dialog checks.
    EXPECT_FALSE(dialog()->ShouldShowCloseButton());
    EXPECT_FALSE(dialog()->ShouldShowWindowTitle());

    // Default buttons should not be shown.
    EXPECT_FALSE(dialog()->GetOkButton());
    EXPECT_FALSE(dialog()->GetCancelButton());

    // Order: Brand icon, title, potentially body
    std::vector<std::string> expected_class_names = {"BrandIconImageView",
                                                     "Label"};
    if (!expected_body.empty()) {
      expected_class_names.push_back("Label");
    }
    EXPECT_THAT(GetChildClassNames(header),
                testing::ElementsAreArray(expected_class_names));

    std::vector<raw_ptr<views::View, VectorExperimental>> header_children =
        header->children();
    ASSERT_EQ(header_children.size(), expected_class_names.size());

    // Check title text.
    views::Label* title_view = static_cast<views::Label*>(header_children[1]);
    ASSERT_TRUE(title_view);
    EXPECT_EQ(title_view->GetText(), expected_title);

    if (expected_body.empty()) {
      return;
    }

    // Check body text.
    views::Label* body_view = static_cast<views::Label*>(header_children[2]);
    ASSERT_TRUE(body_view);
    EXPECT_EQ(body_view->GetText(), expected_body);
  }

  void CheckButtonRow(views::View* button_row,
                      bool expect_continue_button,
                      bool expect_add_account_button) {
    std::vector<raw_ptr<views::View, VectorExperimental>> button_row_children =
        button_row->children();

    // Cancel button is always expected.
    size_t num_expected_buttons = 1u;

    if (expect_continue_button) {
      ++num_expected_buttons;
    }

    if (expect_add_account_button) {
      ++num_expected_buttons;
    }

    ASSERT_EQ(button_row_children.size(), num_expected_buttons);

    size_t button_index = 0;
    if (expect_add_account_button) {
      std::vector<raw_ptr<views::View, VectorExperimental>>
          add_account_container_children =
              button_row_children[button_index++]->children();
      ASSERT_EQ(add_account_container_children.size(), 1u);
      views::MdTextButton* add_account_button =
          static_cast<views::MdTextButton*>(add_account_container_children[0]);
      ASSERT_TRUE(add_account_button);
      EXPECT_EQ(add_account_button->GetText(), u"Use a different account");
    }

    views::MdTextButton* cancel_button =
        static_cast<views::MdTextButton*>(button_row_children[button_index++]);
    ASSERT_TRUE(cancel_button);
    EXPECT_EQ(cancel_button->GetText(), u"Cancel");

    if (expect_continue_button) {
      views::MdTextButton* continue_button =
          static_cast<views::MdTextButton*>(button_row_children[button_index]);
      ASSERT_TRUE(continue_button);
      EXPECT_EQ(continue_button->GetText(), u"Continue");
    }
  }

  void TestSingleAccount(const std::u16string& expected_title,
                         const std::u16string& expected_body,
                         bool supports_add_account = false) {
    const std::string kAccountSuffix = "suffix";
    content::IdentityRequestAccount account(CreateTestIdentityRequestAccount(
        kAccountSuffix, content::IdentityRequestAccount::LoginState::kSignUp));
    content::IdentityProviderMetadata idp_metadata;
    idp_metadata.supports_add_account = supports_add_account;
    CreateSingleAccountPicker(
        /*show_back_button=*/false, account, idp_metadata, kTermsOfServiceUrl);

    std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    ASSERT_EQ(children.size(), 3u);
    PerformHeaderChecks(children[0], expected_title, expected_body);

    views::View* account_rows = children[1];
    ASSERT_EQ(account_rows->children().size(), 1u);

    size_t accounts_index = 0;
    CheckHoverableAccountRows(account_rows->children(), {kAccountSuffix},
                              accounts_index);
    CheckButtonRow(children[2], /*expect_continue_button=*/true,
                   supports_add_account);
  }

  void TestMultipleAccounts(const std::u16string& expected_title,
                            const std::u16string& expected_body,
                            bool supports_add_account = false) {
    const std::vector<std::string> kAccountSuffixes = {"0", "1", "2"};
    CreateMultiAccountPicker(kAccountSuffixes, supports_add_account);

    std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    ASSERT_EQ(children.size(), 3u);
    PerformHeaderChecks(children[0], expected_title, expected_body);

    views::ScrollView* scroller = static_cast<views::ScrollView*>(children[1]);
    ASSERT_FALSE(scroller->children().empty());
    views::View* wrapper = scroller->children()[0];
    ASSERT_FALSE(wrapper->children().empty());
    views::View* contents = wrapper->children()[0];

    views::BoxLayout* layout_manager =
        static_cast<views::BoxLayout*>(contents->GetLayoutManager());
    EXPECT_TRUE(layout_manager);
    EXPECT_EQ(layout_manager->GetOrientation(),
              views::BoxLayout::Orientation::kVertical);
    std::vector<raw_ptr<views::View, VectorExperimental>> accounts =
        contents->children();

    size_t accounts_index = 0;
    CheckHoverableAccountRows(accounts, kAccountSuffixes, accounts_index);
    CheckButtonRow(children[2], /*expect_continue_button=*/false,
                   supports_add_account);
  }

  void TestRequestPermission(const std::u16string& expected_title,
                             const std::u16string& expected_body = u"") {
    const std::string kAccountSuffix = "suffix";
    content::IdentityRequestAccount account(CreateTestIdentityRequestAccount(
        kAccountSuffix, content::IdentityRequestAccount::LoginState::kSignUp));
    CreateRequestPermissionDialog(account, content::IdentityProviderMetadata(),
                                  kTermsOfServiceUrl);

    std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    // Order: Header, single account chooser, button row
    ASSERT_EQ(children.size(), 3u);
    PerformHeaderChecks(children[0], expected_title, expected_body);

    views::View* single_account_chooser = children[1];
    // Order: Account row, disclosure text
    ASSERT_EQ(single_account_chooser->children().size(), 2u);

    CheckNonHoverableAccountRow(single_account_chooser->children()[0],
                                kAccountSuffix);
    CheckDisclosureText(single_account_chooser->children()[1],
                        /*expect_terms_of_service=*/true,
                        /*expect_privacy_policy=*/true);
    CheckButtonRow(children[2], /*expect_continue_button=*/true,
                   /*expect_add_account_button=*/false);
  }

  void TestVerifyingSheet(const std::u16string& expected_title,
                          const std::u16string& expected_body = u"",
                          bool has_multiple_accounts = false) {
    // Order: Progress bar, header, account chooser, button row
    std::vector<std::string> expected_class_names = {
        "ProgressBar", "View", has_multiple_accounts ? "ScrollView" : "View",
        "View"};
    EXPECT_THAT(GetChildClassNames(dialog()),
                testing::ElementsAreArray(expected_class_names));

    PerformHeaderChecks(dialog()->children()[1], expected_title, expected_body);

    std::vector<raw_ptr<views::View, VectorExperimental>> account_chooser =
        dialog()->children()[2]->children();
    // Based on the modal type, there could be different items from the
    // account_chooser section. e.g. accounts, disclosure text, scroll view etc.
    // and all of them should be disabled.
    for (const auto& item : account_chooser) {
      ASSERT_FALSE(item->GetEnabled());
    }

    std::vector<raw_ptr<views::View, VectorExperimental>> button_row =
        dialog()->children()[3]->children();
    for (const auto& button : button_row) {
      auto* text_button = static_cast<views::MdTextButton*>(button);
      ASSERT_TRUE(!text_button->GetEnabled() ||
                  text_button->GetText() ==
                      l10n_util::GetStringUTF16(IDS_CANCEL));
    }
  }

  void TestLoadingDialog(const std::u16string& expected_title,
                         const std::u16string& expected_body = u"") {
    CreateLoadingDialog();
    // Order: Progress bar, header, placeholder account chooser, button row
    std::vector<std::string> expected_class_names = {"ProgressBar", "View",
                                                     "View", "View"};
    EXPECT_THAT(GetChildClassNames(dialog()),
                testing::ElementsAreArray(expected_class_names));

    PerformHeaderChecks(dialog()->children()[1], expected_title, expected_body);

    std::vector<raw_ptr<views::View, VectorExperimental>>
        placeholder_account_chooser = dialog()->children()[2]->children();
    // Order: Placeholder account image, placeholder text column
    ASSERT_EQ(placeholder_account_chooser.size(), 2u);

    std::vector<raw_ptr<views::View, VectorExperimental>>
        placeholder_text_column = placeholder_account_chooser[1]->children();
    // Order: Placeholder account name, placeholder account email
    ASSERT_EQ(placeholder_text_column.size(), 2u);

    std::vector<raw_ptr<views::View, VectorExperimental>> button_row =
        dialog()->children()[3]->children();
    for (const auto& button : button_row) {
      auto* text_button = static_cast<views::MdTextButton*>(button);
      ASSERT_TRUE(!text_button->GetEnabled() ||
                  text_button->GetText() ==
                      l10n_util::GetStringUTF16(IDS_CANCEL));
    }
  }

  AccountSelectionModalView* dialog() { return dialog_; }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory()
      const {
    return test_shared_url_loader_factory_;
  }

 private:
  raw_ptr<AccountSelectionModalView, DanglingUntriaged> dialog_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

// Tests that the single account dialog is rendered correctly.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest, SingleAccount) {
  TestSingleAccount(kTitleSignIn, kBodySignIn);
}

// Tests that the multiple accounts dialog is rendered correctly.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest, MultipleAccounts) {
  TestMultipleAccounts(kTitleSignIn, kBodySignIn);
}

// Tests that the request permission dialog is rendered correctly.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest, RequestPermission) {
  TestRequestPermission(kTitleRequestPermission);
}

// Tests that the loading dialog is rendered correctly.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest, Loading) {
  TestLoadingDialog(kTitleSignIn, kBodySignIn);
}

// Tests that the verifying sheet is rendered correctly, when it is shown after
// the single account dialog.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       VerifyingAfterSingleAccount) {
  TestSingleAccount(kTitleSignIn, kBodySignIn);
  ShowVerifyingSheet();
  TestVerifyingSheet(kTitleSignIn, kBodySignIn);
}

// Tests that the verifying sheet is rendered correctly, when it is shown after
// the multiple accounts dialog.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       VerifyingAfterMultipleAccounts) {
  TestMultipleAccounts(kTitleSignIn, kBodySignIn);
  ShowVerifyingSheet();
  TestVerifyingSheet(kTitleSignIn, kBodySignIn, /*has_multiple_accounts=*/true);
}

// Tests that the verifying sheet is rendered correctly, when it is shown after
// the request permission dialog.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       VerifyingAfterRequestPermission) {
  TestRequestPermission(kTitleRequestPermission);
  ShowVerifyingSheet();
  TestVerifyingSheet(kTitleRequestPermission);
}

// Tests that the single account dialog is rendered correctly when IDP supports
// use other account.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       SingleAccountUseOtherAccount) {
  TestSingleAccount(kTitleSignIn, kBodySignIn, /*supports_add_account=*/true);
}

// Tests that the multiple accounts dialog is rendered correctly when IDP
// supports use other account.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       MultipleAccountsUseOtherAccount) {
  TestMultipleAccounts(kTitleSignIn, kBodySignIn,
                       /*supports_add_account=*/true);
}
