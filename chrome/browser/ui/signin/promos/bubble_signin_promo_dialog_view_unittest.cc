// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/signin/promos/bubble_signin_promo_dialog_view.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/chrome_signin_client_factory.h"
#include "chrome/browser/signin/chrome_signin_client_test_util.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_delegate.h"
#include "chrome/browser/ui/signin/promos/bubble_signin_promo_view.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/browser_resources.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/views/chrome_views_test_base.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/base/signin_metrics.h"
#include "components/signin/public/base/signin_switches.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/bubble/bubble_frame_view.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/layout/layout_provider.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/widget.h"

namespace {

class BubbleSignInPromoDialogViewTest : public ChromeViewsTestBase {
 protected:
  BubbleSignInPromoDialogViewTest() {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {switches::kEnableSearchAIModeSigninPromo,
         omnibox::kComposeboxDriveContextMenuOptionSigninPromo},
        /*disabled_features=*/{});
  }

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

    anchor_widget_ =
        CreateTestWidget(views::Widget::InitParams::CLIENT_OWNS_WIDGET,
                         views::Widget::InitParams::TYPE_WINDOW);
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), nullptr);
  }

  void TearDown() override {
    dialog_view_ = nullptr;
    if (bubble_widget_) {
      views::Widget* widget = bubble_widget_;
      bubble_widget_ = nullptr;
      widget->CloseNow();
    }
    anchor_widget_.reset();
    web_contents_.reset();
    ChromeViewsTestBase::TearDown();
    identity_test_env_adaptor_.reset();
    profile_.reset();
  }

  void CreatePromoDialog(
      signin_metrics::AccessPoint access_point,
      int signin_title_resource_id,
      std::optional<int> verify_title_resource_id = std::nullopt,
      std::optional<int> lottie_resource_id = std::nullopt) {
    auto delegate = std::make_unique<DefaultBubbleSignInPromoDelegate>(
        *web_contents_, access_point);
    auto* dialog = new BubbleSignInPromoDialogView(
        views::BubbleAnchor(anchor_widget_->GetContentsView()),
        web_contents_.get(), access_point, std::move(delegate),
        signin_title_resource_id, verify_title_resource_id, lottie_resource_id);
    bubble_widget_ = views::BubbleDialogDelegateView::CreateBubble(dialog);
    dialog_view_ = dialog;
  }

  signin::IdentityTestEnvironment* identity_test_env() {
    return identity_test_env_adaptor_->identity_test_env();
  }

  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfile> profile_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  content::RenderViewHostTestEnabler test_render_host_factories_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<views::Widget> anchor_widget_;
  raw_ptr<BubbleSignInPromoDialogView> dialog_view_ = nullptr;
  raw_ptr<views::Widget> bubble_widget_ = nullptr;
};

// Verifies that when the user is signed out, `BubbleSignInPromoDialogView`
// selects `signin_title_resource_id` even if `verify_title_resource_id` is
// provided.
TEST_F(BubbleSignInPromoDialogViewTest, UsesSignInTitleWhenSignedOut) {
  CreatePromoDialog(
      signin_metrics::AccessPoint::kComposeboxDriveContextMenuOptionBubble,
      IDS_COMPOSEBOX_DRIVE_CONTEXT_MENU_OPTION_SIGNIN_PROMO_TITLE,
      IDS_COMPOSEBOX_DRIVE_CONTEXT_MENU_OPTION_VERIFY_PROMO_TITLE);

  EXPECT_EQ(dialog_view_->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_COMPOSEBOX_DRIVE_CONTEXT_MENU_OPTION_SIGNIN_PROMO_TITLE));
  EXPECT_TRUE(dialog_view_->ShouldShowCloseButton());
  EXPECT_FALSE(dialog_view_->GetOkButton());
  ASSERT_EQ(dialog_view_->children().size(), 1u);
  auto* promo_view =
      views::AsViewClass<BubbleSignInPromoView>(dialog_view_->children()[0]);
  ASSERT_TRUE(promo_view);
  EXPECT_EQ(dialog_view_->GetInitiallyFocusedView(),
            promo_view->GetSignInButton());
}

// Verifies that when the user has a web-only signed-in account,
// `BubbleSignInPromoDialogView` selects `signin_title_resource_id`.
TEST_F(BubbleSignInPromoDialogViewTest, UsesSignInTitleWhenWebOnlySignedIn) {
  identity_test_env()->MakeAccountAvailable("user@gmail.com",
                                            {.set_cookie = true});

  CreatePromoDialog(
      signin_metrics::AccessPoint::kComposeboxDriveContextMenuOptionBubble,
      IDS_COMPOSEBOX_DRIVE_CONTEXT_MENU_OPTION_SIGNIN_PROMO_TITLE,
      IDS_COMPOSEBOX_DRIVE_CONTEXT_MENU_OPTION_VERIFY_PROMO_TITLE);

  EXPECT_EQ(dialog_view_->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_COMPOSEBOX_DRIVE_CONTEXT_MENU_OPTION_SIGNIN_PROMO_TITLE));
}

// Verifies that when the primary account is in `kSignInPending` (persistent
// auth error), `BubbleSignInPromoDialogView` selects
// `verify_title_resource_id`.
TEST_F(BubbleSignInPromoDialogViewTest, UsesVerifyTitleWhenSignInPending) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();

  CreatePromoDialog(
      signin_metrics::AccessPoint::kComposeboxDriveContextMenuOptionBubble,
      IDS_COMPOSEBOX_DRIVE_CONTEXT_MENU_OPTION_SIGNIN_PROMO_TITLE,
      IDS_COMPOSEBOX_DRIVE_CONTEXT_MENU_OPTION_VERIFY_PROMO_TITLE);

  EXPECT_EQ(dialog_view_->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_COMPOSEBOX_DRIVE_CONTEXT_MENU_OPTION_VERIFY_PROMO_TITLE));
}

// Verifies that when the primary account is in `kSignInPending` and
// `verify_title_resource_id` is omitted, `BubbleSignInPromoDialogView` falls
// back to `signin_title_resource_id`.
TEST_F(BubbleSignInPromoDialogViewTest,
       FallsBackToSignInTitleWhenVerifyTitleOmitted) {
  identity_test_env()->MakePrimaryAccountAvailable(
      "user@gmail.com", signin::ConsentLevel::kSignin);
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();

  CreatePromoDialog(signin_metrics::AccessPoint::kSearchAIModeBubble,
                    IDS_AI_SIGNIN_PROMO_TITLE,
                    /*verify_title_resource_id=*/std::nullopt);

  EXPECT_EQ(dialog_view_->GetWindowTitle(),
            l10n_util::GetStringUTF16(IDS_AI_SIGNIN_PROMO_TITLE));
}

// Verifies that when a legacy syncing user has a persistent auth error
// (`kSyncPaused`), `BubbleSignInPromoDialogView` selects
// `verify_title_resource_id`.
TEST_F(BubbleSignInPromoDialogViewTest, UsesVerifyTitleWhenSyncPaused) {
  identity_test_env()->MakePrimaryAccountAvailable("user@gmail.com",
                                                   signin::ConsentLevel::kSync);
  identity_test_env()->SetInvalidRefreshTokenForPrimaryAccount();

  CreatePromoDialog(signin_metrics::AccessPoint::kSendTabToSelfPromo,
                    IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_TITLE,
                    IDS_SEND_TAB_TO_SELF_VERIFY_ITS_YOU_PROMO_TITLE);

  EXPECT_EQ(dialog_view_->GetWindowTitle(),
            l10n_util::GetStringUTF16(
                IDS_SEND_TAB_TO_SELF_VERIFY_ITS_YOU_PROMO_TITLE));
}

// Verifies that when `lottie_resource_id` is omitted, no header view is
// attached to the bubble frame view.
TEST_F(BubbleSignInPromoDialogViewTest,
       OmitsHeaderViewWhenLottieResourceIdNotProvided) {
  CreatePromoDialog(signin_metrics::AccessPoint::kSearchAIModeBubble,
                    IDS_AI_SIGNIN_PROMO_TITLE,
                    /*verify_title_resource_id=*/std::nullopt,
                    /*lottie_resource_id=*/std::nullopt);

  EXPECT_EQ(dialog_view_->GetBubbleFrameView()->GetHeaderViewForTesting(),
            nullptr);
}

// Verifies that when `lottie_resource_id` is provided, `AddedToWidget()`
// attaches a `views::ImageView` header scaled to the bubble preferred width
// while preserving aspect ratio.
TEST_F(BubbleSignInPromoDialogViewTest,
       ScalesLottieHeaderViewToBubblePreferredWidth) {
  CreatePromoDialog(signin_metrics::AccessPoint::kSendTabToSelfPromo,
                    IDS_SEND_TAB_TO_SELF_SIGN_IN_PROMO_TITLE,
                    IDS_SEND_TAB_TO_SELF_VERIFY_ITS_YOU_PROMO_TITLE,
                    IDR_INSTANT_HANDOFF_ILLUSTRATION);

  views::View* header_view =
      dialog_view_->GetBubbleFrameView()->GetHeaderViewForTesting();
  ASSERT_TRUE(header_view);

  auto* header_image = views::AsViewClass<views::ImageView>(header_view);
  ASSERT_TRUE(header_image);

  const int expected_width = views::LayoutProvider::Get()->GetDistanceMetric(
      views::DISTANCE_BUBBLE_PREFERRED_WIDTH);
  const gfx::Size original_size =
      ui::ResourceBundle::GetSharedInstance()
          .GetThemedLottieImageNamed(IDR_INSTANT_HANDOFF_ILLUSTRATION)
          .Size();
  ASSERT_GT(original_size.width(), 0);
  EXPECT_NE(original_size.width(), expected_width);
  const float expected_scale =
      static_cast<float>(expected_width) / original_size.width();
  const gfx::Size expected_size =
      gfx::ScaleToRoundedSize(original_size, expected_scale);

  EXPECT_EQ(header_image->GetPreferredSize(), expected_size);
}

}  // namespace
