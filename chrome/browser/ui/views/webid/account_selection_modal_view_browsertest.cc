// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_modal_view.h"

#include <string>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/webid/account_selection_view_base.h"
#include "chrome/browser/ui/views/webid/account_selection_view_test_base.h"
#include "chrome/browser/ui/views/webid/fake_delegate.h"
#include "chrome/browser/ui/views/webid/fedcm_account_selection_view_desktop.h"
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
#include "ui/events/base_event_utils.h"
#include "ui/gfx/image/image_unittest_util.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/layout/box_layout.h"

namespace webid {
namespace {
class FakeFedCmAccountSelectionView : public FedCmAccountSelectionView {
 public:
  FakeFedCmAccountSelectionView(
      AccountSelectionView::Delegate* delegate,
      tabs::TabInterface* tab,
      scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory)
      : FedCmAccountSelectionView(delegate, tab),
        url_loader_factory_(url_loader_factory) {}

 private:
  scoped_refptr<network::SharedURLLoaderFactory> GetURLLoaderFactory()
      override {
    return url_loader_factory_;
  }

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
};
}  // namespace

class AccountSelectionModalViewTest : public DialogBrowserTest,
                                      public AccountSelectionViewTestBase {
 public:
  AccountSelectionModalViewTest()
      : idp_data_(base::MakeRefCounted<content::IdentityProviderData>(
            kIdpForDisplay,
            content::IdentityProviderMetadata(),
            CreateTestClientMetadata(),
            blink::mojom::RpContext::kSignIn,
            /*format=*/std::nullopt,
            kDefaultDisclosureFields,
            /*has_login_status_mismatch=*/false)) {
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }
  AccountSelectionModalViewTest(const AccountSelectionModalViewTest&) = delete;
  AccountSelectionModalViewTest& operator=(
      const AccountSelectionModalViewTest&) = delete;
  ~AccountSelectionModalViewTest() override = default;

  // DialogBrowserTest:
  void ShowUi(const std::string& name) override {
    if (dialog_) {
      return;
    }

    delegate_ = std::make_unique<FakeDelegate>(
        browser()->GetActiveTabInterface()->GetContents());
    account_selection_view_ = std::make_unique<FakeFedCmAccountSelectionView>(
        delegate_.get(), browser()->GetActiveTabInterface(),
        test_shared_url_loader_factory_);
    account_selection_view_->ShowLoadingDialog(
        content::RelyingPartyData(kRpETLDPlusOne, iframe_for_display_),
        base::UTF16ToASCII(kIdpETLDPlusOne), blink::mojom::RpContext::kSignIn,
        blink::mojom::RpMode::kActive);
    dialog_ = static_cast<AccountSelectionModalView*>(
        account_selection_view_->account_selection_view());

    // Loading dialog is always shown first. All other dialogs reuse the header
    // of this loading dialog.
    TestLoadingDialog();
  }

 protected:
  void CreateAccountSelectionModal() { ShowUi(""); }

  void CreateAndShowSingleAccountPicker(
      bool show_back_button,
      content::IdentityRequestAccount& account) {
    CreateAccountSelectionModal();
    dialog_->ShowSingleAccountConfirmDialog(&account, show_back_button);
    account_selection_view_->UpdateDialogPosition();
  }

  void CreateAndShowMultiAccountPicker(
      const std::vector<std::string>& account_suffixes,
      bool has_display_identifier,
      bool supports_add_account = false) {
    idp_data_->idp_metadata.supports_add_account = supports_add_account;
    account_list_ =
        CreateTestIdentityRequestAccounts(account_suffixes, idp_data_);
    if (!has_display_identifier) {
      for (auto& account : account_list_) {
        account->display_identifier = "";
      }
    }

    CreateAccountSelectionModal();
    dialog_->ShowMultiAccountPicker(account_list_, {idp_data_},
                                    /*rp_icon=*/gfx::Image(),
                                    /*show_back_button=*/false);
    account_selection_view_->UpdateDialogPosition();
  }

  void CreateAndShowRequestPermissionDialog(
      content::IdentityRequestAccount& account) {
    CreateAccountSelectionModal();
    account.identity_provider = idp_data_;
    dialog_->ShowRequestPermissionDialog(&account);
    account_selection_view_->UpdateDialogPosition();
  }

  void CreateAndShowVerifyingSheet() {
    CreateAccountSelectionModal();
    ShowVerifyingSheet();
  }

  void ShowVerifyingSheet() {
    const std::string kAccountSuffix = "suffix";
    IdentityRequestAccountPtr account(CreateTestIdentityRequestAccount(
        kAccountSuffix, idp_data_,
        content::IdentityRequestAccount::LoginState::kSignUp));
    dialog_->ShowVerifyingSheet(account, kTitleSignIn);
    account_selection_view_->UpdateDialogPosition();
  }

  IdentityRequestAccountPtr CreateSingleAccount(
      const std::string& account_suffix,
      bool is_filtered_out = false) {
    IdentityRequestAccountPtr account = CreateTestIdentityRequestAccount(
        account_suffix, idp_data_,
        content::IdentityRequestAccount::LoginState::kSignUp);
    account->is_filtered_out = is_filtered_out;
    return account;
  }

  void PerformHeaderChecks(views::View* header,
                           bool expect_visible_idp_icon = true,
                           bool expect_visible_combined_icons = false) {
    // Perform some basic dialog checks.
    EXPECT_FALSE(dialog()->ShouldShowCloseButton());
    EXPECT_FALSE(dialog()->ShouldShowWindowTitle());

    // Default buttons should not be shown.
    EXPECT_FALSE(dialog()->GetOkButton());
    EXPECT_FALSE(dialog()->GetCancelButton());

    bool has_subtitle = !iframe_for_display_.empty();

    // Order: Brand icon, title and body for non loading UI.
    std::vector<std::string> expected_class_names = {"View", "Label"};
    if (has_subtitle) {
      expected_class_names.push_back("Label");
    }
    bool is_loading_dialog =
        !expect_visible_idp_icon && !expect_visible_combined_icons;
    if (!is_loading_dialog) {
      expected_class_names.push_back("Label");
    }
    EXPECT_THAT(GetChildClassNames(header),
                testing::ElementsAreArray(expected_class_names));

    std::vector<raw_ptr<views::View, VectorExperimental>> header_children =
        header->children();
    ASSERT_EQ(header_children.size(), expected_class_names.size());

    // Check IDP brand icon.
    views::View* background_container =
        static_cast<views::View*>(header_children[0]);
    ASSERT_TRUE(background_container);

    // Check background container contains the background image, spinner, IDP
    // icon and combined icon container.
    std::vector<raw_ptr<views::View, VectorExperimental>>
        background_container_children = background_container->children();
    ASSERT_EQ(background_container_children.size(),
              expect_visible_combined_icons ? 4u : 3u);

    views::View* background_image =
        static_cast<views::View*>(background_container_children[0]);
    ASSERT_TRUE(background_image);

    // Check IDP icon container contains the IDP icon image. The IDP icon
    // container is always present. Its visibility is updated when we want to
    // show the combined icons container instead.
    views::View* spinner_container =
        static_cast<views::View*>(background_container_children[1]);
    ASSERT_TRUE(spinner_container);

    std::vector<raw_ptr<views::View, VectorExperimental>>
        spinner_container_children = spinner_container->children();
    ASSERT_EQ(spinner_container_children.size(), 1u);

    views::Throbber* spinner =
        static_cast<views::Throbber*>(spinner_container_children[0]);
    ASSERT_TRUE(spinner);
    EXPECT_TRUE(spinner->GetVisible());

    // Check spinner is of the correct size.
    EXPECT_EQ(spinner->size(),
              gfx::Size(kModalIconSpinnerSize, kModalIconSpinnerSize));

    // Check IDP icon container contains the IDP icon image. The IDP icon
    // container is always present. Its visibility is updated when we want to
    // show the combined icons container instead.
    views::View* idp_icon_container =
        static_cast<views::View*>(background_container_children[2]);
    ASSERT_TRUE(idp_icon_container);

    std::vector<raw_ptr<views::View, VectorExperimental>>
        idp_icon_container_children = idp_icon_container->children();
    ASSERT_EQ(idp_icon_container_children.size(), 1u);

    views::View* idp_icon_image =
        static_cast<views::View*>(idp_icon_container_children[0]);
    ASSERT_TRUE(idp_icon_image);

    if (expect_visible_idp_icon) {
      EXPECT_TRUE(idp_icon_image->GetVisible());

      // Check icon image is of the correct size.
      EXPECT_EQ(idp_icon_image->size(),
                gfx::Size(kModalIdpIconSize, kModalIdpIconSize));
    }

    // The combined icons container is present only when we expect it to be
    // visible. Its visibility is updated only after the icons have been
    // fetched.
    if (expect_visible_combined_icons) {
      // Check combined icons container contains the IDP, arrow and RP icon
      // images.
      views::View* combined_icons_container =
          static_cast<views::View*>(background_container_children[3]);
      ASSERT_TRUE(combined_icons_container);

      std::vector<raw_ptr<views::View, VectorExperimental>>
          combined_icons_container_children =
              combined_icons_container->children();
      ASSERT_EQ(combined_icons_container_children.size(), 3u);

      // Icons in the combined icons container are always visible individually.
      // Instead, the visibility of the container is changed to show/hide these
      // icons.
      for (const auto& icon : combined_icons_container_children) {
        EXPECT_TRUE(icon->GetVisible());
        EXPECT_EQ(icon->size(),
                  gfx::Size(kModalCombinedIconSize, kModalCombinedIconSize));
      }
    }

    // Check title text.
    views::Label* title_view = static_cast<views::Label*>(header_children[1]);
    ASSERT_TRUE(title_view);
    if (has_subtitle) {
      EXPECT_EQ(title_view->GetText(), kTitleIframeSignIn);
      EXPECT_EQ(dialog()->GetDialogTitle(),
                base::UTF16ToUTF8(kTitleIframeSignIn));

      views::Label* subtitle_view =
          static_cast<views::Label*>(header_children[2]);
      ASSERT_TRUE(subtitle_view);
      EXPECT_EQ(subtitle_view->GetText(), kSubtitleIframeSignIn);
      EXPECT_EQ(dialog()->GetDialogSubtitle(),
                base::UTF16ToUTF8(kSubtitleIframeSignIn));
    } else {
      EXPECT_EQ(title_view->GetText(), kTitleSignIn);
      EXPECT_EQ(dialog()->GetDialogTitle(), base::UTF16ToUTF8(kTitleSignIn));
      EXPECT_EQ(dialog()->GetDialogSubtitle(), std::nullopt);
    }

    if (!is_loading_dialog) {
      // Check body text.
      views::Label* body_view =
          static_cast<views::Label*>(header_children[has_subtitle ? 3 : 2]);
      ASSERT_TRUE(body_view);
      EXPECT_EQ(body_view->GetText(), kBodySignIn);
      EXPECT_EQ(body_view->GetVisible(), expect_visible_body_label_);
    }
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
      CheckReplaceButtonWithSpinner(leftmost_button);
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
      CheckReplaceButtonWithSpinner(continue_button);
    }
  }

  void CheckReplaceButtonWithSpinner(views::MdTextButton* button) {
    dialog()->ReplaceButtonWithSpinner(button);
    bool has_spinner = false;
    for (const auto& child : button->children()) {
      // Spinner is placed in a BoxLayoutView.
      if (child->GetClassName() == "BoxLayoutView") {
        views::Throbber* spinner =
            static_cast<views::Throbber*>(child->children()[0]);
        EXPECT_TRUE(spinner);
        has_spinner = true;
      }
    }
    ASSERT_TRUE(has_spinner);
  }

  void CheckDisabledButtonRow(views::View* button_row) {
    for (const auto& button : button_row->children()) {
      auto* text_button = static_cast<views::MdTextButton*>(
          (button->GetClassName() == "FlexLayoutView") ? button->children()[0]
                                                       : button);

      if (text_button->GetText() == l10n_util::GetStringUTF16(IDS_CANCEL)) {
        ASSERT_TRUE(text_button->GetEnabled());
        continue;
      }
    }
  }

  void TestSingleAccount(bool supports_add_account = false) {
    const std::string kAccountSuffix = "suffix";
    idp_data_->idp_metadata.supports_add_account = supports_add_account;
    IdentityRequestAccountPtr account = CreateTestIdentityRequestAccount(
        kAccountSuffix, idp_data_,
        content::IdentityRequestAccount::LoginState::kSignUp);
    CreateAndShowSingleAccountPicker(
        /*show_back_button=*/false, *account);

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
    CreateAndShowMultiAccountPicker(kAccountSuffixes,
                                    /*has_display_identifier=*/true,
                                    supports_add_account);

    std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    ASSERT_EQ(children.size(), 3u);

    expect_visible_body_label_ = true;
    PerformHeaderChecks(children[0]);

    std::vector<raw_ptr<views::View, VectorExperimental>> accounts =
        TestStructureAndGetAccounts(children[1]);
    size_t accounts_index = 0;
    CheckHoverableAccountRows(accounts, kAccountSuffixes, accounts_index,
                              /*expect_idp=*/false, /*is_modal_dialog=*/true);
    CheckButtonRow(children[2], /*expect_continue_button=*/false,
                   supports_add_account, /*expect_back_button=*/false);
  }

  std::vector<raw_ptr<views::View, VectorExperimental>>
  TestStructureAndGetAccounts(views::View* container) {
    views::ScrollView* scroller = static_cast<views::ScrollView*>(container);
    EXPECT_FALSE(scroller->children().empty());
    views::View* wrapper = scroller->children()[0];
    EXPECT_FALSE(wrapper->children().empty());
    views::View* contents = wrapper->children()[0];

    views::BoxLayout* layout_manager =
        static_cast<views::BoxLayout*>(contents->GetLayoutManager());
    EXPECT_TRUE(layout_manager);
    EXPECT_EQ(layout_manager->GetOrientation(),
              views::BoxLayout::Orientation::kVertical);
    return contents->children();
  }

  void TestRequestPermission(
      bool has_display_identifier,
      content::IdentityRequestAccount::LoginState login_state =
          content::IdentityRequestAccount::LoginState::kSignUp,
      const std::string& idp_brand_icon_url = kIdpBrandIconUrl,
      const std::string& rp_brand_icon_url = kRpBrandIconUrl) {
    const std::string kAccountSuffix = "suffix";
    idp_data_->idp_metadata.brand_icon_url = GURL(idp_brand_icon_url);
    if (idp_data_->idp_metadata.brand_icon_url.is_valid()) {
      idp_data_->idp_metadata.brand_decoded_icon =
          gfx::Image::CreateFrom1xBitmap(gfx::test::CreateBitmap(1));
    }
    idp_data_->client_metadata.brand_icon_url = GURL(rp_brand_icon_url);
    if (idp_data_->client_metadata.brand_icon_url.is_valid()) {
      idp_data_->client_metadata.brand_decoded_icon =
          gfx::Image::CreateFrom1xBitmap(gfx::test::CreateBitmap(1));
    }
    IdentityRequestAccountPtr account(CreateTestIdentityRequestAccount(
        kAccountSuffix, idp_data_, login_state));
    if (!has_display_identifier) {
      account->display_identifier = "";
    }
    CreateAndShowRequestPermissionDialog(*account);

    std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    // Order: Header, single account chooser, button row
    ASSERT_EQ(children.size(), 3u);

    expect_visible_body_label_ = false;
    bool expect_combined_icons =
        !idp_brand_icon_url.empty() && !rp_brand_icon_url.empty();
    PerformHeaderChecks(
        children[0], /*expect_visible_idp_icon=*/!expect_combined_icons,
        /*expect_visible_combined_icons=*/expect_combined_icons);

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
                                kAccountSuffix, has_display_identifier);
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

  void TestVerifyingSheet(bool has_multiple_accounts = false,
                          bool expect_visible_idp_icon = true,
                          bool expect_visible_combined_icons = false) {
    CreateAndShowVerifyingSheet();
    // Order: Progress bar, header, account chooser, button row
    std::vector<std::string> expected_class_names = {
        "View", has_multiple_accounts ? "ScrollView" : "View", "View"};
    EXPECT_THAT(GetChildClassNames(dialog()),
                testing::ElementsAreArray(expected_class_names));

    // Verifying should be set as screen reader announcement.
    // TODO(https://crbug.com/338094770): re-enable this check. Currently it's
    // causing many test flakes due to a bug in the code under test.
#if 0
    EXPECT_EQ(dialog()->GetQueuedAnnouncementForTesting(),
              l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE));
#endif

    PerformHeaderChecks(dialog()->children()[0], expect_visible_idp_icon,
                        expect_visible_combined_icons);

    std::vector<raw_ptr<views::View, VectorExperimental>> account_chooser =
        dialog()->children()[1]->children();
    // Based on the modal type, there could be different items from the
    // account_chooser section. e.g. accounts, disclosure text, scroll view etc.
    // and all of them should be disabled.
    for (const auto& item : account_chooser) {
      if (item->GetClassName() == "HoverButton") {
        AccountHoverButton* button = static_cast<AccountHoverButton*>(item);
        ASSERT_FALSE(item->GetEnabled());
        ASSERT_TRUE(button->HasDisabledOpacity());
      }
    }

    CheckDisabledButtonRow(dialog()->children()[2]);
  }

  void TestLoadingDialog() {
    // Order: Header, placeholder account chooser, button row
    std::vector<std::string> expected_class_names = {"View", "View", "View"};
    EXPECT_THAT(GetChildClassNames(dialog()),
                testing::ElementsAreArray(expected_class_names));

    PerformHeaderChecks(dialog()->children()[0],
                        /*expect_visible_idp_icon=*/false);

    std::vector<raw_ptr<views::View, VectorExperimental>>
        placeholder_account_chooser = dialog()->children()[1]->children();
    // Order: Placeholder account image, placeholder text column
    ASSERT_EQ(placeholder_account_chooser.size(), 2u);

    std::vector<raw_ptr<views::View, VectorExperimental>>
        placeholder_text_column = placeholder_account_chooser[1]->children();
    // Order: Placeholder account name, placeholder account email
    ASSERT_EQ(placeholder_text_column.size(), 2u);

    CheckDisabledButtonRow(dialog()->children()[2]);
  }

  void TestErrorDialog(const std::u16string expected_summary,
                       const std::u16string expected_description,
                       const std::string& error_code,
                       const GURL& error_url) {
    IdentityRequestAccountPtr account = CreateTestIdentityRequestAccount(
        /*account_suffix=*/"account", idp_data_,
        content::IdentityRequestAccount::LoginState::kSignUp);
    CreateAndShowSingleAccountPicker(/*show_back_button=*/false, *account);
    dialog_->ShowVerifyingSheet(account, kTitleSignIn);
    account_selection_view_->UpdateDialogPosition();
    dialog_->ShowErrorDialog(
        kIdpETLDPlusOne, idp_data_->idp_metadata,
        content::IdentityCredentialTokenError(error_code, error_url));
    auto header_view = dialog()->children()[0];
    // header icon view, title_label and body_label
    ASSERT_EQ(header_view->children().size(), 3u);

    auto* title_label = static_cast<views::Label*>(header_view->children()[1]);
    EXPECT_EQ(title_label->GetText(), expected_summary);

    auto* body_label = static_cast<views::Label*>(header_view->children()[2]);
    EXPECT_EQ(body_label->GetText(), expected_description);

    auto button_container = dialog()->children()[1];
    const std::vector<raw_ptr<views::View, VectorExperimental>> button_row =
        button_container->children();

    if (error_url.is_empty()) {
      ASSERT_EQ(button_row.size(), 1u);

      views::MdTextButton* got_it_button =
          static_cast<views::MdTextButton*>(button_row[0]);
      ASSERT_TRUE(got_it_button);
      EXPECT_EQ(
          got_it_button->GetText(),
          l10n_util::GetStringUTF16(IDS_SIGNIN_ERROR_DIALOG_GOT_IT_BUTTON));
      return;
    }

    ASSERT_EQ(button_row.size(), 2u);
    for (size_t i = 0; i < button_row.size(); ++i) {
      views::MdTextButton* button =
          static_cast<views::MdTextButton*>(button_row[i]);
      ASSERT_TRUE(button);
      EXPECT_EQ(button->GetText(),
                l10n_util::GetStringUTF16(
                    i == 0 ? IDS_SIGNIN_ERROR_DIALOG_MORE_DETAILS_BUTTON
                           : IDS_SIGNIN_ERROR_DIALOG_GOT_IT_BUTTON));
    }
  }

  void TestDisabledAccounts(const std::vector<std::string>& account_suffixes) {
    idp_data_->idp_metadata.has_filtered_out_account = true;
    account_list_ =
        CreateTestIdentityRequestAccounts(account_suffixes, idp_data_);
    for (const auto& account : account_list_) {
      account->is_filtered_out = true;
    }
    CreateAccountSelectionModal();
    dialog()->ShowMultiAccountPicker(account_list_, {idp_data()},
                                     /*rp_icon=*/gfx::Image(),
                                     /*show_back_button=*/false);
    account_selection_view_->UpdateDialogPosition();

    std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    ASSERT_EQ(children.size(), 3u);

    expect_visible_body_label_ = true;
    PerformHeaderChecks(children[0]);

    std::vector<raw_ptr<views::View, VectorExperimental>> accounts =
        TestStructureAndGetAccounts(children[1]);
    ASSERT_GE(accounts.size(), account_suffixes.size());

    size_t accounts_index = 0;
    for (const auto& account_suffix : account_suffixes) {
      if (accounts[accounts_index]->GetClassName() == "Separator") {
        ++accounts_index;
      }
      CheckHoverableAccountRow(accounts[accounts_index++], account_suffix,
                               /*has_display_identifier=*/true,
                               /*expect_idp=*/false, /*is_modal_dialog=*/true,
                               /*is_disabled=*/true);
    }
    CheckButtonRow(children[2], /*expect_continue_button=*/false,
                   /*expect_add_account_button=*/true,
                   /*expect_back_button=*/false);
  }

  void TestEnabledAndDisabled(bool has_display_identifier) {
    idp_data_->idp_metadata.has_filtered_out_account = true;
    std::vector<std::string> account_suffixes = {"enabled", "disabled"};
    account_list_ =
        CreateTestIdentityRequestAccounts(account_suffixes, idp_data_);
    if (!has_display_identifier) {
      account_list_[0]->display_identifier = "";
      account_list_[1]->display_identifier = "";
    }
    account_list_[1]->is_filtered_out = true;
    CreateAccountSelectionModal();
    dialog()->ShowMultiAccountPicker(account_list_, {idp_data()},
                                     /*rp_icon=*/gfx::Image(),
                                     /*show_back_button=*/false);
    account_selection_view_->UpdateDialogPosition();

    std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    ASSERT_EQ(children.size(), 3u);

    expect_visible_body_label_ = true;
    PerformHeaderChecks(children[0]);

    std::vector<raw_ptr<views::View, VectorExperimental>> accounts =
        TestStructureAndGetAccounts(children[1]);

    ASSERT_EQ(accounts[0]->GetClassName(), "Separator");
    CheckHoverableAccountRow(accounts[1], "enabled",
                             /*has_display_identifier=*/has_display_identifier,
                             /*expect_idp=*/false, /*is_modal_dialog=*/true,
                             /*is_disabled=*/false);
    ASSERT_EQ(accounts[2]->GetClassName(), "Separator");
    CheckHoverableAccountRow(accounts[3], "disabled",
                             /*has_display_identifier=*/has_display_identifier,
                             /*expect_idp=*/false, /*is_modal_dialog=*/true,
                             /*is_disabled=*/true);

    CheckButtonRow(children[2], /*expect_continue_button=*/false,
                   /*expect_add_account_button=*/true,
                   /*expect_back_button=*/false);
  }

  AccountSelectionModalView* dialog() { return dialog_; }

  IdentityProviderDataPtr idp_data() { return idp_data_; }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory()
      const {
    return test_shared_url_loader_factory_;
  }

  void SetIdpBrandIcon(const std::string& url) {
    idp_data_->idp_metadata.brand_icon_url = GURL(url);
    if (!idp_data_->idp_metadata.brand_icon_url.is_valid()) {
      idp_data_->idp_metadata.brand_decoded_icon = gfx::Image();
    }
  }

  // Can be set before the dialog is created to be set on the RelyingPartyData
  // that is passed to the dialog.
  std::u16string iframe_for_display_;

 private:
  bool expect_visible_body_label_{true};
  ui::ImageModel idp_brand_icon_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<FakeDelegate> delegate_;
  std::unique_ptr<FedCmAccountSelectionView> account_selection_view_;
  raw_ptr<AccountSelectionModalView, DanglingUntriaged> dialog_;
  std::vector<IdentityRequestAccountPtr> account_list_;
  IdentityProviderDataPtr idp_data_;
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
  TestRequestPermission(/*has_display_identifier=*/true);
}

// Tests that the request permission dialog is rendered correctly, when it is
// shown after the multiple accounts dialog.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       RequestPermissionAfterMultipleAccounts) {
  TestMultipleAccounts();
  TestRequestPermission(/*has_display_identifier=*/true);
}

// Tests that the single account dialog is rendered correctly, when it is
// shown after the request permission dialog. This can happen when user clicks
// on the "back" button.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       SingleAccountAfterRequestPermission) {
  TestRequestPermission(/*has_display_identifier=*/true);
  TestSingleAccount();
}

// Tests that the multiple accounts dialog is rendered correctly, when it is
// shown after the request permission dialog. This can happen when user clicks
// on the "back" button.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       MultipleAccountsAfterRequestPermission) {
  TestRequestPermission(/*has_display_identifier=*/true);
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
  TestRequestPermission(/*has_display_identifier=*/true);
  TestVerifyingSheet(/*has_multiple_accounts=*/false,
                     /*expect_visible_idp_icon=*/false,
                     /*expect_visible_combined_icons=*/true);
}

// Tests that the verifying sheet is rendered correctly, for the multiple
// account flow.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       VerifyingForMultipleAccountFlow) {
  TestMultipleAccounts();
  TestRequestPermission(/*has_display_identifier=*/true);
  TestVerifyingSheet(/*has_multiple_accounts=*/false,
                     /*expect_visible_idp_icon=*/false,
                     /*expect_visible_combined_icons=*/true);
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
  TestRequestPermission(/*has_display_identifier=*/true,
                        content::IdentityRequestAccount::LoginState::kSignUp);
}

// Tests that the request permission dialog is rendered correctly, when it is
// shown after the loading dialog for a returning user.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       RequestPermissionReturningUser) {
  TestRequestPermission(/*has_display_identifier=*/true,
                        content::IdentityRequestAccount::LoginState::kSignIn);
}

// Tests that the brand icon view does not hide the brand icon like it does on
// bubble. This is because we show a placeholder globe icon on modal.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       InvalidBrandIconUrlDoesNotHideBrandIcon) {
  SetIdpBrandIcon("invalid url");
  const std::string kAccountSuffix = "suffix";
  IdentityRequestAccountPtr account = CreateTestIdentityRequestAccount(
      kAccountSuffix, idp_data(),
      content::IdentityRequestAccount::LoginState::kSignUp);
  CreateAndShowSingleAccountPicker(
      /*show_back_button=*/false, *account);

  // We check that the icon is visible in PerformHeaderChecks.
  PerformHeaderChecks(dialog()->children()[0]);
}

// Tests that the request permission dialog is rendered correctly, when only IDP
// icon is available.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       RequestPermissionOnlyIdpIconAvailable) {
  TestRequestPermission(/*has_display_identifier=*/true,
                        content::IdentityRequestAccount::LoginState::kSignIn,
                        /*idp_brand_icon_url=*/kIdpBrandIconUrl,
                        /*rp_brand_icon_url=*/"");
}

// Tests that the request permission dialog is rendered correctly, when only RP
// icon is available.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       RequestPermissionOnlyRpIconAvailable) {
  TestRequestPermission(/*has_display_identifier=*/true,
                        content::IdentityRequestAccount::LoginState::kSignIn,
                        /*idp_brand_icon_url=*/"",
                        /*rp_brand_icon_url=*/kRpBrandIconUrl);
}

// Tests that the request permission dialog is rendered correctly, when neither
// RP nor IDP icon is available.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       RequestPermissionNeitherRpNorIdpIconsAvailable) {
  TestRequestPermission(/*has_display_identifier=*/true,
                        content::IdentityRequestAccount::LoginState::kSignIn,
                        /*idp_brand_icon_url=*/"", /*rp_brand_icon_url=*/"");
}

// Tests that the request permission dialog is rendered correctly, when both RP
// and IDP icons are available.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       RequestPermissionBothRpAndIdpIconsAvailable) {
  TestRequestPermission(/*has_display_identifier=*/true,
                        content::IdentityRequestAccount::LoginState::kSignIn,
                        /*idp_brand_icon_url=*/kIdpBrandIconUrl,
                        /*rp_brand_icon_url=*/kRpBrandIconUrl);
}

// Tests that the verifying sheet is rendered correctly, for the single account
// flow if the user clicks the back button during the flow.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest, SingleAccountFlowBack) {
  TestSingleAccount();
  TestRequestPermission(/*has_display_identifier=*/true);

  // Simulate user clicking the back button before completing the sign-in flow.
  TestSingleAccount();
  TestRequestPermission(/*has_display_identifier=*/true);
  TestVerifyingSheet(/*has_multiple_accounts=*/false,
                     /*expect_visible_idp_icon=*/false,
                     /*expect_visible_combined_icons=*/true);
}

// Tests that the verifying sheet is rendered correctly, for the multiple
// account flow if the user clicks the back button during the flow.
IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest, MultipleAccountFlowBack) {
  TestMultipleAccounts();
  TestRequestPermission(/*has_display_identifier=*/true);

  // Simulate user clicking the back button before completing the sign-in flow.
  TestMultipleAccounts();
  TestRequestPermission(/*has_display_identifier=*/true);
  TestVerifyingSheet(/*has_multiple_accounts=*/false,
                     /*expect_visible_idp_icon=*/false,
                     /*expect_visible_combined_icons=*/true);
}

IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest, ErrorDialogTest) {
  // Generic error without error URL
  TestErrorDialog(u"Can't continue with idp-example.com",
                  u"Something went wrong",
                  /*error_code=*/"",
                  /*error_url=*/GURL());

  // Generic error with error URL
  TestErrorDialog(
      u"Can't continue with idp-example.com", u"Something went wrong",
      /*error_code=*/"", GURL(u"https://idp-example.com/more-details"));

  // Invalid request without error URL
  TestErrorDialog(u"rp-example.com can't continue using idp-example.com",
                  u"This option is unavailable right now. You can try other "
                  u"ways to continue on rp-example.com.",
                  /*error_code=*/"invalid_request",
                  /*error_url=*/GURL());

  // Invalid request with error URL
  TestErrorDialog(
      u"rp-example.com can't continue using idp-example.com",
      u"This option is unavailable right now. Choose \"More "
      u"details\" below to get more information from idp-example.com.",
      /*error_code=*/"invalid_request",
      GURL(u"https://idp-example.com/more-details"));

  // Unauthorized client without error URL
  TestErrorDialog(u"rp-example.com can't continue using idp-example.com",
                  u"This option is unavailable right now. You can try other "
                  u"ways to continue on rp-example.com.",
                  /*error_code=*/"unauthorized_client",
                  /*error_url=*/GURL());

  // Unauthorized client with error URL
  TestErrorDialog(
      u"rp-example.com can't continue using idp-example.com",
      u"This option is unavailable right now. Choose \"More "
      u"details\" below to get more information from idp-example.com.",
      /*error_code=*/"unauthorized_client",
      GURL(u"https://idp-example.com/more-details"));

  // Access denied without error URL
  TestErrorDialog(u"Check that you chose the right account",
                  u"Check if the selected account is supported. You can try "
                  u"other ways to continue on rp-example.com.",
                  /*error_code=*/"access_denied",
                  /*error_url=*/GURL());

  // Access denied with error URL
  TestErrorDialog(
      u"Check that you chose the right account",
      u"Check if the selected account is supported. Choose \"More "
      u"details\" below to get more information from idp-example.com.",
      /*error_code=*/"access_denied",
      GURL(u"https://idp-example.com/more-details"));

  // Temporarily unavailable without error URL
  TestErrorDialog(u"Try again later",
                  u"idp-example.com isn't available right now. If this issue "
                  u"keeps happening, you can try other ways to continue on "
                  u"rp-example.com.",
                  /*error_code=*/"temporarily_unavailable",
                  /*error_url=*/GURL());

  // Temporarily unavailable with error URL
  TestErrorDialog(u"Try again later",
                  u"idp-example.com isn't available right now. If this issue "
                  u"keeps happening, choose \"More details\" below to get more "
                  u"information from idp-example.com.",
                  /*error_code=*/"temporarily_unavailable",
                  GURL(u"https://idp-example.com/more-details"));

  // Server error without error URL
  TestErrorDialog(u"Check your internet connection",
                  u"If you're online but this issue keeps happening, you can "
                  u"try other ways to continue on rp-example.com.",
                  /*error_code=*/"server_error",
                  /*error_url=*/GURL());

  // Server error with error URL
  TestErrorDialog(u"Check your internet connection",
                  u"If you're online but this issue keeps happening, you can "
                  u"try other ways to continue on rp-example.com.",
                  /*error_code=*/"server_error",
                  GURL(u"https://idp-example.com/more-details"));

  // Error not in our predefined list without error URL
  TestErrorDialog(u"Can't continue with idp-example.com",
                  u"Something went wrong",
                  /*error_code=*/"error_we_dont_support",
                  /*error_url=*/GURL());

  // Error not in our predefined list with error URL
  TestErrorDialog(u"Can't continue with idp-example.com",
                  u"Something went wrong",
                  /*error_code=*/"error_we_dont_support",
                  GURL(u"https://idp-example.com/more-details"));
}

IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest, OneDisabledAccount) {
  TestDisabledAccounts(/*account_suffixes=*/{"0"});
}

IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       MultipleDisabledAccounts) {
  TestDisabledAccounts(/*account_suffixes=*/{"0", "1", "2"});
}

IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       OneDisabledAccountAndOneEnabledAccount) {
  TestEnabledAndDisabled(/*has_display_identifier=*/true);
}

IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest, SingleIdentifier) {
  TestEnabledAndDisabled(/*has_display_identifier=*/false);
}

IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       RequestPermissionSingleIdentifier) {
  TestRequestPermission(/*has_display_identifier=*/false,
                        content::IdentityRequestAccount::LoginState::kSignIn,
                        /*idp_brand_icon_url=*/kIdpBrandIconUrl,
                        /*rp_brand_icon_url=*/kRpBrandIconUrl);
}

IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest,
                       VerifyingSheetSingleIdentifier) {
  CreateAndShowMultiAccountPicker(/*account_suffixes=*/{"0", "suffix", "2"},
                                  /*has_display_identifier=*/false);
  ShowVerifyingSheet();
}

IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest, IframeTitle) {
  iframe_for_display_ = kIframeETLDPlusOne;
  // This will also run the header/title tests.
  CreateAccountSelectionModal();
}

}  //  namespace webid
