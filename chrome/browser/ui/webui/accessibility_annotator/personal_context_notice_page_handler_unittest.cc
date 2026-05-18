// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/accessibility_annotator/personal_context_notice_page_handler.h"

#include <memory>
#include <string>

#include "base/functional/bind.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/webui/accessibility_annotator/personal_context_notice_ui.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/accessibility_annotator/core/url_constants.h"
#include "components/accessibility_annotator/first_run/accessibility_annotator_first_run_types.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace personal_context::notice {
using accessibility_annotator::InfoShowRequestResult;
namespace {

constexpr char kDialogResultHistogramName[] =
    "PersonalContext.NoticeInteractions";

class PersonalContextNoticePageHandlerTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    identity_test_env_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(profile());

    test_web_ui_.set_web_contents(web_contents());
    webui::SetBrowserWindowInterface(web_contents(), &mock_browser_interface_);

    info_ui_ = std::make_unique<PersonalContextNoticeUI>(&test_web_ui_);

    handler_ = std::make_unique<PersonalContextNoticePageHandler>(
        page_handler_.BindNewPipeAndPassReceiver(),
        base::BindLambdaForTesting(
            [this](NoticeDialogResult result) { result_ = result; }),
        *info_ui_, test_web_ui_.GetWebContents());
  }

  void TearDown() override {
    handler_.reset();
    info_ui_.reset();
    test_web_ui_.set_web_contents(nullptr);
    identity_test_env_adaptor_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return IdentityTestEnvironmentProfileAdaptor::
        GetIdentityTestEnvironmentFactories();
  }

 protected:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_env_adaptor_;
  testing::NiceMock<MockBrowserWindowInterface> mock_browser_interface_;
  mojo::Remote<personal_context::notice::mojom::PageHandler> page_handler_;
  std::unique_ptr<PersonalContextNoticeUI> info_ui_;
  std::unique_ptr<PersonalContextNoticePageHandler> handler_;
  std::optional<NoticeDialogResult> result_;
  base::UserActionTester user_action_tester_;
  base::HistogramTester histogram_tester_;
  content::TestWebUI test_web_ui_;
};

TEST_F(PersonalContextNoticePageHandlerTest, GetAccountInfo) {
  auto* identity_test_env = identity_test_env_adaptor_->identity_test_env();
  AccountInfo account_info = identity_test_env->MakePrimaryAccountAvailable(
      "test@example.com", signin::ConsentLevel::kSignin);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.eraseColor(SK_ColorRED);
  gfx::Image avatar_image = gfx::Image::CreateFrom1xBitmap(bitmap);

  signin::SimulateAccountImageFetch(identity_test_env->identity_manager(),
                                    account_info.account_id,
                                    "https://example.com/avatar", avatar_image);

  bool callback_called = false;
  handler_->GetAccountInfo(base::BindLambdaForTesting(
      [&](personal_context::notice::mojom::AccountInfoPtr info) {
        callback_called = true;
        ASSERT_TRUE(info);
        EXPECT_EQ("test@example.com", info->email);
        EXPECT_EQ(webui::GetBitmapDataUrl(bitmap), info->avatar_url);
      }));

  EXPECT_TRUE(callback_called);
}

TEST_F(PersonalContextNoticePageHandlerTest, GetAccountInfoSignedOut) {
  bool callback_called = false;
  handler_->GetAccountInfo(base::BindLambdaForTesting(
      [&](personal_context::notice::mojom::AccountInfoPtr info) {
        callback_called = true;
        ASSERT_TRUE(info);
        EXPECT_TRUE(info->email.empty());
        EXPECT_TRUE(info->avatar_url.empty());
      }));

  EXPECT_TRUE(callback_called);
}

TEST_F(PersonalContextNoticePageHandlerTest, OnInfoAcknowledged) {
  // Check callback was not run yet.
  EXPECT_FALSE(result_.has_value());
  histogram_tester_.ExpectTotalCount(kDialogResultHistogramName, 0);

  // User clicks "Acknowledge" on the dialog.
  handler_->OnInfoAcknowledged();

  // Check callback returned kAccepted and corresponding metrics were recorded.
  EXPECT_TRUE(result_.has_value());
  EXPECT_EQ(NoticeDialogResult::kAcknowledged, result_);
  histogram_tester_.ExpectUniqueSample(kDialogResultHistogramName,
                                       InfoShowRequestResult::kAccepted, 1);
}

// TODO(crbug.com/506117669): If no UI element for dismissal is added: delete
// this test and simplify the name of `OnInfoDismissedOnFrameworkClosure` test.
// Otherwise: use this test and drop this todo.
TEST_F(PersonalContextNoticePageHandlerTest, OnInfoDismissedByExplicitCall) {
  // Check callback was not run yet.
  EXPECT_FALSE(result_.has_value());
  histogram_tester_.ExpectTotalCount(kDialogResultHistogramName, 0);

  // User closes the dialog using the dialog UI.
  handler_->OnInfoDismissed();

  // Check callback returned kDismissed and corresponding metrics were recorded.
  EXPECT_TRUE(result_.has_value());
  EXPECT_EQ(NoticeDialogResult::kDismissed, result_);
  histogram_tester_.ExpectUniqueSample(kDialogResultHistogramName,
                                       InfoShowRequestResult::kDismissed, 1);
}

TEST_F(PersonalContextNoticePageHandlerTest,
       OnInfoDismissedOnFrameworkClosure) {
  // Check callback was not run yet.
  EXPECT_FALSE(result_.has_value());
  histogram_tester_.ExpectTotalCount(kDialogResultHistogramName, 0);

  // User closes the dialog by an Esc press or an outside click.
  handler_.reset();

  // Check callback returned kDismissed and corresponding metrics were recorded.
  EXPECT_TRUE(result_.has_value());
  EXPECT_EQ(NoticeDialogResult::kDismissed, result_);
  histogram_tester_.ExpectUniqueSample(kDialogResultHistogramName,
                                       InfoShowRequestResult::kDismissed, 1);
}

TEST_F(PersonalContextNoticePageHandlerTest, OnLearnMoreClicked) {
  EXPECT_EQ(0, user_action_tester_.GetActionCount(
                   "PersonalContext.Notice.LearnMoreLinkClick"));
  EXPECT_CALL(
      mock_browser_interface_,
      OpenURL(testing::Field(&content::OpenURLParams::url,
                             GURL(accessibility_annotator::
                                      kAccessibilityAnnotatorLearnMoreURL)),
              testing::_))
      .WillOnce(testing::Return(nullptr));

  handler_->OnLearnMoreClicked();

  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "PersonalContext.Notice.LearnMoreLinkClick"));
}

TEST_F(PersonalContextNoticePageHandlerTest, OnManageSettingsClicked) {
  EXPECT_EQ(0, user_action_tester_.GetActionCount(
                   "PersonalContext.Notice.SettingsLinkClick"));

  EXPECT_CALL(
      mock_browser_interface_,
      OpenURL(
          testing::Field(
              &content::OpenURLParams::url,
              GURL(
                  accessibility_annotator::kAccessibilityAnnotatorSettingsURL)),
          testing::_))
      .WillOnce(testing::Return(nullptr));

  handler_->OnManageSettingsClicked();

  EXPECT_EQ(1, user_action_tester_.GetActionCount(
                   "PersonalContext.Notice.SettingsLinkClick"));
}

}  // namespace
}  // namespace personal_context::notice
