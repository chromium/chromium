// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class runs functional tests for lens overlay. These tests spin up a full
// web browser, but allow for inspection and modification of internal state of
// LensOverlayController and other business-logic classes.

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

#include <memory>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/strings/string_util.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/test/with_feature_override.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/polygon.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom-forward.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/pdf/pdf_extension_test_base.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_browsertest_util.h"
#include "chrome/browser/renderer_context_menu/render_view_context_menu_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_actions.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/lens/lens_overlay_colors.h"
#include "chrome/browser/ui/lens/lens_overlay_controller_glue.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_gen204_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/lens_permission_bubble_controller.h"
#include "chrome/browser/ui/lens/lens_search_bubble_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/permissions/test/permission_request_observer.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/page_navigator.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/common/result_codes.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/hit_test_region_observer.h"
#include "content/public/test/network_connection_change_simulator.h"
#include "content/public/test/no_renderer_crashes_assertion.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "pdf/pdf_features.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_utils.h"
#include "ui/views/widget/any_widget_observer.h"
#include "url/origin.h"

namespace {

constexpr char kDocumentWithNamedElement[] = "/select.html";
constexpr char kDocumentWithNamedElementWithFragment[] =
    "/select.html#fragment";
constexpr char kDocumentWithImage[] = "/test_visual.html";
constexpr char kDocumentWithDynamicColor[] = "/lens/dynamic_color.html";
constexpr char kPdfDocument[] = "/pdf/test.pdf";
constexpr char kDocumentWithNonAsciiCharacters[] = "/non-ascii.html";

constexpr char kPdfDocument12KbFileName[] = "pdf/test-title.pdf";

using ::testing::_;
using State = LensOverlayController::State;
using LensOverlayInvocationSource = lens::LensOverlayInvocationSource;
using LensOverlayDismissalSource = lens::LensOverlayDismissalSource;

constexpr char kNewTabLinkClickScript[] =
    "(function() {const anchor = document.createElement('a');anchor.href = "
    "$1;anchor.target = "
    "'_blank';document.body.appendChild(anchor);anchor.click();})();";

constexpr char kSameTabLinkClickScript[] =
    "(function() {const anchor = document.createElement('a');anchor.href = "
    "$1;document.body.appendChild(anchor);anchor.click();})();";

constexpr char kCheckSearchboxInput[] =
    "(function() {const root = "
    "document.getElementsByTagName('lens-side-panel-app')[0].shadowRoot;"
    "const searchboxInputLoaded = "
    "  "
    "root.getElementById('searchbox').shadowRoot.getElementById('input').value "
    "  === $1; return  searchboxInputLoaded;})();";

constexpr char kRequestNotificationsScript[] = R"(
      new Promise(resolve => {
        Notification.requestPermission().then(function (permission) {
          resolve(permission);
        });
      })
      )";

constexpr char kCheckSidePanelResultsLoadedScript[] =
    "(function() {const root = "
    "document.getElementsByTagName('lens-side-panel-app')[0].shadowRoot; "
    "const iframeSrcLoaded = "
    "  root.getElementById('results').src.includes('q=' + $1);"
    "const searchboxInputLoaded = "
    "  "
    "root.getElementById('searchbox').shadowRoot.getElementById('input').value "
    "  === $1; return iframeSrcLoaded && searchboxInputLoaded;})();";

constexpr char kCheckSidePanelTranslateResultsLoadedScript[] =
    "(function() {const root = "
    "document.getElementsByTagName('lens-side-panel-app')[0].shadowRoot; "
    "const iframeSrcLoaded = "
    "  root.getElementById('results').src.includes('q=' + $1);"
    "const stickPresent = "
    "  root.getElementById('results').src.includes('stick=');"
    "const searchboxInputLoaded = "
    "  "
    "root.getElementById('searchbox').shadowRoot.getElementById('input').value "
    "  === $1; return iframeSrcLoaded && stickPresent && "
    "  searchboxInputLoaded;})();";

constexpr char kCheckSidePanelThumbnailShownScript[] =
    "(function() {const appRoot = "
    "document.getElementsByTagName('lens-side-panel-app')[0].shadowRoot;"
    "const searchboxRoot = appRoot.getElementById('searchbox').shadowRoot;"
    "const thumbContainer = searchboxRoot.getElementById('thumbnailContainer');"
    "const thumbnailRoot = "
    "searchboxRoot.getElementById('thumbnail').shadowRoot;"
    "const imageSrc = thumbnailRoot.getElementById('image').src;"
    "return window.getComputedStyle(thumbContainer).display !== 'none' && "
    "       imageSrc.startsWith('data:image/jpeg');})();";

constexpr char kHistoryStateScript[] =
    "(function() {history.replaceState({'test':1}, 'test'); "
    "history.pushState({'test':1}, 'test'); history.back();})();";

constexpr char kCloseSearchBubbleScript[] =
    "(function() {const appRoot = "
    "document.getElementsByTagName('search-bubble-app')[0].shadowRoot;"
    "const closeButton = appRoot.getElementById('closeButton');"
    "closeButton.click();})();";

// Returns true if the selection elements have bounds and are ready for input
// events.
constexpr char kOverlayReadyScript[] =
    "(function() {const regionLayerBounds "
    "=document.querySelector('lens-overlay-app').shadowRoot.querySelector('"
    "lens-selection-overlay').shadowRoot.querySelector('region-selection')."
    "getBoundingClientRect(); return regionLayerBounds.width > 0 && "
    "regionLayerBounds.height > 0;})();";

constexpr char kTestSuggestSignals[] = "encoded_image_signals";

constexpr char kStartTimeQueryParamKey[] = "qsubts";
constexpr char kViewportWidthQueryParamKey[] = "biw";
constexpr char kViewportHeightQueryParamKey[] = "bih";
constexpr char kTextQueryParamKey[] = "q";

constexpr char kResultsSearchBaseUrl[] = "https://www.google.com/search";

void ClickBubbleDialogButton(
    views::BubbleDialogDelegate* bubble_widget_delegate,
    views::View* button) {
  // Reset the timer so that the test click isn't discarded as unintended.
  bubble_widget_delegate->ResetViewShownTimeStampForTesting();
  gfx::Point center(button->width() / 2, button->height() / 2);
  const ui::MouseEvent event(ui::EventType::kMousePressed, center, center,
                             ui::EventTimeForNow(), ui::EF_LEFT_MOUSE_BUTTON,
                             ui::EF_LEFT_MOUSE_BUTTON);
  button->OnMousePressed(event);
  button->OnMouseReleased(event);
}

std::optional<int64_t> GetFileSizeForTestDataFile(std::string_view file_name) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath path = base::PathService::CheckedGet(chrome::DIR_TEST_DATA)
                            .AppendASCII(file_name);
  return base::GetFileSize(path);
}

const lens::mojom::GeometryPtr kTestGeometry = lens::mojom::Geometry::New(
    lens::mojom::CenterRotatedBox::New(
        gfx::RectF(0.1, 0.1, 0.8, 0.8),
        0.1,
        lens::mojom::CenterRotatedBox_CoordinateType::kNormalized),
    std::vector<lens::mojom::PolygonPtr>());
const lens::mojom::OverlayObjectPtr kTestOverlayObject =
    lens::mojom::OverlayObject::New("unique_id", kTestGeometry->Clone());
const lens::mojom::TextPtr kTestText =
    lens::mojom::Text::New(lens::mojom::TextLayout::New(), "es");
const lens::mojom::CenterRotatedBoxPtr kTestRegion =
    lens::mojom::CenterRotatedBox::New(
        gfx::RectF(0.1, 0.1, 0.8, 0.8),
        0.1,
        lens::mojom::CenterRotatedBox_CoordinateType::kNormalized);

class LensOverlayPageFake : public lens::mojom::LensPage {
 public:
  void ScreenshotDataReceived(const SkBitmap& screenshot) override {
    last_received_screenshot_ = screenshot;
    // Do the real call on the open WebUI we intercepted.
    overlay_page_->ScreenshotDataReceived(screenshot);
  }

  void ObjectsReceived(
      std::vector<lens::mojom::OverlayObjectPtr> objects) override {
    last_received_objects_ = std::move(objects);
  }

  void TextReceived(lens::mojom::TextPtr text) override {
    last_received_text_ = std::move(text);
  }

  void ThemeReceived(lens::mojom::OverlayThemePtr theme) override {
    last_received_theme_ = std::move(theme);
  }

  void ShouldShowContextualSearchBox(bool should_show) override {
    last_received_should_show_contextual_searchbox_ = should_show;
  }

  void NotifyResultsPanelOpened() override {
    did_notify_results_opened_ = true;
  }

  void NotifyOverlayClosing() override { did_notify_overlay_closing_ = true; }

  void SetPostRegionSelection(
      lens::mojom::CenterRotatedBoxPtr region) override {
    post_region_selection_ = std::move(region);
  }

  void SetTextSelection(int selection_start_index,
                        int selection_end_index) override {
    text_selection_indexes_ =
        std::make_pair(selection_start_index, selection_end_index);
  }

  void SetTranslateMode(const std::string& source_language,
                        const std::string& target_language) override {
    source_language_ = source_language;
    target_language_ = target_language;
  }

  void ClearRegionSelection() override { did_clear_region_selection_ = true; }

  void ClearTextSelection() override { did_clear_text_selection_ = true; }

  void ClearAllSelections() override {
    did_clear_region_selection_ = true;
    did_clear_text_selection_ = true;
  }

  void TriggerCopyText() override { did_trigger_copy = true; }

  void Reset() {
    last_received_screenshot_.reset();
    last_received_theme_->reset();
    last_received_objects_ = std::vector<lens::mojom::OverlayObjectPtr>();
    last_received_text_.reset();
    post_region_selection_.reset();
    source_language_.clear();
    target_language_.clear();
    last_received_should_show_contextual_searchbox_ = false;
    did_notify_results_opened_ = false;
    did_notify_overlay_closing_ = false;
    did_clear_region_selection_ = false;
    did_clear_text_selection_ = false;
    did_trigger_copy = false;
  }

  // The real side panel page that was opened by the lens overlay. Needed to
  // call real functions on the WebUI.
  mojo::Remote<lens::mojom::LensPage> overlay_page_;

  SkBitmap last_received_screenshot_;
  std::optional<lens::mojom::OverlayThemePtr> last_received_theme_;
  std::vector<lens::mojom::OverlayObjectPtr> last_received_objects_;
  bool last_received_should_show_contextual_searchbox_ = false;
  std::string source_language_;
  std::string target_language_;
  lens::mojom::TextPtr last_received_text_;
  bool did_notify_results_opened_ = false;
  bool did_notify_overlay_closing_ = false;
  lens::mojom::CenterRotatedBoxPtr post_region_selection_;
  std::pair<int, int> text_selection_indexes_;
  bool did_clear_region_selection_ = false;
  bool did_clear_text_selection_ = false;
  bool did_trigger_copy = false;
};

// TODO(b/334147680): Since both our interactive UI tests and our browser tests
// both mock out network calls via this method, we should factor this out so it
// can be used across files.
class LensOverlayQueryControllerFake : public lens::LensOverlayQueryController {
 public:
  explicit LensOverlayQueryControllerFake(
      lens::LensOverlayFullImageResponseCallback full_image_callback,
      lens::LensOverlayUrlResponseCallback url_callback,
      lens::LensOverlaySuggestInputsCallback suggest_inputs_callback,
      lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      Profile* profile,
      LensOverlayInvocationSource invocation_source,
      bool use_dark_mode,
      lens::LensOverlayGen204Controller* gen204_controller)
      : LensOverlayQueryController(full_image_callback,
                                   url_callback,
                                   suggest_inputs_callback,
                                   thumbnail_created_callback,
                                   variations_client,
                                   identity_manager,
                                   profile,
                                   invocation_source,
                                   use_dark_mode,
                                   gen204_controller) {}

  void StartQueryFlow(
      const SkBitmap& screenshot,
      std::optional<GURL> page_url,
      std::optional<std::string> page_title,
      std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes,
      base::span<const uint8_t> underlying_content_bytes,
      lens::PageContentMimeType underlying_content_type,
      float ui_scale_factor) override {
    // Send response for full image callback / HandleStartQueryResponse.
    std::vector<lens::mojom::OverlayObjectPtr> test_objects;
    test_objects.push_back(kTestOverlayObject->Clone());
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(full_image_callback_, std::move(test_objects),
                       kTestText->Clone(),
                       /*is_error=*/full_image_request_should_return_error_));

    // Send response for interaction data callback /
    // HandleInteractionDataResponse.
    lens::proto::LensOverlaySuggestInputs suggest_inputs;
    suggest_inputs.set_encoded_image_signals(kTestSuggestSignals);
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(suggest_inputs_callback_, suggest_inputs));

    last_sent_underlying_content_bytes_ = underlying_content_bytes;
    last_sent_underlying_content_type_ = underlying_content_type;
  }

  void SendTaskCompletionGen204IfEnabled(
      lens::mojom::UserAction user_action) override {
    last_user_action_ = user_action;
  }

  void SendRegionSearch(
      lens::mojom::CenterRotatedBoxPtr region,
      lens::LensOverlaySelectionType selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bytes) override {
    Reset();
    last_queried_region_ = region->Clone();
    last_queried_region_bytes_ = region_bytes;
    last_lens_selection_type_ = selection_type;
    LensOverlayQueryController::SendRegionSearch(
        std::move(region), selection_type, additional_search_query_params,
        region_bytes);
  }

  void SendMultimodalRequest(
      lens::mojom::CenterRotatedBoxPtr region,
      const std::string& query_text,
      lens::LensOverlaySelectionType multimodal_selection_type,
      std::map<std::string, std::string> additional_search_query_params,
      std::optional<SkBitmap> region_bitmap) override {
    Reset();
    last_queried_region_ = region.Clone();
    last_queried_region_bytes_ = region_bitmap;
    last_queried_text_ = query_text;
    last_lens_selection_type_ = multimodal_selection_type;
  }

  void SendFullPageTranslateQuery(const std::string& source_language,
                                  const std::string& target_language) override {
    Reset();
    std::vector<lens::mojom::OverlayObjectPtr> test_objects;
    test_objects.push_back(kTestOverlayObject->Clone());
    lens::mojom::TextPtr text = kTestText->Clone();
    text->content_language = target_language;
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(full_image_callback_, std::move(test_objects),
                                  std::move(text),
                                  /*is_error=*/false));
  }

  void SendEndTranslateModeQuery() override {
    Reset();
    std::vector<lens::mojom::OverlayObjectPtr> test_objects;
    test_objects.push_back(kTestOverlayObject->Clone());
    lens::mojom::TextPtr text = kTestText->Clone();
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE, base::BindOnce(full_image_callback_, std::move(test_objects),
                                  std::move(text),
                                  /*is_error=*/false));
  }

  void SetShouldReturnError(bool full_image_request_should_return_error) {
    full_image_request_should_return_error_ =
        full_image_request_should_return_error;
  }

  void Reset() {
    last_lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;
    last_queried_region_.reset();
    last_queried_text_.clear();
    last_queried_region_bytes_ = std::nullopt;
    last_sent_underlying_content_bytes_ = base::span<const uint8_t>();
    last_sent_underlying_content_type_ = lens::PageContentMimeType::kNone;
  }

  bool full_image_request_should_return_error_ = false;
  std::string last_queried_text_;
  lens::LensOverlaySelectionType last_lens_selection_type_;
  lens::mojom::CenterRotatedBoxPtr last_queried_region_;
  std::optional<SkBitmap> last_queried_region_bytes_;
  base::span<const uint8_t> last_sent_underlying_content_bytes_;
  lens::PageContentMimeType last_sent_underlying_content_type_;
  std::optional<lens::mojom::UserAction> last_user_action_;
};

// Stubs out network requests and mojo calls.
class LensOverlayControllerFake : public LensOverlayController {
 public:
  LensOverlayControllerFake(tabs::TabInterface* tab,
                            variations::VariationsClient* variations_client,
                            signin::IdentityManager* identity_manager,
                            PrefService* pref_service,
                            syncer::SyncService* sync_service,
                            ThemeService* theme_service,
                            Profile* profile)
      : LensOverlayController(tab,
                              variations_client,
                              identity_manager,
                              pref_service,
                              sync_service,
                              theme_service) {}

  std::unique_ptr<lens::LensOverlayQueryController> CreateLensQueryController(
      lens::LensOverlayFullImageResponseCallback full_image_callback,
      lens::LensOverlayUrlResponseCallback url_callback,
      lens::LensOverlaySuggestInputsCallback suggest_inputs_callback,
      lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      Profile* profile,
      lens::LensOverlayInvocationSource invocation_source,
      bool use_dark_mode,
      lens::LensOverlayGen204Controller* gen204_controller) override {
    auto fake_query_controller =
        std::make_unique<LensOverlayQueryControllerFake>(
            full_image_callback, url_callback, suggest_inputs_callback,
            thumbnail_created_callback, variations_client, identity_manager,
            profile, invocation_source, use_dark_mode, gen204_controller);
    fake_query_controller->SetShouldReturnError(
        full_image_request_should_return_error_);
    return fake_query_controller;
  }

  void BindOverlay(mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
                   mojo::PendingRemote<lens::mojom::LensPage> page) override {
    fake_overlay_page_.overlay_page_.Bind(std::move(page));
    // Set up the fake overlay page to intercept the mojo call.
    LensOverlayController::BindOverlay(
        std::move(receiver),
        fake_overlay_page_receiver_.BindNewPipeAndPassRemote());
  }

  void SetSidePanelIsLoadingResults(bool is_loading) override {
    if (is_loading) {
      is_side_panel_loading_set_to_true_++;
      return;
    }

    is_side_panel_loading_set_to_false_++;
  }

  void SetSidePanelShowErrorPage(bool should_show_error_page) override {
    if (should_show_error_page) {
      side_panel_set_show_error_page_++;
      return;
    }

    side_panel_set_hide_error_page_++;
  }

  // Helper function to force the fake query controller to return errors in its
  // responses to full image requests. This should be called before ShowUI.
  void SetFullImageRequestShouldReturnError() {
    full_image_request_should_return_error_ = true;
  }

  void ResetSidePanelTracking() {
    side_panel_set_show_error_page_ = 0;
    side_panel_set_hide_error_page_ = 0;
    is_side_panel_loading_set_to_true_ = 0;
    is_side_panel_loading_set_to_false_ = 0;
  }

  void FlushForTesting() { fake_overlay_page_receiver_.FlushForTesting(); }

  int side_panel_set_show_error_page_ = 0;
  int side_panel_set_hide_error_page_ = 0;
  int is_side_panel_loading_set_to_true_ = 0;
  int is_side_panel_loading_set_to_false_ = 0;
  LensOverlayPageFake fake_overlay_page_;
  bool full_image_request_should_return_error_ = false;
  mojo::Receiver<lens::mojom::LensPage> fake_overlay_page_receiver_{
      &fake_overlay_page_};
};

class TabFeaturesFake : public tabs::TabFeatures {
 public:
  TabFeaturesFake() = default;

 protected:
  std::unique_ptr<LensOverlayController> CreateLensController(
      tabs::TabInterface* tab,
      Profile* profile) override {
    auto* theme_service = ThemeServiceFactory::GetForProfile(profile);
    // Set browser color scheme to light mode for consistency.
    theme_service->SetBrowserColorScheme(
        ThemeService::BrowserColorScheme::kLight);
    return std::make_unique<LensOverlayControllerFake>(
        tab, profile->GetVariationsClient(),
        IdentityManagerFactory::GetForProfile(profile), profile->GetPrefs(),
        SyncServiceFactory::GetForProfile(profile), theme_service, profile);
  }
};

std::unique_ptr<tabs::TabFeatures> CreateTabFeatures() {
  return std::make_unique<TabFeaturesFake>();
}

class LensOverlayControllerBrowserTest : public InProcessBrowserTest {
 protected:
  LensOverlayControllerBrowserTest() {
    tabs::TabFeatures::ReplaceTabFeaturesForTesting(
        base::BindRepeating(&CreateTabFeatures));
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    SetupFeatureList();
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    embedded_test_server()->StartAcceptingConnections();

    // Permits sharing the page screenshot by default.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, true);
    prefs->SetBoolean(lens::prefs::kLensSharingPageContentEnabled, true);

    mock_hats_service_ = static_cast<MockHatsService*>(
        HatsServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            browser()->profile(), base::BindRepeating(&BuildMockHatsService)));
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());
    InProcessBrowserTest::TearDownOnMainThread();

    // Disallow sharing the page screenshot by default.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, false);
    prefs->SetBoolean(lens::prefs::kLensSharingPageContentEnabled, false);

    mock_hats_service_ = nullptr;
  }

  ~LensOverlayControllerBrowserTest() override {
    tabs::TabFeatures::ReplaceTabFeaturesForTesting(base::NullCallback());
  }

  virtual void SetupFeatureList() {
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlay,
          {{"search-bubble", "true"},
           {"results-search-url", kResultsSearchBaseUrl},
           {"use-dynamic-theme", "true"},
           {"use-dynamic-theme-min-population-pct", "0.002"},
           {"use-dynamic-theme-min-chroma", "3.0"}}},
         {lens::features::kLensOverlayContextualSearchbox,
          {
              {"use-inner-text-as-context", "true"},
              {"use-inner-html-as-context", "true"},
          }},
         {lens::features::kLensOverlaySurvey, {}}},
        /*disabled_features=*/{});
  }

  const SkBitmap CreateNonEmptyBitmap(int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseColor(SK_ColorGREEN);
    return bitmap;
  }

  LensOverlayController* GetLensOverlayController() {
    return browser()
        ->tab_strip_model()
        ->GetActiveTab()
        ->tab_features()
        ->lens_overlay_controller();
  }

  content::WebContents* GetOverlayWebContents() {
    auto* controller = GetLensOverlayController();
    return controller->GetOverlayWebViewForTesting()->GetWebContents();
  }

  // Waits for the WebUI to have the screenshot loaded and is ready for input
  // events.
  void WaitForOverlayReady(content::WebContents* overlay_web_contents) {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return content::EvalJs(overlay_web_contents->GetPrimaryMainFrame(),
                             kOverlayReadyScript,
                             content::EXECUTE_SCRIPT_NO_USER_GESTURE)
          .ExtractBool();
    }));
  }

  void SimulateLeftClickDrag(gfx::Point from, gfx::Point to) {
    auto* overlay_web_contents = GetOverlayWebContents();

    // We need to wait until the selection overlay has resized and is ready to
    // receive input events.
    WaitForOverlayReady(overlay_web_contents);

    // We should wait for the main frame's hit-test data to be ready before
    // sending the click event below to avoid flakiness.
    content::WaitForHitTestData(overlay_web_contents->GetPrimaryMainFrame());
    content::SimulateMouseEvent(overlay_web_contents,
                                blink::WebInputEvent::Type::kMouseDown,
                                blink::WebMouseEvent::Button::kLeft, from);
    content::SimulateMouseEvent(overlay_web_contents,
                                blink::WebInputEvent::Type::kMouseMove,
                                blink::WebMouseEvent::Button::kLeft, to);
    content::SimulateMouseEvent(overlay_web_contents,
                                blink::WebInputEvent::Type::kMouseUp,
                                blink::WebMouseEvent::Button::kLeft, to);
    content::RunUntilInputProcessed(
        overlay_web_contents->GetRenderWidgetHostView()->GetRenderWidgetHost());
  }

  // Unable to use `content::SimulateKeyPress()` helper function since it sets
  // `event.skip_if_unhandled` to true which stops the propagation of the event
  // to the delegate web view.
  void SimulateCtrlCKeyPress(content::WebContents* web_content) {
    // Create the escape key press event.
    input::NativeWebKeyboardEvent event(blink::WebKeyboardEvent::Type::kChar,
                                        blink::WebInputEvent::kControlKey,
                                        base::TimeTicks::Now());
    event.windows_key_code = ui::VKEY_C;
    event.dom_key = ui::DomKey::FromCharacter('C');
    event.dom_code = static_cast<int>(ui::DomCode::US_C);

    // Send the event to the Web Contents.
    web_content->GetPrimaryMainFrame()
        ->GetRenderViewHost()
        ->GetWidget()
        ->ForwardKeyboardEvent(event);
  }

  // Lens overlay takes a screenshot of the tab. In order to take a screenshot
  // the tab must not be about:blank and must be painted. By default opens in
  // the current tab.
  void WaitForPaint(
      std::string_view relative_url = kDocumentWithNamedElement,
      WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB,
      int browser_test_flags = ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP) {
    const GURL url = embedded_test_server()->GetURL(relative_url);
    ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, disposition, browser_test_flags));
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return browser()
          ->tab_strip_model()
          ->GetActiveTab()
          ->contents()
          ->CompletedFirstVisuallyNonEmptyPaint();
    }));
  }

  // Helper to remove the start time and viewport size query params from the
  // url.
  GURL RemoveStartTimeAndSizeParams(const GURL& url_to_process) {
    GURL processed_url = url_to_process;
    std::string actual_start_time;
    bool has_start_time = net::GetValueForKeyInQuery(
        GURL(url_to_process), kStartTimeQueryParamKey, &actual_start_time);
    EXPECT_TRUE(has_start_time);
    processed_url = net::AppendOrReplaceQueryParameter(
        processed_url, kStartTimeQueryParamKey, std::nullopt);
    std::string actual_viewport_width;
    bool has_viewport_width = net::GetValueForKeyInQuery(
        GURL(url_to_process), kViewportWidthQueryParamKey,
        &actual_viewport_width);
    std::string actual_viewport_height;
    bool has_viewport_height = net::GetValueForKeyInQuery(
        GURL(url_to_process), kViewportHeightQueryParamKey,
        &actual_viewport_height);
    EXPECT_TRUE(has_viewport_width);
    EXPECT_TRUE(has_viewport_height);
    EXPECT_NE(actual_viewport_width, "0");
    EXPECT_NE(actual_viewport_height, "0");
    processed_url = net::AppendOrReplaceQueryParameter(
        processed_url, kViewportWidthQueryParamKey, std::nullopt);
    processed_url = net::AppendOrReplaceQueryParameter(
        processed_url, kViewportHeightQueryParamKey, std::nullopt);
    return processed_url;
  }

  void VerifyTextQueriesAreEqual(const GURL& url, const GURL& url_to_compare) {
    std::string text_query;
    bool has_text_query =
        net::GetValueForKeyInQuery(GURL(url), kTextQueryParamKey, &text_query);
    EXPECT_TRUE(has_text_query);

    std::string query_to_compare;
    bool has_query_to_compare = net::GetValueForKeyInQuery(
        GURL(url_to_compare), kTextQueryParamKey, &query_to_compare);
    EXPECT_TRUE(has_query_to_compare);

    EXPECT_EQ(query_to_compare, text_query);
  }

  void VerifySearchQueryParameters(const GURL& url_to_process) {
    EXPECT_THAT(url_to_process.spec(),
                testing::MatchesRegex(std::string(kResultsSearchBaseUrl) +
                                      ".*q=.*&gsc=2&hl=.*&biw=\\d+&bih=\\d+"));
  }

  void CloseOverlayAndWaitForOff(LensOverlayController* controller,
                                 LensOverlayDismissalSource dismissal_source) {
    controller->CloseUIAsync(dismissal_source);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return controller->state() == State::kOff; }));
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<MockHatsService> mock_hats_service_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       PermissionBubble_CSBPrefAccept) {
  WaitForPaint();
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Allow sharing the page screenshot but not other page content.
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, true);
  prefs->SetBoolean(lens::prefs::kLensSharingPageContentEnabled, false);
  ASSERT_TRUE(lens::CanSharePageScreenshotWithLensOverlay(prefs));
  ASSERT_FALSE(lens::CanSharePageContentWithLensOverlay(prefs));

  // Verify attempting to show the UI will still show the permission bubble
  // with the contextual searchbox enabled.
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       lens::kLensPermissionDialogName);
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  auto* bubble_widget = waiter.WaitIfNeededAndGet();
  // Wait for the bubble to become visible.
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();
  ASSERT_TRUE(bubble_widget->IsVisible());
  ASSERT_TRUE(controller->get_lens_permission_bubble_controller_for_testing()
                  ->HasOpenDialogWidget());

  // Verify attempting to show the UI again does not close the bubble widget.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  ASSERT_TRUE(bubble_widget->IsVisible());
  ASSERT_TRUE(controller->get_lens_permission_bubble_controller_for_testing()
                  ->HasOpenDialogWidget());

  // Simulate click on the accept button.
  auto* bubble_widget_delegate =
      bubble_widget->widget_delegate()->AsBubbleDialogDelegate();
  ClickBubbleDialogButton(bubble_widget_delegate,
                          bubble_widget_delegate->GetOkButton());
  // Wait for the bubble to be destroyed.
  views::test::WidgetDestroyedWaiter(bubble_widget).Wait();
  ASSERT_FALSE(controller->get_lens_permission_bubble_controller_for_testing()
                   ->HasOpenDialogWidget());

  // Verify sharing the page content and screenshot are now permitted.
  ASSERT_TRUE(lens::CanSharePageContentWithLensOverlay(prefs));
  ASSERT_TRUE(lens::CanSharePageScreenshotWithLensOverlay(prefs));

  // Verify accepting the permission bubble will eventually result in the
  // overlay state.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify screenshot was captured and stored.
  auto screenshot_bitmap = controller->current_screenshot();
  EXPECT_FALSE(screenshot_bitmap.empty());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       PermissionBubble_CSBPrefReject) {
  WaitForPaint();
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Allow sharing the page screenshot but not other page content.
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, true);
  prefs->SetBoolean(lens::prefs::kLensSharingPageContentEnabled, false);
  ASSERT_TRUE(lens::CanSharePageScreenshotWithLensOverlay(prefs));
  ASSERT_FALSE(lens::CanSharePageContentWithLensOverlay(prefs));

  // Verify attempting to show the UI will show the permission bubble.
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       lens::kLensPermissionDialogName);
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  auto* bubble_widget = waiter.WaitIfNeededAndGet();
  // Wait for the bubble to become visible.
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();
  ASSERT_TRUE(bubble_widget->IsVisible());
  ASSERT_TRUE(controller->get_lens_permission_bubble_controller_for_testing()
                  ->HasOpenDialogWidget());

  // Verify attempting to show the UI again does not close the bubble widget.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  ASSERT_TRUE(bubble_widget->IsVisible());
  ASSERT_TRUE(controller->get_lens_permission_bubble_controller_for_testing()
                  ->HasOpenDialogWidget());

  // Simulate click on the reject button.
  auto* bubble_widget_delegate =
      bubble_widget->widget_delegate()->AsBubbleDialogDelegate();
  ClickBubbleDialogButton(bubble_widget_delegate,
                          bubble_widget_delegate->GetCancelButton());
  // Wait for the bubble to be destroyed.
  views::test::WidgetDestroyedWaiter(bubble_widget).Wait();
  ASSERT_FALSE(controller->get_lens_permission_bubble_controller_for_testing()
                   ->HasOpenDialogWidget());

  // Verify sharing the page screenshot is still permitted.
  ASSERT_TRUE(lens::CanSharePageScreenshotWithLensOverlay(prefs));
  // Verify sharing the page content is still not permitted.
  ASSERT_FALSE(lens::CanSharePageContentWithLensOverlay(prefs));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       DoesNotOpenOnCrashedWebContents) {
  WaitForPaint();
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Force the live page renderer to terminate.
  content::WebContents* tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::RenderProcessHost* process =
      tab_contents->GetPrimaryMainFrame()->GetProcess();
  content::ScopedAllowRendererCrashes allow_renderer_crashes(process);
  process->Shutdown(content::RESULT_CODE_KILLED);

  EXPECT_TRUE(
      base::test::RunUntil([&]() { return tab_contents->IsCrashed(); }));

  // Showing UI should be a no-op and remain in state off.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kOff);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, CaptureScreenshot) {
  WaitForPaint();
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify screenshot was captured and stored.
  auto screenshot_bitmap = controller->current_screenshot();
  EXPECT_FALSE(screenshot_bitmap.empty());

  // Verify screenshot was encoded and passed to WebUI.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_FALSE(
      fake_controller->fake_overlay_page_.last_received_screenshot_.empty());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, CreateAndLoadWebUI) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Assert that the web view was created and loaded WebUI.
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  ASSERT_EQ(GetOverlayWebContents()->GetLastCommittedURL(),
            GURL(chrome::kChromeUILensOverlayUntrustedURL));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, ShowSidePanel) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Before showing the results panel, there should be no notification sent to
  // WebUI.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_FALSE(fake_controller->fake_overlay_page_.did_notify_results_opened_);

  // Now show the side panel.
  controller->results_side_panel_coordinator()->RegisterEntryAndShow();

  // Prevent flakiness by flushing the tasks.
  fake_controller->FlushForTesting();

  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
  EXPECT_TRUE(fake_controller->fake_overlay_page_.did_notify_results_opened_);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       ShowSidePanelWithPendingRegion) {
  EXPECT_CALL(*mock_hats_service_, LaunchDelayedSurveyForWebContents(
                                       kHatsSurveyTriggerLensOverlayResults, _,
                                       _, _, _, _, _, _, _, _));
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  controller->ShowUIWithPendingRegion(LensOverlayInvocationSource::kAppMenu,
                                      kTestRegion->Clone(), initial_bitmap);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
  // Expect the Lens Overlay results panel to open.
  ASSERT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);

  // Verify region was passed to WebUI.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_EQ(kTestRegion,
            fake_controller->fake_overlay_page_.post_region_selection_);

  auto* fake_query_controller = static_cast<LensOverlayQueryControllerFake*>(
      controller->get_lens_overlay_query_controller_for_testing());
  EXPECT_TRUE(fake_query_controller->last_queried_region_bytes_);
  EXPECT_TRUE(
      memcmp(fake_query_controller->last_queried_region_bytes_->getPixels(),
             initial_bitmap.getPixels(),
             initial_bitmap.computeByteSize()) == 0);
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes_->width(), 100);
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes_->height(), 100);
  EXPECT_EQ(fake_query_controller->last_lens_selection_type_,
            lens::INJECTED_IMAGE);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, CloseSidePanel) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);
  // Tab contents web view should be enabled.
  ASSERT_TRUE(browser()->GetWebView()->GetEnabled());

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  // Tab contents web view should be disabled.
  ASSERT_FALSE(browser()->GetWebView()->GetEnabled());

  // Grab fake controller to test if notify the overlay of being closed.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_FALSE(fake_controller->fake_overlay_page_.did_notify_overlay_closing_);

  // Now show the side panel.
  controller->results_side_panel_coordinator()->RegisterEntryAndShow();

  // Ensure the side panel is showing.
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  // Tab contents web view should be disabled.
  ASSERT_FALSE(browser()->GetWebView()->GetEnabled());

  // Close the side panel.
  coordinator->Close();

  // Ensure the overlay closes too.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
  // Tab contents web view should be enabled.
  ASSERT_TRUE(browser()->GetWebView()->GetEnabled());
}

// TODO(crbug.com/341383805): Enable once flakiness is fixed on all platforms.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       DISABLED_DelayPermissionsPrompt) {
  // Navigate to a page so we can request permissions
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  permissions::PermissionRequestObserver observer(contents);

  // Request permission in tab under overlay.
  EXPECT_TRUE(content::ExecJs(
      contents->GetPrimaryMainFrame(), kRequestNotificationsScript,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Verify no prompt was shown
  observer.Wait();
  EXPECT_FALSE(observer.request_shown());

  // Close overlay
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  // Verify a prompt was shown
  ASSERT_TRUE(base::test::RunUntil([&]() { return observer.request_shown(); }));
}

// TODO(b/335801964): Test flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SidePanelInteractionsAfterRegionSelection \
  DISABLED_SidePanelInteractionsAfterRegionSelection
#else
#define MAYBE_SidePanelInteractionsAfterRegionSelection \
  SidePanelInteractionsAfterRegionSelection
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       MAYBE_SidePanelInteractionsAfterRegionSelection) {
  EXPECT_CALL(*mock_hats_service_, LaunchDelayedSurveyForWebContents(
                                       kHatsSurveyTriggerLensOverlayResults, _,
                                       _, _, _, _, _, _, _, _));
  WaitForPaint();

  std::string text_query = "Apples";
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  EXPECT_TRUE(controller->GetThumbnailForTesting().empty());
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::SEARCH_SIDE_PANEL_SEARCHBOX);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  // We need to flush the mojo receiver calls to make sure the screenshot was
  // passed back to the WebUI or else the region selection UI will not render.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->FlushForTesting();
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Simulate mouse events on the overlay for drawing a manual region.
  gfx::Point center =
      GetOverlayWebContents()->GetContainerBounds().CenterPoint();
  gfx::Point off_center = gfx::Point(center);
  off_center.Offset(100, 100);
  SimulateLeftClickDrag(center, off_center);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
  // Expect the Lens Overlay results panel to open.
  ASSERT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);

  // Verify that the side panel searchbox displays a thumbnail and that the
  // controller has a copy.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return true == content::EvalJs(
                       controller->GetSidePanelWebContentsForTesting(),
                       content::JsReplace(kCheckSidePanelThumbnailShownScript));
  }));
  EXPECT_FALSE(controller->get_selected_text_for_region().has_value());
  EXPECT_FALSE(controller->get_selected_region_for_testing().is_null());
  EXPECT_TRUE(base::StartsWith(controller->GetThumbnailForTesting(), "data:"));
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX);

  // Verify that after text selection, the controller has a copy of the text,
  // the thumbnail is no longer shown and the controller's copy of the
  // thumbnail is empty.
  controller->IssueTextSelectionRequestForTesting(text_query,
                                                  /*selection_start_index=*/10,
                                                  /*selection_end_index=*/16);
  EXPECT_TRUE(content::EvalJs(
                  controller->GetSidePanelWebContentsForTesting()
                      ->GetPrimaryMainFrame(),
                  content::JsReplace(kCheckSearchboxInput, text_query),
                  content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES)
                  .ExtractBool());
  EXPECT_TRUE(controller->get_selected_text_for_region().has_value());
  EXPECT_TRUE(controller->get_selected_region_for_testing().is_null());
  EXPECT_TRUE(controller->GetThumbnailForTesting().empty());
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::SEARCH_SIDE_PANEL_SEARCHBOX);

  // Verify that after a signal from the searchbox that the text was modified,
  // no text selection is present.
  EXPECT_FALSE(fake_controller->fake_overlay_page_.did_clear_text_selection_);
  controller->OnTextModifiedForTesting();
  EXPECT_FALSE(controller->get_selected_text_for_region().has_value());
  fake_controller->FlushForTesting();
  EXPECT_TRUE(fake_controller->fake_overlay_page_.did_clear_text_selection_);
}

// TODO(b/350991033): Test flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ShowSidePanelAfterTextSelectionRequest \
  DISABLED_ShowSidePanelAfterTextSelectionRequest
#else
#define MAYBE_ShowSidePanelAfterTextSelectionRequest \
  ShowSidePanelAfterTextSelectionRequest
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       MAYBE_ShowSidePanelAfterTextSelectionRequest) {
  EXPECT_CALL(*mock_hats_service_, LaunchDelayedSurveyForWebContents(
                                       kHatsSurveyTriggerLensOverlayResults, _,
                                       _, _, _, _, _, _, _, _));
  WaitForPaint();

  std::string text_query = "Apples";
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  controller->IssueTextSelectionRequestForTesting(text_query,
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));

  // Expect the Lens Overlay results panel to open.
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
  EXPECT_TRUE(coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntry::Id::kLensOverlayResults)));

  // Verify that the side panel displays our query.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return true ==
           content::EvalJs(controller->GetSidePanelWebContentsForTesting(),
                           content::JsReplace(
                               kCheckSidePanelResultsLoadedScript, text_query));
  }));
}

// TODO(b/335028577): Test flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ShowSidePanelAfterTranslateSelectionRequest \
  DISABLED_ShowSidePanelAfterTranslateSelectionRequest
#else
#define MAYBE_ShowSidePanelAfterTranslateSelectionRequest \
  ShowSidePanelAfterTranslateSelectionRequest
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       MAYBE_ShowSidePanelAfterTranslateSelectionRequest) {
  EXPECT_CALL(*mock_hats_service_, LaunchDelayedSurveyForWebContents(
                                       kHatsSurveyTriggerLensOverlayResults, _,
                                       _, _, _, _, _, _, _, _));
  WaitForPaint();

  std::string text_query = "Manzanas";
  std::string content_language = "es";
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  controller->IssueTranslateSelectionRequestForTesting(
      text_query, content_language,
      /*selection_start_index=*/0,
      /*selection_end_index=*/0);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));

  // Expect the Lens Overlay results panel to open.
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
  EXPECT_TRUE(coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntry::Id::kLensOverlayResults)));

  // Verify that the side panel displays our query.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return true ==
           content::EvalJs(
               controller->GetSidePanelWebContentsForTesting(),
               content::JsReplace(kCheckSidePanelTranslateResultsLoadedScript,
                                  text_query));
  }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       IssueTranslateFullPageRequest) {
  EXPECT_CALL(*mock_hats_service_, LaunchDelayedSurveyForWebContents(
                                       kHatsSurveyTriggerLensOverlayResults, _,
                                       _, _, _, _, _, _, _, _));
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Before sending the requests, we need to reset the fake controller..
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->FlushForTesting();
  fake_controller->fake_overlay_page_.Reset();
  EXPECT_TRUE(
      fake_controller->fake_overlay_page_.last_received_objects_.empty());
  EXPECT_FALSE(fake_controller->fake_overlay_page_.last_received_text_);

  std::string source = "auto";
  std::string target = "fr";
  controller->IssueTranslateFullPageRequestForTesting(source, target);

  // Prevent flakiness by flushing the tasks.
  fake_controller->FlushForTesting();

  // Expect the Lens Overlay results panel to remain closed.
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
  EXPECT_FALSE(coordinator->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntry::Id::kLensOverlayResults)));

  // After flushing the mojo calls, the data should be present.
  EXPECT_FALSE(
      fake_controller->fake_overlay_page_.last_received_objects_.empty());

  auto* translate_object =
      fake_controller->fake_overlay_page_.last_received_objects_[0].get();
  auto* translated_text =
      fake_controller->fake_overlay_page_.last_received_text_.get();
  EXPECT_TRUE(translate_object);
  EXPECT_TRUE(translated_text);
  EXPECT_TRUE(kTestOverlayObject->Equals(*translate_object));
  EXPECT_EQ(target, translated_text->content_language);

  // Now disable translate mode.
  controller->IssueEndTranslateModeRequestForTesting();

  // Prevent flakiness by flushing the tasks.
  fake_controller->FlushForTesting();

  auto* non_translate_object =
      fake_controller->fake_overlay_page_.last_received_objects_[0].get();
  auto* non_translated_text =
      fake_controller->fake_overlay_page_.last_received_text_.get();
  EXPECT_TRUE(non_translate_object);
  EXPECT_TRUE(non_translated_text);
  EXPECT_TRUE(kTestOverlayObject->Equals(*non_translate_object));
  EXPECT_EQ(kTestText->content_language, non_translated_text->content_language);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       IssueTranslateFullPageRequestWithSelectedRegion) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Before sending the requests, we need to reset the fake controller..
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->FlushForTesting();
  fake_controller->fake_overlay_page_.Reset();
  EXPECT_TRUE(
      fake_controller->fake_overlay_page_.last_received_objects_.empty());
  EXPECT_FALSE(fake_controller->fake_overlay_page_.last_received_text_);

  // Issuing a region selection request should update the results page.
  const GURL first_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.menu&vsint=KgwKAggHEgIIAxgBIAI&q=&lns_fp=1"
      "&lns_mode=un&gsc=2&hl=en-US&cs=0");
  controller->IssueLensRegionRequestForTesting(kTestRegion->Clone(),
                                               /*is_click=*/false);
  // The full sequence of events necessary to load Lens search results is not
  // currently testable, so load the expected URL manually.
  controller->LoadURLInResultsFrame(first_search_url);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));
  EXPECT_EQ(controller->get_selected_region_for_testing(), kTestRegion);

  std::string source = "auto";
  std::string target = "fr";
  controller->IssueTranslateFullPageRequestForTesting(source, target);

  // Prevent flakiness by flushing the tasks.
  fake_controller->FlushForTesting();

  // The selected region should be null now, as a result of turning on
  // translate mode.
  EXPECT_TRUE(controller->get_selected_region_for_testing().is_null());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       HandleStartQueryResponse) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Before showing the UI, there should be no set objects or text as
  // no query flow has started.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_TRUE(
      fake_controller->fake_overlay_page_.last_received_objects_.empty());
  EXPECT_FALSE(fake_controller->fake_overlay_page_.last_received_text_);

  // Showing UI should change the state to screenshot and eventually to overlay.
  // When the overlay is bound, it should start the query flow which returns a
  // response for the full image callback.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Prevent flakiness by flushing the tasks.
  fake_controller->FlushForTesting();

  // After flushing the mojo calls, the data should be present.
  EXPECT_FALSE(
      fake_controller->fake_overlay_page_.last_received_objects_.empty());

  auto* object =
      fake_controller->fake_overlay_page_.last_received_objects_[0].get();
  auto* text = fake_controller->fake_overlay_page_.last_received_text_.get();
  EXPECT_TRUE(object);
  EXPECT_TRUE(text);
  EXPECT_TRUE(kTestOverlayObject->Equals(*object));
  EXPECT_EQ(kTestText->content_language, text->content_language);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       HandleStartQueryResponseError) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Set the full image request to return an error.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->SetFullImageRequestShouldReturnError();

  // Showing UI should change the state to screenshot and eventually to overlay.
  // When the overlay is bound, it should start the query flow which returns a
  // response for the full image callback.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Verify the error page was set correctly.
  EXPECT_EQ(fake_controller->side_panel_set_hide_error_page_, 0);
  EXPECT_EQ(fake_controller->side_panel_set_show_error_page_, 1);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       HandleInteractionDataResponse) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Before showing the UI, there should be no suggest signals as no query flow
  // has started.
  EXPECT_FALSE(
      controller->GetLensSuggestInputsForTesting().has_encoded_image_signals());

  // Showing UI should change the state to screenshot and eventually to overlay.
  // When the overlay is bound, it should start the query flow which returns a
  // response for the full image callback.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // The lens response should have been correctly set for use by the searchbox.
  EXPECT_TRUE(
      controller->GetLensSuggestInputsForTesting().has_encoded_image_signals());
  EXPECT_EQ(
      controller->GetLensSuggestInputsForTesting().encoded_image_signals(),
      kTestSuggestSignals);

  // The tab ID should have been correctly set for use by the searchbox.
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  SessionID tab_id = sessions::SessionTabHelper::IdForTab(tab_web_contents);
  EXPECT_EQ(controller->GetTabIdForTesting(), tab_id);

  EXPECT_TRUE(!controller->GetPageURLForTesting().is_empty());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       BackgroundAndForegroundUI) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);
  // Tab contents web view should be enabled.
  ASSERT_TRUE(browser()->GetWebView()->GetEnabled());

  // Grab the index of the currently active tab so we can return to it later.
  int active_controller_tab_index =
      browser()->tab_strip_model()->active_index();

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());
  // Tab contents web view should be disabled.
  ASSERT_FALSE(browser()->GetWebView()->GetEnabled());

  // Open a side panel to test that the side panel persists between tab
  // switches.
  controller->results_side_panel_coordinator()->RegisterEntryAndShow();
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);

  // Opening a new tab should background the overlay UI.
  WaitForPaint(kDocumentWithNamedElement,
               WindowOpenDisposition::NEW_FOREGROUND_TAB,
               ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
                   ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kBackground; }));
  EXPECT_FALSE(controller->GetOverlayViewForTesting()->GetVisible());
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return !coordinator->IsSidePanelShowing(); }));
  // Tab contents web view should be enabled.
  ASSERT_TRUE(browser()->GetWebView()->GetEnabled());

  // Returning back to the previous tab should show the overlay UI again.
  browser()->tab_strip_model()->ActivateTabAt(active_controller_tab_index);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());
  // Side panel should come back when returning to previous tab.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return coordinator->IsSidePanelShowing(); }));
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
  // Tab contents web view should be disabled.
  ASSERT_FALSE(browser()->GetWebView()->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       LoadURLInResultsFrame) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Side panel is not showing at first.
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
  EXPECT_FALSE(coordinator->IsSidePanelShowing());

  // Loading a url in the side panel should show the results page.
  const GURL search_url("https://www.google.com/search");
  controller->LoadURLInResultsFrame(search_url);

  // Expect the Lens Overlay results panel to open.
  ASSERT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OfflineErrorPageInSidePanel) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Set the network connection type to being offline.
  auto scoped_mock_network_change_notifier =
      std::make_unique<net::test::ScopedMockNetworkChangeNotifier>();
  scoped_mock_network_change_notifier->mock_network_change_notifier()
      ->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Side panel is not showing at first.
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
  EXPECT_FALSE(coordinator->IsSidePanelShowing());
  EXPECT_FALSE(controller->GetSidePanelWebContentsForTesting());

  // Reset any side panel tracking values that may have been set by initial
  // open.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->ResetSidePanelTracking();

  // Loading a url in the side panel should show the side panel even if we
  // expect the navigation to fail.
  const GURL search_url("https://www.google.com/search");
  controller->LoadURLInResultsFrame(search_url);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Expect the Lens Overlay results panel to open.
  ASSERT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);

  // Verify the error page was set correctly. The error page is set to show
  // twice because the URL is modified and attempted to reload due to not having
  // the proper query parameters.
  EXPECT_EQ(fake_controller->side_panel_set_hide_error_page_, 0);
  EXPECT_EQ(fake_controller->side_panel_set_show_error_page_, 2);

  // Set the network connection type to being online.
  scoped_mock_network_change_notifier->mock_network_change_notifier()
      ->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);
  fake_controller->ResetSidePanelTracking();

  // Loading a url in the side panel should show the results page.
  content::TestNavigationObserver observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->LoadURLInResultsFrame(search_url);
  observer.WaitForNavigationFinished();

  // Verify the error page was set correctly. It should be hidden after a
  // successful navigation. This happens twice because the URL is modified to
  // add extra search query parameters.
  EXPECT_EQ(fake_controller->side_panel_set_hide_error_page_, 2);
  EXPECT_EQ(fake_controller->side_panel_set_show_error_page_, 0);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       LoadURLInResultsFrameOverlayNotShowing) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);
  const GURL search_url("https://www.google.com/search");
  controller->LoadURLInResultsFrame(search_url);

  // Controller should not open and load URLs when overlay is not showing.
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
  EXPECT_FALSE(coordinator->IsSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_SameTabSameOriginLinkClick) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Loading a url in the side panel should show the results page. This needs to
  // be done to set up the WebContentsObserver.
  const GURL search_url("https://www.google.com/search");
  controller->LoadURLInResultsFrame(search_url);

  // Expect the Lens Overlay results panel to open.
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));
  int tabs = browser()->tab_strip_model()->count();

  // Verify the fake controller exists and reset any loading that was done
  // before as part of setup.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->ResetSidePanelTracking();

  // The results frame should be the only child frame of the side panel web
  // contents.
  content::RenderFrameHost* results_frame = content::ChildFrameAt(
      controller->GetSidePanelWebContentsForTesting()->GetPrimaryMainFrame(),
      0);
  EXPECT_TRUE(results_frame);

  // Simulate a same-origin navigation on the results frame.
  const GURL nav_url("https://www.google.com/search?q=apples");
  content::TestNavigationObserver observer(
      controller->GetSidePanelWebContentsForTesting());
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kSameTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait for the navigation to finish and the page to finish loading.
  observer.WaitForNavigationFinished();
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // It should not open a new tab as this is a same-origin navigation.
  EXPECT_EQ(tabs, browser()->tab_strip_model()->count());

  VerifySearchQueryParameters(observer.last_navigation_url());
  VerifyTextQueriesAreEqual(observer.last_navigation_url(), nav_url);

  // Verify the loading state was set correctly.
  // Loading is set to true twice because the URL is originally malformed.
  EXPECT_EQ(fake_controller->is_side_panel_loading_set_to_true_, 2);
  EXPECT_EQ(fake_controller->is_side_panel_loading_set_to_false_, 1);

  // We should find that the input text on the searchbox is the same as the text
  // query of the nav_url.
  EXPECT_TRUE(content::EvalJs(
                  controller->GetSidePanelWebContentsForTesting()
                      ->GetPrimaryMainFrame(),
                  content::JsReplace(kCheckSearchboxInput, "apples"),
                  content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES)
                  .ExtractBool());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_SameTabCrossOriginLinkClick) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Loading a url in the side panel should show the results page. This needs to
  // be done to set up the WebContentsObserver.
  const GURL search_url("https://www.google.com/search");
  controller->LoadURLInResultsFrame(search_url);

  // Expect the Lens Overlay results panel to open.
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // The results frame should be the only child frame of the side panel web
  // contents.
  content::RenderFrameHost* results_frame = content::ChildFrameAt(
      controller->GetSidePanelWebContentsForTesting()->GetPrimaryMainFrame(),
      0);
  EXPECT_TRUE(results_frame);

  // Verify the fake controller exists and reset any loading that was done
  // before as part of setup.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->ResetSidePanelTracking();

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  const GURL nav_url("http://new.domain.com/");
  // Simulate a cross-origin navigation on the results frame.
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kSameTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Verify the new tab has the URL.
  content::WebContents* new_tab = add_tab.Wait();
  content::WaitForLoadStop(new_tab);
  EXPECT_EQ(new_tab->GetLastCommittedURL(), nav_url);

  // Verify the loading state was never set.
  EXPECT_EQ(fake_controller->is_side_panel_loading_set_to_true_, 0);
  EXPECT_EQ(fake_controller->is_side_panel_loading_set_to_false_, 0);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_NewTabCrossOriginLinkClick) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Loading a url in the side panel should show the results page. This needs to
  // be done to set up the WebContentsObserver.
  const GURL search_url("https://www.google.com/search");
  controller->LoadURLInResultsFrame(search_url);

  // Expect the Lens Overlay results panel to open.
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // The results frame should be the only child frame of the side panel web
  // contents.
  content::RenderFrameHost* results_frame = content::ChildFrameAt(
      controller->GetSidePanelWebContentsForTesting()->GetPrimaryMainFrame(),
      0);
  const GURL nav_url("http://new.domain.com/");
  content::OverrideLastCommittedOrigin(results_frame,
                                       url::Origin::Create(search_url));
  EXPECT_TRUE(results_frame);

  // Verify the fake controller exists and reset any loading that was done
  // before as part of setup.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->ResetSidePanelTracking();

  // Simulate a cross-origin navigation on the results frame.
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kNewTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Verify the new tab has the URL.
  content::WebContents* new_tab = add_tab.Wait();
  content::WaitForLoadStop(new_tab);
  EXPECT_EQ(new_tab->GetLastCommittedURL(), nav_url);
  // Verify the loading state was never set.
  EXPECT_EQ(fake_controller->is_side_panel_loading_set_to_true_, 0);
  EXPECT_EQ(fake_controller->is_side_panel_loading_set_to_false_, 0);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_NewTabCrossOriginLinkClickFromUntrustedSite) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Loading a url in the side panel should show the results page. This needs to
  // be done to set up the WebContentsObserver.
  const GURL search_url("https://www.google.com/search");
  controller->LoadURLInResultsFrame(search_url);

  // Expect the Lens Overlay results panel to open.
  auto* coordinator = browser()->GetFeatures().side_panel_coordinator();
  EXPECT_TRUE(coordinator->IsSidePanelShowing());
  EXPECT_EQ(coordinator->GetCurrentEntryId(),
            SidePanelEntry::Id::kLensOverlayResults);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));
  int tabs = browser()->tab_strip_model()->count();

  // The results frame should be the only child frame of the side panel web
  // contents.
  content::RenderFrameHost* results_frame = content::ChildFrameAt(
      controller->GetSidePanelWebContentsForTesting()->GetPrimaryMainFrame(),
      0);
  const GURL nav_url("http://new.domain.com/");
  content::OverrideLastCommittedOrigin(results_frame,
                                       url::Origin::Create(nav_url));
  EXPECT_TRUE(results_frame);

  // Verify the fake controller exists and reset any loading that was done
  // before as part of setup.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->ResetSidePanelTracking();

  // Simulate a cross-origin navigation on the results frame.
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kNewTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // It should not open a new tab as the initatior origin should not be
  // considered "trusted".
  EXPECT_EQ(tabs, browser()->tab_strip_model()->count());
  // Verify the loading state was never set.
  EXPECT_EQ(fake_controller->is_side_panel_loading_set_to_true_, 0);
  EXPECT_EQ(fake_controller->is_side_panel_loading_set_to_false_, 0);
}

// TODO(crbug.com/360161233): This test is flaky.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       DISABLED_CloseSearchBubbleOnOverlayInteraction) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state. When the overlay is
  // bound, it should start the query flow which returns a response for the
  // interaction data callback.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  auto* bubble_controller =
      controller->get_lens_search_bubble_controller_for_testing();
  EXPECT_TRUE(!!bubble_controller->bubble_view_for_testing());

  // We need to flush the mojo receiver calls to make sure the screenshot was
  // passed back to the WebUI or else the region selection UI will not render.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->FlushForTesting();
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Simulate mouse events on the overlay for drawing a manual region.
  gfx::Point center =
      GetOverlayWebContents()->GetContainerBounds().CenterPoint();
  gfx::Point off_center = gfx::Point(center);
  off_center.Offset(100, 100);
  SimulateLeftClickDrag(center, off_center);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));
  EXPECT_FALSE(!!bubble_controller->bubble_view_for_testing());
}

// TODO(crbug.com/360161233): This test is flaky.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       DISABLED_SearchBubblePageClassification) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state. When the overlay is
  // bound, it should start the query flow which returns a response for the
  // interaction data callback.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);

  // We need to flush the mojo receiver calls to make sure the screenshot was
  // passed back to the WebUI or else the region selection UI will not render.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->FlushForTesting();
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Simulate mouse events on the overlay for drawing a manual region.
  gfx::Point center =
      GetOverlayWebContents()->GetContainerBounds().CenterPoint();
  gfx::Point off_center = gfx::Point(center);
  off_center.Offset(100, 100);
  SimulateLeftClickDrag(center, off_center);

  // A lens request results in a kOverlayAndResults state. This results in the
  // page classification switching from CONTEXTUAL_SEARCHBOX to
  // LENS_SIDE_PANEL_SEARCHBOX.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SearchBubbleLivePageAndResults) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);

  controller->IssueSearchBoxRequestForTesting(
      "green", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Issuing a searchbox request when the controller is in kOverlay state
  // should result in the state being kLivePageAndResults. This shouldn't
  // change the CONTEXTUAL_SEARCHBOX page classification.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kLivePageAndResults; }));
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SearchBubbleCloseButtonClosesOverlay) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state. When the overlay is
  // bound, it should start the query flow which returns a response for the
  // interaction data callback.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  auto* bubble_controller =
      controller->get_lens_search_bubble_controller_for_testing();
  EXPECT_TRUE(!!bubble_controller->bubble_view_for_testing());

  // We need to flush the Mojo receiver calls to make sure the screenshot was
  // passed back to the WebUI or else the region selection UI will not render.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->FlushForTesting();
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Click the search bubble close button.
  EXPECT_TRUE(content::ExecJs(
      bubble_controller->search_bubble_web_contents_for_testing()
          ->GetPrimaryMainFrame(),
      content::JsReplace(kCloseSearchBubbleScript)));

  // Verify the overlay turns off.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       PopAndLoadQueryFromHistory) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Loading a url in the side panel should show the results page.
  const GURL first_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.menu&q=oranges&lns_fp=1&lns_mode=text"
      "&gsc=2&hl=en-US&cs=0");
  controller->LoadURLInResultsFrame(first_search_url);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // The search query history stack should be empty and the currently loaded
  // query should be set.
  EXPECT_TRUE(controller->get_search_query_history_for_testing().empty());
  auto loaded_search_query = controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "oranges");
  VerifySearchQueryParameters(loaded_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(loaded_search_query->search_query_url_,
                            first_search_url);
  EXPECT_TRUE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_FALSE(loaded_search_query->selected_region_);
  EXPECT_FALSE(loaded_search_query->selected_text_);
  EXPECT_FALSE(loaded_search_query->translate_options_);
  EXPECT_EQ(loaded_search_query->lens_selection_type_,
            lens::UNKNOWN_SELECTION_TYPE);

  // Loading a second url in the side panel should show the results page.
  const GURL second_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.menu&q=kiwi&lns_fp=1&lns_mode=text&gsc=2"
      "&hl=en-US&cs=0");
  // We can't use content::WaitForLoadStop here since the last navigation is
  // successful.
  content::TestNavigationObserver observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->LoadURLInResultsFrame(second_search_url);
  observer.WaitForNavigationFinished();

  // The search query history stack should have 1 entry and the currently loaded
  // query should be set to the new query
  EXPECT_EQ(controller->get_search_query_history_for_testing().size(), 1UL);
  loaded_search_query = controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "kiwi");
  VerifySearchQueryParameters(loaded_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(loaded_search_query->search_query_url_,
                            second_search_url);
  EXPECT_TRUE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_FALSE(loaded_search_query->selected_region_);
  EXPECT_FALSE(loaded_search_query->selected_text_);
  EXPECT_FALSE(loaded_search_query->translate_options_);
  EXPECT_EQ(loaded_search_query->lens_selection_type_,
            lens::UNKNOWN_SELECTION_TYPE);
  VerifySearchQueryParameters(observer.last_navigation_url());
  VerifyTextQueriesAreEqual(observer.last_navigation_url(), second_search_url);
  // Popping the query should load the previous query into the results frame.
  content::TestNavigationObserver pop_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->PopAndLoadQueryFromHistory();
  pop_observer.WaitForNavigationFinished();

  // The search query history stack should be empty and the currently loaded
  // query should be set to the previous query.
  EXPECT_TRUE(controller->get_search_query_history_for_testing().empty());
  loaded_search_query = controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "oranges");
  VerifySearchQueryParameters(loaded_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(loaded_search_query->search_query_url_,
                            first_search_url);
  EXPECT_TRUE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_FALSE(loaded_search_query->selected_region_);
  EXPECT_FALSE(loaded_search_query->selected_text_);
  EXPECT_FALSE(loaded_search_query->translate_options_);
  EXPECT_EQ(loaded_search_query->lens_selection_type_,
            lens::UNKNOWN_SELECTION_TYPE);
  VerifySearchQueryParameters(pop_observer.last_navigation_url());
  VerifyTextQueriesAreEqual(pop_observer.last_navigation_url(),
                            first_search_url);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       PopAndLoadTranslateQueryFromHistory) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUIWithPendingRegion(
      LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), CreateNonEmptyBitmap(100, 100));
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());
  auto* fake_query_controller = static_cast<LensOverlayQueryControllerFake*>(
      controller->get_lens_overlay_query_controller_for_testing());
  EXPECT_EQ(fake_query_controller->last_lens_selection_type_,
            lens::INJECTED_IMAGE);

  // Loading a url in the side panel should show the results page.
  const GURL first_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.ctxi&q=&lns_fp=1&lns_mode=un"
      "&gsc=2&hl=en-US&cs=0");
  content::TestNavigationObserver first_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->LoadURLInResultsFrame(first_search_url);
  first_observer.WaitForNavigationFinished();

  // The search query history stack should be empty and the currently loaded
  // query should be set.
  EXPECT_TRUE(controller->get_search_query_history_for_testing().empty());
  const auto first_search_query =
      controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(first_search_query);
  EXPECT_TRUE(first_search_query->search_query_text_.empty());
  VerifySearchQueryParameters(first_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(first_search_query->search_query_url_,
                            first_search_url);
  EXPECT_FALSE(first_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_EQ(first_search_query->selected_region_, kTestRegion);
  EXPECT_FALSE(first_search_query->selected_region_bitmap_.drawsNothing());
  EXPECT_EQ(first_search_query->selected_region_bitmap_.width(), 100);
  EXPECT_EQ(first_search_query->selected_region_bitmap_.height(), 100);
  EXPECT_FALSE(first_search_query->selected_text_);
  EXPECT_EQ(first_search_query->lens_selection_type_, lens::INJECTED_IMAGE);

  // Loading a url in the side panel should show the results page.
  const GURL second_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.menu&q=oranges&lns_fp=1&lns_mode=text"
      "&gsc=2&hl=en-US&cs=0");
  // Do the full image translate query first so it gets attached to the query.
  const std::string source_lang = "auto";
  const std::string target_lang = "fi";
  content::TestNavigationObserver second_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueTranslateFullPageRequestForTesting(source_lang, target_lang);
  controller->IssueTextSelectionRequestForTesting("oranges", 20, 200);
  second_observer.WaitForNavigationFinished();

  // The search query history stack should be size 1.
  EXPECT_EQ(controller->get_search_query_history_for_testing().size(), 1UL);
  const auto second_search_query =
      controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(second_search_query);
  EXPECT_EQ(second_search_query->search_query_text_, "oranges");
  VerifySearchQueryParameters(second_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(second_search_query->search_query_url_,
                            second_search_url);
  EXPECT_TRUE(second_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_TRUE(second_search_query->selected_region_bitmap_.drawsNothing());
  EXPECT_FALSE(second_search_query->selected_region_);
  EXPECT_TRUE(second_search_query->selected_text_);
  EXPECT_EQ(second_search_query->selected_text_->first, 20);
  EXPECT_EQ(second_search_query->selected_text_->second, 200);
  EXPECT_TRUE(second_search_query->translate_options_);
  EXPECT_EQ(second_search_query->translate_options_->source_language,
            source_lang);
  EXPECT_EQ(second_search_query->translate_options_->target_language,
            target_lang);
  EXPECT_EQ(second_search_query->lens_selection_type_,
            lens::SELECT_TEXT_HIGHLIGHT);
  VerifySearchQueryParameters(second_observer.last_navigation_url());
  VerifyTextQueriesAreEqual(second_observer.last_navigation_url(),
                            second_search_url);

  // Loading a second url in the side panel should show the results page.
  const GURL third_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.menu&q=kiwi&lns_fp=1&lns_mode=text&gsc=2"
      "&hl=en-US&cs=0");
  // We can't use content::WaitForLoadStop here since the last navigation is
  // successful.
  content::TestNavigationObserver third_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueEndTranslateModeRequestForTesting();
  controller->LoadURLInResultsFrame(third_search_url);
  third_observer.WaitForNavigationFinished();

  // The search query history stack should have 2 entries and the currently
  // loaded query should be set to the new query
  EXPECT_EQ(controller->get_search_query_history_for_testing().size(), 2UL);
  const auto third_search_query =
      controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(third_search_query);
  EXPECT_EQ(third_search_query->search_query_text_, "kiwi");
  VerifySearchQueryParameters(third_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(third_search_query->search_query_url_,
                            third_search_url);
  EXPECT_TRUE(third_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_FALSE(third_search_query->selected_region_);
  EXPECT_FALSE(third_search_query->selected_text_);
  EXPECT_FALSE(third_search_query->translate_options_);
  EXPECT_EQ(third_search_query->lens_selection_type_,
            lens::UNKNOWN_SELECTION_TYPE);
  VerifySearchQueryParameters(third_observer.last_navigation_url());
  VerifyTextQueriesAreEqual(third_observer.last_navigation_url(),
                            third_search_url);

  // Popping the query should load the previous query into the results frame.
  content::TestNavigationObserver fourth_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->PopAndLoadQueryFromHistory();
  fourth_observer.WaitForNavigationFinished();

  // The search query history stack should have 1 entry and the currently loaded
  // query should be set to the previous query.
  EXPECT_EQ(controller->get_search_query_history_for_testing().size(), 1UL);
  const auto first_popped_query =
      controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(first_popped_query);
  EXPECT_EQ(first_popped_query->search_query_text_, "oranges");
  VerifySearchQueryParameters(first_popped_query->search_query_url_);
  VerifyTextQueriesAreEqual(first_popped_query->search_query_url_,
                            second_search_url);
  EXPECT_TRUE(first_popped_query->selected_region_thumbnail_uri_.empty());
  EXPECT_FALSE(first_popped_query->selected_region_);
  EXPECT_TRUE(first_popped_query->selected_text_);
  EXPECT_EQ(first_popped_query->selected_text_->first, 20);
  EXPECT_EQ(first_popped_query->selected_text_->second, 200);
  EXPECT_TRUE(first_popped_query->translate_options_);
  EXPECT_EQ(first_popped_query->translate_options_->source_language,
            source_lang);
  EXPECT_EQ(first_popped_query->translate_options_->target_language,
            target_lang);
  EXPECT_EQ(first_popped_query->lens_selection_type_,
            lens::SELECT_TEXT_HIGHLIGHT);
  VerifySearchQueryParameters(fourth_observer.last_navigation_url());
  VerifyTextQueriesAreEqual(fourth_observer.last_navigation_url(),
                            second_search_url);
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  fake_controller->FlushForTesting();
  EXPECT_EQ(fake_controller->fake_overlay_page_.source_language_, source_lang);
  EXPECT_EQ(fake_controller->fake_overlay_page_.target_language_, target_lang);

  // Popping the query should load the previous query into the results frame.
  content::TestNavigationObserver fifth_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->PopAndLoadQueryFromHistory();
  // The full sequence of events necessary to load Lens search results is not
  // currently testable, so load the expected URL manually.
  controller->LoadURLInResultsFrame(first_search_url);
  fifth_observer.WaitForNavigationFinished();

  // The search query history stack should be empty and the currently loaded
  // query should be set.
  EXPECT_TRUE(controller->get_search_query_history_for_testing().empty());
  const auto second_popped_query =
      controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(second_popped_query);
  EXPECT_TRUE(second_popped_query->search_query_text_.empty());
  VerifySearchQueryParameters(second_popped_query->search_query_url_);
  VerifyTextQueriesAreEqual(second_popped_query->search_query_url_,
                            first_search_url);
  EXPECT_FALSE(second_popped_query->selected_region_thumbnail_uri_.empty());
  EXPECT_EQ(second_popped_query->selected_region_, kTestRegion);
  EXPECT_FALSE(second_popped_query->selected_region_bitmap_.drawsNothing());
  EXPECT_EQ(second_popped_query->selected_region_bitmap_.width(), 100);
  EXPECT_EQ(second_popped_query->selected_region_bitmap_.height(), 100);
  EXPECT_FALSE(second_popped_query->selected_text_);
  EXPECT_EQ(second_popped_query->lens_selection_type_, lens::INJECTED_IMAGE);
  EXPECT_FALSE(second_popped_query->translate_options_);
  fake_controller->FlushForTesting();
  EXPECT_TRUE(fake_controller->fake_overlay_page_.source_language_.empty());
  EXPECT_TRUE(fake_controller->fake_overlay_page_.target_language_.empty());
}

// TODO(crbug.com/342390515): Test flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_PopAndLoadQueryFromHistoryWithRegionAndTextSelection \
  DISABLED_PopAndLoadQueryFromHistoryWithRegionAndTextSelection
#else
#define MAYBE_PopAndLoadQueryFromHistoryWithRegionAndTextSelection \
  PopAndLoadQueryFromHistoryWithRegionAndTextSelection
#endif
IN_PROC_BROWSER_TEST_F(
    LensOverlayControllerBrowserTest,
    MAYBE_PopAndLoadQueryFromHistoryWithRegionAndTextSelection) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Issuing a text selection request should show the results page.
  const GURL first_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.menu&vsint=KgwKAggHEgIIAxgBIAI&q=oranges"
      "&lns_fp=1&lns_mode=text&gsc=2&hl=en-US&cs=0");
  controller->IssueTextSelectionRequestForTesting("oranges", 20, 200);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // The search query history stack should be empty and the currently loaded
  // query should be set.
  EXPECT_TRUE(controller->get_search_query_history_for_testing().empty());
  auto loaded_search_query = controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "oranges");
  GURL url_without_start_time_or_size =
      RemoveStartTimeAndSizeParams(loaded_search_query->search_query_url_);
  EXPECT_EQ(url_without_start_time_or_size, first_search_url);
  EXPECT_TRUE(loaded_search_query->selected_text_);
  EXPECT_EQ(loaded_search_query->selected_text_->first, 20);
  EXPECT_EQ(loaded_search_query->selected_text_->second, 200);
  EXPECT_TRUE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_FALSE(loaded_search_query->selected_region_);
  EXPECT_EQ(loaded_search_query->lens_selection_type_,
            lens::SELECT_TEXT_HIGHLIGHT);

  // Issuing a region selection request should update the results page.
  const GURL second_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.menu&vsint=KgwKAggHEgIIAxgBIAI&q=&lns_fp=1"
      "&lns_mode=un&gsc=2&hl=en-US&cs=0");
  // We can't use content::WaitForLoadStop here and below since the last
  // navigation was already successful.
  content::TestNavigationObserver second_search_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueLensRegionRequestForTesting(kTestRegion->Clone(),
                                               /*is_click=*/false);
  // The full sequence of events necessary to load Lens search results is not
  // currently testable, so load the expected URL manually.
  controller->LoadURLInResultsFrame(second_search_url);
  second_search_observer.Wait();

  // The search query history stack should have 1 entry and the currently loaded
  // region should be set.
  EXPECT_EQ(controller->get_search_query_history_for_testing().size(), 1UL);
  loaded_search_query.reset();
  loaded_search_query = controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, std::string());
  EXPECT_FALSE(loaded_search_query->selected_text_);
  EXPECT_FALSE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_TRUE(loaded_search_query->selected_region_);
  EXPECT_EQ(loaded_search_query->lens_selection_type_, lens::REGION_SEARCH);

  // Loading another url in the side panel should update the results page.
  const GURL third_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.menu&vsint=KgoKAggHEgIIAyAC&q=kiwi&lns_fp=1"
      "&lns_mode=text&gsc=2&hl=en-US&cs=0");
  content::TestNavigationObserver third_search_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueTextSelectionRequestForTesting("kiwi", 1, 100);
  third_search_observer.Wait();

  // The search query history stack should have 2 entries and the currently
  // loaded query should be set to the new query
  EXPECT_EQ(controller->get_search_query_history_for_testing().size(), 2UL);
  loaded_search_query.reset();
  loaded_search_query = controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "kiwi");
  url_without_start_time_or_size =
      RemoveStartTimeAndSizeParams(loaded_search_query->search_query_url_);
  EXPECT_EQ(url_without_start_time_or_size, third_search_url);
  EXPECT_TRUE(loaded_search_query->selected_text_);
  EXPECT_EQ(loaded_search_query->selected_text_->first, 1);
  EXPECT_EQ(loaded_search_query->selected_text_->second, 100);
  EXPECT_TRUE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_FALSE(loaded_search_query->selected_region_);
  EXPECT_EQ(loaded_search_query->lens_selection_type_,
            lens::SELECT_TEXT_HIGHLIGHT);
  url_without_start_time_or_size =
      RemoveStartTimeAndSizeParams(third_search_observer.last_navigation_url());
  EXPECT_EQ(url_without_start_time_or_size, third_search_url);

  // Popping a query with a region should resend a region search request.
  auto* fake_query_controller = static_cast<LensOverlayQueryControllerFake*>(
      controller->get_lens_overlay_query_controller_for_testing());
  fake_query_controller->Reset();
  controller->PopAndLoadQueryFromHistory();

  // Verify the new interaction request was sent.
  EXPECT_EQ(controller->get_selected_region_for_testing(), kTestRegion);
  EXPECT_FALSE(controller->get_selected_text_for_region());
  EXPECT_EQ(fake_query_controller->last_queried_region_, kTestRegion);
  EXPECT_FALSE(fake_query_controller->last_queried_region_bytes_.has_value());
  EXPECT_EQ(fake_query_controller->last_lens_selection_type_,
            lens::REGION_SEARCH);

  // The full sequence of events necessary to load Lens search results is not
  // currently testable, so load the expected URL manually.
  content::TestNavigationObserver first_pop_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->LoadURLInResultsFrame(second_search_url);
  first_pop_observer.Wait();

  // The search query history stack should have 1 entry and the previously
  // loaded region should be present.
  EXPECT_EQ(controller->get_search_query_history_for_testing().size(), 1UL);
  loaded_search_query.reset();
  loaded_search_query = controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, std::string());
  EXPECT_FALSE(loaded_search_query->selected_text_);
  EXPECT_FALSE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_EQ(loaded_search_query->selected_region_, kTestRegion);
  EXPECT_EQ(loaded_search_query->lens_selection_type_, lens::REGION_SEARCH);

  // Popping another query should load the original query into the results
  // frame.
  content::TestNavigationObserver second_pop_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->PopAndLoadQueryFromHistory();
  second_pop_observer.Wait();

  // The search query history stack should be empty and the currently loaded
  // query should be set to the original query.
  EXPECT_TRUE(controller->get_search_query_history_for_testing().empty());
  loaded_search_query.reset();
  loaded_search_query = controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "oranges");
  url_without_start_time_or_size =
      RemoveStartTimeAndSizeParams(loaded_search_query->search_query_url_);
  EXPECT_EQ(url_without_start_time_or_size, first_search_url);
  EXPECT_TRUE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_FALSE(loaded_search_query->selected_region_);
  EXPECT_TRUE(loaded_search_query->selected_text_);
  EXPECT_EQ(loaded_search_query->lens_selection_type_,
            lens::SELECT_TEXT_HIGHLIGHT);
  EXPECT_EQ(loaded_search_query->selected_text_->first, 20);
  EXPECT_EQ(loaded_search_query->selected_text_->second, 200);
  url_without_start_time_or_size =
      RemoveStartTimeAndSizeParams(second_pop_observer.last_navigation_url());
  EXPECT_EQ(url_without_start_time_or_size, first_search_url);

  // Verify the text selection was sent back to mojo and any old selections
  // were cleared.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_TRUE(fake_controller->fake_overlay_page_.did_clear_text_selection_);
  EXPECT_TRUE(fake_controller->fake_overlay_page_.did_clear_region_selection_);
  EXPECT_EQ(fake_controller->fake_overlay_page_.text_selection_indexes_,
            loaded_search_query->selected_text_);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       PopAndLoadQueryFromHistoryWithInitialImageBytes) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUIWithPendingRegion(
      LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), CreateNonEmptyBitmap(100, 100));
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());
  auto* fake_query_controller = static_cast<LensOverlayQueryControllerFake*>(
      controller->get_lens_overlay_query_controller_for_testing());
  EXPECT_EQ(fake_query_controller->last_lens_selection_type_,
            lens::INJECTED_IMAGE);

  // Loading a url in the side panel should show the results page.
  const GURL first_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.ctxi&q=&lns_fp=1&lns_mode=un"
      "&gsc=2&hl=en-US&cs=0");
  content::TestNavigationObserver first_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->LoadURLInResultsFrame(first_search_url);
  first_observer.WaitForNavigationFinished();

  // The search query history stack should be empty and the currently loaded
  // query should be set.
  EXPECT_TRUE(controller->get_search_query_history_for_testing().empty());
  auto loaded_search_query = controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_TRUE(loaded_search_query->search_query_text_.empty());
  VerifySearchQueryParameters(loaded_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(loaded_search_query->search_query_url_,
                            first_search_url);
  EXPECT_FALSE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_EQ(loaded_search_query->selected_region_, kTestRegion);
  EXPECT_FALSE(loaded_search_query->selected_region_bitmap_.drawsNothing());
  EXPECT_EQ(loaded_search_query->selected_region_bitmap_.width(), 100);
  EXPECT_EQ(loaded_search_query->selected_region_bitmap_.height(), 100);
  EXPECT_FALSE(loaded_search_query->selected_text_);
  EXPECT_EQ(loaded_search_query->lens_selection_type_, lens::INJECTED_IMAGE);

  // Loading a second url in the side panel should show the results page.
  const GURL second_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.ctxi&vsint=KgwKAggHEgIIAxgBIAI&q=kiwi&lns_fp=1&"
      "lns_mode=text&gsc=2&hl=en-US&cs=0");
  content::TestNavigationObserver second_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueTextSelectionRequestForTesting("kiwi", 1, 100);
  second_observer.WaitForNavigationFinished();

  // The search query history stack should have 1 entry and the currently loaded
  // query should be set to the new query
  EXPECT_EQ(controller->get_search_query_history_for_testing().size(), 1UL);
  loaded_search_query.reset();
  loaded_search_query = controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "kiwi");
  VerifySearchQueryParameters(loaded_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(loaded_search_query->search_query_url_,
                            second_search_url);
  EXPECT_TRUE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_FALSE(loaded_search_query->selected_region_);
  EXPECT_TRUE(loaded_search_query->selected_region_bitmap_.drawsNothing());
  EXPECT_TRUE(loaded_search_query->selected_text_);
  EXPECT_EQ(loaded_search_query->lens_selection_type_,
            lens::SELECT_TEXT_HIGHLIGHT);
  GURL url_without_start_time_or_size =
      RemoveStartTimeAndSizeParams(second_observer.last_navigation_url());
  EXPECT_EQ(url_without_start_time_or_size, second_search_url);

  // Popping a query with a region should resend a region search request.
  fake_query_controller->Reset();
  controller->PopAndLoadQueryFromHistory();

  // Verify the new interaction request was sent.
  EXPECT_EQ(controller->get_selected_region_for_testing(), kTestRegion);
  EXPECT_FALSE(controller->get_selected_text_for_region());
  EXPECT_EQ(fake_query_controller->last_queried_region_, kTestRegion);
  EXPECT_TRUE(fake_query_controller->last_queried_region_bytes_);
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes_->width(), 100);
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes_->height(), 100);
  EXPECT_EQ(fake_query_controller->last_lens_selection_type_,
            lens::INJECTED_IMAGE);

  // The full sequence of events necessary to load Lens search results is not
  // currently testable, so load the expected URL manually.
  content::TestNavigationObserver third_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->LoadURLInResultsFrame(first_search_url);
  third_observer.WaitForNavigationFinished();

  // The search query history stack should be empty and the currently loaded
  // query should be set to the original query.
  EXPECT_EQ(controller->get_search_query_history_for_testing().size(), 0UL);
  loaded_search_query.reset();
  loaded_search_query = controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_TRUE(loaded_search_query->search_query_text_.empty());
  VerifySearchQueryParameters(loaded_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(loaded_search_query->search_query_url_,
                            first_search_url);
  EXPECT_FALSE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_EQ(loaded_search_query->selected_region_, kTestRegion);
  EXPECT_FALSE(loaded_search_query->selected_region_bitmap_.drawsNothing());
  EXPECT_EQ(loaded_search_query->selected_region_bitmap_.width(), 100);
  EXPECT_EQ(loaded_search_query->selected_region_bitmap_.height(), 100);
  EXPECT_FALSE(loaded_search_query->selected_text_);
  EXPECT_EQ(loaded_search_query->lens_selection_type_, lens::INJECTED_IMAGE);
  VerifySearchQueryParameters(third_observer.last_navigation_url());
  VerifyTextQueriesAreEqual(third_observer.last_navigation_url(),
                            first_search_url);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       PopAndLoadQueryFromHistoryWithMultimodalRequest) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  controller->ShowUIWithPendingRegion(
      LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Loading a url in the side panel should show the results page.
  const GURL first_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.ctxi&q=&lns_fp=1&lns_mode=un"
      "&gsc=2&hl=en-US&cs=0");
  content::TestNavigationObserver first_load_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->LoadURLInResultsFrame(first_search_url);
  first_load_observer.WaitForNavigationFinished();

  // The search query history stack should be empty and the currently loaded
  // query should be set.
  EXPECT_TRUE(controller->get_search_query_history_for_testing().empty());
  auto loaded_search_query = controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_TRUE(loaded_search_query->search_query_text_.empty());
  VerifySearchQueryParameters(loaded_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(loaded_search_query->search_query_url_,
                            first_search_url);
  EXPECT_FALSE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_EQ(loaded_search_query->selected_region_, kTestRegion);
  EXPECT_FALSE(loaded_search_query->selected_region_bitmap_.drawsNothing());
  EXPECT_EQ(loaded_search_query->selected_region_bitmap_.width(), 100);
  EXPECT_EQ(loaded_search_query->selected_region_bitmap_.height(), 100);
  EXPECT_FALSE(loaded_search_query->selected_text_);
  EXPECT_EQ(loaded_search_query->lens_selection_type_, lens::INJECTED_IMAGE);

  // Loading a second url in the side panel should show the results page.
  const GURL second_search_url(
      "https://www.google.com/"
      "search?source=chrome.gsc&ie=UTF-8&oq=green&vsint=KgwKAggHEgIIEhgAIAI&"
      "gsc=2&hl=en-US&cs=0&q=green&lns_mode=mu&lns_fp=1&udm=24");
  // We can't use content::WaitForLoadStop here since the last navigation is
  // successful.
  content::TestNavigationObserver first_searchbox_query_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueSearchBoxRequestForTesting(
      "green", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());
  controller->LoadURLInResultsFrame(second_search_url);
  first_searchbox_query_observer.WaitForNavigationFinished();

  // The search query history stack should have 1 entry and the currently loaded
  // query should be set to the new query
  EXPECT_EQ(controller->get_search_query_history_for_testing().size(), 1UL);
  loaded_search_query.reset();
  loaded_search_query = controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "green");
  VerifySearchQueryParameters(loaded_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(loaded_search_query->search_query_url_,
                            second_search_url);
  EXPECT_FALSE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_TRUE(loaded_search_query->selected_region_);
  EXPECT_FALSE(loaded_search_query->selected_text_);
  EXPECT_EQ(loaded_search_query->lens_selection_type_, lens::MULTIMODAL_SEARCH);

  // Loading a third search url in the side panel should show the results page.
  const GURL third_search_url(
      "https://www.google.com/"
      "search?source=chrome.gsc&ie=UTF-8&oq=red&vsint=KgwKAggHEgIIEhgAIAI&"
      "gsc=2&hl=en-US&cs=0&q=red&lns_mode=mu&lns_fp=1&udm=24");
  // We can't use content::WaitForLoadStop here since the last navigation is
  // successful.
  content::TestNavigationObserver second_searchbox_query_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueSearchBoxRequestForTesting(
      "red", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/true, std::map<std::string, std::string>());
  controller->LoadURLInResultsFrame(third_search_url);
  second_searchbox_query_observer.WaitForNavigationFinished();

  // The search query history stack should have 2 entries and the currently
  // loaded query should be set to the new query
  EXPECT_EQ(controller->get_search_query_history_for_testing().size(), 2UL);
  loaded_search_query.reset();
  loaded_search_query = controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "red");
  VerifySearchQueryParameters(loaded_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(loaded_search_query->search_query_url_,
                            third_search_url);
  EXPECT_FALSE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_TRUE(loaded_search_query->selected_region_);
  EXPECT_FALSE(loaded_search_query->selected_text_);
  EXPECT_EQ(loaded_search_query->lens_selection_type_,
            lens::MULTIMODAL_SUGGEST_ZERO_PREFIX);

  // Popping a query with a region should resend a multimodal request.
  auto* fake_query_controller = static_cast<LensOverlayQueryControllerFake*>(
      controller->get_lens_overlay_query_controller_for_testing());
  fake_query_controller->Reset();
  controller->PopAndLoadQueryFromHistory();

  // Verify the new interaction request was sent.
  EXPECT_EQ(controller->get_selected_region_for_testing(), kTestRegion);
  EXPECT_FALSE(controller->get_selected_text_for_region());
  EXPECT_EQ(fake_query_controller->last_queried_region_, kTestRegion);
  EXPECT_TRUE(fake_query_controller->last_queried_region_bytes_);
  EXPECT_TRUE(
      memcmp(fake_query_controller->last_queried_region_bytes_->getPixels(),
             initial_bitmap.getPixels(),
             initial_bitmap.computeByteSize()) == 0);
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes_->width(), 100);
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes_->height(), 100);
  EXPECT_EQ(fake_query_controller->last_queried_text_, "green");
  EXPECT_EQ(fake_query_controller->last_lens_selection_type_,
            lens::MULTIMODAL_SEARCH);

  // The full sequence of events necessary to load Lens search results is not
  // currently testable, so load the expected URL manually.
  content::TestNavigationObserver pop_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->LoadURLInResultsFrame(second_search_url);
  pop_observer.Wait();

  // Popping the query stack again should show the initial query.
  fake_query_controller->Reset();
  controller->PopAndLoadQueryFromHistory();

  // Verify that the last queried data did not contain any query text.
  EXPECT_EQ(controller->get_selected_region_for_testing(), kTestRegion);
  EXPECT_FALSE(controller->get_selected_text_for_region());
  EXPECT_EQ(fake_query_controller->last_queried_region_, kTestRegion);
  EXPECT_TRUE(fake_query_controller->last_queried_region_bytes_);
  EXPECT_TRUE(
      memcmp(fake_query_controller->last_queried_region_bytes_->getPixels(),
             initial_bitmap.getPixels(),
             initial_bitmap.computeByteSize()) == 0);
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes_->width(), 100);
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes_->height(), 100);
  EXPECT_TRUE(fake_query_controller->last_queried_text_.empty());
  EXPECT_EQ(fake_query_controller->last_lens_selection_type_,
            lens::INJECTED_IMAGE);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       AddQueryToHistoryAfterResize) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Loading a url in the side panel should show the results page.
  const GURL first_search_url(
      "https://www.google.com/search?q=oranges&gsc=2&hl=en-US");
  controller->LoadURLInResultsFrame(first_search_url);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Loading a second url in the side panel should show the results page.
  const GURL second_search_url(
      "https://www.google.com/search?q=kiwi&gsc=2&hl=en-US");
  // We can't use content::WaitForLoadStop here since the last navigation is
  // successful.
  content::TestNavigationObserver observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->LoadURLInResultsFrame(second_search_url);
  observer.WaitForNavigationFinished();

  // Make the side panel larger.
  const int increment = -50;
  BrowserView::GetBrowserViewForBrowser(browser())
      ->unified_side_panel()
      ->OnResize(increment, true);
  // Popping the query should load the previous query into the results frame.
  content::TestNavigationObserver pop_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->PopAndLoadQueryFromHistory();
  pop_observer.WaitForNavigationFinished();
  // The search query history stack should be empty and the currently loaded
  // query should be set to the previous query.
  EXPECT_TRUE(controller->get_search_query_history_for_testing().empty());
}

// TODO(346840584): Disabled due to flakiness on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_RecordHistogramsShowAndClose DISABLED_RecordHistogramsShowAndClose
#else
#define MAYBE_RecordHistogramsShowAndClose RecordHistogramsShowAndClose
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       MAYBE_RecordHistogramsShowAndClose) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // No metrics should be emitted before anything happens.
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked",
                                    /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Dismissed",
                                    /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("Lens.Overlay.InvocationResultedInSearch",
                                    /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("Lens.Overlay.SessionDuration",
                                    /*expected_count=*/0);
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_SessionEnd::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Showing the UI and then closing it should record an entry in the
  // appropriate buckets and the total count of invocations, dismissals,
  // "resulted in search" and session duration should each be 1. In particular,
  // the "resulted in search" metric should have an entry in the false bucket.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);
  histogram_tester.ExpectBucketCount("Lens.Overlay.Invoked",
                                     LensOverlayInvocationSource::kAppMenu,
                                     /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked",
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.Dismissed", LensOverlayDismissalSource::kOverlayCloseButton,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Dismissed",
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Lens.Overlay.InvocationResultedInSearch",
                                     false, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Lens.Overlay.InvocationResultedInSearch",
                                     true, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("Lens.Overlay.InvocationResultedInSearch",
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ByInvocationSource.AppMenu.InvocationResultedInSearch",
      false, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ByInvocationSource.AppMenu.InvocationResultedInSearch",
      true, /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ByInvocationSource.AppMenu.InvocationResultedInSearch",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.SessionDuration",
                                    /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ByInvocationSource.AppMenu.SessionDuration",
      /*expected_count=*/1);
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_SessionEnd::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Lens_Overlay_SessionEnd::kInvocationSourceName,
      static_cast<int64_t>(LensOverlayInvocationSource::kAppMenu));
  test_ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::Lens_Overlay_SessionEnd::kInvocationResultedInSearchName,
      false);
  const char kSessionDuration[] = "SessionDuration";
  EXPECT_TRUE(
      ukm::TestUkmRecorder::EntryHasMetric(entries[0].get(), kSessionDuration));
}

// TODO(346840584): Disabled due to flakiness on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_RecordHistogramsShowSearchAndClose \
  DISABLED_RecordHistogramsShowSearchAndClose
#else
#define MAYBE_RecordHistogramsShowSearchAndClose \
  RecordHistogramsShowSearchAndClose
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       MAYBE_RecordHistogramsShowSearchAndClose) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // No metrics should be emitted before anything happens.
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked",
                                    /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Dismissed",
                                    /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("Lens.Overlay.InvocationResultedInSearch",
                                    /*expected_count=*/0);
  histogram_tester.ExpectTotalCount("Lens.Overlay.SessionDuration",
                                    /*expected_count=*/0);
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_SessionEnd::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Showing the UI, issuing a search and then closing it should record
  // an entry in the true bucket of the "resulted in search" metric.
  controller->ShowUI(LensOverlayInvocationSource::kToolbar);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Issue a search.
  controller->IssueTextSelectionRequestForTesting("oranges", 20, 200);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Close the overlay and verify that a successful session was recorded.
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);
  histogram_tester.ExpectBucketCount("Lens.Overlay.Invoked",
                                     LensOverlayInvocationSource::kToolbar,
                                     /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked",
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Lens.Overlay.InvocationResultedInSearch",
                                     false, /*expected_count=*/0);
  histogram_tester.ExpectBucketCount("Lens.Overlay.InvocationResultedInSearch",
                                     true, /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ByInvocationSource.Toolbar.InvocationResultedInSearch",
      false, /*expected_count=*/0);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ByInvocationSource.Toolbar.InvocationResultedInSearch",
      true, /*expected_count=*/1);
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_SessionEnd::kEntryName);
  EXPECT_EQ(1u, entries.size());
  test_ukm_recorder.ExpectEntryMetric(
      entries[0], ukm::builders::Lens_Overlay_SessionEnd::kInvocationSourceName,
      static_cast<int64_t>(LensOverlayInvocationSource::kToolbar));
  test_ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::Lens_Overlay_SessionEnd::kInvocationResultedInSearchName,
      true);
  const char kSessionDuration[] = "SessionDuration";
  EXPECT_TRUE(
      ukm::TestUkmRecorder::EntryHasMetric(entries[0].get(), kSessionDuration));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       RecordHistogramsDoubleOpenClose) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Attempting to invoke the overlay twice without closing it in between
  // should record only a single new entry.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Attempting to close the overlay twice without opening it in between should
  // only record a single entry.
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);
  histogram_tester.ExpectBucketCount("Lens.Overlay.Invoked",
                                     LensOverlayInvocationSource::kAppMenu,
                                     /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked",
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.Dismissed", LensOverlayDismissalSource::kOverlayCloseButton,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Dismissed",
                                    /*expected_count=*/1);
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.Dismissed", LensOverlayDismissalSource::kOverlayCloseButton,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Dismissed",
                                    /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       RecordUkmAndTaskCompletionForLensOverlayInteraction) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // No metrics should be emitted before anything happens.
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_Overlay_UserAction::kEntryName);
  EXPECT_EQ(0u, entries.size());

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // We need to flush the mojo receiver calls to make sure the screenshot was
  // passed back to the WebUI or else the region selection UI will not render.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->FlushForTesting();
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Test that the RecordUkmAndTaskCompletionForLensOverlayInteraction function
  // which is called from the WebUI side, records the entry successfully.
  controller->RecordUkmAndTaskCompletionForLensOverlayInteractionForTesting(
      lens::mojom::UserAction::kRegionSelection);
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_Overlay_UserAction::kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry, ukm::builders::Lens_Overlay_Overlay_UserAction::kUserActionName,
      static_cast<int64_t>(lens::mojom::UserAction::kRegionSelection));

  auto* fake_query_controller = static_cast<LensOverlayQueryControllerFake*>(
      controller->get_lens_overlay_query_controller_for_testing());
  EXPECT_THAT(fake_query_controller->last_user_action_,
              testing::Optional(lens::mojom::UserAction::kRegionSelection));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OverlayClosesIfTabUrlPathChanges) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the Overlay
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Navigate the main tab URL to a different path.
  WaitForPaint(kDocumentWithImage);

  // Overlay should close
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OverlayClosesIfTabUrlFragmentChanges) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the Overlay
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Navigate the main tab URL to a different path.
  WaitForPaint(kDocumentWithNamedElementWithFragment);

  // Overlay should close
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OverlayClosesOnReload) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the Overlay
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Simulate user pressing the reload button.
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);

  // Overlay should close
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OverlayStaysOpenWithHistoryState) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the Overlay
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  content::WebContents* contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(contents);

  // Call replaceState, pushState, and back on the underlying page.
  EXPECT_TRUE(content::ExecJs(
      contents->GetPrimaryMainFrame(), kHistoryStateScript,
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Overlay should have stayed open.
  ASSERT_TRUE(controller->state() == State::kOverlay);
}

// TODO(b/340886492): Fix and reenable test on MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_OverlayClosesSidePanelBeforeOpening \
  DISABLED_OverlayClosesSidePanelBeforeOpening
#else
#define MAYBE_OverlayClosesSidePanelBeforeOpening \
  OverlayClosesSidePanelBeforeOpening
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       MAYBE_OverlayClosesSidePanelBeforeOpening) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the side panel
  auto* side_panel_coordinator =
      browser()->GetFeatures().side_panel_coordinator();
  side_panel_coordinator->Show(SidePanelEntry::Id::kBookmarks);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return side_panel_coordinator->IsSidePanelShowing(); }));

  // Showing UI should eventually result in overlay state. When the overlay is
  // bound, it should start the query flow which returns a response for the
  // interaction data callback.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Side panel should now be closed.
  EXPECT_FALSE(side_panel_coordinator->IsSidePanelShowing());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OverlayClosesIfSidePanelIsOpened) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state. When the overlay is
  // bound, it should start the query flow which returns a response for the
  // interaction data callback.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Grab fake controller to test if notify the overlay of being closed.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_FALSE(fake_controller->fake_overlay_page_.did_notify_overlay_closing_);

  // Open the side panel
  auto* side_panel_coordinator =
      browser()->GetFeatures().side_panel_coordinator();
  side_panel_coordinator->Show(SidePanelEntry::Id::kBookmarks);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return side_panel_coordinator->IsSidePanelShowing(); }));

  // Overlay should close.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OverlayClosesIfNewSidePanelEntryAppears) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state. When the overlay is
  // bound, it should start the query flow which returns a response for the
  // interaction data callback.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Grab fake controller to test if notify the overlay of being closed.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_FALSE(fake_controller->fake_overlay_page_.did_notify_overlay_closing_);

  // Open our results panel
  controller->IssueTextSelectionRequestForTesting("test query",
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));

  // Open a different side panel
  auto* side_panel_coordinator =
      browser()->GetFeatures().side_panel_coordinator();

  // Verify the side panel is open
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return side_panel_coordinator->IsSidePanelShowing(); }));

  side_panel_coordinator->Show(SidePanelEntry::Id::kBookmarks);

  // Overlay should close.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
}

// TODO(b/340886492, b/349876638): Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       DISABLED_OverlaySidePanelEdgeCaseInteractions) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // First, test the flow if the user has a side panel open, we request to close
  // it, and then a new side panel opens before the close is finished.
  // Open the side panel
  auto* side_panel_coordinator =
      browser()->GetFeatures().side_panel_coordinator();
  side_panel_coordinator->Show(SidePanelEntry::Id::kBookmarks);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_coordinator->IsSidePanelShowing();
  }));  // issue here

  // Wait for the side panel to open fully.
  SidePanel* side_panel =
      BrowserView::GetBrowserViewForBrowser(browser())->unified_side_panel();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return side_panel->GetAnimationValue() >= 1; }));

  //   Showing UI should eventually result in overlay state. When the overlay is
  //   bound, it should start the query flow which returns a response for the
  //   interaction data callback.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(controller->state() == State::kClosingOpenedSidePanel);

  // Wait for the side panel to start closing.
  ASSERT_TRUE(base::test::RunUntil([&]() { return side_panel->IsClosing(); }));

  // Reshow the side panel to prevent the side panel from closing.
  side_panel_coordinator->Show(SidePanelEntry::Id::kBookmarks);

  // Verify the overlay returns to kOff state.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));

  // Secondly, test the flow if the side panel is open with our results, if we
  // close our UI and request the close the side panel, we gracefully handle a
  // new side panel opening which prevents our requested close.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Open our results panel
  controller->IssueTextSelectionRequestForTesting("test query",
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));

  // Request a close which will start to close the side panel.
  controller->CloseUIAsync(LensOverlayDismissalSource::kOverlayBackgroundClick);
  ASSERT_TRUE(controller->state() == State::kClosingSidePanel);

  // Reshow the side panel to prevent a the side panel from closing.
  side_panel_coordinator->Show(SidePanelEntry::Id::kBookmarks);

  // The overlay eventually returns to kOff state.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, FindBarClosesOverlay) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Open the find bar.
  browser()->GetFindBarController()->Show();

  // Verify the overlay turns off.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, EnterprisePolicy) {
  Profile* profile = browser()->profile();

  // The default policy is to allow the feature to be enabled.
  EXPECT_TRUE(browser()
                  ->GetFeatures()
                  .lens_overlay_entry_point_controller()
                  ->IsEnabled());

  profile->GetPrefs()->SetInteger(
      lens::prefs::kLensOverlaySettings,
      static_cast<int>(lens::prefs::LensOverlaySettingsPolicyValue::kDisabled));
  EXPECT_FALSE(browser()
                   ->GetFeatures()
                   .lens_overlay_entry_point_controller()
                   ->IsEnabled());

  profile->GetPrefs()->SetInteger(
      lens::prefs::kLensOverlaySettings,
      static_cast<int>(lens::prefs::LensOverlaySettingsPolicyValue::kEnabled));
  EXPECT_TRUE(browser()
                  ->GetFeatures()
                  .lens_overlay_entry_point_controller()
                  ->IsEnabled());
}

// TODO(crbug.com/350292135): Flaky on all platforms. Re-enable once flakiness
// is fixed.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       DISABLED_OverlayCopyShortcut) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the Overlay and wait for the WebUI to be ready.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Before showing the results panel, there should be no TriggerCopy sent to
  // the overlay.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_FALSE(fake_controller->fake_overlay_page_.did_trigger_copy);

  // Send CTRL+C to overlay
  SimulateCtrlCKeyPress(GetOverlayWebContents());
  fake_controller->FlushForTesting();

  // Verify that TriggerCopyText was sent.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_controller->fake_overlay_page_.did_trigger_copy; }));

  // Reset did_trigger_copy.
  fake_controller->fake_overlay_page_.did_trigger_copy = false;

  // Open side panel.
  controller->IssueTextSelectionRequestForTesting("test query",
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));

  // Send CTRL+C to side panel
  SimulateCtrlCKeyPress(controller->GetSidePanelWebContentsForTesting());
  fake_controller->FlushForTesting();

  // Verify that TriggerCopyText was sent.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_controller->fake_overlay_page_.did_trigger_copy; }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OverlayClosesIfRendererExits) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the Overlay
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Force the renderer to crash.
  content::RenderProcessHost* process =
      controller->GetOverlayWebViewForTesting()
          ->GetWebContents()
          ->GetPrimaryMainFrame()
          ->GetProcess();
  content::ScopedAllowRendererCrashes allow_renderer_crashes(process);
  process->ForceCrash();

  // Overlay should close
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
  // Tab contents web view should be enabled.
  ASSERT_TRUE(browser()->GetWebView()->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OverlayInBackgroundClosesIfRendererExits) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Get the underlying tab before we open a new tab.
  content::WebContents* underlying_tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Open the Overlay
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Opening a new tab to background the overlay UI.
  WaitForPaint(kDocumentWithNamedElement,
               WindowOpenDisposition::NEW_FOREGROUND_TAB,
               ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
                   ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kBackground; }));

  // Force the old tab renderer to crash.
  content::RenderProcessHost* process =
      underlying_tab_contents->GetPrimaryMainFrame()->GetProcess();
  content::ScopedAllowRendererCrashes allow_renderer_crashes(process);
  process->ForceCrash();

  // Overlay should close
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OverlayInBackgroundClosesIfPageNavigates) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Get the underlying tab before we open a new tab.
  content::WebContents* underlying_tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Open the Overlay
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Opening a new tab to background the overlay UI.
  WaitForPaint(kDocumentWithNamedElement,
               WindowOpenDisposition::NEW_FOREGROUND_TAB,
               ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
                   ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kBackground; }));

  // Navigate the old tab to a new URL.
  const GURL new_url = embedded_test_server()->GetURL(kDocumentWithImage);
  ASSERT_TRUE(content::NavigateToURL(underlying_tab_contents, new_url));

  // Overlay should close
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
}

// Regression test for crbug.com/360710001. Asserts the untrusted lens page will
// load in an unsupported context (browser tab for e.g.) without crashing.
// TODO(crbug.com/360724768): This should be updated to assert the WebUI does
// not load in an unsupported context at all once improvements have landed.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       LoadsInUnsupportedContext) {
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_FALSE(
      lens::LensOverlayControllerGlue::FromWebContents(active_contents));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUILensOverlayUntrustedURL)));
}

class LensOverlayControllerBrowserFullscreenDisabled
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlay, {
                                          {"enable-in-fullscreen", "false"},
                                      });
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserFullscreenDisabled,
                       FullscreenClosesOverlay) {
  WaitForPaint();
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Enter into fullscreen mode.
  FullscreenController* fullscreen_controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  fullscreen_controller->EnterFullscreenModeForTab(
      tab_web_contents->GetPrimaryMainFrame());

  // Verify the overlay turns off.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserFullscreenDisabled,
                       ToolbarEntryPointState) {
  WaitForPaint();

  // Assert entry points are enabled.
  actions::ActionItem* toolbar_entry_point =
      actions::ActionManager::Get().FindAction(
          kActionSidePanelShowLensOverlayResults,
          browser()->browser_actions()->root_action_item());
  EXPECT_TRUE(toolbar_entry_point->GetEnabled());
  EXPECT_TRUE(browser()->command_controller()->IsCommandEnabled(
      IDC_CONTENT_CONTEXT_LENS_OVERLAY));

  // Enter into fullscreen mode.
  FullscreenController* fullscreen_controller =
      browser()->exclusive_access_manager()->fullscreen_controller();
  content::WebContents* tab_web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  fullscreen_controller->EnterFullscreenModeForTab(
      tab_web_contents->GetPrimaryMainFrame());

  // Assert entry points become disabled.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !toolbar_entry_point->GetEnabled(); }));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !browser()->command_controller()->IsCommandEnabled(
        IDC_CONTENT_CONTEXT_LENS_OVERLAY);
  }));

  // Exit fullscreen.
  fullscreen_controller->ExitFullscreenModeForTab(tab_web_contents);

  // Verify the entry points become re-enabled.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return toolbar_entry_point->GetEnabled(); }));
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()->command_controller()->IsCommandEnabled(
        IDC_CONTENT_CONTEXT_LENS_OVERLAY);
  }));
}

class LensOverlayControllerBrowserPDFTest
    : public base::test::WithFeatureOverride,
      public PDFExtensionTestBase {
 public:
  LensOverlayControllerBrowserPDFTest()
      : base::test::WithFeatureOverride(chrome_pdf::features::kPdfOopif) {
    tabs::TabFeatures::ReplaceTabFeaturesForTesting(
        base::BindRepeating(&CreateTabFeatures));
  }

  void SetUpOnMainThread() override {
    PDFExtensionTestBase::SetUpOnMainThread();

    // Permits sharing the page screenshot by default. This disables the
    // permission dialog.
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, true);
    prefs->SetBoolean(lens::prefs::kLensSharingPageContentEnabled, true);
  }

  bool UseOopif() const override { return GetParam(); }

  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    auto enabled = PDFExtensionTestBase::GetEnabledFeatures();
    enabled.push_back({lens::features::kLensOverlay, {}});
    return enabled;
  }

  LensOverlayController* GetLensOverlayController() {
    return browser()
        ->tab_strip_model()
        ->GetActiveTab()
        ->tab_features()
        ->lens_overlay_controller();
  }
};

IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFTest,
                       ContextMenuOpensOverlay) {
  // Open the PDF document and wait for it to finish loading.
  const GURL url = embedded_test_server()->GetURL(kPdfDocument);
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay on the PDF using the context menu.
  bool run_observed = false;
  auto menu_observer = std::make_unique<ContextMenuNotificationObserver>(
      IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH, ui::EF_MOUSE_BUTTON,
      base::BindLambdaForTesting([&](RenderViewContextMenu* menu) {
        // Verify the overlay activates.
        run_observed = true;
      }));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::SimulateMouseClick(tab, 0, blink::WebMouseEvent::Button::kRight);

  // Verify the overlay eventually opens.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return run_observed && controller->state() == State::kOverlay;
  }));
}

IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFTest,
                       PdfBytesExcludedInRequest) {
  // Open the PDF document and wait for it to finish loading.
  const GURL url = embedded_test_server()->GetURL(kPdfDocument);
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify PDF bytes were excluded from the query.
  auto* fake_query_controller = static_cast<LensOverlayQueryControllerFake*>(
      controller->get_lens_overlay_query_controller_for_testing());
  ASSERT_TRUE(
      fake_query_controller->last_sent_underlying_content_bytes_.empty());
}

// This test is wrapped in this BUILDFLAG block because the fallback region
// search functionality will not be enabled if the flag is unset.
#if BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)
IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFTest,
                       ContextMenuViaKeyboardDoesNotOpenOverlay) {
  // Open the PDF document and wait for it to finish loading.
  const GURL url = embedded_test_server()->GetURL(kPdfDocument);
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  bool run_observed = false;
  // Using EF_NONE event type represents a keyboard action.
  auto menu_observer = std::make_unique<ContextMenuNotificationObserver>(
      IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH, ui::EF_NONE,
      base::BindLambdaForTesting([&](RenderViewContextMenu* menu) {
        // Verify the normal region search flow activates.
        lens::LensRegionSearchController* lens_region_search_controller =
            menu->GetLensRegionSearchControllerForTesting();
        ASSERT_NE(lens_region_search_controller, nullptr);
        run_observed = true;
      }));

  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::SimulateMouseClick(tab, 0, blink::WebMouseEvent::Button::kRight);

  // Verify the region search flow eventually opens.
  ASSERT_TRUE(base::test::RunUntil([&]() { return run_observed; }));
  ASSERT_EQ(controller->state(), State::kOff);
}
#endif  // BUILDFLAG(ENABLE_LENS_DESKTOP_GOOGLE_BRANDED_FEATURES)

class LensOverlayControllerBrowserPDFContextualizationTest
    : public LensOverlayControllerBrowserPDFTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    auto enabled = PDFExtensionTestBase::GetEnabledFeatures();
    enabled.push_back({lens::features::kLensOverlayContextualSearchbox,
                       {{"use-pdfs-as-context", "true"},
                        {"file-upload-limit-bytes",
                         base::NumberToString(file_size_limit_bytes_)}}});
    return enabled;
  }

 protected:
  const uint32_t file_size_limit_bytes_ = 10000;
};

IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFContextualizationTest,
                       PdfBytesIncludedInRequest) {
  // Open the PDF document and wait for it to finish loading.
  const GURL url = embedded_test_server()->GetURL(kPdfDocument);
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_TRUE(controller);
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify PDF bytes were included in the query.
  auto* fake_query_controller = static_cast<LensOverlayQueryControllerFake*>(
      controller->get_lens_overlay_query_controller_for_testing());
  ASSERT_FALSE(
      fake_query_controller->last_sent_underlying_content_bytes_.empty());
  ASSERT_EQ(lens::PageContentMimeType::kPdf,
            fake_query_controller->last_sent_underlying_content_type_);

  // Verify the searchbox was shown.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_TRUE(fake_controller->fake_overlay_page_
                  .last_received_should_show_contextual_searchbox_);
}

IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFContextualizationTest,
                       LargePdfNotIncludedInRequest) {
  // Verify the document is over the size limit.
  auto file_size = GetFileSizeForTestDataFile(kPdfDocument12KbFileName);
  ASSERT_TRUE(file_size.has_value());
  ASSERT_GT(file_size.value(), file_size_limit_bytes_);

  // Open the PDF document that is over our size limit and wait for it to finish
  // loading.
  const GURL url = embedded_test_server()->GetURL(
      base::StrCat({"/", kPdfDocument12KbFileName}));
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_TRUE(controller);
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify PDF bytes were not included in the query.
  auto* fake_query_controller = static_cast<LensOverlayQueryControllerFake*>(
      controller->get_lens_overlay_query_controller_for_testing());
  ASSERT_TRUE(
      fake_query_controller->last_sent_underlying_content_bytes_.empty());

  // Verify the searchbox was hidden.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_FALSE(fake_controller->fake_overlay_page_
                   .last_received_should_show_contextual_searchbox_);
}

// TODO(crbug.com/40268279): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(LensOverlayControllerBrowserPDFTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    LensOverlayControllerBrowserPDFContextualizationTest);

// Test with --enable-pixel-output-in-tests enabled, required to actually grab
// screenshots for color extraction.
class LensOverlayControllerBrowserWithPixelsTest
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnablePixelOutputInTests);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  bool IsNotEmptyAndNotTransparentBlack(SkBitmap bitmap) {
    if (!bitmap.empty()) {
      for (int x = 0; x < bitmap.width(); x++) {
        for (int y = 0; y < bitmap.height(); y++) {
          if (bitmap.getColor(x, y) != SK_ColorTRANSPARENT) {
            return true;
          }
        }
      }
    }
    return false;
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserWithPixelsTest,
                       DynamicTheme_Fallback) {
  WaitForPaint();
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify screenshot was captured and stored.
  auto screenshot_bitmap = controller->current_screenshot();
  EXPECT_TRUE(IsNotEmptyAndNotTransparentBlack(screenshot_bitmap));

  // Verify screenshot was encoded and passed to WebUI.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_FALSE(
      fake_controller->fake_overlay_page_.last_received_screenshot_.empty());

  // Verify expected color palette was identified, fallback expected
  // with the page being mostly colorless.
  ASSERT_EQ(lens::PaletteId::kFallback, controller->color_palette());
  // Verify expected theme color were passed to WebUI.
  auto expected_theme = lens::mojom::OverlayTheme::New();
  expected_theme->primary = lens::kColorFallbackPrimary;
  expected_theme->shader_layer_1 = lens::kColorFallbackShaderLayer1;
  expected_theme->shader_layer_2 = lens::kColorFallbackShaderLayer2;
  expected_theme->shader_layer_3 = lens::kColorFallbackShaderLayer3;
  expected_theme->shader_layer_4 = lens::kColorFallbackShaderLayer4;
  expected_theme->shader_layer_5 = lens::kColorFallbackShaderLayer5;
  expected_theme->scrim = lens::kColorFallbackScrim;
  expected_theme->surface_container_highest_light =
      lens::kColorFallbackSurfaceContainerHighestLight;
  expected_theme->surface_container_highest_dark =
      lens::kColorFallbackSurfaceContainerHighestDark;
  expected_theme->selection_element = lens::kColorFallbackSelectionElement;
  EXPECT_TRUE(
      fake_controller->fake_overlay_page_.last_received_theme_.has_value());
  const auto& received_theme =
      fake_controller->fake_overlay_page_.last_received_theme_.value();
  EXPECT_EQ(received_theme->primary, expected_theme->primary);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserWithPixelsTest,
                       DynamicTheme_DynamicColorTangerine) {
  WaitForPaint(kDocumentWithDynamicColor);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_TRUE(
      fake_controller->fake_overlay_page_.last_received_screenshot_.empty());
  EXPECT_FALSE(
      fake_controller->fake_overlay_page_.last_received_theme_.has_value());

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify screenshot was captured and stored.
  auto screenshot_bitmap = controller->current_screenshot();
  EXPECT_TRUE(IsNotEmptyAndNotTransparentBlack(screenshot_bitmap));

  // Verify screenshot was encoded and passed to WebUI.
  EXPECT_FALSE(
      fake_controller->fake_overlay_page_.last_received_screenshot_.empty());

  // Verify expected color palette was identified.
  ASSERT_EQ(lens::PaletteId::kTangerine, controller->color_palette());
  // Verify expected theme color were passed to WebUI.
  auto expected_theme = controller->CreateTheme(lens::PaletteId::kTangerine);
  EXPECT_TRUE(
      fake_controller->fake_overlay_page_.last_received_theme_.has_value());
  const auto& received_theme =
      fake_controller->fake_overlay_page_.last_received_theme_.value();
  EXPECT_EQ(received_theme->primary, expected_theme->primary);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserWithPixelsTest,
                       ViewportImageBoundingBoxes) {
  WaitForPaint(kDocumentWithImage);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_TRUE(
      fake_controller->fake_overlay_page_.last_received_screenshot_.empty());

  // Showing UI should change the state to screenshot and eventually to starting
  // WebUI.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kStartingWebUI; }));

  // Verify screenshot was captured and stored.
  auto screenshot_bitmap = controller->current_screenshot();
  EXPECT_TRUE(IsNotEmptyAndNotTransparentBlack(screenshot_bitmap));

  auto& boxes = controller->GetSignificantRegionBoxesForTesting();
  EXPECT_EQ(boxes.size(), 1UL);
  EXPECT_GT(boxes[0]->box.x(), 0);
  EXPECT_LT(boxes[0]->box.x(), 1);
  EXPECT_GT(boxes[0]->box.y(), 0);
  EXPECT_LT(boxes[0]->box.y(), 1);
  EXPECT_GT(boxes[0]->box.width(), 0);
  EXPECT_LT(boxes[0]->box.width(), 1);
  EXPECT_GT(boxes[0]->box.height(), 0);
  EXPECT_LT(boxes[0]->box.height(), 1);
  EXPECT_EQ(boxes[0]->rotation, 0);
  EXPECT_EQ(boxes[0]->coordinate_type,
            lens::mojom::CenterRotatedBox_CoordinateType::kNormalized);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       RecordTimeToFirstInteraction) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // No metrics should be emitted before anything happens.
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_TimeToFirstInteraction::kEntryName);
  EXPECT_EQ(0u, entries.size());
  histogram_tester.ExpectTotalCount("Lens.Overlay.TimeToFirstInteraction",
                                    /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ByInvocationSource.AppMenu.TimeToFirstInteraction",
      /*expected_count=*/0);

  // Issue a search.
  controller->IssueTextSelectionRequestForTesting("oranges", 20, 200);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  histogram_tester.ExpectTotalCount("Lens.Overlay.TimeToFirstInteraction",
                                    /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ByInvocationSource.AppMenu.TimeToFirstInteraction",
      /*expected_count=*/1);
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_TimeToFirstInteraction::kEntryName);
  EXPECT_EQ(2u, entries.size());
  const char kAllEntryPoints[] = "AllEntryPoints";
  const char kAppMenu[] = "AppMenu";
  EXPECT_TRUE(
      ukm::TestUkmRecorder::EntryHasMetric(entries[0].get(), kAllEntryPoints));
  EXPECT_TRUE(ukm::TestUkmRecorder::EntryHasMetric(entries[1].get(), kAppMenu));

  // Issue another search.
  controller->IssueTextSelectionRequestForTesting("apples", 30, 250);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Another search should not log another time to first interaction metric.
  histogram_tester.ExpectTotalCount("Lens.Overlay.TimeToFirstInteraction",
                                    /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ByInvocationSource.AppMenu.TimeToFirstInteraction",
      /*expected_count=*/1);
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_TimeToFirstInteraction::kEntryName);
  EXPECT_EQ(2u, entries.size());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       RecordTimeToFirstInteractionPendingRegion) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUIWithPendingRegion(
      LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), CreateNonEmptyBitmap(100, 100));
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // No metrics should be emitted before anything happens.
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_TimeToFirstInteraction::kEntryName);
  EXPECT_EQ(0u, entries.size());
  histogram_tester.ExpectTotalCount("Lens.Overlay.TimeToFirstInteraction",
                                    /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ByInvocationSource.ContentAreaContextMenuImage."
      "TimeToFirstInteraction",
      /*expected_count=*/0);

  // Issue a search.
  controller->IssueTextSelectionRequestForTesting("oranges", 20, 200);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // When a lens overlay instance was invoked with an initial region selected,
  // we shouldn't record TimeToFirstInteraction.
  histogram_tester.ExpectTotalCount("Lens.Overlay.TimeToFirstInteraction",
                                    /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ByInvocationSource.ContentAreaContextMenuImage."
      "TimeToFirstInteraction",
      /*expected_count=*/0);
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_TimeToFirstInteraction::kEntryName);
  EXPECT_EQ(0u, entries.size());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       InnerTextBytesInRequest) {
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify inner text was incluced as bytes in the the query.
  auto* fake_query_controller = static_cast<LensOverlayQueryControllerFake*>(
      controller->get_lens_overlay_query_controller_for_testing());
  ASSERT_FALSE(
      fake_query_controller->last_sent_underlying_content_bytes_.empty());
  ASSERT_EQ(lens::PageContentMimeType::kPlainText,
            fake_query_controller->last_sent_underlying_content_type_);

  // Verify the bytes are actually what we expect them to be.
  auto last_sent_underlying_content_bytes =
      fake_query_controller->last_sent_underlying_content_bytes_;
  ASSERT_EQ("The below are non-ascii characters.\n\nこんにちは thêrē 🐶 ©",
            std::string(last_sent_underlying_content_bytes.begin(),
                        last_sent_underlying_content_bytes.end()));
}

class LensOverlayControllerInnerHtmlEnabledTest
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitAndEnableFeatureWithParameters(
        lens::features::kLensOverlayContextualSearchbox,
        {
            {"use-inner-text-as-context", "false"},
            {"use-inner-html-as-context", "true"},
        });
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerInnerHtmlEnabledTest,
                       InnerHtmlBytesInRequest) {
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify inner HTML was included as bytes in the the query.
  auto* fake_query_controller = static_cast<LensOverlayQueryControllerFake*>(
      controller->get_lens_overlay_query_controller_for_testing());
  ASSERT_FALSE(
      fake_query_controller->last_sent_underlying_content_bytes_.empty());
  ASSERT_EQ(lens::PageContentMimeType::kHtml,
            fake_query_controller->last_sent_underlying_content_type_);

  // Verify the bytes are actually what we expect them to be.
  auto last_sent_underlying_content_bytes =
      fake_query_controller->last_sent_underlying_content_bytes_;
  ASSERT_EQ(
      "<body>\n  <h1>The below are non-ascii characters.</h1>\n  <p>こんにちは "
      "thêrē 🐶 ©</p>\n\n</body>",
      std::string(last_sent_underlying_content_bytes.begin(),
                  last_sent_underlying_content_bytes.end()));

  // Verify the searchbox was shown.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_TRUE(fake_controller->fake_overlay_page_
                  .last_received_should_show_contextual_searchbox_);
}

class LensOverlayControllerContextualFeaturesDisabledTest
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            lens::features::kLensOverlayContextualSearchbox});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerContextualFeaturesDisabledTest,
                       PreselectionToastShows) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Preselection toast should be visible when the overlay is showing and is in
  // the kOverlay state.
  auto* preselection_widget = controller->get_preselection_widget_for_testing();
  ASSERT_TRUE(preselection_widget->IsVisible());
}

// TODO(crbug.com/360161233): This test is flaky.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerContextualFeaturesDisabledTest,
                       DISABLED_PreselectionToastDisappearsOnSelection) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // We need to flush the mojo receiver calls to make sure the screenshot was
  // passed back to the WebUI or else the region selection UI will not render.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->FlushForTesting();
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Preselection toast should be visible when the overlay is showing and is in
  // the kOverlay state.
  auto* preselection_widget = controller->get_preselection_widget_for_testing();
  ASSERT_TRUE(preselection_widget->IsVisible());

  // Simulate mouse events on the overlay for drawing a manual region.
  gfx::Point center =
      GetOverlayWebContents()->GetContainerBounds().CenterPoint();
  gfx::Point off_center = gfx::Point(center);
  off_center.Offset(100, 100);
  SimulateLeftClickDrag(center, off_center);

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlayAndResults; }));
  // Must explicitly get preselection bubble from controller.
  ASSERT_EQ(controller->get_preselection_widget_for_testing(), nullptr);
}

// TODO(crbug.com/351958199): Flaky on all platforms.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerContextualFeaturesDisabledTest,
                       DISABLED_PreselectionToastOmniboxFocusState) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // We need to flush the mojo receiver calls to make sure the screenshot was
  // passed back to the WebUI or else the region selection UI will not render.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->FlushForTesting();
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Preselection toast should be visible when the overlay is showing and is in
  // the kOverlay state.
  auto* preselection_widget = controller->get_preselection_widget_for_testing();
  ASSERT_TRUE(preselection_widget->IsVisible());

  // Focus the location bar.
  browser()->window()->GetLocationBar()->FocusLocation(false);

  // Must explicitly get preselection bubble from controller. Widget should be
  // hidden when omnibox has focus.
  ASSERT_FALSE(controller->get_preselection_widget_for_testing()->IsVisible());

  // Move focus back to contents view.
  BrowserView* browser_view = BrowserView::GetBrowserViewForBrowser(browser());
  browser_view->GetContentsView()->RequestFocus();

  // Widget should be visible when contents view receives focus and overlay is
  // open.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return controller->get_preselection_widget_for_testing()->IsVisible();
  }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerContextualFeaturesDisabledTest,
                       NoUnderlyingContentBytesInRequest) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify no bytes were excluded from the query.
  auto* fake_query_controller = static_cast<LensOverlayQueryControllerFake*>(
      controller->get_lens_overlay_query_controller_for_testing());
  ASSERT_TRUE(
      fake_query_controller->last_sent_underlying_content_bytes_.empty());
  ASSERT_EQ(lens::PageContentMimeType::kNone,
      fake_query_controller->last_sent_underlying_content_type_);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerContextualFeaturesDisabledTest,
                       PermissionBubble_Accept) {
  WaitForPaint();
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Disallow sharing the page screenshot.
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, false);
  prefs->SetBoolean(lens::prefs::kLensSharingPageContentEnabled, false);
  ASSERT_FALSE(lens::CanSharePageScreenshotWithLensOverlay(prefs));
  ASSERT_FALSE(lens::CanSharePageContentWithLensOverlay(prefs));

  // Verify attempting to show the UI will show the permission bubble.
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       lens::kLensPermissionDialogName);
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  auto* bubble_widget = waiter.WaitIfNeededAndGet();
  // Wait for the bubble to become visible.
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();
  ASSERT_TRUE(bubble_widget->IsVisible());
  ASSERT_TRUE(controller->get_lens_permission_bubble_controller_for_testing()
                  ->HasOpenDialogWidget());

  // Verify attempting to show the UI again does not close the bubble widget.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  ASSERT_TRUE(bubble_widget->IsVisible());
  ASSERT_TRUE(controller->get_lens_permission_bubble_controller_for_testing()
                  ->HasOpenDialogWidget());

  // Simulate click on the accept button.
  auto* bubble_widget_delegate =
      bubble_widget->widget_delegate()->AsBubbleDialogDelegate();
  ClickBubbleDialogButton(bubble_widget_delegate,
                          bubble_widget_delegate->GetOkButton());
  // Wait for the bubble to be destroyed.
  views::test::WidgetDestroyedWaiter(bubble_widget).Wait();
  ASSERT_FALSE(controller->get_lens_permission_bubble_controller_for_testing()
                   ->HasOpenDialogWidget());

  // Verify sharing the page screenshot is now permitted.
  ASSERT_TRUE(lens::CanSharePageScreenshotWithLensOverlay(prefs));
  // Page content should still not be able to be shared when CSB isn't enabled.
  ASSERT_FALSE(lens::CanSharePageContentWithLensOverlay(prefs));

  // Verify accepting the permission bubble will eventually result in the
  // overlay state.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify screenshot was captured and stored.
  auto screenshot_bitmap = controller->current_screenshot();
  EXPECT_FALSE(screenshot_bitmap.empty());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerContextualFeaturesDisabledTest,
                       PermissionBubble_Reject) {
  WaitForPaint();
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Disallow sharing the page screenshot.
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, false);
  ASSERT_FALSE(lens::CanSharePageScreenshotWithLensOverlay(prefs));

  // Verify attempting to show the UI will show the permission bubble.
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       lens::kLensPermissionDialogName);
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  auto* bubble_widget = waiter.WaitIfNeededAndGet();
  // Wait for the bubble to become visible.
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();
  ASSERT_TRUE(bubble_widget->IsVisible());
  ASSERT_TRUE(controller->get_lens_permission_bubble_controller_for_testing()
                  ->HasOpenDialogWidget());

  // Verify attempting to show the UI again does not close the bubble widget.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  ASSERT_TRUE(bubble_widget->IsVisible());
  ASSERT_TRUE(controller->get_lens_permission_bubble_controller_for_testing()
                  ->HasOpenDialogWidget());

  // Simulate click on the reject button.
  auto* bubble_widget_delegate =
      bubble_widget->widget_delegate()->AsBubbleDialogDelegate();
  ClickBubbleDialogButton(bubble_widget_delegate,
                          bubble_widget_delegate->GetCancelButton());
  // Wait for the bubble to be destroyed.
  views::test::WidgetDestroyedWaiter(bubble_widget).Wait();
  ASSERT_FALSE(controller->get_lens_permission_bubble_controller_for_testing()
                   ->HasOpenDialogWidget());

  // Verify sharing the page screenshot is still not permitted.
  ASSERT_FALSE(lens::CanSharePageScreenshotWithLensOverlay(prefs));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerContextualFeaturesDisabledTest,
                       PermissionBubble_PrefChange) {
  WaitForPaint();
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Disallow sharing the page screenshot.
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, false);
  ASSERT_FALSE(lens::CanSharePageScreenshotWithLensOverlay(prefs));

  // Verify attempting to show the UI will show the permission bubble.
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       lens::kLensPermissionDialogName);
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  auto* bubble_widget = waiter.WaitIfNeededAndGet();
  // Wait for the bubble to become visible.
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();
  ASSERT_TRUE(bubble_widget->IsVisible());
  ASSERT_TRUE(controller->get_lens_permission_bubble_controller_for_testing()
                  ->HasOpenDialogWidget());

  // Verify attempting to show the UI again does not close the bubble widget.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  ASSERT_TRUE(bubble_widget->IsVisible());
  ASSERT_TRUE(controller->get_lens_permission_bubble_controller_for_testing()
                  ->HasOpenDialogWidget());

  // Simulate pref being enabled elsewhere.
  prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, true);
  // Wait for the bubble to be destroyed.
  views::test::WidgetDestroyedWaiter(bubble_widget).Wait();
  ASSERT_FALSE(controller->get_lens_permission_bubble_controller_for_testing()
                   ->HasOpenDialogWidget());

  // Verify sharing the page screenshot is now permitted.
  ASSERT_TRUE(lens::CanSharePageScreenshotWithLensOverlay(prefs));

  // Verify accepting the permission bubble will eventually result in the
  // overlay state.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify screenshot was captured and stored.
  auto screenshot_bitmap = controller->current_screenshot();
  EXPECT_FALSE(screenshot_bitmap.empty());
}

class LensOverlayControllerOverlaySearchbox
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{lens::features::kLensOverlay,
                              lens::features::kLensOverlayContextualSearchbox},
        /*disabled_features=*/{});
  }

  void VerifyContextualSearchQueryParameters(const GURL& url_to_process) {
    EXPECT_THAT(
        url_to_process.spec(),
        testing::MatchesRegex(
            std::string(kResultsSearchBaseUrl) +
            ".*source=chrome.cr.menu.*&q=.*&gsc=2&hl=.*&biw=\\d+&bih=\\d+"));
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerOverlaySearchbox,
                       OverlaySearchboxPageClassificationAndState) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  // Searchbox should start in contextual mode if we are in the overlay state.
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);

  controller->IssueSearchBoxRequestForTesting(
      "hello", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Issuing a search from the overlay state can only be done through the
  // contextual searchbox and should result in a live page with results.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kLivePageAndResults; }));
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerOverlaySearchbox,
                       OverlaySearchboxCorrectResultsUrl) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  controller->ShowUI(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify searchbox is in contextual mode.
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);

  controller->IssueSearchBoxRequestForTesting(
      "hello", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Wait for URL to load in side panel.
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Verify the query and params are set.
  auto loaded_search_query = controller->get_loaded_search_query_for_testing();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "hello");
  VerifyContextualSearchQueryParameters(loaded_search_query->search_query_url_);
}
}  // namespace
