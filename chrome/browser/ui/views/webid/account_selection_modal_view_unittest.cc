// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/webid/account_selection_modal_view.h"

#include <string>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ui/views/chrome_constrained_window_views_client.h"
#include "chrome/browser/ui/views/controls/hover_button.h"
#include "chrome/browser/ui/views/webid/fake_delegate.h"
#include "chrome/browser/ui/views/webid/identity_provider_display_data.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/constrained_window/constrained_window_views.h"
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

namespace {

const std::u16string kTopFrameETLDPlusOne = u"top-frame-example.com";
const std::u16string kIframeETLDPlusOne = u"iframe-example.com";
const std::u16string kIdpETLDPlusOne = u"idp-example.com";
const std::u16string kTitleSignIn =
    u"Sign in to top-frame-example.com with idp-example.com";
const std::u16string kBodySignIn = u"Choose an account to continue";

constexpr char kIdBase[] = "id";
constexpr char kEmailBase[] = "email";
constexpr char kNameBase[] = "name";
constexpr char kGivenNameBase[] = "given_name";

const char kTermsOfServiceUrl[] = "htpps://terms-of-service.com";
const char kPrivacyPolicyUrl[] = "https://privacy-policy.com";

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

content::ClientMetadata CreateTestClientMetadata(
    const std::string& terms_of_service_url) {
  return content::ClientMetadata((GURL(terms_of_service_url)),
                                 (GURL(kPrivacyPolicyUrl)));
}

std::vector<std::string> GetChildClassNames(views::View* parent) {
  std::vector<std::string> child_class_names;
  for (views::View* child_view : parent->children()) {
    child_class_names.push_back(child_view->GetClassName());
    std::cout << child_view->GetClassName() << std::endl;
  }
  return child_class_names;
}

}  // namespace

class AccountSelectionModalViewTest : public ChromeViewsTestBase {
 public:
  AccountSelectionModalViewTest() = default;

 protected:
  void CreateAccountSelectionModal() {
    anchor_widget_ = CreateTestWidget();
    anchor_widget_->Show();

    dialog_ = new AccountSelectionModalView(blink::mojom::RpContext::kSignIn,
                                            /*browser=*/nullptr,
                                            shared_url_loader_factory(),
                                            /*observer=*/nullptr,
                                            /*widget_observer=*/nullptr);
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
        kTopFrameETLDPlusOne,
        exclude_iframe ? std::nullopt
                       : std::make_optional<std::u16string>(kIframeETLDPlusOne),
        account, idp_data, show_back_button);
    constrained_window::CreateBrowserModalDialogViews(
        dialog_->AsDialogDelegate(), anchor_widget_->GetNativeWindow());
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

  void PerformHeaderChecks(views::View* header,
                           const std::u16string& expected_title) {
    // Perform some basic dialog checks.
    EXPECT_FALSE(dialog()->ShouldShowCloseButton());
    EXPECT_FALSE(dialog()->ShouldShowWindowTitle());

    EXPECT_FALSE(dialog()->GetOkButton());
    EXPECT_TRUE(dialog()->GetCancelButton());

    // Order: Title, body
    std::vector<std::string> expected_class_names = {"Label", "Label"};
    EXPECT_THAT(GetChildClassNames(header),
                testing::ElementsAreArray(expected_class_names));

    std::vector<raw_ptr<views::View, VectorExperimental>> header_children =
        header->children();
    ASSERT_EQ(header_children.size(), 2u);

    // Check title text.
    views::Label* title_view = static_cast<views::Label*>(header_children[0]);
    ASSERT_TRUE(title_view);
    EXPECT_EQ(title_view->GetText(), expected_title);

    // Check body text.
    views::Label* body_view = static_cast<views::Label*>(header_children[1]);
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

  void SetUp() override {
    feature_list_.InitAndEnableFeature(features::kFedCm);
    test_web_contents_ =
        content::WebContentsTester::CreateTestWebContents(&profile_, nullptr);
    delegate_ = std::make_unique<FakeDelegate>(test_web_contents_.get());
    test_shared_url_loader_factory_ =
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_);
    SetConstrainedWindowViewsClient(CreateChromeConstrainedWindowViewsClient());
    ChromeViewsTestBase::SetUp();
  }

  void TearDown() override {
    anchor_widget_.reset();
    feature_list_.Reset();
    ChromeViewsTestBase::TearDown();
  }

  AccountSelectionModalView* dialog() { return dialog_; }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory()
      const {
    return test_shared_url_loader_factory_;
  }

  raw_ptr<AccountSelectionModalView, DanglingUntriaged> dialog_;

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

TEST_F(AccountSelectionModalViewTest, SingleAccount) {
  TestSingleAccount(kTitleSignIn);
}
