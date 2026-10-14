// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_promo_bubble_view.h"

#include <memory>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/views/send_tab_to_self/send_tab_to_self_bubble_controller.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/signin/public/base/signin_buildflags.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/view_utils.h"

#if BUILDFLAG(ENABLE_DICE_SUPPORT)
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_view.h"

namespace send_tab_to_self {

namespace {

class StubSendTabToSelfBubbleController : public SendTabToSelfBubbleController {
 public:
  explicit StubSendTabToSelfBubbleController(content::WebContents* web_contents)
      : SendTabToSelfBubbleController(web_contents) {}

  ~StubSendTabToSelfBubbleController() override = default;

  AccountInfo GetSharingAccountInfo() override {
    auto* identity_manager = IdentityManagerFactory::GetForProfile(
        Profile::FromBrowserContext(GetWebContents().GetBrowserContext()));
    CHECK(identity_manager);
    return identity_manager->FindExtendedAccountInfo(
        identity_manager->GetPrimaryAccountInfo(signin::ConsentLevel::kSignin));
  }
};

}  // namespace

class SendTabToSelfPromoBubbleViewTest : public ChromeViewsTestBase {
 protected:
  void SetUp() override {
    TestingProfile::Builder builder;
    builder.AddTestingFactories(
        IdentityTestEnvironmentProfileAdaptor::
            GetIdentityTestEnvironmentFactoriesWithAppendedFactories(
                {TestingProfile::TestingFactory{
                    ChromeSigninClientFactory::GetInstance(),
                    base::BindRepeating(&BuildChromeSigninClientWithURLLoader,
                                        &test_url_loader_factory_)}}));
    profile_ = builder.Build();

    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile_.get());
    identity_test_env()->SetTestURLLoaderFactory(&test_url_loader_factory_);

    ChromeViewsTestBase::SetUp();

    // Create an anchor for the widget.
    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);

    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
    // Owned by WebContents.
    auto controller = std::make_unique<StubSendTabToSelfBubbleController>(
        web_contents_.get());
    controller_ = controller.get();
    web_contents_->SetUserData(StubSendTabToSelfBubbleController::UserDataKey(),
                               std::move(controller));
  }

  void CreateSignInPromoBubble() {
    auto* bubble = new SendTabToSelfSignInPromoBubbleView(
        views::BubbleAnchor(anchor_widget_->GetContentsView()),
        web_contents_.get(),
        SendTabToSelfSignInPromoBubbleView::PromoMode::kSignIn);
    bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble);
    bubble_ = bubble;
  }

  void CreateReauthPromoBubble() {
    auto* bubble = new SendTabToSelfSignInPromoBubbleView(
        views::BubbleAnchor(anchor_widget_->GetContentsView()),
        web_contents_.get(),
        SendTabToSelfSignInPromoBubbleView::PromoMode::kReauth);
    bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(bubble);
    bubble_ = bubble;
  }

  void TearDown() override {
    bubble_ = nullptr;
    views::Widget* widget = bubble_widget_;
    bubble_widget_ = nullptr;
    widget->CloseNow();
    anchor_widget_.reset();
    controller_ = nullptr;
    web_contents_.reset();
    ChromeViewsTestBase::TearDown();
    identity_test_env_adaptor_.reset();
    profile_.reset();
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  views::LabelButton* GetSignInButton() {
    CHECK(bubble_);
    CHECK(!bubble_->children().empty());
    auto* promo_view =
        views::AsViewClass<BubbleSignInPromoView>(bubble_->children()[0]);
    CHECK(promo_view);
    auto* button =
        views::AsViewClass<views::LabelButton>(promo_view->GetSignInButton());
    CHECK(button);
    return button;
  }

  // Recursively searches for a views::Label child with the matching text.
  views::Label* FindLabelWithText(const std::u16string& text) {
    return FindLabelWithTextRecursively(bubble_, text);
  }

  views::Label* FindLabelWithTextRecursively(views::View* view,
                                             const std::u16string& text) {
    if (auto* label = views::AsViewClass<views::Label>(view)) {
      if (label->GetText() == text) {
        return label;
      }
    }
    for (views::View* child : view->children()) {
      if (auto* result = FindLabelWithTextRecursively(child, text)) {
        return result;
      }
    }
    return nullptr;
  }

  std::unique_ptr<TestingProfile> profile_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<SendTabToSelfBubbleView> bubble_ = nullptr;
  raw_ptr<views::Widget> bubble_widget_ = nullptr;
  // Owned by WebContents.
  raw_ptr<StubSendTabToSelfBubbleController> controller_;
};

// Verifies that the sign-in promo layout is loaded correctly when the user is
// signed out.
TEST_F(SendTabToSelfPromoBubbleViewTest, LoadsSignInPromoDesign) {
  CreateSignInPromoBubble();

  // Title should match sign-in promo title strictly.
  EXPECT_EQ(
      bubble_->GetWindowTitle(),
      l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_TITLE));

  // Dialog button should be null.
  EXPECT_FALSE(bubble_->GetOkButton());

  // Sign in button inside the content view should be visible and use the
  // sign-in promo label.
  auto* sign_in_button = GetSignInButton();
  ASSERT_TRUE(sign_in_button);
  EXPECT_EQ(sign_in_button->GetText(),
            l10n_util::GetStringUTF16(IDS_PROFILE_MENU_SIGNIN_PROMO_BUTTON));

  // Frame view should have a header view (illustration).
  EXPECT_NE(nullptr, bubble_->GetBubbleFrameView()->GetHeaderViewForTesting());
}

// Verifies that the account-aware sign-in promo layout is loaded with the
// user's given name on the sign-in button when an account is available.
TEST_F(SendTabToSelfPromoBubbleViewTest, LoadsAccountAwareDesign) {
  AccountInfo account_info = identity_test_env()->MakeAccountAvailable(
      "elisa@example.com", {.set_cookie = true});
  AccountInfo::Builder builder(account_info);
  builder.SetFullName("Elisa Beckett");
  builder.SetGivenName("Elisa");
  identity_test_env()->UpdateAccountInfoForAccount(builder.Build());

  CreateSignInPromoBubble();

  // Title should match sign-in promo title strictly.
  EXPECT_EQ(
      bubble_->GetWindowTitle(),
      l10n_util::GetStringUTF16(IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_TITLE));

  // Dialog button should be null.
  EXPECT_FALSE(bubble_->GetOkButton());

  // Sign in button inside the content view should be visible and use
  // account-aware label.
  auto* sign_in_button = GetSignInButton();
  ASSERT_TRUE(sign_in_button);
  EXPECT_EQ(sign_in_button->GetText(),
            l10n_util::GetStringFUTF16(
                IDS_SIGNIN_DICE_WEB_INTERCEPT_BUBBLE_CHROME_SIGNIN_ACCEPT_TEXT,
                u"Elisa"));

  // Frame view should have a header view (illustration) in the
  // account-aware state.
  EXPECT_NE(nullptr, bubble_->GetBubbleFrameView()->GetHeaderViewForTesting());

  // Should have a single child (BubbleSignInPromoView).
  EXPECT_EQ(bubble_->children().size(), 1u);
}

// Verifies that the verify-it-is-you promo layout is loaded correctly when the
// user has an account in a persistent auth error state.
TEST_F(SendTabToSelfPromoBubbleViewTest, LoadsVerifyItsYouDesign) {
  AccountInfo account_info = identity_test_env()->MakePrimaryAccountAvailable(
      "user@host.com", signin::ConsentLevel::kSignin);
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();

  CreateReauthPromoBubble();

  // Title should match verify title strictly.
  EXPECT_EQ(bubble_->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_SEND_TAB_TO_SELF_VERIFY_ITS_YOU_PROMO_TITLE));

  // Dialog button should be null.
  EXPECT_FALSE(bubble_->GetOkButton());

  // Sign in button inside the content view should be visible and use verify
  // label.
  auto* sign_in_button = GetSignInButton();
  ASSERT_TRUE(sign_in_button);
  EXPECT_EQ(sign_in_button->GetText(),
            l10n_util::GetStringUTF16(IDS_PROFILES_VERIFY_ACCOUNT_BUTTON));

  // Verify body contains the verify text string.
  EXPECT_NE(nullptr, FindLabelWithText(l10n_util::GetStringUTF16(
                         IDS_SEND_TAB_TO_SELF_VERIFY_ITS_YOU_PROMO_LABEL)));
}

}  // namespace send_tab_to_self
#endif  // BUILDFLAG(ENABLE_DICE_SUPPORT)
