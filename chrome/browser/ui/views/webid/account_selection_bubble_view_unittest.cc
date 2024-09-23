// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_bubble_view.h"

#include <array>
#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/webid/account_selection_view_base.h"
#include "chrome/browser/ui/views/webid/account_selection_view_test_base.h"
#include "chrome/browser/ui/views/webid/fake_delegate.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "content/public/browser/identity_request_dialog_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/color_parser.h"
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
#include "ui/views/view_utils.h"

using IdentityProviderDataPtr = scoped_refptr<content::IdentityProviderData>;
using IdentityRequestAccountPtr =
    scoped_refptr<content::IdentityRequestAccount>;
using LoginState = content::IdentityRequestAccount::LoginState;

namespace {
constexpr char kAccountSuffix[] = "suffix";
}  // namespace

class AccountSelectionBubbleViewTest : public ChromeViewsTestBase,
                                       public AccountSelectionViewTestBase {
 public:
  AccountSelectionBubbleViewTest() {
    content::IdentityProviderMetadata idp_metadata;
    idp_metadata.brand_icon_url = GURL(kIdpBrandIconUrl);
    idp_data_ = base::MakeRefCounted<content::IdentityProviderData>(
        kIdpForDisplay, idp_metadata, CreateTestClientMetadata(),
        blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
        /*has_login_status_mismatch=*/false);
  }

 protected:
  void CreateAccountSelectionBubble(bool exclude_title) {
    views::Widget::InitParams params =
        CreateParams(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET,
                     views::Widget::InitParams::TYPE_WINDOW);

    anchor_widget_ = std::make_unique<views::Widget>();
    anchor_widget_->Init(std::move(params));
    anchor_widget_->Show();

    std::optional<std::u16string> title =
        exclude_title ? std::nullopt
                      : std::make_optional<std::u16string>(kIdpETLDPlusOne);
    dialog_ = new AccountSelectionBubbleView(
        kRpETLDPlusOne, title, blink::mojom::RpContext::kSignIn,
        test_web_contents_.get(), anchor_widget_->GetContentsView(),
        shared_url_loader_factory(),
        /*observer=*/nullptr, /*widget_observer=*/nullptr);
  }

  void CreateAndShowSingleAccountPicker(
      LoginState login_state = LoginState::kSignUp) {
    IdentityRequestAccountPtr account = CreateTestIdentityRequestAccount(
        kAccountSuffix, idp_data_, login_state);

    CreateAccountSelectionBubble(/*exclude_title=*/false);
    account->identity_provider = idp_data_;
    dialog_->ShowSingleAccountConfirmDialog(*account,
                                            /*show_back_button=*/false);
  }

  void CreateAndShowMultiAccountPicker(
      const std::vector<std::string>& account_suffixes,
      bool supports_add_account = false) {
    idp_data_->idp_metadata.supports_add_account = supports_add_account;
    std::vector<IdentityRequestAccountPtr> account_list =
        CreateTestIdentityRequestAccounts(account_suffixes, idp_data_);

    CreateAccountSelectionBubble(/*exclude_title=*/false);
    dialog_->ShowMultiAccountPicker(account_list, {idp_data_},
                                    /*show_back_button=*/false,
                                    /*is_choose_an_account=*/false);
  }

  void CreateAndShowMultiIdpAccountPicker(
      const std::vector<IdentityRequestAccountPtr>& accounts,
      const std::vector<IdentityProviderDataPtr>& idp_list) {
    CreateAccountSelectionBubble(/*exclude_title=*/true);
    dialog_->ShowMultiAccountPicker(accounts, idp_list,
                                    /*show_back_button=*/false,
                                    /*is_choose_an_account=*/false);
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
      expected_class_names.insert(expected_class_names.begin(),
                                  "BrandIconImageView");
    }
    EXPECT_THAT(GetChildClassNames(header),
                testing::ElementsAreArray(expected_class_names));

    // Check title text.
    views::Label* title_view =
        static_cast<views::Label*>(GetViewWithClassName(header, "Label"));
    ASSERT_TRUE(title_view);
    EXPECT_EQ(title_view->GetText(), expected_title);

    if (expect_idp_brand_icon_in_header) {
      views::ImageView* idp_brand_icon = static_cast<views::ImageView*>(
          GetViewWithClassName(header, "BrandIconImageView"));
      ASSERT_TRUE(idp_brand_icon);
      EXPECT_TRUE(idp_brand_icon->GetVisible());
    }
  }

  void PerformMultiAccountChecks(views::View* container,
                                 size_t expected_account_rows,
                                 size_t expected_mismatch_rows) {
    views::LayoutManager* layout_manager = container->GetLayoutManager();
    ASSERT_TRUE(layout_manager);
    views::BoxLayout* box_layout_manager =
        static_cast<views::BoxLayout*>(layout_manager);
    ASSERT_TRUE(box_layout_manager);
    EXPECT_EQ(box_layout_manager->GetOrientation(),
              views::BoxLayout::Orientation::kVertical);

    std::vector<raw_ptr<views::View, VectorExperimental>> children =
        container->children();
    bool has_account_mismatch_separator =
        expected_account_rows > 0u && expected_mismatch_rows > 0u;
    ASSERT_EQ(children.size(), has_account_mismatch_separator ? 4u : 3u);

    EXPECT_TRUE(IsViewClass<views::Separator>(children[0]));

    EXPECT_TRUE(IsViewClass<views::ScrollView>(children[1]));
    views::ScrollView* accounts_scroller =
        static_cast<views::ScrollView*>(children[1]);
    EXPECT_TRUE(accounts_scroller->GetDrawOverflowIndicator());
    views::View* accounts_contents = accounts_scroller->contents();
    ASSERT_EQ(accounts_contents->children().size(), expected_account_rows);
    layout_manager = accounts_contents->GetLayoutManager();
    ASSERT_TRUE(layout_manager);
    box_layout_manager = static_cast<views::BoxLayout*>(layout_manager);
    ASSERT_TRUE(box_layout_manager);
    EXPECT_EQ(box_layout_manager->GetOrientation(),
              views::BoxLayout::Orientation::kVertical);
    if (expected_account_rows == 0u) {
      EXPECT_EQ(accounts_contents->GetPreferredSize(), gfx::Size());
    }

    if (has_account_mismatch_separator) {
      EXPECT_TRUE(IsViewClass<views::Separator>(children[2]));
    }

    views::View* mismatch_view = children.back();
    EXPECT_TRUE(IsViewClass<views::ScrollView>(mismatch_view));
    views::ScrollView* mismatch_scroller =
        static_cast<views::ScrollView*>(mismatch_view);
    EXPECT_TRUE(mismatch_scroller->GetDrawOverflowIndicator());
    views::View* mismatch_contents = mismatch_scroller->contents();
    layout_manager = mismatch_contents->GetLayoutManager();
    ASSERT_TRUE(layout_manager);
    box_layout_manager = static_cast<views::BoxLayout*>(layout_manager);
    ASSERT_TRUE(box_layout_manager);
    EXPECT_EQ(box_layout_manager->GetOrientation(),
              views::BoxLayout::Orientation::kVertical);
    ASSERT_EQ(mismatch_contents->children().size(), expected_mismatch_rows);
    if (expected_mismatch_rows == 0u) {
      EXPECT_EQ(mismatch_contents->GetPreferredSize(), gfx::Size());
    }
  }

  std::vector<raw_ptr<views::View, VectorExperimental>> GetAccounts(
      views::View* container) {
    return static_cast<views::ScrollView*>(container->children()[1])
        ->contents()
        ->children();
  }

  std::vector<raw_ptr<views::View, VectorExperimental>> GetMismatches(
      views::View* container) {
    return static_cast<views::ScrollView*>(container->children().back())
        ->contents()
        ->children();
  }

  void TestSingleAccount(const std::u16string expected_title,
                         bool expect_idp_brand_icon_in_header) {
    CreateAndShowSingleAccountPicker();

    std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    ASSERT_EQ(children.size(), 3u);
    PerformHeaderChecks(children[0], expected_title,
                        expect_idp_brand_icon_in_header);
    EXPECT_TRUE(IsViewClass<views::Separator>(children[1]));

    views::View* single_account_chooser = children[2];
    ASSERT_EQ(single_account_chooser->children().size(), 3u);

    CheckNonHoverableAccountRow(single_account_chooser->children()[0],
                                kAccountSuffix);

    // Check the "Continue as" button.
    views::MdTextButton* button = static_cast<views::MdTextButton*>(
        single_account_chooser->children()[1]);
    ASSERT_TRUE(button);
    EXPECT_EQ(button->GetText(),
              base::UTF8ToUTF16("Continue as " + std::string(kGivenNameBase) +
                                kAccountSuffix));

    CheckDisclosureText(single_account_chooser->children()[2],
                        /*expect_terms_of_service=*/true,
                        /*expect_privacy_policy=*/true);
  }

  void TestMultipleAccounts(
      const std::u16string& expected_title,
      bool expect_idp_brand_icon_in_header) {
    const std::vector<std::string> kAccountSuffixes = {"0", "1", "2"};
    CreateAndShowMultiAccountPicker(kAccountSuffixes);

    std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    // The separator is in the multiple accounts container.
    ASSERT_EQ(children.size(), 2u);
    PerformHeaderChecks(children[0], expected_title,
                        expect_idp_brand_icon_in_header);

    PerformMultiAccountChecks(children[1], /*expected_account_rows=*/3,
                              /*expected_mismatch_rows=*/0);

    std::vector<raw_ptr<views::View, VectorExperimental>> accounts =
        GetAccounts(children[1]);
    size_t accounts_index = 0;

    // Check the text shown.
    CheckHoverableAccountRows(accounts, kAccountSuffixes, accounts_index);
    EXPECT_EQ(accounts_index, accounts.size());
  }

  void TestFailureDialog(const std::u16string expected_title,
                         bool expect_idp_brand_icon_in_header) {
    CreateAccountSelectionBubble(
        /*exclude_title=*/false);
    dialog_->ShowFailureDialog(kIdpETLDPlusOne, idp_data_->idp_metadata);

    const std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    ASSERT_EQ(children.size(), 3u);

    PerformHeaderChecks(children[0], expected_title,
                        expect_idp_brand_icon_in_header);
    EXPECT_TRUE(IsViewClass<views::Separator>(children[1]));

    const views::View* failure_dialog = children[2];
    const std::vector<raw_ptr<views::View, VectorExperimental>>
        failure_dialog_children = failure_dialog->children();
    ASSERT_EQ(failure_dialog_children.size(), 2u);

    // Check the body shown.
    views::Label* body = static_cast<views::Label*>(failure_dialog_children[0]);
    ASSERT_TRUE(body);
    EXPECT_EQ(body->GetText(),
              u"You can use your idp-example.com account on this site. To "
              u"continue, sign in to idp-example.com.");

    // Check the "Continue" button.
    views::MdTextButton* button =
        static_cast<views::MdTextButton*>(failure_dialog_children[1]);
    ASSERT_TRUE(button);
    EXPECT_EQ(button->GetText(),
              l10n_util::GetStringUTF16(IDS_SIGNIN_CONTINUE));
  }

  void TestErrorDialog(const std::u16string expected_title,
                       const std::u16string expected_summary,
                       const std::u16string expected_description,
                       bool expect_idp_brand_icon_in_header,
                       const std::string& error_code,
                       const GURL& error_url) {
    CreateAccountSelectionBubble(
        /*exclude_title=*/false);
    dialog_->ShowErrorDialog(
        kIdpETLDPlusOne, idp_data_->idp_metadata,
        content::IdentityCredentialTokenError(error_code, error_url));

    const std::vector<raw_ptr<views::View, VectorExperimental>> children =
        dialog()->children();
    ASSERT_EQ(children.size(), 4u);

    PerformHeaderChecks(children[0], expected_title,
                        expect_idp_brand_icon_in_header);
    EXPECT_TRUE(IsViewClass<views::Separator>(children[1]));

    const views::View* error_dialog = children[2];
    const std::vector<raw_ptr<views::View, VectorExperimental>>
        error_dialog_children = error_dialog->children();
    ASSERT_EQ(error_dialog_children.size(), 2u);

    // Check the summary shown.
    views::Label* summary =
        static_cast<views::Label*>(error_dialog_children[0]);
    ASSERT_TRUE(summary);
    EXPECT_EQ(summary->GetText(), expected_summary);

    // Check the description shown.
    views::Label* description =
        static_cast<views::Label*>(error_dialog_children[1]);
    ASSERT_TRUE(description);
    EXPECT_EQ(description->GetText(), expected_description);

    // Check the buttons shown.
    const std::vector<raw_ptr<views::View, VectorExperimental>> button_row =
        children[3]->children();

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

  void CheckMismatchIdp(views::View* idp_row,
                        const std::u16string& expected_idp) {
    ASSERT_STREQ("HoverButton", idp_row->GetClassName());
    HoverButton* idp_button = static_cast<HoverButton*>(idp_row);
    ASSERT_TRUE(idp_button);
    EXPECT_EQ(GetHoverButtonTitle(idp_button), u"Sign in to " + expected_idp);
    EXPECT_EQ(GetHoverButtonSubtitle(idp_button), nullptr);
    ASSERT_TRUE(GetHoverButtonIconView(idp_button));
    // Using GetPreferredSize() since BrandIconImageView uses a fetched image.
    EXPECT_EQ(GetHoverButtonIconView(idp_button)->GetPreferredSize(),
              gfx::Size(kMultiIdpIconSize, kMultiIdpIconSize));
  }

  void CheckUseOtherAccount(
      const std::vector<raw_ptr<views::View, VectorExperimental>>& accounts,
      size_t& accounts_index) {
    EXPECT_TRUE(IsViewClass<views::Separator>(accounts[accounts_index++]));
    views::View* button = accounts[accounts_index++];
    EXPECT_TRUE(IsViewClass<HoverButton>(button));
    HoverButton* idp_button = static_cast<HoverButton*>(button);
    ASSERT_TRUE(idp_button);
    EXPECT_EQ(idp_button->GetText(), u"Use a different account");
  }

  void CheckChooseAnAccount(
      const std::vector<raw_ptr<views::View, VectorExperimental>>& accounts,
      size_t& accounts_index,
      std::u16string expected_subtitle) {
    EXPECT_TRUE(IsViewClass<views::Separator>(accounts[accounts_index++]));
    views::View* button = accounts[accounts_index++];
    EXPECT_TRUE(IsViewClass<HoverButton>(button));
    HoverButton* choose_account_button = static_cast<HoverButton*>(button);
    ASSERT_TRUE(choose_account_button);
    EXPECT_EQ(GetHoverButtonTitle(choose_account_button), u"Choose an account");
    ASSERT_TRUE(GetHoverButtonSubtitle(choose_account_button));
    EXPECT_EQ(GetHoverButtonSubtitle(choose_account_button)->GetText(),
              expected_subtitle);
    ASSERT_TRUE(GetHoverButtonIconView(choose_account_button));
    EXPECT_EQ(GetHoverButtonIconView(choose_account_button)->size(),
              gfx::Size(kMultiIdpIconSize, kMultiIdpIconSize));
  }

  void SetUp() override {
    ChromeViewsTestBase::SetUp();
    feature_list_.InitAndEnableFeature(features::kFedCm);
    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    // The x, y coordinates shouldn't matter but the width and height are set to
    // an arbitrary number that is large enough to fit the bubble to ensure that
    // the bubble is not hidden because the web contents is too small.
    test_web_contents_->Resize(
        gfx::Rect(/*x=*/0, /*y=*/0, /*width=*/1000, /*height=*/1000));
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
  }

  void TearDown() override {
    anchor_widget_.reset();
    feature_list_.Reset();
    ChromeViewsTestBase::TearDown();
  }

  void ResetWebContents() { test_web_contents_.reset(); }

  AccountSelectionBubbleView* dialog() { return dialog_; }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory()
      const {
    return test_shared_url_loader_factory_;
  }

  content::WebContents* web_contents() { return test_web_contents_.get(); }

  raw_ptr<AccountSelectionBubbleView, DanglingUntriaged> dialog_;

  IdentityProviderDataPtr idp_data_;

 private:
  base::test::ScopedFeatureList feature_list_;
  TestingProfile profile_;
  // This enables uses of TestWebContents.
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> test_web_contents_;

  std::unique_ptr<views::Widget> anchor_widget_;

  scoped_refptr<network::SharedURLLoaderFactory>
      test_shared_url_loader_factory_;
  network::TestURLLoaderFactory test_url_loader_factory_;
};

TEST_F(AccountSelectionBubbleViewTest, SingleAccount) {
  TestSingleAccount(kTitleSignIn,
                    /*expect_idp_brand_icon_in_header=*/true);
}

TEST_F(AccountSelectionBubbleViewTest, SingleAccountNoTermsOfService) {
  idp_data_->client_metadata.terms_of_service_url = GURL("");
  CreateAndShowSingleAccountPicker();

  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSignIn,
                      /*expect_idp_brand_icon_in_header=*/true);
  EXPECT_TRUE(IsViewClass<views::Separator>(children[1]));

  views::View* single_account_chooser = children[2];
  ASSERT_EQ(single_account_chooser->children().size(), 3u);

  // Check the "Continue as" button.
  views::MdTextButton* button =
      static_cast<views::MdTextButton*>(single_account_chooser->children()[1]);
  ASSERT_TRUE(button);
  EXPECT_EQ(button->GetText(),
            base::UTF8ToUTF16("Continue as " + std::string(kGivenNameBase) +
                              kAccountSuffix));

  CheckDisclosureText(single_account_chooser->children()[2],
                      /*expect_terms_of_service=*/false,
                      /*expect_privacy_policy=*/true);
}

TEST_F(AccountSelectionBubbleViewTest, SingleAccountOnlyTwoDisclosureFields) {
  idp_data_->disclosure_fields = {
      content::IdentityRequestDialogDisclosureField::kName,
      content::IdentityRequestDialogDisclosureField::kEmail};
  idp_data_->client_metadata.terms_of_service_url = GURL();
  CreateAndShowSingleAccountPicker();

  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSignIn,
                      /*expect_idp_brand_icon_in_header=*/true);
  EXPECT_TRUE(IsViewClass<views::Separator>(children[1]));

  views::View* single_account_chooser = children[2];
  ASSERT_EQ(single_account_chooser->children().size(), 3u);

  // Check the "Continue as" button.
  views::MdTextButton* button =
      static_cast<views::MdTextButton*>(single_account_chooser->children()[1]);
  ASSERT_TRUE(button);
  EXPECT_EQ(button->GetText(),
            base::UTF8ToUTF16("Continue as " + std::string(kGivenNameBase) +
                              kAccountSuffix));

  constexpr char16_t kExpectedText[] =
      u"To continue, idp-example.com will share your name and email address "
      u"with this site. See this site's privacy policy.";

  views::StyledLabel* disclosure_label =
      static_cast<views::StyledLabel*>(single_account_chooser->children()[2]);
  EXPECT_EQ(disclosure_label->GetText(), kExpectedText);
}

TEST_F(AccountSelectionBubbleViewTest, MultipleAccounts) {
  TestMultipleAccounts(kTitleSignIn,
                       /*expect_idp_brand_icon_in_header=*/true);
}

TEST_F(AccountSelectionBubbleViewTest, UseDifferentAccount) {
  const std::vector<std::string> kAccountSuffixes = {"0"};
  CreateAndShowMultiAccountPicker(kAccountSuffixes, true);

  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 2u);
  PerformHeaderChecks(children[0], kTitleSignIn,
                      /*expect_idp_brand_icon_in_header=*/true);

  PerformMultiAccountChecks(children[1], /*expected_account_rows=*/3,
                            /*expected_mismatch_rows=*/0);

  std::vector<raw_ptr<views::View, VectorExperimental>> accounts =
      GetAccounts(children[1]);

  size_t index = 1;
  CheckUseOtherAccount(accounts, index);
}

TEST_F(AccountSelectionBubbleViewTest, ReturningAccount) {
  CreateAndShowSingleAccountPicker(LoginState::kSignIn);

  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSignIn,
                      /*expect_idp_brand_icon_in_header=*/true);
  EXPECT_TRUE(IsViewClass<views::Separator>(children[1]));

  views::View* single_account_chooser = children[2];
  std::vector<raw_ptr<views::View, VectorExperimental>> chooser_children =
      single_account_chooser->children();
  ASSERT_EQ(chooser_children.size(), 2u);
  views::View* single_account_row = chooser_children[0];

  CheckNonHoverableAccountRow(single_account_row, kAccountSuffix);

  // Check the "Continue as" button.
  views::MdTextButton* button =
      static_cast<views::MdTextButton*>(chooser_children[1]);
  EXPECT_EQ(button->GetText(),
            base::UTF8ToUTF16("Continue as " + std::string(kGivenNameBase) +
                              kAccountSuffix));
}

TEST_F(AccountSelectionBubbleViewTest, NewAccountWithoutRequestPermission) {
  idp_data_->disclosure_fields = {};
  CreateAndShowSingleAccountPicker();

  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSignIn,
                      /*expect_idp_brand_icon_in_header=*/true);
  EXPECT_TRUE(IsViewClass<views::Separator>(children[1]));

  views::View* single_account_chooser = children[2];
  std::vector<raw_ptr<views::View, VectorExperimental>> chooser_children =
      single_account_chooser->children();
  ASSERT_EQ(chooser_children.size(), 2u);
  views::View* single_account_row = chooser_children[0];

  CheckNonHoverableAccountRow(single_account_row, kAccountSuffix);

  // Check the "Continue as" button.
  views::MdTextButton* button =
      static_cast<views::MdTextButton*>(chooser_children[1]);
  EXPECT_EQ(button->GetText(),
            base::UTF8ToUTF16("Continue as " + std::string(kGivenNameBase) +
                              kAccountSuffix));
}

TEST_F(AccountSelectionBubbleViewTest,
       ContinueButtonWithProperBackgroundColor) {
  CreateAccountSelectionBubble(/*exclude_title=*/false);

  // Set the dialog background color to white.
  dialog()->set_color(SK_ColorWHITE);

  const std::string kDarkBlue = "#1a73e8";
  SkColor bg_color;
  // A blue background sufficiently contracts with the dialog background.
  content::ParseCssColorString(kDarkBlue, &bg_color);
  idp_data_->idp_metadata.brand_background_color = SkColorSetA(bg_color, 0xff);
  IdentityRequestAccountPtr account = CreateTestIdentityRequestAccount(
      kAccountSuffix, idp_data_, LoginState::kSignIn);

  dialog()->ShowSingleAccountConfirmDialog(*account,
                                           /*show_back_button=*/false);

  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 3u);

  views::View* single_account_chooser = children[2];
  std::vector<raw_ptr<views::View, VectorExperimental>> chooser_children =
      single_account_chooser->children();
  ASSERT_EQ(chooser_children.size(), 2u);

  views::MdTextButton* button =
      static_cast<views::MdTextButton*>(chooser_children[1]);
  ASSERT_TRUE(button);
  EXPECT_EQ(*(button->GetBgColorOverrideDeprecated()), bg_color);
}

TEST_F(AccountSelectionBubbleViewTest,
       ContinueButtonWithImproperBackgroundColor) {
  CreateAccountSelectionBubble(/*exclude_title=*/false);

  // Set the dialog background color to white.
  dialog()->set_color(SK_ColorWHITE);

  const std::string kWhite = "#fff";
  SkColor bg_color;
  // By default a white button does not contrast with the dialog background so
  // the specified color will be ignored.
  content::ParseCssColorString(kWhite, &bg_color);
  content::IdentityProviderMetadata idp_metadata =
      content::IdentityProviderMetadata();
  idp_data_->idp_metadata.brand_background_color = SkColorSetA(bg_color, 0xff);
  IdentityRequestAccountPtr account = CreateTestIdentityRequestAccount(
      kAccountSuffix, idp_data_, LoginState::kSignIn);

  dialog()->ShowSingleAccountConfirmDialog(*account,
                                           /*show_back_button=*/false);

  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 3u);

  views::View* single_account_chooser = children[2];
  std::vector<raw_ptr<views::View, VectorExperimental>> chooser_children =
      single_account_chooser->children();
  ASSERT_EQ(chooser_children.size(), 2u);

  views::MdTextButton* button =
      static_cast<views::MdTextButton*>(chooser_children[1]);
  ASSERT_TRUE(button);
  // The button color is not customized by the IDP.
  EXPECT_FALSE(button->GetBgColorOverrideDeprecated());
}

TEST_F(AccountSelectionBubbleViewTest, Verifying) {
  IdentityRequestAccountPtr account = CreateTestIdentityRequestAccount(
      kAccountSuffix, idp_data_, LoginState::kSignIn);

  CreateAccountSelectionBubble(/*exclude_title=*/false);
  dialog_->ShowVerifyingSheet(
      *account, l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE));

  const std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSigningIn,
                      /*expect_idp_brand_icon_in_header=*/true);
  EXPECT_TRUE(IsViewClass<views::ProgressBar>(children[1]));

  views::View* row_container = dialog()->children()[2];
  ASSERT_EQ(row_container->children().size(), 1u);
  CheckNonHoverableAccountRow(row_container->children()[0], kAccountSuffix);
}

TEST_F(AccountSelectionBubbleViewTest, VerifyingForAutoReauthn) {
  IdentityRequestAccountPtr account = CreateTestIdentityRequestAccount(
      kAccountSuffix, idp_data_, LoginState::kSignIn);
  CreateAccountSelectionBubble(/*exclude_title=*/false);
  const auto title =
      l10n_util::GetStringUTF16(IDS_VERIFY_SHEET_TITLE_AUTO_REAUTHN);
  dialog_->ShowVerifyingSheet(*account, title);

  const std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSigningInWithAutoReauthn,
                      /*expect_idp_brand_icon_in_header=*/true);
  EXPECT_TRUE(IsViewClass<views::ProgressBar>(children[1]));

  views::View* row_container = dialog()->children()[2];
  ASSERT_EQ(row_container->children().size(), 1u);
  CheckNonHoverableAccountRow(row_container->children()[0], kAccountSuffix);
}

TEST_F(AccountSelectionBubbleViewTest, Failure) {
  TestFailureDialog(kTitleSignIn, /*expect_idp_brand_icon_in_header=*/true);
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
  TestSingleAccount(kTitleSignIn,
                    /*expect_idp_brand_icon_in_header=*/true);
}

// Tests that when there is multiple accounts but only one IDP, the UI is
// exactly the same with features::kFedCmMultipleIdentityProviders enabled (see
// AccountSelectionBubbleViewTest's MultipleAccounts test).
TEST_F(MultipleIdpAccountSelectionBubbleViewTest, MultipleAccountsSingleIdp) {
  TestMultipleAccounts(kTitleSignIn,
                       /*expect_idp_brand_icon_in_header=*/true);
}

// Tests that the logo is visible with features::kFedCmMultipleIdentityProviders
// enabled and multiple IDPs.
TEST_F(MultipleIdpAccountSelectionBubbleViewTest,
       MultipleAccountsMultipleIdps) {
  const std::vector<std::string> kAccountSuffixes1 = {"1", "2"};
  const std::vector<std::string> kAccountSuffixes2 = {"3", "4"};
  std::vector<IdentityProviderDataPtr> idp_list = {
      base::MakeRefCounted<content::IdentityProviderData>(
          kIdpForDisplay, content::IdentityProviderMetadata(),
          CreateTestClientMetadata(kTermsOfServiceUrl),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/false),
      base::MakeRefCounted<content::IdentityProviderData>(
          kSecondIdpForDisplay, content::IdentityProviderMetadata(),
          CreateTestClientMetadata("https://tos-2.com"),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/false)};
  std::vector<IdentityRequestAccountPtr> account_list = {
      CreateTestIdentityRequestAccount(kAccountSuffixes1[0], idp_list[0]),
      CreateTestIdentityRequestAccount(kAccountSuffixes1[1], idp_list[0]),
      CreateTestIdentityRequestAccount(kAccountSuffixes2[0], idp_list[1]),
      CreateTestIdentityRequestAccount(kAccountSuffixes2[1], idp_list[1])};
  CreateAndShowMultiIdpAccountPicker(account_list, idp_list);

  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 2u);
  PerformHeaderChecks(children[0], kTitleSignInWithoutIdp,
                      /*expect_idp_brand_icon_in_header=*/false);

  PerformMultiAccountChecks(children[1], /*expected_account_rows=*/4,
                            /*expected_mismatch_rows=*/0);

  std::vector<raw_ptr<views::View, VectorExperimental>> accounts =
      GetAccounts(children[1]);

  // Check the first IDP.
  size_t accounts_index = 0;
  CheckHoverableAccountRows(accounts, kAccountSuffixes1, accounts_index,
                            /*expect_idp=*/true);

  // Check the second IDP.
  CheckHoverableAccountRows(accounts, kAccountSuffixes2, accounts_index,
                            /*expect_idp=*/true);
}

TEST_F(MultipleIdpAccountSelectionBubbleViewTest, OneIdpWithMismatch) {
  const std::vector<std::string> kAccountSuffixes1 = {"1", "2"};
  std::vector<IdentityProviderDataPtr> idp_list = {
      base::MakeRefCounted<content::IdentityProviderData>(
          kIdpForDisplay, content::IdentityProviderMetadata(),
          CreateTestClientMetadata(kTermsOfServiceUrl),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/false),
      base::MakeRefCounted<content::IdentityProviderData>(
          kSecondIdpForDisplay, content::IdentityProviderMetadata(),
          CreateTestClientMetadata("https://tos-2.com"),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/true)};
  std::vector<IdentityRequestAccountPtr> accounts_list =
      CreateTestIdentityRequestAccounts(kAccountSuffixes1, idp_list[0]);
  CreateAndShowMultiIdpAccountPicker(accounts_list, idp_list);

  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 2u);
  PerformHeaderChecks(children[0], kTitleSignInWithoutIdp,
                      /*expect_idp_brand_icon_in_header=*/false);

  PerformMultiAccountChecks(children[1], /*expected_account_rows=*/2,
                            /*expected_mismatch_rows=*/1);

  std::vector<raw_ptr<views::View, VectorExperimental>> accounts =
      GetAccounts(children[1]);

  size_t accounts_index = 0;
  CheckHoverableAccountRows(accounts, kAccountSuffixes1, accounts_index,
                            /*expect_idp=*/true);

  std::vector<raw_ptr<views::View, VectorExperimental>> mismatches =
      GetMismatches(children[1]);

  CheckMismatchIdp(mismatches[0], kSecondIdpETLDPlusOne);
}

TEST_F(MultipleIdpAccountSelectionBubbleViewTest, MultiIdpUseOtherAccount) {
  const std::vector<std::string> kAccountSuffixes1 = {"1", "2"};
  const std::vector<std::string> kAccountSuffixes2 = {"3"};
  content::IdentityProviderMetadata idp_with_supports_add =
      content::IdentityProviderMetadata();
  idp_with_supports_add.supports_add_account = true;
  std::vector<IdentityProviderDataPtr> idp_list = {
      base::MakeRefCounted<content::IdentityProviderData>(
          kIdpForDisplay, idp_with_supports_add,
          CreateTestClientMetadata(kTermsOfServiceUrl),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/false),
      base::MakeRefCounted<content::IdentityProviderData>(
          kSecondIdpForDisplay, idp_with_supports_add,
          CreateTestClientMetadata("https://tos-2.com"),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/false)};
  std::vector<IdentityRequestAccountPtr> accounts_list = {
      CreateTestIdentityRequestAccount(kAccountSuffixes1[0], idp_list[0]),
      CreateTestIdentityRequestAccount(kAccountSuffixes1[1], idp_list[0]),
      CreateTestIdentityRequestAccount(kAccountSuffixes2[0], idp_list[1])};
  CreateAndShowMultiIdpAccountPicker(accounts_list, idp_list);

  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 2u);
  PerformHeaderChecks(children[0], kTitleSignInWithoutIdp,
                      /*expect_idp_brand_icon_in_header=*/false);

  PerformMultiAccountChecks(children[1], /*expected_account_rows=*/7,
                            /*expected_mismatch_rows=*/0);

  std::vector<raw_ptr<views::View, VectorExperimental>> accounts =
      GetAccounts(children[1]);

  // Check the first IDP.
  size_t accounts_index = 0;
  CheckHoverableAccountRows(accounts, kAccountSuffixes1, accounts_index,
                            /*expect_idp=*/true);

  // Check the second IDP.
  CheckHoverableAccountRows(accounts, kAccountSuffixes2, accounts_index,
                            /*expect_idp=*/true);
  CheckUseOtherAccount(accounts, accounts_index);
  CheckUseOtherAccount(accounts, accounts_index);
}

TEST_F(MultipleIdpAccountSelectionBubbleViewTest,
       ShowSingleReturningAccountDialog) {
  const std::vector<std::string> kAccountSuffixes1 = {"1", "2"};
  const std::vector<std::string> kAccountSuffixes2 = {"3"};
  std::vector<IdentityProviderDataPtr> idp_list = {
      base::MakeRefCounted<content::IdentityProviderData>(
          kIdpForDisplay, content::IdentityProviderMetadata(),
          CreateTestClientMetadata(kTermsOfServiceUrl),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/false),
      base::MakeRefCounted<content::IdentityProviderData>(
          kSecondIdpForDisplay, content::IdentityProviderMetadata(),
          CreateTestClientMetadata("https://tos-2.com"),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/false),
      base::MakeRefCounted<content::IdentityProviderData>(
          "idp3.com", content::IdentityProviderMetadata(),
          CreateTestClientMetadata("https://tos-3.com"),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/true),
      base::MakeRefCounted<content::IdentityProviderData>(
          "idp4.com", content::IdentityProviderMetadata(),
          CreateTestClientMetadata("https://tos-4.com"),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/true)};
  std::vector<IdentityRequestAccountPtr> accounts_list = {
      CreateTestIdentityRequestAccount(kAccountSuffixes2[0], idp_list[1],
                                       LoginState::kSignIn),
      CreateTestIdentityRequestAccount(kAccountSuffixes1[0], idp_list[0]),
      CreateTestIdentityRequestAccount(kAccountSuffixes1[1], idp_list[0])};
  CreateAccountSelectionBubble(/*exclude_title=*/true);
  dialog_->ShowSingleReturningAccountDialog(accounts_list, idp_list);

  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 3u);
  PerformHeaderChecks(children[0], kTitleSignInWithoutIdp,
                      /*expect_idp_brand_icon_in_header=*/false);
  EXPECT_TRUE(IsViewClass<views::Separator>(children[1]));

  views::View* wrapper = children[2];

  views::BoxLayout* layout_manager =
      static_cast<views::BoxLayout*>(wrapper->GetLayoutManager());
  EXPECT_TRUE(layout_manager);
  EXPECT_EQ(layout_manager->GetOrientation(),
            views::BoxLayout::Orientation::kVertical);

  std::vector<raw_ptr<views::View, VectorExperimental>> contents =
      wrapper->children();
  ASSERT_EQ(3u, contents.size());

  // Check the first IDP.
  size_t accounts_index = 0;
  CheckHoverableAccountRows(contents, kAccountSuffixes2, accounts_index,
                            /*expect_idp=*/true);
  EXPECT_TRUE(IsViewClass<views::Separator>(contents[1]));
  CheckChooseAnAccount(contents, accounts_index,
                       u"idp3.com, idp4.com, idp-example.com");

  // Simulate clicking on the choose an account button.
  dialog_->ShowMultiAccountPicker(accounts_list, idp_list,
                                  /*show_back_button=*/true,
                                  /*is_choose_an_account=*/true);

  children = dialog()->children();
  ASSERT_EQ(children.size(), 2u);

  // Check title text.
  views::Label* title_view =
      static_cast<views::Label*>(GetViewWithClassName(children[0], "Label"));
  ASSERT_TRUE(title_view);
  EXPECT_EQ(title_view->GetText(),
            u"Choose an account to sign in to rp-example.com");
}

TEST_F(MultipleIdpAccountSelectionBubbleViewTest, MultiIdpWithAllIdpsMismatch) {
  std::vector<IdentityProviderDataPtr> idp_list = {
      base::MakeRefCounted<content::IdentityProviderData>(
          kIdpForDisplay, content::IdentityProviderMetadata(),
          CreateTestClientMetadata(kTermsOfServiceUrl),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/true),
      base::MakeRefCounted<content::IdentityProviderData>(
          kSecondIdpForDisplay, content::IdentityProviderMetadata(),
          CreateTestClientMetadata("https://tos-2.com"),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/true)};
  CreateAndShowMultiIdpAccountPicker(std::vector<IdentityRequestAccountPtr>(),
                                     idp_list);

  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 2u);
  PerformHeaderChecks(children[0], kTitleSignInWithoutIdp,
                      /*expect_idp_brand_icon_in_header=*/false);

  PerformMultiAccountChecks(children[1], /*expected_account_rows=*/0,
                            /*expected_mismatch_rows=*/2);

  std::vector<raw_ptr<views::View, VectorExperimental>> mismatches =
      GetMismatches(children[1]);

  CheckMismatchIdp(mismatches[0], kIdpETLDPlusOne);
  CheckMismatchIdp(mismatches[1], kSecondIdpETLDPlusOne);
}

TEST_F(MultipleIdpAccountSelectionBubbleViewTest, MultipleReturningAccounts) {
  std::vector<IdentityProviderDataPtr> idp_list = {
      base::MakeRefCounted<content::IdentityProviderData>(
          kIdpForDisplay, content::IdentityProviderMetadata(),
          CreateTestClientMetadata(kTermsOfServiceUrl),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/false),
      base::MakeRefCounted<content::IdentityProviderData>(
          kSecondIdpForDisplay, content::IdentityProviderMetadata(),
          CreateTestClientMetadata("https://tos-2.com"),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/false)};
  // The UI code receives the accounts sorted in the order in which they should
  // be displayed.
  std::vector<IdentityRequestAccountPtr> accounts_list = {
      CreateTestIdentityRequestAccount("returning1", idp_list[0],
                                       LoginState::kSignIn),
      CreateTestIdentityRequestAccount("returning2", idp_list[1],
                                       LoginState::kSignIn),
      CreateTestIdentityRequestAccount("new1", idp_list[0]),
      CreateTestIdentityRequestAccount("new2", idp_list[1])};

  CreateAndShowMultiIdpAccountPicker(accounts_list, idp_list);

  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 2u);
  PerformHeaderChecks(children[0], kTitleSignInWithoutIdp,
                      /*expect_idp_brand_icon_in_header=*/false);

  PerformMultiAccountChecks(children[1], /*expected_account_rows=*/4,
                            /*expected_mismatch_rows=*/0);

  std::vector<raw_ptr<views::View, VectorExperimental>> accounts =
      GetAccounts(children[1]);

  // Returning accounts are shown first.
  const std::vector<std::string> expected_account_order = {
      "returning1", "returning2", "new1", "new2"};
  size_t accounts_index = 0;
  CheckHoverableAccountRows(accounts, expected_account_order, accounts_index,
                            /*expect_idp=*/true);
}

TEST_F(MultipleIdpAccountSelectionBubbleViewTest,
       MultipleReturningAccountsWithTimestamps) {
  const std::vector<std::string> kAccountSuffixes1 = {"new1", "returning1",
                                                      "returning2"};
  const std::vector<std::string> kAccountSuffixes2 = {"new2", "returning3",
                                                      "returning4"};
  std::vector<IdentityProviderDataPtr> idp_list = {
      base::MakeRefCounted<content::IdentityProviderData>(
          kIdpForDisplay, content::IdentityProviderMetadata(),
          CreateTestClientMetadata(kTermsOfServiceUrl),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/false),
      base::MakeRefCounted<content::IdentityProviderData>(
          kSecondIdpForDisplay, content::IdentityProviderMetadata(),
          CreateTestClientMetadata("https://tos-2.com"),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/false)};
  // Note that `new2` is last despite having last_used_timestamp because it is
  // not considered a returning account.
  std::vector<IdentityRequestAccountPtr> accounts_list = {
      CreateTestIdentityRequestAccount("returning3", idp_list[1],
                                       LoginState::kSignIn,
                                       base::Time() + base::Microseconds(2)),
      CreateTestIdentityRequestAccount("returning1", idp_list[0],
                                       LoginState::kSignIn,
                                       base::Time() + base::Microseconds(1)),
      CreateTestIdentityRequestAccount("returning2", idp_list[0],
                                       LoginState::kSignIn, base::Time()),
      CreateTestIdentityRequestAccount("returning4", idp_list[1],
                                       LoginState::kSignIn, base::Time()),
      CreateTestIdentityRequestAccount("new1", idp_list[0]),
      CreateTestIdentityRequestAccount("new2", idp_list[1], LoginState::kSignUp,
                                       base::Time() + base::Microseconds(3))};

  CreateAndShowMultiIdpAccountPicker(accounts_list, idp_list);

  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 2u);
  // The multiple account chooser container includes the separator.
  PerformHeaderChecks(children[0], kTitleSignInWithoutIdp,
                      /*expect_idp_brand_icon_in_header=*/false);

  PerformMultiAccountChecks(children[1], /*expected_account_rows=*/6,
                            /*expected_mismatch_rows=*/0);

  std::vector<raw_ptr<views::View, VectorExperimental>> accounts =
      GetAccounts(children[1]);

  const std::vector<std::string> expected_account_order = {
      "returning3", "returning1", "returning2", "returning4", "new1", "new2"};
  size_t accounts_index = 0;
  CheckHoverableAccountRows(accounts, expected_account_order, accounts_index,
                            /*expect_idp=*/true);
}

// Tests that in the multi IDP account button, hovering over the button modifies
// the background circle containing the IDP icon.
TEST_F(MultipleIdpAccountSelectionBubbleViewTest, HoverChangesIdpCircle) {
  // Need two IDPs to show the multi IDP UI.
  constexpr char kAccountSuffix1[] = "1";
  constexpr char kAccountSuffix2[] = "2";
  std::vector<IdentityProviderDataPtr> idp_list = {
      base::MakeRefCounted<content::IdentityProviderData>(
          kIdpForDisplay, content::IdentityProviderMetadata(),
          CreateTestClientMetadata(kTermsOfServiceUrl),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/false),
      base::MakeRefCounted<content::IdentityProviderData>(
          kSecondIdpForDisplay, content::IdentityProviderMetadata(),
          CreateTestClientMetadata("https://tos-2.com"),
          blink::mojom::RpContext::kSignIn, kDefaultDisclosureFields,
          /*has_login_status_mismatch=*/false)};
  std::vector<IdentityRequestAccountPtr> accounts_list = {
      CreateTestIdentityRequestAccount(kAccountSuffix1, idp_list[0]),
      CreateTestIdentityRequestAccount(kAccountSuffix2, idp_list[1])};

  CreateAndShowMultiIdpAccountPicker(accounts_list, idp_list);

  std::vector<raw_ptr<views::View, VectorExperimental>> children =
      dialog()->children();
  ASSERT_EQ(children.size(), 2u);
  PerformHeaderChecks(children[0], kTitleSignInWithoutIdp,
                      /*expect_idp_brand_icon_in_header=*/false);

  PerformMultiAccountChecks(children[1], /*expected_account_rows=*/2,
                            /*expected_mismatch_rows=*/0);

  std::vector<raw_ptr<views::View, VectorExperimental>> accounts =
      GetAccounts(children[1]);

  HoverButton* account_row = static_cast<HoverButton*>(accounts[1]);
  views::View* icon_view = GetHoverButtonIconView(account_row);
  ASSERT_TRUE(icon_view);
  std::vector<raw_ptr<views::View, VectorExperimental>> icon_children =
      icon_view->children();
  ASSERT_EQ(icon_children.size(), 2u);
  EXPECT_STREQ(icon_children[1]->GetClassName(), "BoxLayoutView");
  ASSERT_EQ(icon_children[1]->children().size(), 1u);
  EXPECT_STREQ(icon_children[1]->children()[0]->GetClassName(),
               "BrandIconImageView");
  auto* brand_icon_image_view =
      static_cast<BrandIconImageView*>(icon_children[1]->children()[0]);
  auto* color_provider = account_row->GetColorProvider();
  ASSERT_TRUE(color_provider);
  EXPECT_EQ(brand_icon_image_view->background_color_for_testing(),
            color_provider->GetColor(ui::kColorDialogBackground));

  account_row->SetState(HoverButton::ButtonState::STATE_HOVERED);
  EXPECT_EQ(brand_icon_image_view->background_color_for_testing(),
            color_provider->GetColor(ui::kColorMenuButtonBackgroundSelected));
}

TEST_F(AccountSelectionBubbleViewTest, GenericError) {
  TestErrorDialog(kTitleSignIn, u"Can't continue with idp-example.com",
                  u"Something went wrong",
                  /*expect_idp_brand_icon_in_header=*/true,
                  /*error_code=*/"",
                  /*error_url=*/GURL());
}

TEST_F(AccountSelectionBubbleViewTest, GenericErrorWithErrorUrl) {
  TestErrorDialog(kTitleSignIn, u"Can't continue with idp-example.com",
                  u"Something went wrong",
                  /*expect_idp_brand_icon_in_header=*/true,
                  /*error_code=*/"",
                  GURL(u"https://idp-example.com/more-details"));
}

TEST_F(AccountSelectionBubbleViewTest, ErrorWithDifferentErrorCodes) {
  // Invalid request without error URL
  TestErrorDialog(kTitleSignIn,
                  u"rp-example.com can't continue using idp-example.com",
                  u"This option is unavailable right now. You can try other "
                  u"ways to continue on rp-example.com.",
                  /*expect_idp_brand_icon_in_header=*/true,
                  /*error_code=*/"invalid_request",
                  /*error_url=*/GURL());

  // Invalid request with error URL
  TestErrorDialog(
      kTitleSignIn, u"rp-example.com can't continue using idp-example.com",
      u"This option is unavailable right now. Choose \"More "
      u"details\" below to get more information from idp-example.com.",
      /*expect_idp_brand_icon_in_header=*/true,
      /*error_code=*/"invalid_request",
      GURL(u"https://idp-example.com/more-details"));

  // Unauthorized client without error URL
  TestErrorDialog(kTitleSignIn,
                  u"rp-example.com can't continue using idp-example.com",
                  u"This option is unavailable right now. You can try other "
                  u"ways to continue on rp-example.com.",
                  /*expect_idp_brand_icon_in_header=*/true,
                  /*error_code=*/"unauthorized_client",
                  /*error_url=*/GURL());

  // Unauthorized client with error URL
  TestErrorDialog(
      kTitleSignIn, u"rp-example.com can't continue using idp-example.com",
      u"This option is unavailable right now. Choose \"More "
      u"details\" below to get more information from idp-example.com.",
      /*expect_idp_brand_icon_in_header=*/true,
      /*error_code=*/"unauthorized_client",
      GURL(u"https://idp-example.com/more-details"));

  // Access denied without error URL
  TestErrorDialog(kTitleSignIn, u"Check that you chose the right account",
                  u"Check if the selected account is supported. You can try "
                  u"other ways to continue on rp-example.com.",
                  /*expect_idp_brand_icon_in_header=*/true,
                  /*error_code=*/"access_denied",
                  /*error_url=*/GURL());

  // Access denied with error URL
  TestErrorDialog(
      kTitleSignIn, u"Check that you chose the right account",
      u"Check if the selected account is supported. Choose \"More "
      u"details\" below to get more information from idp-example.com.",
      /*expect_idp_brand_icon_in_header=*/true,
      /*error_code=*/"access_denied",
      GURL(u"https://idp-example.com/more-details"));

  // Temporarily unavailable without error URL
  TestErrorDialog(kTitleSignIn, u"Try again later",
                  u"idp-example.com isn't available right now. If this issue "
                  u"keeps happening, you can try other ways to continue on "
                  u"rp-example.com.",
                  /*expect_idp_brand_icon_in_header=*/true,
                  /*error_code=*/"temporarily_unavailable",
                  /*error_url=*/GURL());

  // Temporarily unavailable with error URL
  TestErrorDialog(kTitleSignIn, u"Try again later",
                  u"idp-example.com isn't available right now. If this issue "
                  u"keeps happening, choose \"More details\" below to get more "
                  u"information from idp-example.com.",
                  /*expect_idp_brand_icon_in_header=*/true,
                  /*error_code=*/"temporarily_unavailable",
                  GURL(u"https://idp-example.com/more-details"));

  // Server error without error URL
  TestErrorDialog(kTitleSignIn, u"Check your internet connection",
                  u"If you're online but this issue keeps happening, you can "
                  u"try other ways to continue on rp-example.com.",
                  /*expect_idp_brand_icon_in_header=*/true,
                  /*error_code=*/"server_error",
                  /*error_url=*/GURL());

  // Server error with error URL
  TestErrorDialog(kTitleSignIn, u"Check your internet connection",
                  u"If you're online but this issue keeps happening, you can "
                  u"try other ways to continue on rp-example.com.",
                  /*expect_idp_brand_icon_in_header=*/true,
                  /*error_code=*/"server_error",
                  GURL(u"https://idp-example.com/more-details"));

  // Error not in our predefined list without error URL
  TestErrorDialog(kTitleSignIn, u"Can't continue with idp-example.com",
                  u"Something went wrong",
                  /*expect_idp_brand_icon_in_header=*/true,
                  /*error_code=*/"error_we_dont_support",
                  /*error_url=*/GURL());

  // Error not in our predefined list with error URL
  TestErrorDialog(kTitleSignIn, u"Can't continue with idp-example.com",
                  u"Something went wrong",
                  /*expect_idp_brand_icon_in_header=*/true,
                  /*error_code=*/"error_we_dont_support",
                  GURL(u"https://idp-example.com/more-details"));
}

// Tests that CanFitInWebContents returns true when the web contents is large
// enough to fit the bubble and bubble bounds computed are contained within the
// web contents' bounds.
TEST_F(AccountSelectionBubbleViewTest, WebContentsLargeEnoughToFitDialog) {
  TestSingleAccount(kTitleSignIn,
                    /*expect_idp_brand_icon_in_header=*/true);
  EXPECT_TRUE(dialog()->CanFitInWebContents());
  EXPECT_TRUE(
      web_contents()->GetViewBounds().Contains(dialog_->GetBubbleBounds()));
}

// Tests that CanFitInWebContents returns false when the web contents is too
// small to fit the bubble. We do not test GetBubbleBounds here because the
// bubble would be hidden so GetBubbleBounds is not relevant.
TEST_F(AccountSelectionBubbleViewTest, WebContentsTooSmallToFitDialog) {
  TestSingleAccount(kTitleSignIn,
                    /*expect_idp_brand_icon_in_header=*/true);

  // Web contents is too small, vertically.
  web_contents()->Resize(gfx::Rect(/*x=*/0, /*y=*/0, /*width=*/1000,
                                   /*height=*/10));
  EXPECT_FALSE(dialog()->CanFitInWebContents());

  // Web contents is too small, horizontally.
  web_contents()->Resize(gfx::Rect(/*x=*/0, /*y=*/0, /*width=*/10,
                                   /*height=*/1000));
  EXPECT_FALSE(dialog()->CanFitInWebContents());

  // Web contents is too small, both vertically and horizontally.
  web_contents()->Resize(gfx::Rect(/*x=*/0, /*y=*/0, /*width=*/10,
                                   /*height=*/10));
  EXPECT_FALSE(dialog()->CanFitInWebContents());
}

// Tests crash scenario from crbug.com/341240034.
TEST_F(AccountSelectionBubbleViewTest, BoundsChangedAfterWebContentsDestroyed) {
  TestSingleAccount(kTitleSignIn,
                    /*expect_idp_brand_icon_in_header=*/true);

  // Reset the web contents associated with the dialog.
  ResetWebContents();
  EXPECT_FALSE(web_contents());

  // Dialog is somehow still alive and receives OnAnchorBoundsChanged calls.
  // This should not crash.
  dialog()->OnAnchorBoundsChanged();
}

// Tests that the brand icon view is hidden if the brand icon URL is invalid.
TEST_F(AccountSelectionBubbleViewTest, InvalidBrandIconUrlHidesBrandIcon) {
  idp_data_->idp_metadata.brand_icon_url = GURL("invalid url");
  CreateAndShowSingleAccountPicker();

  views::View* brand_icon_image_view = static_cast<views::View*>(
      GetViewWithClassName(dialog()->children()[0], "BrandIconImageView"));
  ASSERT_TRUE(brand_icon_image_view);
  EXPECT_FALSE(brand_icon_image_view->GetVisible());
}
