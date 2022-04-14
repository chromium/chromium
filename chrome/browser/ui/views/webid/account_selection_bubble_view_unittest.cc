// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/hover_button.h"
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
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace {

constexpr char kRpETLDPlusOne[] = "rp-example.com";
constexpr char kIdpETLDPlusOne[] = "idp-example.com";
const std::u16string kTitle = u"Sign in to rp-example.com with idp-example.com";

}  // namespace

class FakeDelegate : public AccountSelectionView::Delegate {
 public:
  explicit FakeDelegate(content::WebContents* web_contents)
      : web_contents_(web_contents) {}

  ~FakeDelegate() override = default;

  void OnAccountSelected(const Account& account) override {}

  void OnDismiss() override {}

  gfx::NativeView GetNativeView() override { return gfx::kNullNativeView; }

  content::WebContents* GetWebContents() override { return web_contents_; }

 private:
  content::WebContents* web_contents_;
};

class AccountSelectionBubbleViewTest : public ChromeViewsTestBase {
 public:
  AccountSelectionBubbleViewTest() = default;

 protected:
  void CreateSingleAccountViewAndShow(bool exclude_terms_of_service = false) {
    const content::IdentityRequestAccount accounts[] = {
        {"id", "email", "name", "given_name", GURL::EmptyGURL()}};
    CreateViewAndShow(accounts, exclude_terms_of_service);
  }

  void CreateMultipleAccountViewAndShow() {
    const content::IdentityRequestAccount accounts[] = {
        {"id0", "email0", "name0", "given_name0", GURL::EmptyGURL()},
        {"id1", "email1", "name1", "given_name1", GURL::EmptyGURL()},
        {"id2", "email2", "name2", "given_name2", GURL::EmptyGURL()}};
    CreateViewAndShow(accounts);
  }

  void CreateViewAndShow(
      const base::span<const content::IdentityRequestAccount>& accounts,
      bool exclude_terms_of_service = false) {
    content::IdentityProviderMetadata idp_metadata;
    const GURL kPrivacyPolicyURL = GURL("htpps://privacy-policy.com");
    const GURL kTermsOfServiceURL = GURL("htpps://terms-of-service.com");
    content::ClientIdData client_data(
        exclude_terms_of_service ? GURL::EmptyGURL() : kTermsOfServiceURL,
        kPrivacyPolicyURL);
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::TYPE_WINDOW);
    params.ownership = views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET;

    anchor_widget_ = std::make_unique<views::Widget>();
    anchor_widget_->Init(std::move(params));
    anchor_widget_->Show();
    dialog_ = new AccountSelectionBubbleView(
        delegate_.get(), kRpETLDPlusOne, kIdpETLDPlusOne, accounts,
        idp_metadata, client_data, anchor_widget_->GetContentsView(),
        shared_url_loader_factory(), nullptr);
    views::BubbleDialogDelegateView::CreateBubble(dialog_)->Show();
  }

  void CheckAccountRow(views::View* view,
                       std::u16string name,
                       std::u16string email) {}

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
  // testing::NiceMock<MockEditAddressProfileDialogController> mock_controller_;
};

TEST_F(AccountSelectionBubbleViewTest, SingleAccount) {
  CreateSingleAccountViewAndShow();

  // Perform some basic dialog checks.
  EXPECT_TRUE(dialog()->ShouldShowCloseButton());
  EXPECT_TRUE(dialog()->ShouldShowWindowTitle());

  EXPECT_FALSE(dialog()->GetOkButton());
  EXPECT_FALSE(dialog()->GetCancelButton());

  EXPECT_EQ(dialog()->GetWindowTitle(), kTitle);

  // Check basic structure.
  std::vector<views::View*> children = dialog()->children();
  ASSERT_EQ(children.size(), 2u);
  views::Separator* separator = static_cast<views::Separator*>(children[0]);
  EXPECT_TRUE(separator);
  views::View* single_account_chooser = children[1];
  ASSERT_EQ(single_account_chooser->children().size(), 3u);
  views::View* account_row = single_account_chooser->children()[0];
  std::vector<views::View*> row_children = account_row->children();
  ASSERT_EQ(row_children.size(), 2u);
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
  views::Label* name_view = static_cast<views::Label*>(text_view_children[0]);
  ASSERT_TRUE(name_view);
  EXPECT_EQ(name_view->GetText(), u"name");
  views::Label* email_view = static_cast<views::Label*>(text_view_children[1]);
  ASSERT_TRUE(email_view);
  EXPECT_EQ(email_view->GetText(), u"email");

  // Check the "Continue as" button.
  views::MdTextButton* button =
      static_cast<views::MdTextButton*>(single_account_chooser->children()[1]);
  ASSERT_TRUE(button);
  EXPECT_EQ(button->GetText(), u"Continue as given_name");

  views::StyledLabel* consent_text =
      static_cast<views::StyledLabel*>(single_account_chooser->children()[2]);
  ASSERT_TRUE(consent_text);
  EXPECT_EQ(consent_text->GetText(),
            u"To continue, idp-example.com will share your name, email "
            u"address, and profile picture with this site. See this site's "
            u"privacy policy and terms of service.");
}

TEST_F(AccountSelectionBubbleViewTest, SingleAccountNoTermsOfService) {
  CreateSingleAccountViewAndShow(true /*=exclude_terms_of_service*/);

  // Check basic structure
  std::vector<views::View*> children = dialog()->children();
  ASSERT_EQ(children.size(), 2u);
  views::Separator* separator = static_cast<views::Separator*>(children[0]);
  EXPECT_TRUE(separator);
  views::View* single_account_chooser = children[1];
  ASSERT_EQ(single_account_chooser->children().size(), 3u);

  // Check the "Continue as" button.
  views::MdTextButton* button =
      static_cast<views::MdTextButton*>(single_account_chooser->children()[1]);
  ASSERT_TRUE(button);
  EXPECT_EQ(button->GetText(), u"Continue as given_name");

  views::StyledLabel* consent_text =
      static_cast<views::StyledLabel*>(single_account_chooser->children()[2]);
  ASSERT_TRUE(consent_text);
  EXPECT_EQ(consent_text->GetText(),
            u"To continue, idp-example.com will share your name, email "
            u"address, and profile picture with this site. See this site's "
            u"privacy policy.");
}

TEST_F(AccountSelectionBubbleViewTest, MultipleAccounts) {
  CreateMultipleAccountViewAndShow();

  // Perform some basic dialog checks.
  EXPECT_TRUE(dialog()->ShouldShowCloseButton());
  EXPECT_TRUE(dialog()->ShouldShowWindowTitle());

  EXPECT_FALSE(dialog()->GetOkButton());
  EXPECT_FALSE(dialog()->GetCancelButton());

  EXPECT_EQ(dialog()->GetWindowTitle(), kTitle);

  // Check basic structure.
  std::vector<views::View*> children = dialog()->children();
  ASSERT_EQ(children.size(), 2u);
  views::Separator* separator = static_cast<views::Separator*>(children[0]);
  EXPECT_TRUE(separator);
  views::View* multiple_account_chooser = children[1];
  views::BoxLayout* layout_manager = static_cast<views::BoxLayout*>(
      multiple_account_chooser->GetLayoutManager());
  EXPECT_TRUE(layout_manager);
  EXPECT_EQ(layout_manager->GetOrientation(),
            views::BoxLayout::Orientation::kVertical);
  std::vector<views::View*> accounts = multiple_account_chooser->children();
  ASSERT_EQ(accounts.size(), 3u);

  // Check the text shown.
  struct {
    std::u16string name;
    std::u16string email;
  } expected[3] = {
      {u"name0", u"email0"},
      {u"name1", u"email1"},
      {u"name2", u"email2"},
  };
  for (size_t i = 0; i < 3; ++i) {
    HoverButton* account_row = static_cast<HoverButton*>(accounts[i]);
    ASSERT_TRUE(account_row);
    EXPECT_EQ(account_row->title()->GetText(), expected[i].name);
    EXPECT_EQ(account_row->subtitle()->GetText(), expected[i].email);
  }
}
