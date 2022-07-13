// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"

#include <string>

#include "base/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/hover_button.h"
#include "chrome/browser/ui/views/webid/fake_delegate.h"
#include "chrome/browser/ui/webid/identity_dialog_controller.h"
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
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/test/button_test_api.h"
#include "ui/views/view.h"

namespace {

constexpr char kRpETLDPlusOne[] = "rp-example.com";
constexpr char kIdpETLDPlusOne[] = "idp-example.com";
const std::u16string kTitleSignIn =
    u"Sign in to rp-example.com with idp-example.com";
const std::u16string kTitleSigningIn = u"Verifying…";

void MockAccountSelectedCallback(
    const content::IdentityRequestAccount& selected_account) {}

constexpr char kIdBase[] = "id";
constexpr char kEmailBase[] = "email";
constexpr char kNameBase[] = "name";
constexpr char kGivenNameBase[] = "given_name";

const char kTermsOfServiceUrl[] = "htpps://terms-of-service.com";

}  // namespace

class AccountSelectionBubbleViewTest : public ChromeViewsTestBase {
 public:
  AccountSelectionBubbleViewTest() = default;

 protected:
  void CreateViewAndShow(
      const std::vector<std::string>& account_suffixes,
      absl::optional<content::IdentityRequestAccount::LoginState> login_state =
          absl::nullopt,
      const GURL& terms_of_service_url = GURL(kTermsOfServiceUrl)) {
    std::vector<content::IdentityRequestAccount> accounts;
    for (const std::string& account_suffix : account_suffixes) {
      accounts.emplace_back(std::string(kIdBase) + account_suffix,
                            std::string(kEmailBase) + account_suffix,
                            std::string(kNameBase) + account_suffix,
                            std::string(kGivenNameBase) + account_suffix,
                            GURL::EmptyGURL(), login_state);
    }

    content::IdentityProviderMetadata idp_metadata;
    const GURL kPrivacyPolicyURL = GURL("htpps://privacy-policy.com");
    content::ClientIdData client_data(terms_of_service_url, kPrivacyPolicyURL);
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

    anchor_widget_ = std::make_unique<views::Widget>();
    anchor_widget_->Init(std::move(params));
    anchor_widget_->Show();

    dialog_ = new AccountSelectionBubbleView(
        kRpETLDPlusOne, kIdpETLDPlusOne, accounts, idp_metadata, client_data,
        anchor_widget_->GetContentsView(), shared_url_loader_factory(), nullptr,
        base::BindOnce(&MockAccountSelectedCallback));
    views::BubbleDialogDelegateView::CreateBubble(dialog_)->Show();
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

  void PerformHeaderChecks(views::View* header, const std::u16string& title) {
    // Perform some basic dialog checks.
    EXPECT_FALSE(dialog()->ShouldShowCloseButton());
    EXPECT_FALSE(dialog()->ShouldShowWindowTitle());

    EXPECT_FALSE(dialog()->GetOkButton());
    EXPECT_FALSE(dialog()->GetCancelButton());

    std::vector<views::View*> header_children = header->children();
    ASSERT_EQ(header_children.size(), 3u);

    // Potentially hidden back button.
    EXPECT_STREQ("ImageButton", header_children[0]->GetClassName());

    // Check title text.
    views::Label* title_view = static_cast<views::Label*>(header_children[1]);
    ASSERT_TRUE(title_view);
    EXPECT_EQ(title_view->GetText(), title);

    // Check close button.
    EXPECT_STREQ("ImageButton", header_children[2]->GetClassName());

    // Check separator.
    if (title == kTitleSignIn) {
      EXPECT_STREQ("Separator", dialog()->children()[1]->GetClassName());
    } else if (title == kTitleSigningIn) {
      EXPECT_STREQ("ProgressBar", dialog()->children()[1]->GetClassName());
    }
  }

  // Test that the bubble is showing the single account selection.
  void TestAtSingleAccountChooser(const std::string& account_suffix,
                                  bool expected_disclosure,
                                  bool click_button) {
    std::vector<views::View*> children = dialog()->children();
    ASSERT_EQ(children.size(), 3u);
    PerformHeaderChecks(children[0], kTitleSignIn);

    views::View* single_account_chooser = children[2];
    std::vector<views::View*> chooser_children =
        single_account_chooser->children();
    ASSERT_EQ(chooser_children.size(), expected_disclosure ? 3u : 2u);
    views::View* single_account_row = chooser_children[0];

    CheckAccountRow(single_account_row, account_suffix);

    // Check the "Continue as" button.
    views::MdTextButton* button =
        static_cast<views::MdTextButton*>(chooser_children[1]);
    EXPECT_EQ(button->GetText(),
              base::UTF8ToUTF16("Continue as " + std::string(kGivenNameBase) +
                                account_suffix));

    if (expected_disclosure) {
      views::StyledLabel* disclosure_text =
          static_cast<views::StyledLabel*>(chooser_children[2]);
      EXPECT_TRUE(
          base::StartsWith(disclosure_text->GetText(), u"To continue,"));
    }

    if (click_button) {
      const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(),
                                 gfx::Point(), ui::EventTimeForNow(), 0, 0);
      views::test::ButtonTestApi(button).NotifyClick(event);
    }
  }

  // Test that the bubble is showing the multiple account picker. Clicks the
  // accounts at `click_index`.
  void TestAtMultipleAccountChooser(size_t num_expected_accounts,
                                    size_t click_index) {
    std::vector<views::View*> children = dialog()->children();
    ASSERT_EQ(children.size(), 3u);

    PerformHeaderChecks(children[0], kTitleSignIn);

    views::ScrollView* scroller = static_cast<views::ScrollView*>(children[2]);
    ASSERT_FALSE(scroller->children().empty());
    views::View* wrapper = scroller->children()[0];
    ASSERT_FALSE(wrapper->children().empty());
    views::View* multiple_account_chooser = wrapper->children()[0];

    std::vector<views::View*> accounts = multiple_account_chooser->children();
    ASSERT_EQ(accounts.size(), num_expected_accounts);
    for (views::View* account : accounts)
      ASSERT_STREQ("HoverButton", account->GetClassName());

    HoverButton* button = static_cast<HoverButton*>(accounts[click_index]);
    const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                               ui::EventTimeForNow(), 0, 0);
    views::test::ButtonTestApi(button).NotifyClick(event);
  }

  void TestAtVerifyingScreen(const std::string& account_suffix) {
    const std::vector<views::View*> children = dialog()->children();
    ASSERT_EQ(children.size(), 3u);
    PerformHeaderChecks(children[0], kTitleSigningIn);

    views::View* row_container = dialog()->children()[2];
    ASSERT_EQ(row_container->children().size(), 1u);
    CheckAccountRow(row_container->children()[0], account_suffix);
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
    ChromeViewsTestBase::TearDown();
  }

  AccountSelectionBubbleView* dialog() { return dialog_; }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory()
      const {
    return test_shared_url_loader_factory_;
  }

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> test_web_contents_;

  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<AccountSelectionBubbleView> dialog_;

  std::unique_ptr<FakeDelegate> delegate_;
  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(AccountSelectionBubbleViewTest, SingleAccount) {
  const std::string kAccountSuffix = "suffix";
  CreateViewAndShow({kAccountSuffix});

  std::vector<views::View*> children = dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSignIn);

  views::View* single_account_chooser = children[2];
  ASSERT_EQ(single_account_chooser->children().size(), 3u);

  CheckAccountRow(single_account_chooser->children()[0], kAccountSuffix);

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
            u"privacy policy and terms of service.");
}

TEST_F(AccountSelectionBubbleViewTest, SingleAccountNoTermsOfService) {
  const std::string kAccountSuffix = "suffix";
  CreateViewAndShow({kAccountSuffix},
                    /*login_state=*/absl::nullopt,
                    /*terms_of_service_url=*/GURL::EmptyGURL());

  std::vector<views::View*> children = dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSignIn);

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
  const std::vector<std::string> kAccountSuffixes = {"0", "1", "2"};
  CreateViewAndShow(kAccountSuffixes);

  std::vector<views::View*> children = dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSignIn);

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
  ASSERT_EQ(accounts.size(), 3u);

  // Check the text shown.
  for (size_t i = 0; i < std::size(kAccountSuffixes); ++i) {
    HoverButton* account_row = static_cast<HoverButton*>(accounts[i]);
    ASSERT_TRUE(account_row);
    EXPECT_EQ(account_row->title()->GetText(),
              base::UTF8ToUTF16(kNameBase + kAccountSuffixes[i]));
    EXPECT_EQ(account_row->subtitle()->GetText(),
              base::UTF8ToUTF16(std::string(kEmailBase) + kAccountSuffixes[i]));
    // The subtitle has changed style, so AutoColorReadabilityEnabled should be
    // set.
    EXPECT_TRUE(account_row->subtitle()->GetAutoColorReadabilityEnabled());
  }
}

TEST_F(AccountSelectionBubbleViewTest, MultipleAccountsFlow) {
  // Create multiple account view.
  const std::vector<std::string> kAccountSuffixes = {"0", "1", "2"};
  CreateViewAndShow(kAccountSuffixes);
  TestAtMultipleAccountChooser(3u, /*click_index=*/1u);
  TestAtSingleAccountChooser(kAccountSuffixes[1u],
                             /*expected_disclosure=*/true,
                             /*click_button=*/true);
  TestAtVerifyingScreen(kAccountSuffixes[1u]);
}

// Test that clicking 'back' on the consent page in the multi-account signup
// flow brings the user back to the account chooser.
TEST_F(AccountSelectionBubbleViewTest, MultipleAccountsFlowClickBack) {
  const std::vector<std::string> kAccountSuffixes = {"0", "1", "2"};
  CreateViewAndShow(kAccountSuffixes);

  // Button should not be visible in multi account chooser.
  views::Button* back_button =
      static_cast<views::Button*>(dialog()->children()[0]->children()[0]);
  EXPECT_FALSE(back_button->GetVisible());

  TestAtMultipleAccountChooser(3u, /*click_index=*/1u);
  TestAtSingleAccountChooser(kAccountSuffixes[1u],
                             /*expected_disclosure=*/true,
                             /*click_button=*/false);

  // Button should be visible after navigating to consent screen.
  EXPECT_TRUE(back_button->GetVisible());

  const ui::MouseEvent event(ui::ET_MOUSE_PRESSED, gfx::Point(), gfx::Point(),
                             ui::EventTimeForNow(), 0, 0);
  views::test::ButtonTestApi(back_button).NotifyClick(event);

  TestAtMultipleAccountChooser(3u, /*click_index=*/2u);
  TestAtSingleAccountChooser(kAccountSuffixes[2u],
                             /*expected_disclosure=*/true,
                             /*click_button=*/true);
  TestAtVerifyingScreen(kAccountSuffixes[2u]);
}

TEST_F(AccountSelectionBubbleViewTest, ReturningAccount) {
  const std::string kAccountSuffix = "";
  CreateViewAndShow({kAccountSuffix},
                    content::IdentityRequestAccount::LoginState::kSignIn);
  TestAtSingleAccountChooser(kAccountSuffix,
                             /*expected_disclosure=*/false,
                             /*click_button=*/false);
}

TEST_F(AccountSelectionBubbleViewTest, MultipleReturningAccounts) {
  const std::vector<std::string> kAccountSuffixes = {"0", "1", "2"};
  CreateViewAndShow(kAccountSuffixes,
                    content::IdentityRequestAccount::LoginState::kSignIn);
  TestAtMultipleAccountChooser(3u, /*click_index=*/1u);
  TestAtVerifyingScreen(kAccountSuffixes[1]);
}
