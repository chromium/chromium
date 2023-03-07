// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"

#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/webid/fake_delegate.h"
#include "chrome/browser/ui/views/webid/identity_provider_display_data.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"

namespace {

const std::u16string kTopFrameETLDPlusOne = u"top-frame-example.com";
const std::u16string kIframeETLDPlusOne = u"iframe-example.com";
const std::u16string kIdpETLDPlusOne = u"idp-example.com";
const std::u16string kTitleSignIn =
    u"Sign in to top-frame-example.com with idp-example.com";
const std::u16string kTitleSignInWithoutIdp =
    u"Sign in to top-frame-example.com";
const std::u16string kTitleSigningIn = u"Verifying…";
const std::u16string kTitleSigningInWithAutoReauthn = u"Signing you in…";

constexpr char kIdBase[] = "id";
constexpr char kEmailBase[] = "email";
constexpr char kNameBase[] = "name";
constexpr char kGivenNameBase[] = "given_name";

const char kTermsOfServiceUrl[] = "htpps://terms-of-service.com";
const char kPrivacyPolicyUrl[] = "https://privacy-policy.com";

constexpr int kDesiredAvatarSize = 30;

content::IdentityRequestAccount CreateTestIdentityRequestAccount(
    const std::string& account_suffix,
    content::IdentityRequestAccount::LoginState login_state) {
  return content::IdentityRequestAccount(
      std::string(kIdBase) + account_suffix,
      std::string(kEmailBase) + account_suffix,
      std::string(kNameBase) + account_suffix,
      std::string(kGivenNameBase) + account_suffix, GURL::EmptyGURL(),
      login_state);
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

views::View* GetViewWithClassName(views::View* parent,
                                  const std::string& class_name) {
  for (views::View* child_view : parent->children()) {
    if (child_view->GetClassName() == class_name)
      return child_view;
  }
  return nullptr;
}

}  // namespace

class AccountSelectionBubbleViewTest : public ChromeViewsTestBase {
 public:
  AccountSelectionBubbleViewTest() = default;

 protected:
  void CreateAccountSelectionBubble(bool exclude_title,
                                    bool exclude_iframe,
                                    bool show_auto_reauthn_checkbox) {
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

    anchor_widget_ = std::make_unique<views::Widget>();
    anchor_widget_->Init(std::move(params));
    anchor_widget_->Show();

    absl::optional<std::u16string> title =
        exclude_title ? absl::nullopt
                      : absl::make_optional<std::u16string>(kIdpETLDPlusOne);
    absl::optional<std::u16string> iframe_etld_plus_one =
        exclude_iframe
            ? absl::nullopt
            : absl::make_optional<std::u16string>(kIframeETLDPlusOne);
    dialog_ = new AccountSelectionBubbleView(
        kTopFrameETLDPlusOne, iframe_etld_plus_one, title,
        blink::mojom::RpContext::kSignIn, show_auto_reauthn_checkbox,
        anchor_widget_->GetContentsView(), shared_url_loader_factory(),
        /*observer=*/nullptr);
    views::BubbleDialogDelegateView::CreateBubble(dialog_)->Show();
  }

  void CreateSingleAccountPicker(bool show_back_button,
                                 const content::IdentityRequestAccount& account,
                                 const std::string& terms_of_service_url,
                                 bool show_auto_reauthn_checkbox = false,
                                 bool exclude_iframe = true) {
    CreateAccountSelectionBubble(/*exclude_title=*/false, exclude_iframe,
                                 show_auto_reauthn_checkbox);
    IdentityProviderDisplayData idp_data(
        kIdpETLDPlusOne, content::IdentityProviderMetadata(),
        CreateTestClientMetadata(terms_of_service_url), {account});
    dialog_->ShowSingleAccountConfirmDialog(
        kTopFrameETLDPlusOne,
        exclude_iframe
            ? absl::nullopt
            : absl::make_optional<std::u16string>(kIframeETLDPlusOne),
        account, idp_data, show_back_button);
  }

  void CreateMultiIdpAccountPicker(
      const std::vector<IdentityProviderDisplayData>& idp_data_list) {
    CreateAccountSelectionBubble(/*exclude_title=*/true,
                                 /*exclude_iframe=*/true,
                                 /*show_auto_reauthn_checkbox=*/false);
    dialog_->ShowMultiAccountPicker(idp_data_list);
  }

  void CheckAccountRow(views::View* row, const std::string& account_suffix) {
    std::vector<views::View*> row_children = row->children();
    ASSERT_EQ(row_children.size(), 2u);

    // Check the image.
    views::ImageView* image_view =
        static_cast<views::ImageView*>(row_children[0]);
    EXPECT_TRUE(image_view);

    // Check the text shown.
    views::View* text_view = row_children[1];
    views::BoxLayout* layout_manager =
        static_cast<views::BoxLayout*>(text_view->GetLayoutManager());
    ASSERT_TRUE(layout_manager);
    EXPECT_EQ(layout_manager->GetOrientation(),
              views::BoxLayout::Orientation::kVertical);
    std::vector<views::View*> text_view_children = text_view->children();
    ASSERT_EQ(text_view_children.size(), 2u);

    std::string expected_name(std::string(kNameBase) + account_suffix);
    views::Label* name_view = static_cast<views::Label*>(text_view_children[0]);
    ASSERT_TRUE(name_view);
    EXPECT_EQ(name_view->GetText(), base::UTF8ToUTF16(expected_name));

    std::string expected_email(std::string(kEmailBase) + account_suffix);
    views::Label* email_view =
        static_cast<views::Label*>(text_view_children[1]);
    ASSERT_TRUE(email_view);
    EXPECT_EQ(email_view->GetText(), base::UTF8ToUTF16(expected_email));
  }

  void PerformHeaderChecks(views::View* header,
                           const std::u16string& expected_title,
                           bool expect_idp_brand_icon_in_header) {
    // Perform some basic dialog checks.
    EXPECT_FALSE(dialog()->ShouldShowCloseButton());
    EXPECT_FALSE(dialog()->ShouldShowWindowTitle());

    EXPECT_FALSE(dialog()->GetOkButton());
    EXPECT_FALSE(dialog()->GetCancelButton());

    // Order: Potentially hidden IDP brand icon, potentially hidden back button,
    // title, close button.
    std::vector<std::string> expected_class_names = {"ImageButton", "Label",
                                                     "ImageButton"};
    if (expect_idp_brand_icon_in_header) {
      expected_class_names.insert(expected_class_names.begin(), "ImageView");
    }
    EXPECT_THAT(GetChildClassNames(header),
                testing::ElementsAreArray(expected_class_names));

    // Check title text.
    views::Label* title_view =
        static_cast<views::Label*>(GetViewWithClassName(header, "Label"));
    ASSERT_TRUE(title_view);
    EXPECT_EQ(title_view->GetText(), expected_title);

    // Check separator.
    if (expected_title == kTitleSignIn ||
        expected_title == kTitleSignInWithoutIdp) {
      EXPECT_STREQ("Separator", dialog()->children()[1]->GetClassName());
    } else if (expected_title == kTitleSigningIn) {
      EXPECT_STREQ("ProgressBar", dialog()->children()[1]->GetClassName());
    }
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

  void TestSingleAccount(const std::u16string expected_title,
                         bool expect_idp_brand_icon_in_header) {
    const std::string kAccountSuffix = "suffix";
    content::IdentityRequestAccount account(CreateTestIdentityRequestAccount(
        kAccountSuffix, content::IdentityRequestAccount::LoginState::kSignUp));
    CreateSingleAccountPicker(
        /*show_back_button=*/false, account, kTermsOfServiceUrl);

    std::vector<views::View*> children = dialog()->children();
    ASSERT_EQ(children.size(), 3u);
    PerformHeaderChecks(children[0], expected_title,
                        expect_idp_brand_icon_in_header);

    views::View* single_account_chooser = children[2];
    ASSERT_EQ(single_account_chooser->children().size(), 3u);

    CheckAccountRow(single_account_chooser->children()[0], kAccountSuffix);

    // Check the "Continue as" button.
    views::MdTextButton* button = static_cast<views::MdTextButton*>(
        single_account_chooser->children()[1]);
    ASSERT_TRUE(button);
    EXPECT_EQ(button->GetText(),
              base::UTF8ToUTF16("Continue as " + std::string(kGivenNameBase) +
                                kAccountSuffix));

    views::StyledLabel* disclosure_text =
        static_cast<views::StyledLabel*>(single_account_chooser->children()[2]);
    ASSERT_TRUE(disclosure_text);
    EXPECT_EQ(disclosure_text->GetText(),
              u"To continue, idp-example.com will share your name, email "
              u"address, and profile picture with this site. See this site's "
              u"privacy policy and terms of service.");
  }

  void TestMultipleAccounts(const std::u16string& expected_title,
                            bool expect_idp_brand_icon_in_header,
                            bool expect_idp_row) {
    const std::vector<std::string> kAccountSuffixes = {"0", "1", "2"};

    {
      std::vector<content::IdentityRequestAccount> account_list =
          CreateTestIdentityRequestAccounts(
              kAccountSuffixes,
              content::IdentityRequestAccount::LoginState::kSignUp);

      CreateAccountSelectionBubble(/*exclude_title=*/false,
                                   /*exclude_iframe=*/true,
                                   /*show_auto_reauthn_checkbox=*/false);
      std::vector<IdentityProviderDisplayData> idp_data;
      idp_data.emplace_back(
          kIdpETLDPlusOne, content::IdentityProviderMetadata(),
          CreateTestClientMetadata(/*terms_of_service_url=*/""), account_list);
      dialog_->ShowMultiAccountPicker(idp_data);
    }

    std::vector<views::View*> children = dialog()->children();
    ASSERT_EQ(children.size(), 3u);
    PerformHeaderChecks(children[0], expected_title,
                        expect_idp_brand_icon_in_header);

    views::ScrollView* scroller = static_cast<views::ScrollView*>(children[2]);
    ASSERT_FALSE(scroller->children().empty());
    views::View* wrapper = scroller->children()[0];
    ASSERT_FALSE(wrapper->children().empty());
    views::View* contents = wrapper->children()[0];

    views::BoxLayout* layout_manager =
        static_cast<views::BoxLayout*>(contents->GetLayoutManager());
    EXPECT_TRUE(layout_manager);
    EXPECT_EQ(layout_manager->GetOrientation(),
              views::BoxLayout::Orientation::kVertical);
    std::vector<views::View*> accounts = contents->children();

    size_t accounts_index = 0;
    if (expect_idp_row) {
      EXPECT_LT(0u, accounts.size());
      CheckIdpRow(accounts[0u], u"idp-example.com");
      ++accounts_index;
    }

    // Check the text shown.
    CheckAccountRows(accounts, kAccountSuffixes, accounts_index);
    EXPECT_EQ(accounts_index, accounts.size());
  }

  // Checks the account rows starting at `accounts[accounts_index]`. Updates
  // `accounts_index` to the first unused index in `accounts`, or to
  // `accounts.size()` if done.
  void CheckAccountRows(const std::vector<views::View*>& accounts,
                        const std::vector<std::string>& account_suffixes,
                        size_t& accounts_index) {
    EXPECT_GE(accounts.size(), account_suffixes.size());
    for (size_t i = 0; i < std::size(account_suffixes); ++i) {
      ASSERT_STREQ("HoverButton", accounts[accounts_index]->GetClassName());
      HoverButton* account_row =
          static_cast<HoverButton*>(accounts[accounts_index++]);
      ASSERT_TRUE(account_row);
      EXPECT_EQ(GetAccountButtonTitle(account_row),
                base::UTF8ToUTF16(kNameBase + account_suffixes[i]));
      EXPECT_EQ(
          GetAccountButtonSubtitle(account_row)->GetText(),
          base::UTF8ToUTF16(std::string(kEmailBase) + account_suffixes[i]));
      // The subtitle has changed style, so AutoColorReadabilityEnabled should
      // be set.
      EXPECT_TRUE(GetAccountButtonSubtitle(account_row)
                      ->GetAutoColorReadabilityEnabled());
      views::View* icon_view = GetAccountButtonIconView(account_row);
      EXPECT_TRUE(icon_view);
      EXPECT_EQ(icon_view->size(),
                gfx::Size(kDesiredAvatarSize, kDesiredAvatarSize));
    }
  }

  void CheckIdpRow(views::View* idp_account,
                   const std::u16string& expected_idp) {
    // Order: Brand icon, title.
    EXPECT_THAT(GetChildClassNames(idp_account),
                testing::ElementsAre("ImageView", "Label"));

    views::Label* title_view =
        static_cast<views::Label*>(idp_account->children()[1]);
    EXPECT_EQ(title_view->GetText(), expected_idp);
  }

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kFedCm);
    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    delegate_ = std::make_unique<FakeDelegate>(test_web_contents_.get());
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    ChromeViewsTestBase::SetUp();
  }

  void TearDown() override {
    anchor_widget_.reset();
    feature_list_.Reset();
    ChromeViewsTestBase::TearDown();
  }

  AccountSelectionBubbleView* dialog() { return dialog_; }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory()
      const {
    return test_shared_url_loader_factory_;
  }

  raw_ptr<AccountSelectionBubbleView> dialog_;

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> test_web_contents_;

  std::unique_ptr<views::Widget> anchor_widget_;

  std::unique_ptr<FakeDelegate> delegate_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(AccountSelectionBubbleViewTest, SingleAccount) {
  TestSingleAccount(kTitleSignIn, /* expect_idp_brand_icon_in_header=*/true);
}

TEST_F(AccountSelectionBubbleViewTest, SingleAccountNoTermsOfService) {
  const std::string kAccountSuffix = "suffix";
  content::IdentityRequestAccount account = CreateTestIdentityRequestAccount(
      kAccountSuffix, content::IdentityRequestAccount::LoginState::kSignUp);
  CreateSingleAccountPicker(
      /*show_back_button=*/false, account, /*terms_of_service_url=*/"");

  std::vector<views::View*> children = dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSignIn,
                      /*expect_idp_brand_icon_in_header=*/true);

  views::View* single_account_chooser = children[2];
  ASSERT_EQ(single_account_chooser->children().size(), 3u);

  // Check the "Continue as" button.
  views::MdTextButton* button =
      static_cast<views::MdTextButton*>(single_account_chooser->children()[1]);
  ASSERT_TRUE(button);
  EXPECT_EQ(button->GetText(),
            base::UTF8ToUTF16("Continue as " + std::string(kGivenNameBase) +
                              kAccountSuffix));

  views::StyledLabel* disclosure_text =
      static_cast<views::StyledLabel*>(single_account_chooser->children()[2]);
  ASSERT_TRUE(disclosure_text);
  EXPECT_EQ(disclosure_text->GetText(),
            u"To continue, idp-example.com will share your name, email "
            u"address, and profile picture with this site. See this site's "
            u"privacy policy.");
}

TEST_F(AccountSelectionBubbleViewTest, MultipleAccounts) {
  TestMultipleAccounts(kTitleSignIn,
                       /*expect_idp_brand_icon_in_header=*/true,
                       /*expect_idp_row=*/false);
}

TEST_F(AccountSelectionBubbleViewTest, ReturningAccount) {
  const std::string kAccountSuffix = "suffix";
  content::IdentityRequestAccount account = CreateTestIdentityRequestAccount(
      kAccountSuffix, content::IdentityRequestAccount::LoginState::kSignIn);
  CreateSingleAccountPicker(
      /*show_back_button=*/false, account, /*terms_of_service_url=*/"");

  std::vector<views::View*> children = dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSignIn,
                      /*expect_idp_brand_icon_in_header=*/true);

  views::View* single_account_chooser = children[2];
  std::vector<views::View*> chooser_children =
      single_account_chooser->children();
  ASSERT_EQ(chooser_children.size(), 2u);
  views::View* single_account_row = chooser_children[0];

  CheckAccountRow(single_account_row, kAccountSuffix);

  // Check the "Continue as" button.
  views::MdTextButton* button =
      static_cast<views::MdTextButton*>(chooser_children[1]);
  EXPECT_EQ(button->GetText(),
            base::UTF8ToUTF16("Continue as " + std::string(kGivenNameBase) +
                              kAccountSuffix));
}

TEST_F(AccountSelectionBubbleViewTest, Verifying) {
  const std::string kAccountSuffix = "suffix";
  content::IdentityRequestAccount account = CreateTestIdentityRequestAccount(
      kAccountSuffix, content::IdentityRequestAccount::LoginState::kSignIn);
  IdentityProviderDisplayData idp_data(
      kIdpETLDPlusOne, content::IdentityProviderMetadata(),
      content::ClientMetadata(GURL(), GURL()), {account});

  CreateAccountSelectionBubble(/*exclude_title=*/false, /*exclude_iframe=*/true,
                               /*show_auto_reauthn_checkbox=*/false);
  dialog_->ShowVerifyingSheet(
      account, idp_data, l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE));

  const std::vector<views::View*> children = dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSigningIn,
                      /*expect_idp_brand_icon_in_header=*/true);

  views::View* row_container = dialog()->children()[2];
  ASSERT_EQ(row_container->children().size(), 1u);
  CheckAccountRow(row_container->children()[0], kAccountSuffix);
}

TEST_F(AccountSelectionBubbleViewTest, VerifyingForAutoReauthn) {
  const std::string kAccountSuffix = "suffix";
  content::IdentityRequestAccount account = CreateTestIdentityRequestAccount(
      kAccountSuffix, content::IdentityRequestAccount::LoginState::kSignIn);
  IdentityProviderDisplayData idp_data(
      kIdpETLDPlusOne, content::IdentityProviderMetadata(),
      content::ClientMetadata(GURL(), GURL()), {account});

  CreateAccountSelectionBubble(/*exclude_title=*/false, /*exclude_iframe=*/true,
                               /*show_auto_reauthn_checkbox=*/false);
  const auto title =
      l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE_AUTO_REAUTHN);
  dialog_->ShowVerifyingSheet(account, idp_data, title);

  const std::vector<views::View*> children = dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSigningInWithAutoReauthn,
                      /*expect_idp_brand_icon_in_header=*/true);

  views::View* row_container = dialog()->children()[2];
  ASSERT_EQ(row_container->children().size(), 1u);
  CheckAccountRow(row_container->children()[0], kAccountSuffix);
}

TEST_F(AccountSelectionBubbleViewTest, AutoReauthnCheckboxDisplayed) {
  const std::string kAccountSuffix = "suffix";
  content::IdentityRequestAccount account = CreateTestIdentityRequestAccount(
      {kAccountSuffix}, content::IdentityRequestAccount::LoginState::kSignUp);
  CreateSingleAccountPicker(
      /*show_back_button=*/false, account, /*terms_of_service_url=*/"",
      /*show_auto_reauthn_checkbox=*/true);

  std::vector<views::View*> children = dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSignIn,
                      /*expect_idp_brand_icon_in_header=*/true);

  views::View* single_account_chooser = children[2];
  ASSERT_EQ(single_account_chooser->children().size(), 4u);

  // Check the "Continue as" button.
  views::MdTextButton* button =
      static_cast<views::MdTextButton*>(single_account_chooser->children()[1]);
  ASSERT_TRUE(button);
  EXPECT_EQ(button->GetText(),
            base::UTF8ToUTF16("Continue as " + std::string(kGivenNameBase) +
                              kAccountSuffix));

  // Check the auto re-authn checkbox.
  views::Checkbox* checkbox =
      static_cast<views::Checkbox*>(single_account_chooser->children()[2]);

  ASSERT_TRUE(checkbox->GetEnabled());
}

// Tests that when an iframe URL is provided, it is appropriately added to the
// header.
TEST_F(AccountSelectionBubbleViewTest, IframeSubtitleInHeader) {
  const std::string kAccountSuffix = "suffix";
  content::IdentityRequestAccount account = CreateTestIdentityRequestAccount(
      {kAccountSuffix}, content::IdentityRequestAccount::LoginState::kSignUp);
  CreateSingleAccountPicker(
      /*show_back_button=*/false, account, /*terms_of_service_url=*/"",
      /*show_auto_reauthn_checkbox=*/false, /*exclude_iframe=*/false);

  std::vector<views::View*> children = dialog()->children();
  ASSERT_EQ(children.size(), 3u);

  // Perform some basic dialog checks.
  EXPECT_FALSE(dialog()->ShouldShowCloseButton());
  EXPECT_FALSE(dialog()->ShouldShowWindowTitle());

  EXPECT_FALSE(dialog()->GetOkButton());
  EXPECT_FALSE(dialog()->GetCancelButton());

  views::View* header = children[0];
  EXPECT_THAT(GetChildClassNames(header),
              testing::ElementsAreArray({"View", "Label"}));
  ASSERT_EQ(header->children().size(), 2u);
  // Order: Potentially hidden IDP brand icon, potentially hidden back button,
  // title, close button.
  views::View* inner_header = header->children()[0];
  std::vector<std::string> expected_class_names = {"ImageView", "ImageButton",
                                                   "Label", "ImageButton"};
  EXPECT_THAT(GetChildClassNames(inner_header),
              testing::ElementsAreArray(expected_class_names));

  // Check title text.
  views::Label* title_view =
      static_cast<views::Label*>(GetViewWithClassName(inner_header, "Label"));
  ASSERT_TRUE(title_view);
  EXPECT_EQ(title_view->GetText(),
            u"Sign in to iframe-example.com with idp-example.com");

  // Check subtitle text.
  views::Label* subtitle_view =
      static_cast<views::Label*>(header->children()[1]);
  ASSERT_TRUE(subtitle_view);
  EXPECT_EQ(subtitle_view->GetText(), u"on top-frame-example.com");
}

class MultipleIdpAccountSelectionBubbleViewTest
    : public AccountSelectionBubbleViewTest {
 public:
  MultipleIdpAccountSelectionBubbleViewTest() = default;

 protected:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(
        features::kFedCmMultipleIdentityProviders);
    AccountSelectionBubbleViewTest::SetUp();
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// Tests that the single account case is the same with
// features::kFedCmMultipleIdentityProviders enabled. See
// AccountSelectionBubbleViewTest's SingleAccount test.
TEST_F(MultipleIdpAccountSelectionBubbleViewTest, SingleAccount) {
  TestSingleAccount(kTitleSignIn, /*expect_idp_brand_icon_in_header=*/true);
}

// Tests that when there is multiple accounts but only one IDP, the UI is
// exactly the same with features::kFedCmMultipleIdentityProviders enabled (see
// AccountSelectionBubbleViewTest's MultipleAccounts test).
TEST_F(MultipleIdpAccountSelectionBubbleViewTest, MultipleAccountsSingleIdp) {
  TestMultipleAccounts(kTitleSignIn,
                       /*expect_idp_brand_icon_in_header=*/true,
                       /*expect_idp_row=*/false);
}

// Tests that the logo is visible with features::kFedCmMultipleIdentityProviders
// enabled and multiple IDPs.
TEST_F(MultipleIdpAccountSelectionBubbleViewTest,
       MultipleAccountsMultipleIdps) {
  const std::vector<std::string> kAccountSuffixes1 = {"1", "2"};
  const std::vector<std::string> kAccountSuffixes2 = {"3", "4"};
  std::vector<IdentityProviderDisplayData> idp_data;
  std::vector<Account> accounts_first_idp = CreateTestIdentityRequestAccounts(
      kAccountSuffixes1, content::IdentityRequestAccount::LoginState::kSignUp);
  idp_data.emplace_back(kIdpETLDPlusOne, content::IdentityProviderMetadata(),
                        CreateTestClientMetadata(kTermsOfServiceUrl),
                        accounts_first_idp);
  idp_data.emplace_back(
      u"idp2.com", content::IdentityProviderMetadata(),
      CreateTestClientMetadata("https://tos-2.com"),
      CreateTestIdentityRequestAccounts(
          kAccountSuffixes2,
          content::IdentityRequestAccount::LoginState::kSignUp));
  CreateMultiIdpAccountPicker(idp_data);

  std::vector<views::View*> children = dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSignInWithoutIdp,
                      /*expect_idp_brand_icon_in_header=*/false);

  views::ScrollView* scroller = static_cast<views::ScrollView*>(children[2]);
  ASSERT_FALSE(scroller->children().empty());
  views::View* wrapper = scroller->children()[0];
  ASSERT_FALSE(wrapper->children().empty());
  views::View* contents = wrapper->children()[0];

  views::BoxLayout* layout_manager =
      static_cast<views::BoxLayout*>(contents->GetLayoutManager());
  EXPECT_TRUE(layout_manager);
  EXPECT_EQ(layout_manager->GetOrientation(),
            views::BoxLayout::Orientation::kVertical);
  std::vector<views::View*> accounts = contents->children();

  // There should be 6 rows: 3 for the first IDP, 3 for the second.
  EXPECT_EQ(6u, accounts.size());

  // Check the first IDP.
  CheckIdpRow(accounts[0u], u"idp-example.com");
  size_t accounts_index = 1;
  CheckAccountRows(accounts, kAccountSuffixes1, accounts_index);

  // Check the second IDP.
  CheckIdpRow(accounts[accounts_index++], u"idp2.com");
  CheckAccountRows(accounts, kAccountSuffixes2, accounts_index);
}
