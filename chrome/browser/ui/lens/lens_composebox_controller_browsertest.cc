// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_composebox_controller.h"

#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/lens/lens_composebox_handler.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/test_lens_overlay_controller.h"
#include "chrome/browser/ui/lens/test_lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/test_lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/test_lens_search_contextualization_controller.h"
#include "chrome/browser/ui/lens/test_lens_search_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_composebox_user_action.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

namespace {

class TestLensComposeboxController : public lens::LensComposeboxController {
 public:
  TestLensComposeboxController(LensSearchController* lens_search_controller,
                               Profile* profile)
      : lens::LensComposeboxController(lens_search_controller, profile) {}

  void BindComposebox(
      mojo::PendingReceiver<composebox::mojom::PageHandler> pending_handler,
      mojo::PendingRemote<composebox::mojom::Page> pending_page,
      mojo::PendingRemote<searchbox::mojom::Page> pending_searchbox_page,
      mojo::PendingReceiver<searchbox::mojom::PageHandler>
          pending_searchbox_handler) override {
    // Reset the receiver if it is already bound. This is necessary because
    // some tests re-open the overlay, which triggers a new WebUI to bind to
    // this same controller instance.
    if (mock_searchbox_page_.receiver_.is_bound()) {
      mock_searchbox_page_.receiver_.reset();
    }
    lens::LensComposeboxController::BindComposebox(
        std::move(pending_handler), std::move(pending_page),
        mock_searchbox_page_.BindAndGetRemote(),
        std::move(pending_searchbox_handler));
  }

  MockSearchboxPage& mock_searchbox_page() { return mock_searchbox_page_; }

 private:
  testing::NiceMock<MockSearchboxPage> mock_searchbox_page_;
};

using State = LensOverlayController::State;

constexpr char kTestSearchSessionId[] = "test_search_session_id";
constexpr char kTestServerSessionId[] = "test_server_session_id";

const lens::mojom::CenterRotatedBoxPtr kTestRegion =
    lens::mojom::CenterRotatedBox::New(
        gfx::RectF(0.5, 0.5, 0.8, 0.8),
        0.0,
        lens::mojom::CenterRotatedBox_CoordinateType::kNormalized);

const SkBitmap CreateNonEmptyBitmap(int width, int height) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(SK_ColorGREEN);
  return bitmap;
}

// A fake LensSearchController to inject fake controllers.
class LensSearchControllerFake : public lens::TestLensSearchController {
 public:
  explicit LensSearchControllerFake(tabs::TabInterface* tab)
      : lens::TestLensSearchController(tab) {}

  ~LensSearchControllerFake() override = default;

 protected:
  std::unique_ptr<lens::LensSearchContextualizationController>
  CreateLensSearchContextualizationController() override {
    return std::make_unique<lens::TestLensSearchContextualizationController>(
        this);
  }

  std::unique_ptr<LensOverlayController> CreateLensOverlayController(
      tabs::TabInterface* tab,
      LensSearchController* lens_search_controller,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      syncer::SyncService* sync_service,
      ThemeService* theme_service) override {
    return std::make_unique<lens::TestLensOverlayController>(
        tab, lens_search_controller, variations_client, identity_manager,
        pref_service, sync_service, theme_service);
  }

  std::unique_ptr<lens::LensOverlayQueryController> CreateLensQueryController(
      lens::LensOverlayFullImageResponseCallback full_image_callback,
      lens::LensOverlayUrlResponseCallback url_callback,
      lens::LensOverlayInteractionResponseCallback interaction_callback,
      lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      lens::UploadProgressCallback upload_progress_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      Profile* profile,
      lens::LensOverlayInvocationSource invocation_source,
      bool use_dark_mode,
      lens::LensOverlayGen204Controller* gen204_controller) override {
    auto fake_query_controller =
        std::make_unique<lens::TestLensOverlayQueryController>(
            full_image_callback, url_callback, interaction_callback,
            thumbnail_created_callback, upload_progress_callback,
            variations_client, identity_manager, profile, invocation_source,
            use_dark_mode, gen204_controller);

    // Set up the cluster info to test the search session ID is propagated.
    lens::LensOverlayServerClusterInfoResponse cluster_info_response;
    cluster_info_response.set_server_session_id(kTestServerSessionId);
    cluster_info_response.set_search_session_id(kTestSearchSessionId);
    fake_query_controller->set_fake_cluster_info_response(
        cluster_info_response);

    return fake_query_controller;
  }

  std::unique_ptr<lens::LensOverlaySidePanelCoordinator>
  CreateLensOverlaySidePanelCoordinator() override {
    return std::make_unique<lens::TestLensOverlaySidePanelCoordinator>(this);
  }

  std::unique_ptr<lens::LensComposeboxController>
  CreateLensComposeboxController() override {
    Profile* profile = Profile::FromBrowserContext(
        GetTabInterface()->GetContents()->GetBrowserContext());
    return std::make_unique<TestLensComposeboxController>(this, profile);
  }
};

ui::UserDataFactory::ScopedOverride UseFakeLensSearchController() {
  return tabs::TabFeatures::GetUserDataFactoryForTesting()
      .AddOverrideForTesting(base::BindRepeating([](tabs::TabInterface& tab) {
        return std::make_unique<LensSearchControllerFake>(&tab);
      }));
}

}  // namespace

class LensComposeboxControllerBrowserTest : public InProcessBrowserTest {
 protected:
  LensComposeboxControllerBrowserTest() {
    lens_search_controller_override_ = UseFakeLensSearchController();
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/
        {{lens::features::kLensOverlay,
          /*params=*/{}},
         {lens::features::kLensSearchAimM3, /*params=*/{}},
         {lens::features::kLensOverlayContextualSearchbox,
          {
              //  Updating the viewport each query can cause flakiness
              //  when checking the sequence ids.
              {"update-viewport-each-query", "false"},
          }},
         {lens::features::kLensAimSuggestions,
          {{"lens-aim-suggestions-type", "Contextual"}}},
         {lens::features::kLensSearchReinvocationAffordance, {}}},
        /*disabled_features=*/{omnibox::kAimServerEligibilityEnabled});

    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();

    // Permits sharing the page screenshot by default.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, true);
    prefs->SetBoolean(lens::prefs::kLensSharingPageContentEnabled, true);
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();

    // Disallow sharing the page screenshot by default.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, false);
    prefs->SetBoolean(lens::prefs::kLensSharingPageContentEnabled, false);
  }

  void WaitForPaint(std::string_view relative_url = "/select.html") {
    const GURL url = embedded_test_server()->GetURL(relative_url);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  }

  bool IsResultsSidePanelShowing() {
    return GetLensSidePanelCoordinator()->IsEntryShowing();
  }

  LensSearchController* GetLensSearchController() {
    return LensSearchController::From(browser()->GetActiveTabInterface());
  }

  LensOverlayController* GetLensOverlayController() {
    return GetLensSearchController()->lens_overlay_controller();
  }

  lens::LensComposeboxController* GetLensComposeboxController() {
    return GetLensSearchController()->lens_composebox_controller();
  }

  lens::TestLensOverlaySidePanelCoordinator* GetLensSidePanelCoordinator() {
    return static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
        GetLensSearchController()->lens_overlay_side_panel_coordinator());
  }

  void MockAimToClientMessage(lens::AimToClientMessage aim_to_client_message) {
    auto* lens_side_panel_coordinator = GetLensSidePanelCoordinator();
    ;
    ASSERT_TRUE(lens_side_panel_coordinator);

    // Serialize the message and pass it to the side panel coordinator.
    std::vector<uint8_t> serialized_message(
        aim_to_client_message.ByteSizeLong());
    aim_to_client_message.SerializeToArray(&serialized_message[0],
                                           serialized_message.size());

    lens_side_panel_coordinator->OnAimMessage(serialized_message);
  }

  base::test::ScopedFeatureList feature_list_;

 private:
  ui::UserDataFactory::ScopedOverride lens_search_controller_override_;
};

IN_PROC_BROWSER_TEST_F(LensComposeboxControllerBrowserTest,
                       HandshakeResponseHandling) {
  WaitForPaint();

  auto* lens_controller = GetLensSearchController();
  ASSERT_TRUE(lens_controller);

  // Open the overlay directly to the side panel so composebox is visible.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  GetLensSearchController()->OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));

  // Wait for the composebox handler to be set and then send a fake AIM query
  // via mojo.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()->composebox_handler_for_testing() !=
           nullptr;
  }));

  // Mock a handshake response.
  lens::AimToClientMessage aim_to_client_message;
  aim_to_client_message.mutable_handshake_response()->add_capabilities(
      lens::FeatureCapability::DEFAULT);
  MockAimToClientMessage(aim_to_client_message);

  // Verify the handshake is sent to the side panel.
  auto* test_side_panel_coordinator = GetLensSidePanelCoordinator();
  ASSERT_TRUE(test_side_panel_coordinator);
  ASSERT_EQ(test_side_panel_coordinator->aim_handshake_received_call_count_, 1);
}

IN_PROC_BROWSER_TEST_F(LensComposeboxControllerBrowserTest,
                       IssueComposeboxQuerySendsPostMessage) {
  WaitForPaint();

  auto* lens_controller = GetLensSearchController();
  ASSERT_TRUE(lens_controller);

  // Open the overlay directly to the side panel so composebox is visible.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  lens_controller->OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));

  // Wait for the composebox handler to be set and then send a fake AIM query
  // via mojo.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()->composebox_handler_for_testing() !=
           nullptr;
  }));

  // Also need to run until the query controller has send all requests to avoid
  // flakiness.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          lens_controller->lens_overlay_query_controller());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->num_full_image_requests_sent() == 1 &&
           fake_query_controller->num_page_content_update_requests_sent() ==
               1 &&
           fake_query_controller->num_interaction_requests_sent() == 1;
  }));

  // Mock a handshake call so the composebox controller can send query messages.
  lens::AimToClientMessage aim_to_client_message;
  aim_to_client_message.mutable_handshake_response()->add_capabilities(
      lens::FeatureCapability::DEFAULT);
  MockAimToClientMessage(aim_to_client_message);

  // Send a query.
  GetLensComposeboxController()->composebox_handler_for_testing()->SubmitQuery(
      "test query", /*mouse_button=*/0, /*alt_key=*/false, /*ctrl_key=*/false,
      /*meta_key=*/false,
      /*shift_key=*/false);

  // Verify the client message sent.
  auto* test_side_panel_coordinator = GetLensSidePanelCoordinator();
  ASSERT_TRUE(test_side_panel_coordinator);
  ASSERT_TRUE(test_side_panel_coordinator->last_sent_client_message_to_aim_
                  .has_submit_query());

  // Verify the submit query message.
  auto submit_query = test_side_panel_coordinator
                          ->last_sent_client_message_to_aim_.submit_query();
  ASSERT_EQ(submit_query.payload().query_text(), "test query");
  ASSERT_EQ(submit_query.payload().query_text_source(),
            lens::QueryPayload::QUERY_TEXT_SOURCE_KEYBOARD_INPUT);
  ASSERT_EQ(submit_query.payload().lens_image_query_data_size(), 1);
  auto lens_image_query_data = submit_query.payload().lens_image_query_data(0);
  ASSERT_EQ(lens_image_query_data.search_session_id(), kTestSearchSessionId);
  ASSERT_EQ(lens_image_query_data.request_id().sequence_id(), 4);
  ASSERT_EQ(lens_image_query_data.request_id().long_context_id(), 1);
  ASSERT_EQ(lens_image_query_data.request_id().image_sequence_id(), 1);
}

IN_PROC_BROWSER_TEST_F(LensComposeboxControllerBrowserTest,
                       LogsComposeboxMetrics) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  auto* lens_controller = GetLensSearchController();
  ASSERT_TRUE(lens_controller);

  // Open the overlay directly to the side panel so composebox is visible.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  lens_controller->OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);
  auto* overlay_controller = GetLensOverlayController();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));

  // Wait for the composebox handler to be set and then send a fake AIM query
  // via mojo.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()->composebox_handler_for_testing() !=
           nullptr;
  }));

  auto* composebox_handler =
      GetLensComposeboxController()->composebox_handler_for_testing();
  ASSERT_TRUE(composebox_handler);

  // Verify the side panel has opened and contents have finished loading.
  // This is needed to prevent the handshake from being reset.
  auto* test_side_panel_coordinator = GetLensSidePanelCoordinator();
  ASSERT_TRUE(test_side_panel_coordinator);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return test_side_panel_coordinator->GetSidePanelWebContents() != nullptr;
  }));
  ASSERT_TRUE(content::WaitForLoadStop(
      test_side_panel_coordinator->GetSidePanelWebContents()));

  // Mock a focus of the composebox. Should be logged.
  composebox_handler->FocusChanged(true);
  histogram_tester.ExpectBucketCount("Lens.Composebox.UserAction",
                                     lens::LensComposeboxUserAction::kFocused,
                                     1);

  // Mock a focus out of the composebox. Should not be logged.
  composebox_handler->FocusChanged(false);
  histogram_tester.ExpectBucketCount("Lens.Composebox.UserAction",
                                     lens::LensComposeboxUserAction::kFocused,
                                     1);

  // A new focus should be logged.
  composebox_handler->FocusChanged(true);
  histogram_tester.ExpectBucketCount("Lens.Composebox.UserAction",
                                     lens::LensComposeboxUserAction::kFocused,
                                     2);

  // Mock a handshake response for the session end metrics.
  lens::AimToClientMessage aim_to_client_message;
  aim_to_client_message.mutable_handshake_response()->add_capabilities(
      lens::FeatureCapability::DEFAULT);
  MockAimToClientMessage(aim_to_client_message);

  // Send a query.
  GetLensComposeboxController()->composebox_handler_for_testing()->SubmitQuery(
      "test query", /*mouse_button=*/0, /*alt_key=*/false, /*ctrl_key=*/false,
      /*meta_key=*/false,
      /*shift_key=*/false);
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.UserAction",
      lens::LensComposeboxUserAction::kQuerySubmitted, 1);
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.UserAction",
      lens::LensComposeboxUserAction::kQueryIssued, 1);

  // Also need to wait for the overlay controller to hide to prevent
  // race conditions with `CloseLensSync`.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return overlay_controller->state() == State::kHidden; }));

  // Send another query.
  GetLensComposeboxController()->composebox_handler_for_testing()->SubmitQuery(
      "test query 2", /*mouse_button=*/0, /*alt_key=*/false, /*ctrl_key=*/false,
      /*meta_key=*/false,
      /*shift_key=*/false);
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.UserAction",
      lens::LensComposeboxUserAction::kQuerySubmitted, 2);
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.UserAction",
      lens::LensComposeboxUserAction::kQueryIssued, 2);

  // Close the overlay to trigger session end metrics.
  lens_controller->CloseLensSync(
      lens::LensOverlayDismissalSource::kOverlayCloseButton);
  ASSERT_TRUE(base::test::RunUntil([&]() { return lens_controller->IsOff(); }));

  // Verify session end metrics are logged once.
  histogram_tester.ExpectUniqueSample("Lens.Composebox.ShownInSession", true,
                                      1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Composebox.HandshakeCompletedInSession", true, 1);
  histogram_tester.ExpectBucketCount("Lens.Composebox.UserActionInSession",
                                     lens::LensComposeboxUserAction::kFocused,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.UserActionInSession",
      lens::LensComposeboxUserAction::kQuerySubmitted, 1);
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.UserActionInSession",
      lens::LensComposeboxUserAction::kQueryIssued, 1);

  // Start a new session.
  lens_controller->OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));

  // Wait for the composebox handler to be set.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()->composebox_handler_for_testing() !=
           nullptr;
  }));

  // Submit a query before the handshake so it never is issued.
  GetLensComposeboxController()->composebox_handler_for_testing()->SubmitQuery(
      "test query", /*mouse_button=*/0, /*alt_key=*/false, /*ctrl_key=*/false,
      /*meta_key=*/false,
      /*shift_key=*/false);

  // The new query should be logged as submitted but not issued.
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.UserAction",
      lens::LensComposeboxUserAction::kQuerySubmitted, 3);
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.UserAction",
      lens::LensComposeboxUserAction::kQueryIssued, 2);

  // Close the overlay to trigger session end metrics again.
  lens_controller->CloseLensSync(
      lens::LensOverlayDismissalSource::kSidePanelCloseButton);
  ASSERT_TRUE(base::test::RunUntil([&]() { return lens_controller->IsOff(); }));

  // Verify session end metrics totals.
  histogram_tester.ExpectUniqueSample("Lens.Composebox.ShownInSession", true,
                                      2);
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.HandshakeCompletedInSession", true, 1);
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.HandshakeCompletedInSession", false, 1);
  histogram_tester.ExpectBucketCount("Lens.Composebox.UserActionInSession",
                                     lens::LensComposeboxUserAction::kFocused,
                                     1);
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.UserActionInSession",
      lens::LensComposeboxUserAction::kQuerySubmitted, 2);
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.UserActionInSession",
      lens::LensComposeboxUserAction::kQueryIssued, 1);
}

IN_PROC_BROWSER_TEST_F(LensComposeboxControllerBrowserTest,
                       LensButtonClickReshowsOverlay) {
  WaitForPaint();

  auto* lens_controller = GetLensSearchController();
  ASSERT_TRUE(lens_controller);

  // Open the overlay directly to the side panel so composebox is visible.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  lens_controller->OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));

  // Wait for the composebox handler to be set.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()->composebox_handler_for_testing() !=
           nullptr;
  }));

  // Hide the overlay. The state should transition to hidden since the side
  // panel is open.
  lens_controller->HideOverlay(
      lens::LensOverlayDismissalSource::kOverlayBackgroundClick);
  auto* overlay_controller = GetLensOverlayController();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return overlay_controller->state() == State::kHidden; }));

  // Simulate Lens button click. This should reshow the overlay.
  GetLensComposeboxController()
      ->composebox_handler_for_testing()
      ->HandleLensButtonClick();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));
}

IN_PROC_BROWSER_TEST_F(LensComposeboxControllerBrowserTest,
                       QueryBeforeHandshakeIsQueued) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  auto* lens_controller = GetLensSearchController();
  ASSERT_TRUE(lens_controller);

  // Open the overlay directly to the side panel so composebox is visible.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  lens_controller->OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));

  // Wait for the composebox handler to be set.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()->composebox_handler_for_testing() !=
           nullptr;
  }));

  // Also need to run until the query controller has sent all requests to avoid
  // flakiness.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          lens_controller->lens_overlay_query_controller());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->num_full_image_requests_sent() == 1 &&
           fake_query_controller->num_page_content_update_requests_sent() ==
               1 &&
           fake_query_controller->num_interaction_requests_sent() == 1;
  }));

  // Send a query before handshake.
  GetLensComposeboxController()->composebox_handler_for_testing()->SubmitQuery(
      "test query", /*mouse_button=*/0, /*alt_key=*/false, /*ctrl_key=*/false,
      /*meta_key=*/false,
      /*shift_key=*/false);
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.UserAction",
      lens::LensComposeboxUserAction::kQuerySubmitted, 1);
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.UserAction",
      lens::LensComposeboxUserAction::kQueryIssued, 0);

  // Send another query. This should overwrite the last one.
  GetLensComposeboxController()->composebox_handler_for_testing()->SubmitQuery(
      "test query 2", /*mouse_button=*/0, /*alt_key=*/false,
      /*ctrl_key=*/false,
      /*meta_key=*/false,
      /*shift_key=*/false);
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.UserAction",
      lens::LensComposeboxUserAction::kQuerySubmitted, 2);
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.UserAction",
      lens::LensComposeboxUserAction::kQueryIssued, 0);

  // Verify the client message was not sent.
  auto* test_side_panel_coordinator = GetLensSidePanelCoordinator();
  ASSERT_TRUE(test_side_panel_coordinator);
  ASSERT_FALSE(test_side_panel_coordinator->last_sent_client_message_to_aim_
                   .has_submit_query());

  // Mock a handshake call so the composebox controller can send query messages.
  lens::AimToClientMessage aim_to_client_message;
  aim_to_client_message.mutable_handshake_response()->add_capabilities(
      lens::FeatureCapability::DEFAULT);
  MockAimToClientMessage(aim_to_client_message);

  // Verify the client message sent.
  ASSERT_TRUE(test_side_panel_coordinator->last_sent_client_message_to_aim_
                  .has_submit_query());
  histogram_tester.ExpectBucketCount(
      "Lens.Composebox.UserAction",
      lens::LensComposeboxUserAction::kQueryIssued, 1);

  // Verify the submit query message.
  auto submit_query = test_side_panel_coordinator
                          ->last_sent_client_message_to_aim_.submit_query();
  ASSERT_EQ(submit_query.payload().query_text(), "test query 2");
}

IN_PROC_BROWSER_TEST_F(LensComposeboxControllerBrowserTest,
                       MediaTypeChangesWithRegionSelection) {
  WaitForPaint();

  auto* lens_controller = GetLensSearchController();
  ASSERT_TRUE(lens_controller);

  // Open the overlay directly to the side panel so composebox is visible.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  lens_controller->OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));

  // Wait for the composebox handler to be set and then send a fake AIM query
  // via mojo.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()->composebox_handler_for_testing() !=
           nullptr;
  }));

  // Also need to run until the query controller has send all requests to avoid
  // flakiness.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          lens_controller->lens_overlay_query_controller());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->num_full_image_requests_sent() == 1 &&
           fake_query_controller->num_page_content_update_requests_sent() ==
               1 &&
           fake_query_controller->num_interaction_requests_sent() == 1;
  }));

  // Verify that there is a region selection.
  auto* overlay_controller = GetLensOverlayController();
  ASSERT_TRUE(overlay_controller->HasRegionSelection());

  // Mock a handshake call so the composebox controller can send query messages.
  lens::AimToClientMessage aim_to_client_message;
  aim_to_client_message.mutable_handshake_response()->add_capabilities(
      lens::FeatureCapability::DEFAULT);
  MockAimToClientMessage(aim_to_client_message);

  // Send a query.
  GetLensComposeboxController()->composebox_handler_for_testing()->SubmitQuery(
      "test query", /*mouse_button=*/0, /*alt_key=*/false, /*ctrl_key=*/false,
      /*meta_key=*/false,
      /*shift_key=*/false);

  // Verify the client message sent.
  auto* test_side_panel_coordinator = GetLensSidePanelCoordinator();
  ASSERT_TRUE(test_side_panel_coordinator);
  ASSERT_TRUE(test_side_panel_coordinator->last_sent_client_message_to_aim_
                  .has_submit_query());

  // Verify the media type.
  auto submit_query = test_side_panel_coordinator
                          ->last_sent_client_message_to_aim_.submit_query();
  ASSERT_EQ(submit_query.payload().lens_image_query_data_size(), 1);
  auto lens_image_query_data = submit_query.payload().lens_image_query_data(0);
  EXPECT_EQ(lens_image_query_data.request_id().media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);

  // Clear the region selection.
  overlay_controller->ClearRegionSelectionForTesting();
  ASSERT_FALSE(overlay_controller->HasRegionSelection());

  // Send another query.
  GetLensComposeboxController()->composebox_handler_for_testing()->SubmitQuery(
      "test query 2", /*mouse_button=*/0, /*alt_key=*/false, /*ctrl_key=*/false,
      /*meta_key=*/false,
      /*shift_key=*/false);

  // Verify the new message.
  submit_query = test_side_panel_coordinator->last_sent_client_message_to_aim_
                     .submit_query();
  ASSERT_EQ(submit_query.payload().lens_image_query_data_size(), 1);
  lens_image_query_data = submit_query.payload().lens_image_query_data(0);
  EXPECT_NE(lens_image_query_data.request_id().media_type(),
            lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE);
}

IN_PROC_BROWSER_TEST_F(LensComposeboxControllerBrowserTest,
                       ComposeboxPopulatesLensSuggestInputs) {
  WaitForPaint();

  auto* lens_controller = GetLensSearchController();
  ASSERT_TRUE(lens_controller);

  // Open the overlay directly to the side panel so composebox is visible.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  lens_controller->OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));

  // Wait for the composebox handler to be set.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()->composebox_handler_for_testing() !=
           nullptr;
  }));

  // Also need to run until the query controller has send all requests to avoid
  // flakiness.
  // The TestLensOverlayQueryController is guaranteed to be the actual type
  // returned by the fake controller.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          lens_controller->lens_overlay_query_controller());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->num_full_image_requests_sent() == 1 &&
           fake_query_controller->num_page_content_update_requests_sent() ==
               1 &&
           fake_query_controller->num_interaction_requests_sent() == 1;
  }));

  // Mock a handshake call so the composebox controller can send query messages.
  lens::AimToClientMessage aim_to_client_message;
  aim_to_client_message.mutable_handshake_response()->add_capabilities(
      lens::FeatureCapability::DEFAULT);
  MockAimToClientMessage(aim_to_client_message);

  // Get composebox handler.
  auto* composebox_handler =
      GetLensComposeboxController()->composebox_handler_for_testing();
  ASSERT_TRUE(composebox_handler);

  // Mock a focus of the composebox.
  composebox_handler->FocusChanged(true);

  // After focusing, one suggestion request should have been sent.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()
               ->GetLensSuggestInputs()
               .ByteSizeLong() > 0;
  }));
}

IN_PROC_BROWSER_TEST_F(LensComposeboxControllerBrowserTest,
                       ComposeboxClearsLensSuggestInputsOnClose) {
  WaitForPaint();

  auto* lens_controller = GetLensSearchController();
  ASSERT_TRUE(lens_controller);

  // Open the overlay directly to the side panel so composebox is visible.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  lens_controller->OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));

  // Wait for the composebox handler to be set.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()->composebox_handler_for_testing() !=
           nullptr;
  }));

  // Also need to run until the query controller has send all requests to avoid
  // flakiness.
  // The TestLensOverlayQueryController is guaranteed to be the actual type
  // returned by the fake controller.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          lens_controller->lens_overlay_query_controller());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->num_full_image_requests_sent() == 1 &&
           fake_query_controller->num_page_content_update_requests_sent() ==
               1 &&
           fake_query_controller->num_interaction_requests_sent() == 1;
  }));

  // Mock a handshake call so the composebox controller can send query messages.
  lens::AimToClientMessage aim_to_client_message;
  aim_to_client_message.mutable_handshake_response()->add_capabilities(
      lens::FeatureCapability::DEFAULT);
  MockAimToClientMessage(aim_to_client_message);

  // Get composebox handler.
  auto* composebox_handler =
      GetLensComposeboxController()->composebox_handler_for_testing();
  ASSERT_TRUE(composebox_handler);

  // Mock a focus of the composebox.
  composebox_handler->FocusChanged(true);

  // After focusing, one suggestion request should have been sent.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()
               ->GetLensSuggestInputs()
               .ByteSizeLong() > 0;
  }));

  // Close the overlay to trigger CloseUI.
  lens_controller->CloseLensSync(
      lens::LensOverlayDismissalSource::kOverlayCloseButton);
  auto* overlay_controller = GetLensOverlayController();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return overlay_controller->state() == State::kOff; }));

  // Verify suggest inputs are cleared.
  ASSERT_EQ(
      GetLensComposeboxController()->GetLensSuggestInputs().ByteSizeLong(),
      static_cast<size_t>(0));
}

IN_PROC_BROWSER_TEST_F(
    LensComposeboxControllerBrowserTest,
    AddVisualSelectionContext_DoesNotAddNonAimFollowUpSelections) {
  WaitForPaint();

  auto* lens_controller = GetLensSearchController();
  ASSERT_TRUE(lens_controller);
  auto* composebox_controller = GetLensComposeboxController();
  auto* overlay_controller = GetLensOverlayController();
  auto* test_composebox_controller =
      static_cast<TestLensComposeboxController*>(composebox_controller);
  ASSERT_TRUE(test_composebox_controller);

  MockSearchboxPage& mock_searchbox_page =
      test_composebox_controller->mock_searchbox_page();
  EXPECT_CALL(mock_searchbox_page, AddFileContext(testing::_, testing::_))
      .Times(0);

  // Open the overlay directly to the side panel so composebox is visible.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  lens_controller->OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));

  // A visual selection context should not have been added but the overlay
  // should still have a region selection. This is because this is a not an AIM
  // follow up selection.
  auto vsc_id = composebox_controller->vsc_image_data_id_for_testing();
  ASSERT_FALSE(vsc_id.has_value());
  ASSERT_TRUE(overlay_controller->HasRegionSelection());
}

IN_PROC_BROWSER_TEST_F(LensComposeboxControllerBrowserTest,
                       AddVisualSelectionContext) {
  WaitForPaint();

  auto* lens_controller = GetLensSearchController();
  ASSERT_TRUE(lens_controller);

  // Open the overlay directly to the side panel so composebox is visible.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  lens_controller->OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));

  // Wait for the composebox handler to be set.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()->composebox_handler_for_testing() !=
           nullptr;
  }));

  // Also need to run until the query controller has send all requests to avoid
  // flakiness.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          lens_controller->lens_overlay_query_controller());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->num_full_image_requests_sent() == 1 &&
           fake_query_controller->num_page_content_update_requests_sent() ==
               1 &&
           fake_query_controller->num_interaction_requests_sent() == 1;
  }));

  auto* composebox_controller = GetLensComposeboxController();
  auto* test_composebox_controller =
      static_cast<TestLensComposeboxController*>(composebox_controller);
  ASSERT_TRUE(test_composebox_controller);
  MockSearchboxPage& mock_searchbox_page =
      test_composebox_controller->mock_searchbox_page();
  const std::string thumbnail_uri = "data:image/png;base64,sometestdata";
  // SelectedFileInfoPtr is a move-only type, so capture it in the lambda.
  searchbox::mojom::SelectedFileInfoPtr captured_file_info;
  EXPECT_CALL(mock_searchbox_page, AddFileContext(testing::_, testing::_))
      .Times(1)
      .WillOnce([&](const base::UnguessableToken&,
                    searchbox::mojom::SelectedFileInfoPtr info) {
        captured_file_info = std::move(info);
      });
  composebox_controller->AddVisualSelectionContext(thumbnail_uri);

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return !captured_file_info.is_null(); }));
  ASSERT_EQ(captured_file_info->file_name, "Visual Selection");
  ASSERT_EQ(captured_file_info->mime_type, "image/png");
  ASSERT_EQ(captured_file_info->image_data_url, thumbnail_uri);
  ASSERT_EQ(captured_file_info->is_deletable, true);
}

IN_PROC_BROWSER_TEST_F(LensComposeboxControllerBrowserTest,
                       UpdatesVisualSearchInteractionDataBetweenInteractions) {
  WaitForPaint();

  auto* lens_controller = GetLensSearchController();
  ASSERT_TRUE(lens_controller);

  // Open the overlay directly to the side panel so composebox is visible.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  lens_controller->OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);
  auto* overlay_controller = GetLensOverlayController();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));

  // Wait for the composebox handler to be set.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()->composebox_handler_for_testing() !=
           nullptr;
  }));

  // Also need to run until the query controller has sent all requests to avoid
  // flakiness.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          lens_controller->lens_overlay_query_controller());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->num_full_image_requests_sent() == 1 &&
           fake_query_controller->num_page_content_update_requests_sent() ==
               1 &&
           fake_query_controller->num_interaction_requests_sent() == 1;
  }));

  // Mock a handshake call so the composebox controller can send query messages.
  lens::AimToClientMessage aim_to_client_message;
  aim_to_client_message.mutable_handshake_response()->add_capabilities(
      lens::FeatureCapability::DEFAULT);
  MockAimToClientMessage(aim_to_client_message);

  // Send a query.
  GetLensComposeboxController()->composebox_handler_for_testing()->SubmitQuery(
      "test query", /*mouse_button=*/0, /*alt_key=*/false, /*ctrl_key=*/false,
      /*meta_key=*/false,
      /*shift_key=*/false);

  // Verify the client message sent.
  auto* test_side_panel_coordinator = GetLensSidePanelCoordinator();
  ASSERT_TRUE(test_side_panel_coordinator);
  ASSERT_TRUE(test_side_panel_coordinator->last_sent_client_message_to_aim_
                  .has_submit_query());

  // Verify the visual search interaction data has the region.
  auto submit_query = test_side_panel_coordinator
                          ->last_sent_client_message_to_aim_.submit_query();
  auto lens_image_query_data = submit_query.payload().lens_image_query_data(0);
  ASSERT_TRUE(lens_image_query_data.has_visual_search_interaction_data());
  ASSERT_TRUE(
      lens_image_query_data.visual_search_interaction_data().has_zoomed_crop());
  auto zoomed_crop =
      lens_image_query_data.visual_search_interaction_data().zoomed_crop();
  const float tolerance = 0.01f;
  EXPECT_NEAR(zoomed_crop.crop().center_x(), 0.5f, tolerance);
  EXPECT_NEAR(zoomed_crop.crop().center_y(), 0.5f, tolerance);
  EXPECT_NEAR(zoomed_crop.crop().width(), 0.8f, tolerance);
  EXPECT_NEAR(zoomed_crop.crop().height(), 0.8f, tolerance);

  // Clear the region selection.
  overlay_controller->ClearRegionSelectionForTesting();

  // Send another query.
  GetLensComposeboxController()->composebox_handler_for_testing()->SubmitQuery(
      "test query 2", /*mouse_button=*/0, /*alt_key=*/false,
      /*ctrl_key=*/false,
      /*meta_key=*/false,
      /*shift_key=*/false);

  // Verify the new message has no image crop in the vsint data.
  submit_query = test_side_panel_coordinator->last_sent_client_message_to_aim_
                     .submit_query();
  lens_image_query_data = submit_query.payload().lens_image_query_data(0);
  EXPECT_FALSE(lens_image_query_data.has_visual_search_interaction_data());
}

IN_PROC_BROWSER_TEST_F(LensComposeboxControllerBrowserTest,
                       DeleteVisualSelectionContext) {
  WaitForPaint();
  auto* controller = GetLensSearchController();
  ASSERT_TRUE(controller->IsOff());

  // Issue a text search request to open the side panel without the overlay.
  controller->IssueTextSearchRequest(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuText, "query",
      {}, AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      /*suppress_contextualization=*/true);

  // Wait for side panel to be visible.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));

  // Wait for the composebox handler to be set.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()->composebox_handler_for_testing() !=
           nullptr;
  }));

  // Also need to run until the query controller has sent all requests to avoid
  // flakiness.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          controller->lens_overlay_query_controller());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->num_full_image_requests_sent() == 1 &&
           fake_query_controller->num_page_content_update_requests_sent() == 1;
  }));

  // No visual selection context should have been added.
  auto* composebox_controller = GetLensComposeboxController();
  auto* overlay_controller = GetLensOverlayController();
  EXPECT_FALSE(
      composebox_controller->vsc_image_data_id_for_testing().has_value());
  EXPECT_FALSE(overlay_controller->HasRegionSelection());
  EXPECT_FALSE(overlay_controller->IsOverlayActive());

  // Reshow the overlay.
  controller->OpenLensOverlayInCurrentSession();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return overlay_controller->state() == State::kOverlay; }));

  // Issue a visual search request.
  overlay_controller->IssueLensRegionRequestForTesting(kTestRegion.Clone(),
                                                       /*is_click=*/false);

  // A visual selection context should have been added.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return composebox_controller->vsc_image_data_id_for_testing().has_value();
  }));
  ASSERT_TRUE(overlay_controller->HasRegionSelection());

  // Delete with a random token should not clear the selection.
  composebox_controller->DeleteContext(base::UnguessableToken::Create());
  ASSERT_TRUE(overlay_controller->HasRegionSelection());
  ASSERT_TRUE(
      composebox_controller->vsc_image_data_id_for_testing().has_value());

  // Delete with the correct token should clear the selection.
  auto vsc_id = composebox_controller->vsc_image_data_id_for_testing();
  composebox_controller->DeleteContext(vsc_id.value());
  ASSERT_FALSE(overlay_controller->HasRegionSelection());
  ASSERT_FALSE(
      composebox_controller->vsc_image_data_id_for_testing().has_value());
}

IN_PROC_BROWSER_TEST_F(LensComposeboxControllerBrowserTest,
                       ClearFilesDeletesVisualSelectionContext) {
  WaitForPaint();
  auto* controller = GetLensSearchController();
  ASSERT_TRUE(controller->IsOff());

  // Issue a text search request to open the side panel without the overlay.
  controller->IssueTextSearchRequest(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuText, "query",
      {}, AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      /*suppress_contextualization=*/true);

  // Wait for side panel to be visible.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));

  // Wait for the composebox handler to be set.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()->composebox_handler_for_testing() !=
           nullptr;
  }));

  // Also need to run until the query controller has sent all requests to avoid
  // flakiness.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          controller->lens_overlay_query_controller());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->num_full_image_requests_sent() == 1 &&
           fake_query_controller->num_page_content_update_requests_sent() == 1;
  }));

  // No visual selection context should have been added.
  auto* composebox_controller = GetLensComposeboxController();
  auto* overlay_controller = GetLensOverlayController();
  EXPECT_FALSE(
      composebox_controller->vsc_image_data_id_for_testing().has_value());
  EXPECT_FALSE(overlay_controller->HasRegionSelection());
  EXPECT_FALSE(overlay_controller->IsOverlayActive());

  // Reshow the overlay.
  controller->OpenLensOverlayInCurrentSession();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return overlay_controller->state() == State::kOverlay; }));

  // Issue a visual search request.
  overlay_controller->IssueLensRegionRequestForTesting(kTestRegion.Clone(),
                                                       /*is_click=*/false);

  // A visual selection context should have been added.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return composebox_controller->vsc_image_data_id_for_testing().has_value();
  }));
  ASSERT_TRUE(overlay_controller->HasRegionSelection());

  // ClearFiles should clear the selection.
  composebox_controller->ClearFiles();
  ASSERT_FALSE(overlay_controller->HasRegionSelection());
  ASSERT_FALSE(
      composebox_controller->vsc_image_data_id_for_testing().has_value());
}

IN_PROC_BROWSER_TEST_F(LensComposeboxControllerBrowserTest,
                       IssueComposeboxQueryHidesOverlay) {
  WaitForPaint();

  auto* lens_controller = GetLensSearchController();
  ASSERT_TRUE(lens_controller);

  // Open the overlay directly to the side panel so composebox is visible.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  lens_controller->OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);
  auto* overlay_controller = GetLensOverlayController();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsResultsSidePanelShowing(); }));

  // Wait for the composebox handler to be set.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return GetLensComposeboxController()->composebox_handler_for_testing() !=
           nullptr;
  }));

  // Mock a handshake call so the composebox controller can send query messages.
  lens::AimToClientMessage aim_to_client_message;
  aim_to_client_message.mutable_handshake_response()->add_capabilities(
      lens::FeatureCapability::DEFAULT);
  MockAimToClientMessage(aim_to_client_message);

  // Verify overlay is showing.
  ASSERT_TRUE(overlay_controller->IsOverlayShowing());

  // Send a query.
  GetLensComposeboxController()->composebox_handler_for_testing()->SubmitQuery(
      "test query", /*mouse_button=*/0, /*alt_key=*/false, /*ctrl_key=*/false,
      /*meta_key=*/false, /*shift_key=*/false);

  // Verify overlay is hidden.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return overlay_controller->state() == State::kHidden; }));
}
