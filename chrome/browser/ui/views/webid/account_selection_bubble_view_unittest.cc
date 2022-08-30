// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"

#include <string>

#include "base/bind.h"
#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/webid/fake_delegate.h"
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
#include "ui/events/base_event_utils.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"

namespace {

const std::u16string kRpETLDPlusOne = u"rp-example.com";
const std::u16string kIdpETLDPlusOne = u"idp-example.com";
const std::u16string kTitleSignIn =
    u"Sign in to rp-example.com with idp-example.com";
const std::u16string kTitleSignInWithoutIdp = u"Sign in to rp-example.com";
const std::u16string kTitleSigningIn = u"Verifying…";

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

content::ClientIdData CreateTestClientIdData(
    const std::string& terms_of_service_url) {
  return content::ClientIdData((GURL(terms_of_service_url)),
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
  void CreateAccountSelectionBubble() {
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

    anchor_widget_ = std::make_unique<views::Widget>();
    anchor_widget_->Init(std::move(params));
    anchor_widget_->Show();

    dialog_ = new AccountSelectionBubbleView(
        kRpETLDPlusOne, kIdpETLDPlusOne, anchor_widget_->GetContentsView(),
        shared_url_loader_factory(), /*observer=*/nullptr);
    views::BubbleDialogDelegateView::CreateBubble(dialog_)->Show();
  }

  void CreateBubbleWithAccountPicker(
      bool show_back_button,
      base::span<const content::IdentityRequestAccount> accounts,
      const std::string& terms_of_service_url) {
    CreateAccountSelectionBubble();
    dialog_->ShowAccountPicker(kIdpETLDPlusOne,
                               /*show_back_button=*/show_back_button, accounts,
                               content::IdentityProviderMetadata(),
                               CreateTestClientIdData(terms_of_service_url));
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
    std::vector<content::IdentityRequestAccount> accounts =
        CreateTestIdentityRequestAccounts(
            {kAccountSuffix},
            content::IdentityRequestAccount::LoginState::kSignUp);
    CreateBubbleWithAccountPicker(
        /*show_back_button=*/false, accounts, kTermsOfServiceUrl);

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
    std::vector<content::IdentityRequestAccount> account_list =
        CreateTestIdentityRequestAccounts(
            kAccountSuffixes,
            content::IdentityRequestAccount::LoginState::kSignUp);
    CreateBubbleWithAccountPicker(
        /*show_back_button=*/false, account_list, /*terms_of_service_url=*/"");

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

    if (expect_idp_row) {
      EXPECT_LT(0u, accounts.size());
      CheckIdpRow(accounts[0u]);
      accounts.erase(accounts.begin());
    }

    // Check the text shown.
    CheckAccountRows(accounts, kAccountSuffixes);
  }

  void CheckAccountRows(const std::vector<views::View*>& accounts,
                        const std::vector<std::string>& account_suffixes) {
    size_t account_index = 0;
    EXPECT_EQ(accounts.size(), account_suffixes.size());
    for (size_t i = 0; i < std::size(account_suffixes); ++i) {
      ASSERT_STREQ("HoverButton", accounts[account_index]->GetClassName());
      HoverButton* account_row =
          static_cast<HoverButton*>(accounts[account_index++]);
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

  void CheckIdpRow(views::View* idp_account) {
    // Order: Brand icon, title.
    EXPECT_THAT(GetChildClassNames(idp_account),
                testing::ElementsAre("ImageView", "Label"));

    views::Label* title_view =
        static_cast<views::Label*>(idp_account->children()[1]);
    EXPECT_EQ(title_view->GetText(), u"idp-example.com");
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
  std::vector<content::IdentityRequestAccount> accounts =
      CreateTestIdentityRequestAccounts(
          {kAccountSuffix},
          content::IdentityRequestAccount::LoginState::kSignUp);
  CreateBubbleWithAccountPicker(
      /*show_back_button=*/false, accounts, /*terms_of_service_url=*/"");

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
  std::vector<content::IdentityRequestAccount> accounts =
      CreateTestIdentityRequestAccounts(
          {kAccountSuffix},
          content::IdentityRequestAccount::LoginState::kSignIn);
  CreateBubbleWithAccountPicker(
      /*show_back_button=*/false, accounts, /*terms_of_service_url=*/"");

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

  CreateAccountSelectionBubble();
  dialog_->ShowVerifyingSheet(account, content::IdentityProviderMetadata());

  const std::vector<views::View*> children = dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSigningIn,
                      /*expect_idp_brand_icon_in_header=*/true);

  views::View* row_container = dialog()->children()[2];
  ASSERT_EQ(row_container->children().size(), 1u);
  CheckAccountRow(row_container->children()[0], kAccountSuffix);
}

class MultipleIDPAccountSelectionBubbleViewTest
    : public AccountSelectionBubbleViewTest {
 public:
  MultipleIDPAccountSelectionBubbleViewTest() = default;

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
// features::kFedCmMultipleIdentityProviders enabled.
TEST_F(MultipleIDPAccountSelectionBubbleViewTest, SingleAccount) {
  TestSingleAccount(kTitleSignInWithoutIdp,
                    /*expect_idp_brand_icon_in_header=*/false);
}

// Tests that the logo is visible with features::kFedCmMultipleIdentityProviders
// enabled.
TEST_F(MultipleIDPAccountSelectionBubbleViewTest, MultipleAccounts) {
  TestMultipleAccounts(kTitleSignInWithoutIdp,
                       /*expect_idp_brand_icon_in_header=*/false,
                       /*expect_idp_row=*/true);
}
