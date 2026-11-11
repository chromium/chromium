// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_search_controller.h"

#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/test_lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/test_lens_search_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_registry.h"
#include "chrome/test/base/testing_profile.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/prefs/pref_service.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/variations/variations.mojom.h"
#include "components/variations/variations_client.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/geometry/rect_f.h"

using ::testing::Return;
using ::testing::ReturnRef;

namespace lens {

namespace {

class FakeVariationsClient : public variations::VariationsClient {
 public:
  bool IsOffTheRecord() const override { return false; }

  variations::mojom::VariationsHeadersPtr GetVariationsHeaders()
      const override {
    base::flat_map<variations::mojom::GoogleWebVisibility, std::string>
        headers = {
            {variations::mojom::GoogleWebVisibility::FIRST_PARTY, "123xyz"}};
    return variations::mojom::VariationsHeaders::New(headers);
  }
};

}  // namespace

class LensSearchControllerTest : public testing::Test {
 public:
  LensSearchControllerTest() = default;
  ~LensSearchControllerTest() override = default;

  void SetUp() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlay,
          {{"enable-gen204-semantic-event", "false"}}},
         {lens::features::kLensOverlayLatencyOptimizations,
          {{"enable-cluster-info-optimization", "true"}}},
         {lens::features::kLensOverlayContextualSearchbox,
          {{"characters-per-page-heuristic", "50"}}},
         {lens::features::kLensOverlayNonBlockingPrivacyNotice, {}},
         {lens::features::kLensOverlayNonBlockingPrivacyNoticeForImageSearch,
          {}}},
        {lens::features::kLensSidePanelUnification});
    TestingProfile::Builder profile_builder;
    profile_builder.AddTestingFactory(
        TemplateURLServiceFactory::GetInstance(),
        base::BindRepeating(&TemplateURLServiceFactory::BuildInstanceFor));
    profile_ = profile_builder.Build();
    web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), content::SiteInstance::Create(profile_.get()));

    mock_browser_window_interface_ =
        std::make_unique<MockBrowserWindowInterface>();
    ON_CALL(*mock_browser_window_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(*mock_browser_window_interface_, GetProfile())
        .WillByDefault(Return(profile_.get()));
    ON_CALL(mock_tab_interface_, GetUnownedUserDataHost())
        .WillByDefault(ReturnRef(user_data_host_));
    ON_CALL(mock_tab_interface_, GetBrowserWindowInterface())
        .WillByDefault(Return(mock_browser_window_interface_.get()));
    ON_CALL(mock_tab_interface_, GetContents())
        .WillByDefault(Return(web_contents_.get()));
    ON_CALL(mock_tab_interface_, IsActivated()).WillByDefault(Return(true));

    side_panel_registry_ =
        std::make_unique<SidePanelRegistry>(&mock_tab_interface_);

    controller_ =
        std::make_unique<TestLensSearchController>(&mock_tab_interface_);

    auto* theme_service = ThemeServiceFactory::GetForProfile(profile_.get());
    auto* identity_manager =
        IdentityManagerFactory::GetForProfile(profile_.get());
    auto* sync_service = SyncServiceFactory::GetForProfile(profile_.get());
    controller_->Initialize(&fake_variations_client_, identity_manager,
                            profile_->GetPrefs(), sync_service, theme_service);
  }

  void TearDown() override {
    if (controller_ && !controller_->IsOff()) {
      controller_->CloseLensSync(
          lens::LensOverlayDismissalSource::kOverlayCloseButton);
    }
    controller_.reset();
    side_panel_registry_.reset();
    mock_browser_window_interface_.reset();
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  content::RenderViewHostTestEnabler rvh_test_enabler_;
  base::test::ScopedFeatureList feature_list_;
  FakeVariationsClient fake_variations_client_;
  ui::UnownedUserDataHost user_data_host_;
  tabs::MockTabInterface mock_tab_interface_;
  std::unique_ptr<MockBrowserWindowInterface> mock_browser_window_interface_;
  std::unique_ptr<TestingProfile> profile_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<SidePanelRegistry> side_panel_registry_;
  std::unique_ptr<TestLensSearchController> controller_;
};

TEST_F(LensSearchControllerTest,
       OpenLensOverlayWithPendingRegion_AllowsNonBlockingPrivacyNotice) {
  profile_->GetPrefs()->SetBoolean(
      lens::prefs::kLensSharingPageScreenshotEnabled, false);
  profile_->GetPrefs()->SetBoolean(lens::prefs::kLensSharingPageContentEnabled,
                                   false);

  ASSERT_TRUE(controller_->IsOff());

  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  auto region = lens::mojom::CenterRotatedBox::New();
  region->box = gfx::RectF(0.1, 0.1, 0.2, 0.2);

  controller_->OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      std::move(region), bitmap);

  EXPECT_FALSE(controller_->IsOff());
}

TEST_F(LensSearchControllerTest,
       OpenLensOverlay_AllowsReinvokingFromContextualTasksComposebox) {
  ASSERT_TRUE(controller_->IsOff());

  // Start an initial Lens session so the controller is no longer off.
  controller_->OpenLensOverlay(lens::LensOverlayInvocationSource::kToolbar);
  EXPECT_FALSE(controller_->IsOff());
  EXPECT_EQ(controller_->invocation_source(),
            lens::LensOverlayInvocationSource::kToolbar);

  // With kLensSearchReinvocationAffordance disabled by default, reinvoking from
  // a standard entrypoint should be blocked while a session is active.
  controller_->OpenLensOverlay(lens::LensOverlayInvocationSource::kAppMenu);
  EXPECT_EQ(controller_->invocation_source(),
            lens::LensOverlayInvocationSource::kToolbar);

  // Reinvoking from the Contextual Tasks composebox should be permitted.
  controller_->OpenLensOverlay(
      lens::LensOverlayInvocationSource::kContextualTasksComposebox);
  EXPECT_EQ(controller_->invocation_source(),
            lens::LensOverlayInvocationSource::kContextualTasksComposebox);
}

TEST_F(LensSearchControllerTest, LensOverlayControllerFromTabInterface) {
  EXPECT_EQ(LensOverlayController::From(nullptr), nullptr);
  EXPECT_NE(controller_->lens_overlay_controller(), nullptr);
  EXPECT_EQ(LensOverlayController::From(&mock_tab_interface_),
            controller_->lens_overlay_controller());

  ui::UnownedUserDataHost empty_user_data_host;
  tabs::MockTabInterface tab_without_lens;
  ON_CALL(tab_without_lens, GetUnownedUserDataHost())
      .WillByDefault(ReturnRef(empty_user_data_host));
  EXPECT_EQ(LensOverlayController::From(&tab_without_lens), nullptr);
}

TEST_F(LensSearchControllerTest, OpenLensOverlay_NotifiesWillOpenCallbacks) {
  int call_count = 0;
  base::CallbackListSubscription subscription =
      controller_->RegisterWillOpenCallback(
          base::BindRepeating([](int* count) { ++(*count); }, &call_count));

  ON_CALL(mock_tab_interface_, IsActivated()).WillByDefault(Return(false));
  controller_->OpenLensOverlay(lens::LensOverlayInvocationSource::kToolbar);
  EXPECT_EQ(call_count, 0);
  EXPECT_TRUE(controller_->IsOff());

  ON_CALL(mock_tab_interface_, IsActivated()).WillByDefault(Return(true));
  controller_->OpenLensOverlay(lens::LensOverlayInvocationSource::kToolbar);
  EXPECT_EQ(call_count, 1);
  EXPECT_FALSE(controller_->IsOff());
}

TEST_F(LensSearchControllerTest,
       PrimaryPageChanged_ResetsQueryControllerPageContentData) {
  profile_->GetPrefs()->SetBoolean(
      lens::prefs::kLensSharingPageScreenshotEnabled, false);
  profile_->GetPrefs()->SetBoolean(lens::prefs::kLensSharingPageContentEnabled,
                                   false);

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://example.com/document.pdf"));

  controller_->OpenLensOverlay(lens::LensOverlayInvocationSource::kAppMenu);
  ASSERT_FALSE(controller_->IsOff());

  auto* query_controller = static_cast<TestLensOverlayQueryController*>(
      controller_->lens_overlay_query_controller());
  ASSERT_TRUE(query_controller);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.eraseColor(SK_ColorGREEN);
  std::vector<lens::PageContent> page_contents = {
      lens::PageContent({1, 2, 3, 4}, lens::MimeType::kPdf)};
  query_controller->StartQueryFlow(
      bitmap, bitmap, GURL("https://example.com/document.pdf"), "Document", {},
      page_contents, lens::MimeType::kPdf, /*pdf_current_page=*/0,
      /*ui_scale_factor=*/1.0f, base::TimeTicks::Now());

  std::vector<std::u16string> partial_content = {
      u"this is a page with enough content to make it substantial",
      u"this is a second page substantial enough content"};
  query_controller->SendPartialPageContentRequest(partial_content);
  EXPECT_EQ(query_controller->num_partial_page_content_requests_sent(), 0);

  // Navigating the primary page triggers CancelInFlightContextualization(),
  // which should reset the pending page content data on the query controller.
  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://other.example/page.html"));
  page_contents.clear();
  page_contents.shrink_to_fit();
  partial_content.clear();
  partial_content.shrink_to_fit();

  query_controller->GrantPermissionForSession();
  query_controller->MaybeRestartQueryFlow();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return query_controller->num_full_image_requests_sent() == 1; }));

  EXPECT_EQ(query_controller->num_page_content_update_requests_sent(), 0);
  EXPECT_EQ(query_controller->num_partial_page_content_requests_sent(), 0);
}

TEST_F(LensSearchControllerTest,
       DeferredPageContentRequests_SentWhenPageDoesNotNavigate) {
  profile_->GetPrefs()->SetBoolean(
      lens::prefs::kLensSharingPageScreenshotEnabled, false);
  profile_->GetPrefs()->SetBoolean(lens::prefs::kLensSharingPageContentEnabled,
                                   false);

  content::WebContentsTester::For(web_contents_.get())
      ->NavigateAndCommit(GURL("https://example.com/document.pdf"));

  controller_->OpenLensOverlay(lens::LensOverlayInvocationSource::kAppMenu);
  ASSERT_FALSE(controller_->IsOff());

  auto* query_controller = static_cast<TestLensOverlayQueryController*>(
      controller_->lens_overlay_query_controller());
  ASSERT_TRUE(query_controller);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(10, 10);
  bitmap.eraseColor(SK_ColorGREEN);
  std::vector<lens::PageContent> page_contents = {
      lens::PageContent({1, 2, 3, 4}, lens::MimeType::kPdf)};
  query_controller->StartQueryFlow(
      bitmap, bitmap, GURL("https://example.com/document.pdf"), "Document", {},
      page_contents, lens::MimeType::kPdf, /*pdf_current_page=*/0,
      /*ui_scale_factor=*/1.0f, base::TimeTicks::Now());

  std::vector<std::u16string> partial_content = {
      u"this is a page with enough content to make it substantial",
      u"this is a second page substantial enough content"};
  query_controller->SendPartialPageContentRequest(partial_content);
  EXPECT_EQ(query_controller->num_partial_page_content_requests_sent(), 0);

  query_controller->GrantPermissionForSession();
  query_controller->MaybeRestartQueryFlow();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return query_controller->num_full_image_requests_sent() == 1 &&
           query_controller->num_page_content_update_requests_sent() == 1 &&
           query_controller->num_partial_page_content_requests_sent() == 1;
  }));
}

}  // namespace lens
