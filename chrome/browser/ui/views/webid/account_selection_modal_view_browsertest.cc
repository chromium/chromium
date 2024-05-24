// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_modal_view.h"

#include <string>

#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/webid/account_selection_view_base.h"
#include "chrome/browser/ui/views/webid/account_selection_view_test_base.h"
#include "chrome/grit/generated_resources.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/image_model.h"
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
    if (dialog_) {
      return;
    }

    dialog_ = new AccountSelectionModalView(
        kTopFrameETLDPlusOne, kIdpETLDPlusOne,
        blink::mojom::RpContext::kSignIn,
        browser()->tab_strip_model()->GetActiveWebContents(),
        shared_url_loader_factory(), /*observer=*/nullptr,
        /*widget_observer=*/nullptr);

    // Loading dialog is always shown first. All other dialogs reuse the header
    // of this loading dialog.
    TestLoadingDialog();
  }

 protected:
  void CreateAccountSelectionModal() {
    ShowUi("");
  }

  void CreateAndShowSingleAccountPicker(
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

  void CreateAndShowMultiAccountPicker(
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
    dialog_->ShowMultiAccountPicker(idp_data, /*show_back_button=*/false);
  }

  void CreateAndShowRequestPermissionDialog(
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

  void CreateAndShowVerifyingSheet() {
    CreateAccountSelectionModal();
    const std::string kAccountSuffix = "suffix";
    content::IdentityRequestAccount account(CreateTestIdentityRequestAccount(
        kAccountSuffix, content::IdentityRequestAccount::LoginState::kSignUp));
    IdentityProviderDisplayData idp_data(
        kIdpETLDPlusOne, content::IdentityProviderMetadata(),
        CreateTestClientMetadata(/*terms_of_service_url=*/""), {account},
        /*request_permission=*/true, /*has_login_status_mismatch=*/false);
    dialog_->ShowVerifyingSheet(account, idp_data, kTitleSignIn);
  }

  void CreateAndShowLoadingDialog() {
    CreateAccountSelectionModal();
    dialog_->ShowLoadingDialog();
  }

  void PerformHeaderChecks(views::View* header) {
    // Perform some basic dialog checks.
    EXPECT_FALSE(dialog()->ShouldShowCloseButton());
    EXPECT_FALSE(dialog()->ShouldShowWindowTitle());

    // Default buttons should not be shown.
    EXPECT_FALSE(dialog()->GetOkButton());
    EXPECT_FALSE(dialog()->GetCancelButton());

    // Order: Brand icon, title, body
    std::vector<std::string> expected_class_names = {"View", "Label", "Label"};
    EXPECT_THAT(GetChildClassNames(header),
                testing::ElementsAreArray(expected_class_names));

    std::vector<raw_ptr<views::View, VectorExperimental>> header_children =
        header->children();
    ASSERT_EQ(header_children.size(), expected_class_names.size());

    // Check IDP brand icon.
    views::View* background_container =
        static_cast<views::View*>(header_children[0]);
    ASSERT_TRUE(background_container);

    // Check background container contains the background image and icon
    // container.
    std::vector<raw_ptr<views::View, VectorExperimental>>
        background_container_children = background_container->children();
    ASSERT_EQ(background_container_children.size(), 2u);

    views::View* background_image =
        static_cast<views::View*>(background_container_children[0]);
    ASSERT_TRUE(background_image);

    views::View* icon_container =
        static_cast<views::View*>(background_container_children[1]);
    ASSERT_TRUE(icon_container);

    // Check icon container contains the icon image.
    std::vector<raw_ptr<views::View, VectorExperimental>>
        icon_container_children = icon_container->children();
    ASSERT_EQ(icon_container_children.size(), 1u);

    views::View* icon_image =
        static_cast<views::View*>(icon_container_children[0]);
    ASSERT_TRUE(icon_image);
    EXPECT_TRUE(icon_image->GetVisible());

    // Check icon image is of the correct size.
    EXPECT_EQ(icon_image->size(),
              gfx::Size(kModalIdpIconSize, kModalIdpIconSize));

    // Check title text.
    views::Label* title_view = static_cast<views::Label*>(header_children[1]);
    ASSERT_TRUE(title_view);
    EXPECT_EQ(title_view->GetText(), kTitleSignIn);
    if (should_focus_title_) {
      EXPECT_EQ(dialog()->GetInitiallyFocusedView(), title_view);
    }

    // Check body text.
    views::Label* body_view = static_cast<views::Label*>(header_children[2]);
    ASSERT_TRUE(body_view);
    EXPECT_EQ(body_view->GetText(), kBodySignIn);
    EXPECT_EQ(body_view->GetVisible(), expect_visible_body_label_);

    // After the first header check, the consecutive header checks do not
    // necessarily have to focus on the title.
    should_focus_title_ = false;
  }

  void CheckButtonRow(views::View* button_row,
                      bool expect_continue_button,
                      bool expect_add_account_button,
                      bool expect_back_button) {
    std::vector<raw_ptr<views::View, VectorExperimental>> button_row_children =
        button_row->children();

    // Cancel button is always expected.
    size_t num_expected_buttons = 1u + expect_continue_button +
                                  expect_add_account_button +
                                  expect_back_button;

    ASSERT_EQ(button_row_children.size(), num_expected_buttons);

    size_t button_index = 0;
    if (expect_add_account_button || expect_back_button) {
      std::vector<raw_ptr<views::View, VectorExperimental>>
          leftmost_button_container_children =
              button_row_children[button_index++]->children();
      ASSERT_EQ(leftmost_button_container_children.size(), 1u);
      views::MdTextButton* leftmost_button = static_cast<views::MdTextButton*>(
          leftmost_button_container_children[0]);
      ASSERT_TRUE(leftmost_button);
      EXPECT_EQ(leftmost_button->GetText(), expect_add_account_button
                                                ? u"Use a different account"
                                                : u"Back");
    }

    views::MdTextButton* cancel_button =
        static_cast<views::MdTextButton*>(button_row_children[button_index++]);
    ASSERT_TRUE(cancel_button);
    EXPECT_EQ(cancel_button->GetText(), u"Cancel");
    EXPECT_EQ(cancel_button->GetStyle(), expect_continue_button
                                             ? ui::ButtonStyle::kTonal
                                             : ui::ButtonStyle::kDefault);

    if (expect_continue_button) {
      views::MdTextButton* continue_button =
          static_cast<views::MdTextButton*>(button_row_children[button_index]);
      ASSERT_TRUE(continue_button);
      EXPECT_EQ(continue_button->GetText(), u"Continue");
      EXPECT_EQ(dialog()->GetInitiallyFocusedView(), continue_button);
    }
  }

  void CheckDisabledButtonRow(views::View* button_row,
                              bool should_focus_cancel = false) {
    for (const auto& button : button_row->children()) {
      auto* text_button = static_cast<views::MdTextButton*>(
          std::string(button->GetClassName()) == "FlexLayoutView"
              ? button->children()[0]
              : button);

      if (text_button->GetText() == l10n_util::GetStringUTF16(IDS_CANCEL)) {
        ASSERT_TRUE(text_button->GetEnabled());
        if (should_focus_cancel) {
          EXPECT_EQ(dialog()->GetInitiallyFocusedView(), text_button);
        }
        continue;
      }

      ASSERT_FALSE(text_button->GetEnabled());
    }
  }

  void TestSingleAccount(bool supports_add_account = false) {
    const std::string kAccountSuffix = "suffix";
    content::IdentityRequestAccount account(CreateTestIdentityRequestAccount(
        kAccountSuffix, content::IdentityRequestAccount::LoginState::kSignUp));
    content::IdentityProviderMetadata idp_metadata;
    idp_metadata.supports_add_account = supports_add_account;
    CreateAndShowSingleAccountPicker(
        /*show_back_button=*/false, account, idp_metadata, kTermsOfServiceUrl);

    std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    ASSERT_EQ(children.size(), 3u);

    expect_visible_body_label_ = true;
    PerformHeaderChecks(children[0]);

    views::View* account_rows = children[1];
    ASSERT_EQ(account_rows->children().size(), 3u);

    size_t accounts_index = 0;
    CheckHoverableAccountRows(account_rows->children(), {kAccountSuffix},
                              accounts_index, /*expect_idp=*/false,
                              /*is_modal_dialog=*/true);
    CheckButtonRow(children[2], /*expect_continue_button=*/false,
                   supports_add_account, /*expect_back_button=*/false);
  }

  void TestMultipleAccounts(bool supports_add_account = false) {
    const std::vector<std::string> kAccountSuffixes = {"0", "1", "2"};
    CreateAndShowMultiAccountPicker(kAccountSuffixes, supports_add_account);

    std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    ASSERT_EQ(children.size(), 3u);

    expect_visible_body_label_ = true;
    PerformHeaderChecks(children[0]);

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
    CheckHoverableAccountRows(accounts, kAccountSuffixes, accounts_index,
                              /*expect_idp=*/false, /*is_modal_dialog=*/true);
    CheckButtonRow(children[2], /*expect_continue_button=*/false,
                   supports_add_account, /*expect_back_button=*/false);
  }

  void TestRequestPermission(
      content::IdentityRequestAccount::LoginState login_state =
          content::IdentityRequestAccount::LoginState::kSignUp) {
    const std::string kAccountSuffix = "suffix";
    content::IdentityRequestAccount account(
        CreateTestIdentityRequestAccount(kAccountSuffix, login_state));
    CreateAndShowRequestPermissionDialog(
        account, content::IdentityProviderMetadata(), kTermsOfServiceUrl);

    std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    // Order: Header, single account chooser, button row
    ASSERT_EQ(children.size(), 3u);

    expect_visible_body_label_ = false;
    PerformHeaderChecks(children[0]);

    views::View* single_account_chooser = children[1];
    // Order: Account row, potentially disclosure text
    std::vector<std::string> expected_class_names = {"View"};
    bool is_returning_user =
        login_state == content::IdentityRequestAccount::LoginState::kSignIn;
    // For non-returning users, we expect a StyledLabel which contains the
    // disclosure text to obtain user permission.
    if (!is_returning_user) {
      expected_class_names.emplace_back("StyledLabel");
    }
    EXPECT_THAT(GetChildClassNames(single_account_chooser),
                testing::ElementsAreArray(expected_class_names));

    CheckNonHoverableAccountRow(single_account_chooser->children()[0],
                                kAccountSuffix);
    if (!is_returning_user) {
      views::View* disclosure_text_view = single_account_chooser->children()[1];
      CheckDisclosureText(disclosure_text_view,
                          /*expect_terms_of_service=*/true,
                          /*expect_privacy_policy=*/true);

      // Verifying should be set as screen reader announcement.
      // TODO(https://crbug.com/338094770): re-enable this check. Currently it's
      // causing many test flakes due to a bug in the code under test.
#if 0
      EXPECT_EQ(
          dialog()->GetQueuedAnnouncementForTesting(),
          static_cast<views::StyledLabel*>(disclosure_text_view)->GetText());
#endif
    }
    CheckButtonRow(children[2], /*expect_continue_button=*/true,
                   /*expect_add_account_button=*/false,
                   /*expect_back_button=*/true);
  }

  void TestVerifyingSheet(bool has_multiple_accounts = false) {
    CreateAndShowVerifyingSheet();
    // Order: Progress bar, header, account chooser, button row
    std::vector<std::string> expected_class_names = {
        "ProgressBar", "View", has_multiple_accounts ? "ScrollView" : "View",
        "View"};
    EXPECT_THAT(GetChildClassNames(dialog()),
                testing::ElementsAreArray(expected_class_names));

    // Verifying should be set as screen reader announcement.
    // TODO(https://crbug.com/338094770): re-enable this check. Currently it's
    // causing many test flakes due to a bug in the code under test.
#if 0
    EXPECT_EQ(dialog()->GetQueuedAnnouncementForTesting(),
              l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE));
#endif

    PerformHeaderChecks(dialog()->children()[1]);

    std::vector<raw_ptr<views::View, VectorExperimental>> account_chooser =
        dialog()->children()[2]->children();
    // Based on the modal type, there could be different items from the
    // account_chooser section. e.g. accounts, disclosure text, scroll view etc.
    // and all of them should be disabled.
    for (const auto& item : account_chooser) {
      ASSERT_FALSE(item->GetEnabled());
    }

    CheckDisabledButtonRow(dialog()->children()[3],
                           /*should_focus_cancel=*/true);
  }

  void TestLoadingDialog() {
    CreateAndShowLoadingDialog();
    // Order: Progress bar, header, placeholder account chooser, button row
    std::vector<std::string> expected_class_names = {"ProgressBar", "View",
                                                     "View", "View"};
    EXPECT_THAT(GetChildClassNames(dialog()),
                testing::ElementsAreArray(expected_class_names));

    PerformHeaderChecks(dialog()->children()[1]);

    std::vector<raw_ptr<views::View, VectorExperimental>>
        placeholder_account_chooser = dialog()->children()[2]->children();
    // Order: Placeholder account image, placeholder text column
    ASSERT_EQ(placeholder_account_chooser.size(), 2u);

    std::vector<raw_ptr<views::View, VectorExperimental>>
        placeholder_text_column = placeholder_account_chooser[1]->children();
    // Order: Placeholder account name, placeholder account email
    ASSERT_EQ(placeholder_text_column.size(), 2u);

    CheckDisabledButtonRow(dialog()->children()[3]);
  }

  AccountSelectionModalView* dialog() { return dialog_; }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory()
      const {
    return test_shared_url_loader_factory_;
  }

 private:
  bool expect_visible_body_label_{true};
  bool should_focus_title_{true};
  ui::ImageModel idp_brand_icon_;
  raw_ptr<AccountSelectionModalView, DanglingUntriaged> dialog_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

// Tests that the single account dialog is rendered correctly.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest, SingleAccount) {
  TestSingleAccount();
}

// Tests that the multiple accounts dialog is rendered correctly.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest, MultipleAccounts) {
  TestMultipleAccounts();
}

// Tests that the request permission dialog is rendered correctly, when it is
// shown after the single account dialog.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       RequestPermissionAfterSingleAccount) {
  TestSingleAccount();
  TestRequestPermission();
}

// Tests that the request permission dialog is rendered correctly, when it is
// shown after the multiple accounts dialog.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       RequestPermissionAfterMultipleAccounts) {
  TestMultipleAccounts();
  TestRequestPermission();
}

// Tests that the single account dialog is rendered correctly, when it is
// shown after the request permission dialog. This can happen when user clicks
// on the "back" button.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       SingleAccountAfterRequestPermission) {
  TestRequestPermission();
  TestSingleAccount();
}

// Tests that the multiple accounts dialog is rendered correctly, when it is
// shown after the request permission dialog. This can happen when user clicks
// on the "back" button.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       MultipleAccountsAfterRequestPermission) {
  TestRequestPermission();
  TestMultipleAccounts();
}

// Tests that the loading dialog is rendered correctly.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest, Loading) {
  // We always show the loading dialog at the creation of the modal.
  CreateAccountSelectionModal();
}

// Tests that the verifying sheet is rendered correctly, when it is shown after
// the single account dialog.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       VerifyingAfterSingleAccount) {
  TestSingleAccount();
  TestVerifyingSheet();
}

// Tests that the verifying sheet is rendered correctly, when it is shown after
// the multiple accounts dialog.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       VerifyingAfterMultipleAccounts) {
  TestMultipleAccounts();
  TestVerifyingSheet(/*has_multiple_accounts=*/true);
}

// Tests that the verifying sheet is rendered correctly, for the single account
// flow.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       VerifyingForSingleAccountFlow) {
  TestSingleAccount();
  TestRequestPermission();
  TestVerifyingSheet();
}

// Tests that the verifying sheet is rendered correctly, for the multiple
// account flow.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       VerifyingForMultipleAccountFlow) {
  TestMultipleAccounts();
  TestRequestPermission();
  TestVerifyingSheet();
}

// Tests that the single account dialog is rendered correctly when IDP supports
// use other account.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       SingleAccountUseOtherAccount) {
  TestSingleAccount(/*supports_add_account=*/true);
}

// Tests that the multiple accounts dialog is rendered correctly when IDP
// supports use other account.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       MultipleAccountsUseOtherAccount) {
  TestMultipleAccounts(/*supports_add_account=*/true);
}

// Tests that the request permission dialog is rendered correctly, when it is
// shown after the loading dialog for a non-returning user.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       RequestPermissionNonReturningUser) {
  TestRequestPermission(content::IdentityRequestAccount::LoginState::kSignUp);
}

// Tests that the request permission dialog is rendered correctly, when it is
// shown after the loading dialog for a returning user.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       RequestPermissionReturningUser) {
  TestRequestPermission(content::IdentityRequestAccount::LoginState::kSignIn);
}

// Tests that the brand icon view does not hide the brand icon like it does on
// bubble. This is because we show a placeholder globe icon on modal.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       InvalidBrandIconUrlDoesNotHideBrandIcon) {
  const std::string kAccountSuffix = "suffix";
  content::IdentityRequestAccount account(CreateTestIdentityRequestAccount(
      kAccountSuffix, content::IdentityRequestAccount::LoginState::kSignUp));
  content::IdentityProviderMetadata idp_metadata;
  idp_metadata.brand_icon_url = GURL("invalid url");
  CreateAndShowSingleAccountPicker(
      /*show_back_button=*/false, account, idp_metadata, kTermsOfServiceUrl);

  // We check that the icon is visible in PerformHeaderChecks.
  PerformHeaderChecks(dialog()->children()[0]);
}
