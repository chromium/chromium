// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_modal_view.h"

#include <string>

#include "chrome/browser/ui/test/test_browser_dialog.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "content/public/test/browser_test.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "ui/views/controls/styled_label.h"

namespace {

const std::u16string kTopFrameETLDPlusOne = u"top-frame-example.com";
const std::u16string kIdpETLDPlusOne = u"idp-example.com";
const std::u16string kTitleSignIn =
    u"Sign in to top-frame-example.com with idp-example.com";
const std::u16string kBodySignIn = u"Choose an account to continue";

constexpr char kIdBase[] = "id";
constexpr char kEmailBase[] = "email";
constexpr char kNameBase[] = "name";
constexpr char kGivenNameBase[] = "given_name";

constexpr char kTermsOfServiceUrl[] = "https://terms-of-service.com";
constexpr char kPrivacyPolicyUrl[] = "https://privacy-policy.com";

content::IdentityRequestAccount CreateTestIdentityRequestAccount(
    const std::string& account_suffix,
    content::IdentityRequestAccount::LoginState login_state) {
  return content::IdentityRequestAccount(
      std::string(kIdBase) + account_suffix,
      std::string(kEmailBase) + account_suffix,
      std::string(kNameBase) + account_suffix,
      std::string(kGivenNameBase) + account_suffix, GURL::EmptyGURL(),
      /*login_hints=*/std::vector<std::string>(),
      /*domain_hints=*/std::vector<std::string>(), login_state);
}

std::vector<content::IdentityRequestAccount> CreateTestIdentityRequestAccounts(
    const std::vector<std::string>& account_suffixes,
    content::IdentityRequestAccount::LoginState login_state) {
  std::vector<content::IdentityRequestAccount> accounts;
  for (const std::string& account_suffix : account_suffixes) {
    accounts.push_back(
        CreateTestIdentityRequestAccount(account_suffix, login_state));
  }
  return accounts;
}

content::ClientMetadata CreateTestClientMetadata(
    const std::string& terms_of_service_url) {
  return content::ClientMetadata((GURL(terms_of_service_url)),
                                 (GURL(kPrivacyPolicyUrl)));
}

std::vector<std::string> GetChildClassNames(views::View* parent) {
  std::vector<std::string> child_class_names;
  for (views::View* child_view : parent->children()) {
    child_class_names.push_back(child_view->GetClassName());
  }
  return child_class_names;
}

}  // namespace

class AccountSelectionModalViewTest : public DialogBrowserTest {
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
        kTopFrameETLDPlusOne, /*iframe_for_display=*/absl::nullopt, account,
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

  void CheckAccountRow(views::View* row, const std::string& account_suffix) {
    ASSERT_STREQ("HoverButton", row->GetClassName());
    HoverButton* account_row = static_cast<HoverButton*>(row);
    ASSERT_TRUE(account_row);
    EXPECT_EQ(GetAccountButtonTitle(account_row),
              base::UTF8ToUTF16(kNameBase + account_suffix));
    EXPECT_EQ(GetAccountButtonSubtitle(account_row)->GetText(),
              base::UTF8ToUTF16(std::string(kEmailBase) + account_suffix));
    // The subtitle has changed style, so AutoColorReadabilityEnabled should
    // be set.
    EXPECT_TRUE(GetAccountButtonSubtitle(account_row)
                    ->GetAutoColorReadabilityEnabled());
    views::View* icon_view = GetAccountButtonIconView(account_row);
    EXPECT_TRUE(icon_view);
    EXPECT_EQ(icon_view->size(),
              gfx::Size(kDesiredAvatarSize, kDesiredAvatarSize));
  }

  void CheckAccountRows(
      const std::vector<raw_ptr<views::View, VectorExperimental>>& accounts,
      const std::vector<std::string>& account_suffixes) {
    EXPECT_EQ(accounts.size(), account_suffixes.size());
    for (size_t i = 0; i < std::size(account_suffixes); ++i) {
      CheckAccountRow(accounts[i], account_suffixes[i]);
    }
  }

  void PerformHeaderChecks(views::View* header,
                           const std::u16string& expected_title) {
    // Perform some basic dialog checks.
    EXPECT_FALSE(dialog()->ShouldShowCloseButton());
    EXPECT_FALSE(dialog()->ShouldShowWindowTitle());

    EXPECT_FALSE(dialog()->GetOkButton());
    EXPECT_TRUE(dialog()->GetCancelButton());

    // Order: Brand icon, title, body
    std::vector<std::string> expected_class_names = {"BrandIconImageView",
                                                     "Label", "Label"};
    EXPECT_THAT(GetChildClassNames(header),
                testing::ElementsAreArray(expected_class_names));

    std::vector<raw_ptr<views::View, VectorExperimental>> header_children =
        header->children();
    ASSERT_EQ(header_children.size(), expected_class_names.size());

    // Check title text.
    views::Label* title_view = static_cast<views::Label*>(header_children[1]);
    ASSERT_TRUE(title_view);
    EXPECT_EQ(title_view->GetText(), expected_title);

    // Check body text.
    views::Label* body_view = static_cast<views::Label*>(header_children[2]);
    ASSERT_TRUE(body_view);
    EXPECT_EQ(body_view->GetText(), kBodySignIn);
  }

  std::u16string GetAccountButtonTitle(HoverButton* account) {
    return account->title()->GetText();
  }

  views::Label* GetAccountButtonSubtitle(HoverButton* account) {
    return account->subtitle();
  }

  views::View* GetAccountButtonIconView(HoverButton* account) {
    return account->icon_view();
  }

  void TestSingleAccount(const std::u16string expected_title) {
    const std::string kAccountSuffix = "suffix";
    content::IdentityRequestAccount account(CreateTestIdentityRequestAccount(
        kAccountSuffix, content::IdentityRequestAccount::LoginState::kSignUp));
    CreateSingleAccountPicker(
        /*show_back_button=*/false, account,
        content::IdentityProviderMetadata(), kTermsOfServiceUrl);

    std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    ASSERT_EQ(children.size(), 2u);
    PerformHeaderChecks(children[0], expected_title);

    views::View* single_account_chooser = children[1];
    ASSERT_EQ(single_account_chooser->children().size(), 1u);

    CheckAccountRow(single_account_chooser->children()[0], kAccountSuffix);
  }

  void TestMultipleAccounts(const std::u16string& expected_title) {
    const std::vector<std::string> kAccountSuffixes = {"0", "1", "2"};
    CreateMultiAccountPicker(kAccountSuffixes);

    std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    ASSERT_EQ(children.size(), 2u);
    PerformHeaderChecks(children[0], expected_title);

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

    CheckAccountRows(accounts, kAccountSuffixes);
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

IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest, SingleAccount) {
  TestSingleAccount(kTitleSignIn);
}

IN_PROC_BROWSER_TEST_F(AccountSelectionModalViewTest, MultipleAccounts) {
  TestMultipleAccounts(kTitleSignIn);
}
