// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This class runs functional tests for lens overlay. These tests spin up a full
// web browser, but allow for inspection and modification of internal state of
// LensOverlayController and other business-logic classes.

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

#include <memory>

#include "base/base64url.h"
#include "base/compiler_specific.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/memory/raw_ptr_exclusion.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/protobuf_matchers.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/with_feature_override.h"
#include "base/threading/thread_restrictions.h"
#include "build/build_config.h"
#include "chrome/browser/companion/text_finder/text_highlighter.h"
#include "chrome/browser/companion/text_finder/text_highlighter_manager.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/lens.mojom.h"
#include "chrome/browser/lens/core/mojom/lens_side_panel.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/page_content_type.mojom.h"
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
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/find_bar/find_bar_controller.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/hats/mock_hats_service.h"
#include "chrome/browser/ui/lens/lens_overlay_colors.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_gen204_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_untrusted_ui.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/lens_permission_bubble_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_searchbox_controller.h"
#include "chrome/browser/ui/lens/lens_side_panel_untrusted_ui.h"
#include "chrome/browser/ui/lens/test_lens_overlay_controller.h"
#include "chrome/browser/ui/lens/test_lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/test_lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/test_lens_search_contextualization_controller.h"
#include "chrome/browser/ui/lens/test_lens_search_controller.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/tabs/split_tab_metrics.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/views/omnibox/omnibox_view_views.h"
#include "chrome/browser/ui/views/page_action/page_action_container_view.h"
#include "chrome/browser/ui/views/page_action/page_action_icon_controller.h"
#include "chrome/browser/ui/views/page_action/page_action_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_id.h"
#include "chrome/browser/ui/views/side_panel/side_panel_header.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/webui/feedback/feedback_dialog.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/extensions/api/pdf_viewer_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/base32/base32.h"
#include "components/constrained_window/constrained_window_views.h"
#include "components/contextual_tasks/public/features.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/lens/lens_overlay_side_panel_menu_option.h"
#include "components/lens/lens_overlay_side_panel_result.h"
#include "components/lens/proto/server/lens_overlay_response.pb.h"
#include "components/omnibox/common/omnibox_features.h"
#include "components/optimization_guide/content/browser/page_context_eligibility.h"
#include "components/optimization_guide/content/browser/page_context_eligibility_api.h"
#include "components/permissions/test/permission_request_observer.h"
#include "components/policy/core/browser/browser_policy_connector.h"
#include "components/policy/core/common/mock_configuration_policy_provider.h"
#include "components/prefs/pref_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
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
#include "extensions/browser/test_event_router_observer.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "net/base/mock_network_change_notifier.h"
#include "net/base/network_change_notifier.h"
#include "net/base/url_util.h"
#include "net/test/embedded_test_server/http_response.h"
#include "pdf/pdf_features.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "third_party/blink/public/mojom/context_menu/context_menu.mojom.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "third_party/lens_server_proto/lens_overlay_server.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "ui/base/page_transition_types.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/base/window_open_disposition.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/events/base_event_utils.h"
#include "ui/events/event_constants.h"
#include "ui/events/test/test_event.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/button_test_api.h"
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
constexpr char kMultiPagePdf[] = "/pdf/test-bookmarks.pdf";
constexpr char kPdfDocumentWithForm[] = "/pdf/submit_form.pdf";
constexpr char kDocumentWithNonAsciiCharacters[] = "/non-ascii.html";
constexpr char kImageFile[] = "/handbag.png";
constexpr char kVideoFile[] = "/media/bear-640x360-a_frag-cenc.mp4";
constexpr char kAudioFile[] = "/media/pink_noise_140ms.wav";
constexpr char kJsonFile[] = "/web_apps/basic.json";

constexpr char kPdfDocument12KbFileName[] = "pdf/test-title.pdf";

constexpr char kHelloWorldDataUri[] =
    "data:text/html,%3Ch1%3EHello%2C%20World%21%3C%2Fh1%3E";

using ::base::test::EqualsProto;
using ::testing::_;
using State = LensOverlayController::State;
using LensOverlayInvocationSource = lens::LensOverlayInvocationSource;
using LensOverlayDismissalSource = lens::LensOverlayDismissalSource;

// The fake server session id.
constexpr char kTestServerSessionId[] = "server_session_id";

// The fake search session id.
constexpr char kTestSearchSessionId[] = "search_session_id";

constexpr char kNewTabLinkClickScript[] =
    "(function() {const anchor = document.createElement('a');anchor.href = "
    "$1;anchor.target = "
    "'_blank';document.body.appendChild(anchor);anchor.click();})();";

constexpr char kSameTabLinkClickScript[] =
    "(function() {const anchor = document.createElement('a');anchor.href = "
    "$1;document.body.appendChild(anchor);anchor.click();})();";

constexpr char kTopLevelNavLinkClickScript[] =
    "(function() {const anchor = document.createElement('a');anchor.href = "
    "$1;anchor.target='_top';document.body.appendChild(anchor);anchor.click();}"
    ")();";

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

constexpr char kCheckSidePanelThumbnailShownScript[] = R"(
  (function() {
    const app = document.body.querySelector('lens-side-panel-app');
    const searchbox = app.shadowRoot.querySelector('#searchbox');
    const thumbnailContainer =
        searchbox.shadowRoot.querySelector('#thumbnailContainer');
    if (!thumbnailContainer) {
      return false;
    }

    const thumbnail = searchbox.shadowRoot.querySelector('#thumbnail');
    const imageSrc = thumbnail.shadowRoot.querySelector('#image').src;
    return window.getComputedStyle(thumbnailContainer).display !== 'none' &&
        imageSrc.startsWith('data:image/jpeg');
  })();)";

constexpr char kHistoryStateScript[] =
    "(function() {history.replaceState({'test':1}, 'test'); "
    "history.pushState({'test':1}, 'test'); history.back();})();";

// `content::ExecJs` can handle promises, so queue a promise that only succeeds
// after the contents have been rendered.
constexpr char kPaintWorkaroundFunction[] =
    "() => new Promise(resolve => requestAnimationFrame(() => resolve(true)))";

constexpr char kTestSuggestSignals[] = "encoded_image_signals";

constexpr char kQuerySubmissionTimeQueryParameter[] = "qsubts";
constexpr char kClientUploadDurationQueryParameter[] = "cud";
constexpr char kViewportWidthQueryParamKey[] = "biw";
constexpr char kViewportHeightQueryParamKey[] = "bih";
constexpr char kTextQueryParamKey[] = "q";
constexpr char kChromeSidePanelParameterKey[] = "gsc";
constexpr char kLensRequestQueryParameter[] = "vsrid";

constexpr char kResultsSearchBaseUrl[] = "https://www.google.com/search";

// The test time.
constexpr base::Time kTestTime = base::Time::FromSecondsSinceUnixEpoch(1000);

std::string EncodeRequestId(const lens::LensOverlayRequestId& request_id) {
  std::string serialized_request_id;
  CHECK(request_id.SerializeToString(&serialized_request_id));
  std::string encoded_request_id;
  base::Base64UrlEncode(serialized_request_id,
                        base::Base64UrlEncodePolicy::OMIT_PADDING,
                        &encoded_request_id);
  return encoded_request_id;
}

// Opens the given URL in the given browser and waits for the first paint to
// complete.
void WaitForPaintImpl(
    Browser* browser,
    const GURL& url,
    WindowOpenDisposition disposition = WindowOpenDisposition::CURRENT_TAB,
    int browser_test_flags = ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser, url, disposition, browser_test_flags));
  const bool first_paint_completed =
      browser->tab_strip_model()
          ->GetActiveTab()
          ->GetContents()
          ->CompletedFirstVisuallyNonEmptyPaint();

  // Return early if first paint is already completed.
  if (first_paint_completed) {
    return;
  }
  // Wait for the first paint to complete. The below code works for a majority
  // of cases, but loading non-html files can lead to the workaround failing, so
  // this check is still needed.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser->tab_strip_model()
        ->GetActiveTab()
        ->GetContents()
        ->CompletedFirstVisuallyNonEmptyPaint();
  }));
  // If the first paint was not mark as completed by the WebContents, use a
  // workaround to request a frame on the WebContents. This function will only
  // return when the promise is resolved and thus there is content painted on
  // the WebContents to allow screenshotting. See crbug.com/334747109 for
  // details on this possible race condition and the workaround used in
  // interactive tests.
  ASSERT_TRUE(
      content::ExecJs(browser->tab_strip_model()->GetActiveTab()->GetContents(),
                      kPaintWorkaroundFunction));
}

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
        gfx::RectF(0.5, 0.5, 0.8, 0.8),
        0.1,
        lens::mojom::CenterRotatedBox_CoordinateType::kNormalized),
    std::vector<lens::mojom::PolygonPtr>());
const lens::mojom::OverlayObjectPtr kTestOverlayObject =
    lens::mojom::OverlayObject::New("unique_id", kTestGeometry->Clone());
const lens::mojom::TextPtr kTestText =
    lens::mojom::Text::New(lens::mojom::TextLayout::New(), "en");
const lens::mojom::CenterRotatedBoxPtr kTestRegion =
    lens::mojom::CenterRotatedBox::New(
        gfx::RectF(0.5, 0.5, 0.8, 0.8),
        0.0,
        lens::mojom::CenterRotatedBox_CoordinateType::kNormalized);

lens::LensOverlayObjectsResponse CreateTestObjectsResponse(
    bool is_translate,
    std::vector<std::string> words = {}) {
  lens::LensOverlayObjectsResponse objects_response;
  auto* overlay_object = objects_response.add_overlay_objects();
  overlay_object->set_id("unique_id");
  overlay_object->mutable_geometry()->mutable_bounding_box()->set_center_x(
      kTestGeometry->bounding_box->box.x());
  overlay_object->mutable_geometry()->mutable_bounding_box()->set_center_y(
      kTestGeometry->bounding_box->box.y());
  overlay_object->mutable_geometry()->mutable_bounding_box()->set_width(
      kTestGeometry->bounding_box->box.width());
  overlay_object->mutable_geometry()->mutable_bounding_box()->set_height(
      kTestGeometry->bounding_box->box.height());
  overlay_object->mutable_geometry()
      ->mutable_bounding_box()
      ->set_coordinate_type(lens::CoordinateType::NORMALIZED);
  overlay_object->mutable_geometry()->mutable_bounding_box()->set_rotation_z(
      kTestGeometry->bounding_box->rotation);

  // The interaction properties must be present or else the proto converter
  // will ignore the object.
  overlay_object->mutable_interaction_properties()->set_select_on_tap(true);

  // Create the test text object.
  lens::Text* text = objects_response.mutable_text();
  text->set_content_language(is_translate ? "fr" : "en");

  // Create a paragraph.
  lens::TextLayout::Paragraph* paragraph =
      text->mutable_text_layout()->add_paragraphs();

  // Create a line.
  lens::TextLayout::Line* line = paragraph->add_lines();

  for (size_t i = 0; i < words.size(); ++i) {
    lens::TextLayout::Word* word = line->add_words();
    word->set_plain_text(words[i]);
    word->set_text_separator(" ");
    word->mutable_geometry()->mutable_bounding_box()->set_center_x(0.1 * i);
    word->mutable_geometry()->mutable_bounding_box()->set_center_y(0.1);
    word->mutable_geometry()->mutable_bounding_box()->set_width(0.1);
    word->mutable_geometry()->mutable_bounding_box()->set_height(0.1);
    word->mutable_geometry()->mutable_bounding_box()->set_coordinate_type(
        lens::NORMALIZED);
  }

  objects_response.mutable_cluster_info()->set_server_session_id(
      kTestServerSessionId);
  objects_response.mutable_cluster_info()->set_search_session_id(
      kTestSearchSessionId);
  return objects_response;
}

class LensOverlayPageFake : public lens::mojom::LensPage {
 public:
  void ScreenshotDataReceived(const SkBitmap& screenshot,
                              bool is_side_panel_open) override {
    last_received_screenshot_ = screenshot;
    last_received_is_side_panel_open_ = is_side_panel_open;
    // Do the real call on the open WebUI we intercepted.
    overlay_page_->ScreenshotDataReceived(screenshot, is_side_panel_open);
  }

  void ObjectsReceived(
      std::vector<lens::mojom::OverlayObjectPtr> objects) override {
    last_received_objects_ = std::move(objects);
  }

  void TextReceived(lens::mojom::TextPtr text) override {
    last_received_text_ = std::move(text);
  }

  void RegionTextReceived(lens::mojom::TextPtr text,
                          bool is_injected_image) override {
    last_received_text_ = std::move(text);
  }

  void ThemeReceived(lens::mojom::OverlayThemePtr theme) override {
    last_received_theme_ = std::move(theme);
  }

  void ShouldShowContextualSearchBox(bool should_show) override {
    last_received_should_show_contextual_searchbox_ = should_show;
  }

  void NotifyHandshakeComplete() override {}

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

  void OnCopyCommand() override { did_trigger_copy = true; }

  void SuppressGhostLoader() override {}

  void PageContentTypeChanged(
      lens::mojom::PageContentType new_page_content_type) override {}

  void OnOverlayReshown(const SkBitmap& screenshot) override {
    last_received_screenshot_ = screenshot;
    overlay_page_->OnOverlayReshown(screenshot);
  }

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
    last_received_is_side_panel_open_.reset();
  }

  // The real side panel page that was opened by the lens overlay. Needed to
  // call real functions on the WebUI.
  mojo::Remote<lens::mojom::LensPage> overlay_page_;

  SkBitmap last_received_screenshot_;
  std::optional<bool> last_received_is_side_panel_open_;
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

// Stubs out network requests and mojo calls.
class LensOverlayControllerFake : public lens::TestLensOverlayController {
 public:
  LensOverlayControllerFake(tabs::TabInterface* tab,
                            LensSearchController* lens_search_controller,
                            variations::VariationsClient* variations_client,
                            signin::IdentityManager* identity_manager,
                            PrefService* pref_service,
                            syncer::SyncService* sync_service,
                            ThemeService* theme_service)
      : lens::TestLensOverlayController(tab,
                                        lens_search_controller,
                                        variations_client,
                                        identity_manager,
                                        pref_service,
                                        sync_service,
                                        theme_service) {}

  void BindOverlay(mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
                   mojo::PendingRemote<lens::mojom::LensPage> page) override {
    // Reset the receiver to close any existing connection.
    fake_overlay_page_receiver_.reset();
    fake_overlay_page_.overlay_page_.reset();
    if (!should_bind_overlay_) {
      return;
    }

    // Set up the fake overlay page to intercept the mojo call.
    fake_overlay_page_.overlay_page_.Bind(std::move(page));
    LensOverlayController::BindOverlay(
        std::move(receiver),
        fake_overlay_page_receiver_.BindNewPipeAndPassRemote());
  }

  bool IsScreenshotPossible(content::RenderWidgetHostView*) override {
    return is_screenshot_possible_;
  }

  void FlushForTesting() { fake_overlay_page_receiver_.FlushForTesting(); }

  LensOverlayPageFake fake_overlay_page_;
  bool should_bind_overlay_ = true;
  bool is_screenshot_possible_ = true;
  mojo::Receiver<lens::mojom::LensPage> fake_overlay_page_receiver_{
      &fake_overlay_page_};
};

class LensSearchControllerFake : public lens::TestLensSearchController {
 public:
  explicit LensSearchControllerFake(tabs::TabInterface* tab)
      : lens::TestLensSearchController(tab) {}

  ~LensSearchControllerFake() override = default;

  // Helper function to force the fake query controller to return errors in its
  // responses to full image requests. This should be called before ShowUI.
  void SetFullImageRequestShouldReturnError() {
    full_image_request_should_return_error_ = true;
  }

  void SetOcrResponseWords(const std::vector<std::string>& words) {
    ocr_response_words_ = words;
  }

  std::string GetLastSearchUrl() { return last_search_url_; }

 protected:
  std::unique_ptr<LensOverlayController> CreateLensOverlayController(
      tabs::TabInterface* tab,
      LensSearchController* lens_search_controller,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      PrefService* pref_service,
      syncer::SyncService* sync_service,
      ThemeService* theme_service) override {
    // Set browser color scheme to light mode for consistency.
    theme_service->SetBrowserColorScheme(
        ThemeService::BrowserColorScheme::kLight);

    return std::make_unique<LensOverlayControllerFake>(
        tab, lens_search_controller, variations_client, identity_manager,
        pref_service, sync_service, theme_service);
  }

  std::unique_ptr<lens::LensOverlayQueryController> CreateLensQueryController(
      lens::LensOverlayFullImageResponseCallback full_image_callback,
      lens::LensOverlayUrlResponseCallback url_callback,
      lens::LensOverlayInteractionResponseCallback interaction_callback,
      lens::LensOverlaySuggestInputsCallback suggest_inputs_callback,
      lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
      lens::UploadProgressCallback upload_progress_callback,
      variations::VariationsClient* variations_client,
      signin::IdentityManager* identity_manager,
      Profile* profile,
      lens::LensOverlayInvocationSource invocation_source,
      bool use_dark_mode,
      lens::LensOverlayGen204Controller* gen204_controller) override {
    url_callback_ = url_callback;
    auto fake_query_controller =
        std::make_unique<lens::TestLensOverlayQueryController>(
            full_image_callback,
            base::BindRepeating(
                &LensSearchControllerFake::RecordUrlResponseCallback,
                base::Unretained(this)),
            interaction_callback, suggest_inputs_callback,
            thumbnail_created_callback, upload_progress_callback,
            variations_client, identity_manager, profile, invocation_source,
            use_dark_mode, gen204_controller);
    // Set up the fake responses for the query controller.
    fake_query_controller->set_next_full_image_request_should_return_error(
        full_image_request_should_return_error_);

    lens::LensOverlayServerClusterInfoResponse cluster_info_response;
    cluster_info_response.set_server_session_id(kTestServerSessionId);
    cluster_info_response.set_search_session_id(kTestSearchSessionId);
    fake_query_controller->set_fake_cluster_info_response(
        cluster_info_response);

    fake_query_controller->set_fake_objects_response(
        CreateTestObjectsResponse(/*is_translate=*/false, ocr_response_words_));

    lens::LensOverlayInteractionResponse interaction_response;
    interaction_response.set_encoded_response(kTestSuggestSignals);
    fake_query_controller->set_fake_interaction_response(interaction_response);
    return fake_query_controller;
  }

  std::unique_ptr<lens::LensOverlaySidePanelCoordinator>
  CreateLensOverlaySidePanelCoordinator() override {
    return std::make_unique<lens::TestLensOverlaySidePanelCoordinator>(this);
  }

 private:
  // A url response callback that records the url sent to the callback.
  void RecordUrlResponseCallback(lens::proto::LensOverlayUrlResponse response) {
    last_search_url_ = response.url();
    if (!url_callback_.is_null()) {
      url_callback_.Run(response);
    }
  }

  std::vector<std::string> ocr_response_words_;
  std::string last_search_url_;
  lens::LensOverlayUrlResponseCallback url_callback_;
  bool full_image_request_should_return_error_ = false;
};

namespace {

ui::UserDataFactory::ScopedOverride UseFakeLensSearchController() {
  return tabs::TabFeatures::GetUserDataFactoryForTesting()
      .AddOverrideForTesting(base::BindRepeating([](tabs::TabInterface& tab) {
        return std::make_unique<LensSearchControllerFake>(&tab);
      }));
}

}  // namespace

class LensOverlayControllerBrowserTest : public InProcessBrowserTest {
 protected:
  LensOverlayControllerBrowserTest() {
    lens_search_controller_override_ = UseFakeLensSearchController();
  }

  void SetUp() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndListen());
    SetupFeatureList();
    policy_provider_.SetDefaultReturns(
        /*is_initialization_complete_return=*/true,
        /*is_first_policy_load_complete_return=*/true);
    policy::BrowserPolicyConnector::SetPolicyProviderForTesting(
        &policy_provider_);
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

  views::WebView* GetWebView() {
    return BrowserElementsViews::From(browser())->RetrieveView(
        kActiveContentsWebViewRetrievalId);
  }

  virtual void SetupFeatureList() {
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlay,
          {{"results-search-url", kResultsSearchBaseUrl},
           {"use-dynamic-theme", "true"},
           {"use-dynamic-theme-min-population-pct", "0.002"},
           {"use-dynamic-theme-min-chroma", "3.0"}}},
         {lens::features::kLensOverlayContextualSearchbox,
          {
              {"send-page-url-for-contextualization", "true"},
              {"use-inner-text-as-context", "true"},
              {"update-viewport-each-query", "true"},
          }},
         {lens::features::kLensOverlaySurvey, {}},
         {lens::features::kLensOverlaySidePanelOpenInNewTab, {}}},
        /*disabled_features=*/{
            lens::features::kLensSearchZeroStateCsb,
            lens::features::kLensAimSuggestions,
            lens::features::kLensOverlaySuggestionsMigration,
            lens::features::kLensOverlayNonBlockingPrivacyNotice});
  }

  const SkBitmap CreateNonEmptyBitmap(int width, int height) {
    SkBitmap bitmap;
    bitmap.allocN32Pixels(width, height);
    bitmap.eraseColor(SK_ColorGREEN);
    return bitmap;
  }

  LensSearchController* GetLensSearchController() {
    return LensSearchController::From(browser()->GetActiveTabInterface());
  }

  lens::LensOverlayQueryController* GetLensOverlayQueryController() {
    return GetLensSearchController()->lens_overlay_query_controller();
  }

  lens::LensOverlaySidePanelCoordinator* GetLensOverlaySidePanelCoordinator() {
    return GetLensSearchController()->lens_overlay_side_panel_coordinator();
  }

  bool IsSidePanelOpen() {
    return browser()->GetFeatures().side_panel_ui()->IsSidePanelShowing(
        GetLensOverlaySidePanelCoordinator()->GetPanelType());
  }

  bool IsLensResultsSidePanelShowing() {
    return GetLensOverlaySidePanelCoordinator()->IsEntryShowing();
  }

  LensOverlayController* GetLensOverlayController() {
    return browser()
        ->tab_strip_model()
        ->GetActiveTab()
        ->GetTabFeatures()
        ->lens_overlay_controller();
  }

  content::WebContents* GetOverlayWebContents() {
    auto* controller = GetLensOverlayController();
    return controller->GetOverlayWebViewForTesting()->GetWebContents();
  }

  const std::optional<lens::SearchQuery> GetLoadedSearchQuery() {
    return GetLensOverlaySidePanelCoordinator()
        ->get_loaded_search_query_for_testing();
  }

  const std::vector<lens::SearchQuery>& GetSearchQueryHistory() {
    return GetLensOverlaySidePanelCoordinator()
        ->get_search_query_history_for_testing();
  }

  void OpenLensOverlay(lens::LensOverlayInvocationSource invocation_source) {
    GetLensSearchController()->OpenLensOverlay(invocation_source);
  }

  void OpenLensOverlayWithPendingRegion(
      lens::LensOverlayInvocationSource invocation_source,
      lens::mojom::CenterRotatedBoxPtr region,
      const SkBitmap& region_bitmap) {
    GetLensSearchController()->OpenLensOverlayWithPendingRegion(
        invocation_source, std::move(region), region_bitmap);
  }

  void SimulateOpenInNewTabButtonClick() {
    views::Button* open_in_new_tab_button =
        browser()
            ->GetBrowserView()
            .contents_height_side_panel()
            ->GetHeaderView<SidePanelHeader>()
            ->header_open_in_new_tab_button();
    views::test::ButtonTestApi(open_in_new_tab_button)
        .NotifyClick(ui::test::TestEvent());
  }

  void SimulateLeftClickDrag(gfx::Point from, gfx::Point to) {
    auto* overlay_web_contents = GetOverlayWebContents();

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
    WaitForPaintImpl(browser(), url, disposition, browser_test_flags);
  }

  // Helper to remove the start time, client upload duration, and viewport size
  // query params from the url.
  GURL RemoveStartTimeAndSizeParams(const GURL& url_to_process) {
    GURL processed_url = url_to_process;
    std::string actual_client_upload_duration;
    bool has_client_upload_duration = net::GetValueForKeyInQuery(
        GURL(url_to_process), kClientUploadDurationQueryParameter,
        &actual_client_upload_duration);
    EXPECT_TRUE(has_client_upload_duration);
    processed_url = net::AppendOrReplaceQueryParameter(
        processed_url, kClientUploadDurationQueryParameter, std::nullopt);
    std::string actual_submission_time;
    bool has_submission_time = net::GetValueForKeyInQuery(
        GURL(url_to_process), kQuerySubmissionTimeQueryParameter,
        &actual_submission_time);
    EXPECT_TRUE(has_submission_time);
    processed_url = net::AppendOrReplaceQueryParameter(
        processed_url, kQuerySubmissionTimeQueryParameter, std::nullopt);
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
    std::string gsc_value;
    bool has_gsc_value =
        net::GetValueForKeyInQuery(url_to_process, "gsc", &gsc_value);
    EXPECT_TRUE(has_gsc_value);

    std::string hl_value;
    bool has_hl_value =
        net::GetValueForKeyInQuery(url_to_process, "hl", &hl_value);
    EXPECT_TRUE(has_hl_value);

    std::string q_value;
    bool has_q_value =
        net::GetValueForKeyInQuery(url_to_process, "q", &q_value);
    EXPECT_TRUE(has_q_value);
  }

  void CloseOverlayAndWaitForOff(LensOverlayController* controller,
                                 LensOverlayDismissalSource dismissal_source) {
    // TODO(crbug.com/404941800): This uses a roundabout way to close the UI.
    // It has to go through the LensOverlayController because the search
    // controller doesn't have proper state management. Use search controller
    // directly once it has its own state for properly determining kOff.
    LensSearchController::From(controller->GetTabInterface())
        ->CloseLensAsync(dismissal_source);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return controller->state() == State::kOff; }));
  }

  // Helper to get a test context menu on the active tab.
  std::unique_ptr<TestRenderViewContextMenu> GetContextMenu() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return TestRenderViewContextMenu::Create(web_contents,
                                             web_contents->GetURL());
  }

  policy::MockConfigurationPolicyProvider* policy_provider() {
    return &policy_provider_;
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<MockHatsService> mock_hats_service_ = nullptr;
  // The words returned by the mock objects response.
  std::vector<std::string> ocr_response_words_;
  testing::NiceMock<policy::MockConfigurationPolicyProvider> policy_provider_;

 private:
  ui::UserDataFactory::ScopedOverride lens_search_controller_override_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       PermissionBubbleAccept_ScreenshotAndCSBPrefDisabled) {
  WaitForPaint();
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Allow sharing the page screenshot but not other page content.
  PrefService* prefs = browser()->profile()->GetPrefs();
  prefs->SetBoolean(lens::prefs::kLensSharingPageScreenshotEnabled, false);
  prefs->SetBoolean(lens::prefs::kLensSharingPageContentEnabled, false);
  ASSERT_FALSE(lens::CanSharePageScreenshotWithLensOverlay(prefs));
  ASSERT_FALSE(lens::CanSharePageContentWithLensOverlay(prefs));

  // Verify attempting to show the UI will still show the permission bubble
  // with the contextual searchbox enabled.
  views::NamedWidgetShownWaiter waiter(views::test::AnyWidgetTestPasskey{},
                                       lens::kLensPermissionDialogName);
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  auto* bubble_widget = waiter.WaitIfNeededAndGet();
  // Wait for the bubble to become visible.
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();
  ASSERT_TRUE(bubble_widget->IsVisible());

  auto* search_controller = GetLensSearchController();
  ASSERT_TRUE(
      search_controller->get_lens_permission_bubble_controller_for_testing()
          ->HasOpenDialogWidget());

  // Verify attempting to show the UI again does not close the bubble widget.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  ASSERT_TRUE(bubble_widget->IsVisible());
  ASSERT_TRUE(
      search_controller->get_lens_permission_bubble_controller_for_testing()
          ->HasOpenDialogWidget());

  // Simulate click on the accept button.
  auto* bubble_widget_delegate =
      bubble_widget->widget_delegate()->AsBubbleDialogDelegate();
  ClickBubbleDialogButton(bubble_widget_delegate,
                          bubble_widget_delegate->GetOkButton());
  ASSERT_FALSE(
      search_controller->get_lens_permission_bubble_controller_for_testing()
          ->HasOpenDialogWidget());

  // Verify sharing the page content and screenshot are now permitted.
  ASSERT_TRUE(lens::CanSharePageContentWithLensOverlay(prefs));
  ASSERT_TRUE(lens::CanSharePageScreenshotWithLensOverlay(prefs));

  // Verify accepting the permission bubble will eventually result in the
  // overlay state.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify screenshot was captured and stored.
  auto screenshot_bitmap = controller->initial_screenshot();
  EXPECT_FALSE(screenshot_bitmap.empty());
  screenshot_bitmap = controller->updated_screenshot();
  EXPECT_FALSE(screenshot_bitmap.empty());
}

IN_PROC_BROWSER_TEST_F(
    LensOverlayControllerBrowserTest,
    PermissionBubbleAccept_ScreenshotPrefEnabledCSBPrefDisabled) {
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  auto* bubble_widget = waiter.WaitIfNeededAndGet();
  // Wait for the bubble to become visible.
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();
  ASSERT_TRUE(bubble_widget->IsVisible());

  auto* search_controller = GetLensSearchController();
  ASSERT_TRUE(
      search_controller->get_lens_permission_bubble_controller_for_testing()
          ->HasOpenDialogWidget());

  // Verify attempting to show the UI again does not close the bubble widget.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  ASSERT_TRUE(bubble_widget->IsVisible());
  ASSERT_TRUE(
      search_controller->get_lens_permission_bubble_controller_for_testing()
          ->HasOpenDialogWidget());

  // Simulate click on the accept button.
  auto* bubble_widget_delegate =
      bubble_widget->widget_delegate()->AsBubbleDialogDelegate();
  ClickBubbleDialogButton(bubble_widget_delegate,
                          bubble_widget_delegate->GetOkButton());
  ASSERT_FALSE(
      search_controller->get_lens_permission_bubble_controller_for_testing()
          ->HasOpenDialogWidget());

  // Verify sharing the page content and screenshot are now permitted.
  ASSERT_TRUE(lens::CanSharePageContentWithLensOverlay(prefs));
  ASSERT_TRUE(lens::CanSharePageScreenshotWithLensOverlay(prefs));

  // Verify accepting the permission bubble will eventually result in the
  // overlay state.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify screenshot was captured and stored.
  auto screenshot_bitmap = controller->initial_screenshot();
  EXPECT_FALSE(screenshot_bitmap.empty());
  screenshot_bitmap = controller->updated_screenshot();
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  auto* bubble_widget = waiter.WaitIfNeededAndGet();
  // Wait for the bubble to become visible.
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();
  ASSERT_TRUE(bubble_widget->IsVisible());

  auto* search_controller = GetLensSearchController();
  ASSERT_TRUE(
      search_controller->get_lens_permission_bubble_controller_for_testing()
          ->HasOpenDialogWidget());

  // Verify attempting to show the UI again does not close the bubble widget.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  ASSERT_TRUE(bubble_widget->IsVisible());
  ASSERT_TRUE(
      search_controller->get_lens_permission_bubble_controller_for_testing()
          ->HasOpenDialogWidget());

  // Simulate click on the reject button.
  auto* bubble_widget_delegate =
      bubble_widget->widget_delegate()->AsBubbleDialogDelegate();
  ClickBubbleDialogButton(bubble_widget_delegate,
                          bubble_widget_delegate->GetCancelButton());
  ASSERT_FALSE(
      search_controller->get_lens_permission_bubble_controller_for_testing()
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kOff);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, CaptureScreenshot) {
  WaitForPaint();
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify screenshot was captured and stored.
  auto screenshot_bitmap = controller->initial_screenshot();
  EXPECT_FALSE(screenshot_bitmap.empty());
  screenshot_bitmap = controller->updated_screenshot();
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Before showing the results panel, there should be no notification sent to
  // WebUI.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_FALSE(fake_controller->fake_overlay_page_.did_notify_results_opened_);

  // Now show the side panel.
  controller->OpenSidePanelForTesting();

  // Prevent flakiness by flushing the tasks.
  fake_controller->FlushForTesting();

  EXPECT_TRUE(IsLensResultsSidePanelShowing());
  EXPECT_TRUE(fake_controller->fake_overlay_page_.did_notify_results_opened_);
}

class TestWebModalDialog : public views::DialogDelegateView {
 public:
  TestWebModalDialog() {
    SetFocusBehavior(FocusBehavior::ALWAYS);
    SetModalType(ui::mojom::ModalType::kChild);
    // Dialogs that take focus must have a name and role to pass accessibility
    // checks.
    GetViewAccessibility().SetRole(ax::mojom::Role::kDialog);
    GetViewAccessibility().SetName("Test dialog",
                                   ax::mojom::NameFrom::kAttribute);
  }

  TestWebModalDialog(const TestWebModalDialog&) = delete;
  TestWebModalDialog& operator=(const TestWebModalDialog&) = delete;

  ~TestWebModalDialog() override = default;

  views::View* GetInitiallyFocusedView() override { return this; }
};

namespace {

// Show a web modal dialog hosted by `host_contents`.
views::Widget* ShowTestWebModalDialog(content::WebContents* host_contents) {
  return constrained_window::ShowWebModalDialogViews(new TestWebModalDialog,
                                                     host_contents);
}

}  // namespace

// Regression test for crbug.com/375224885. The result side panel can open modal
// dialogs (e.g., screen-sharing permission request dialog). If lens overlay
// dies (e.g., due to tab refresh) before the side panel web view, the modal
// should close normally without crashing.
// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, SidePanelModalDialog) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Open the side panel.
  controller->OpenSidePanelForTesting();

  // Prevent flakiness by flushing the tasks.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  fake_controller->FlushForTesting();

  // Wait for open animation to progress. This is important, otherwise when we
  // close the lens overlay at a later time the side panel will be closed
  // together synchronously.
  SidePanel* side_panel = BrowserView::GetBrowserViewForBrowser(browser())
                              ->contents_height_side_panel();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return side_panel->GetAnimationValue() > 0; }));

  // Open a web modal dialog.
  views::Widget* modal_widget = ShowTestWebModalDialog(
      GetLensOverlaySidePanelCoordinator()->GetSidePanelWebContents());
  views::test::WidgetDestroyedWaiter modal_widget_destroy_waiter(modal_widget);

  // Close the lens overlay.
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kPageChanged);

  // Modal dialog should close without crashing.
  modal_widget_destroy_waiter.Wait();
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
  OpenLensOverlayWithPendingRegion(LensOverlayInvocationSource::kAppMenu,
                                   kTestRegion->Clone(), initial_bitmap);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

  // Verify region was passed to WebUI.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_EQ(kTestRegion,
            fake_controller->fake_overlay_page_.post_region_selection_);

  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  EXPECT_TRUE(fake_query_controller->last_queried_region_bytes());
  UNSAFE_TODO(EXPECT_TRUE(
      memcmp(fake_query_controller->last_queried_region_bytes()->getPixels(),
             initial_bitmap.getPixels(),
             initial_bitmap.computeByteSize()) == 0));
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes()->width(), 100);
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes()->height(), 100);
  EXPECT_EQ(fake_query_controller->last_lens_selection_type(),
            lens::INJECTED_IMAGE);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, CloseSidePanel) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);
  // Tab contents web view should be enabled.
  ASSERT_TRUE(GetWebView()->GetEnabled());

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  // Tab contents web view should be disabled.
  ASSERT_FALSE(GetWebView()->GetEnabled());

  // Grab fake controller to test if notify the overlay of being closed.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_FALSE(fake_controller->fake_overlay_page_.did_notify_overlay_closing_);

  // Open the side panel.
  controller->OpenSidePanelForTesting();

  // Ensure the side panel is showing.
  EXPECT_TRUE(IsLensResultsSidePanelShowing());
  // Tab contents web view should be disabled.
  ASSERT_FALSE(GetWebView()->GetEnabled());

  // Close the side panel.
  browser()->GetFeatures().side_panel_ui()->Close(
      GetLensOverlaySidePanelCoordinator()->GetPanelType());

  // Ensure the overlay closes too.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
  // Tab contents web view should be enabled.
  ASSERT_TRUE(GetWebView()->GetEnabled());

  // The overlay should have been notified of the closing.
  EXPECT_TRUE(fake_controller->fake_overlay_page_.did_notify_overlay_closing_);
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

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

// TODO(crbug.com/335801964): Test flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SetShowSidePanelSearchboxThumbnail \
  DISABLED_SetShowSidePanelSearchboxThumbnail
#else
#define MAYBE_SetShowSidePanelSearchboxThumbnail \
  SetShowSidePanelSearchboxThumbnail
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       MAYBE_SetShowSidePanelSearchboxThumbnail) {
  EXPECT_CALL(*mock_hats_service_, LaunchDelayedSurveyForWebContents(
                                       kHatsSurveyTriggerLensOverlayResults, _,
                                       _, _, _, _, _, _, _, _));
  WaitForPaint();

  std::string text_query = "Apples";
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

  // Verify that the side panel searchbox displays a thumbnail and that the
  // controller has a copy.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return true == content::EvalJs(
                       controller->GetSidePanelWebContentsForTesting(),
                       content::JsReplace(kCheckSidePanelThumbnailShownScript));
  }));
  EXPECT_TRUE(base::StartsWith(controller->GetThumbnailForTesting(), "data:"));

  GetLensSearchController()
      ->lens_searchbox_controller()
      ->SetShowSidePanelSearchboxThumbnail(false);

  // Verify that the thumbnail is hidden.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return false ==
           content::EvalJs(
               controller->GetSidePanelWebContentsForTesting(),
               content::JsReplace(kCheckSidePanelThumbnailShownScript));
  }));

  GetLensSearchController()
      ->lens_searchbox_controller()
      ->SetShowSidePanelSearchboxThumbnail(true);

  // Verify that the thumbnail is shown again.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return true == content::EvalJs(
                       controller->GetSidePanelWebContentsForTesting(),
                       content::JsReplace(kCheckSidePanelThumbnailShownScript));
  }));
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  controller->IssueTextSelectionRequestForTesting(text_query,
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  auto search_query = GetLoadedSearchQuery();
  EXPECT_TRUE(search_query);
  EXPECT_EQ(search_query->search_query_text_, text_query);
  EXPECT_EQ(search_query->lens_selection_type_, lens::SELECT_TEXT_HIGHLIGHT);

  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  EXPECT_EQ(fake_query_controller->last_queried_text(), text_query);
  EXPECT_EQ(fake_query_controller->last_lens_selection_type(),
            lens::SELECT_TEXT_HIGHLIGHT);

  // Verify that the side panel displays our query.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return true ==
           content::EvalJs(controller->GetSidePanelWebContentsForTesting(),
                           content::JsReplace(
                               kCheckSidePanelResultsLoadedScript, text_query));
  }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SelectionTypeForTranslateTextSelection) {
  WaitForPaint();

  std::string text_query = "Apples";
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  controller->IssueTextSelectionRequestForTesting(text_query,
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0,
                                                  /*is_translate=*/true);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  auto search_query = GetLoadedSearchQuery();
  EXPECT_TRUE(search_query);
  EXPECT_EQ(search_query->search_query_text_, text_query);
  EXPECT_EQ(search_query->lens_selection_type_, lens::SELECT_TRANSLATED_TEXT);

  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  EXPECT_EQ(fake_query_controller->last_queried_text(), text_query);
  EXPECT_EQ(fake_query_controller->last_lens_selection_type(),
            lens::SELECT_TRANSLATED_TEXT);
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  controller->IssueTranslateSelectionRequestForTesting(
      text_query, content_language,
      /*selection_start_index=*/0,
      /*selection_end_index=*/0);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  auto search_query = GetLoadedSearchQuery();
  EXPECT_TRUE(search_query);
  EXPECT_EQ(search_query->search_query_text_, text_query);
  EXPECT_EQ(search_query->lens_selection_type_, lens::TRANSLATE_CHIP);

  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  EXPECT_EQ(fake_query_controller->last_queried_text(), text_query);
  EXPECT_EQ(fake_query_controller->last_lens_selection_type(),
            lens::TRANSLATE_CHIP);

  // Verify that the side panel displays our query.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return true ==
           content::EvalJs(
               controller->GetSidePanelWebContentsForTesting(),
               content::JsReplace(kCheckSidePanelTranslateResultsLoadedScript,
                                  text_query));
  }));
}

// TODO(crbug.com/335028577): Test flaky on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_ShowSidePanelAfterMathSelectionRequest \
  DISABLED_ShowSidePanelAfterMathSelectionRequest
#else
#define MAYBE_ShowSidePanelAfterMathSelectionRequest \
  ShowSidePanelAfterMathSelectionRequest
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       MAYBE_ShowSidePanelAfterMathSelectionRequest) {
  EXPECT_CALL(*mock_hats_service_, LaunchDelayedSurveyForWebContents(
                                       kHatsSurveyTriggerLensOverlayResults, _,
                                       _, _, _, _, _, _, _, _));
  WaitForPaint();

  std::string query = "query";
  std::string formula = "\\frac{x + 2}{4} = 4";
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  controller->IssueMathSelectionRequestForTesting(query, formula,
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  auto search_query = GetLoadedSearchQuery();
  EXPECT_TRUE(search_query);
  EXPECT_EQ(search_query->search_query_text_, query);
  EXPECT_EQ(search_query->lens_selection_type_, lens::SYMBOLIC_MATH_OBJECT);

  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  EXPECT_EQ(fake_query_controller->last_queried_text(), query);
  EXPECT_EQ(fake_query_controller->last_lens_selection_type(),
            lens::SYMBOLIC_MATH_OBJECT);

  // Verify that the side panel displays our query.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return true == content::EvalJs(
                       controller->GetSidePanelWebContentsForTesting(),
                       content::JsReplace(
                           kCheckSidePanelTranslateResultsLoadedScript, query));
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  fake_query_controller->set_fake_objects_response(
      CreateTestObjectsResponse(/*is_translate=*/true));
  controller->IssueTranslateFullPageRequestForTesting(source, target);

  // Prevent flakiness by flushing the tasks.
  fake_controller->FlushForTesting();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !fake_controller->fake_overlay_page_.last_received_text_.is_null() &&
           target ==
               fake_controller->fake_overlay_page_.last_received_text_.get()
                   ->content_language.value();
  }));

  // Expect the Lens Overlay results panel to remain closed.
  EXPECT_FALSE(IsLensResultsSidePanelShowing());

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

  // Now disable translate mode.
  fake_query_controller->set_fake_objects_response(
      CreateTestObjectsResponse(/*is_translate=*/false));
  controller->IssueEndTranslateModeRequestForTesting();

  // Prevent flakiness by flushing the tasks.
  fake_controller->FlushForTesting();
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !fake_controller->fake_overlay_page_.last_received_text_.is_null() &&
           kTestText->content_language.value() ==
               fake_controller->fake_overlay_page_.last_received_text_.get()
                   ->content_language.value();
  }));

  auto* non_translate_object =
      fake_controller->fake_overlay_page_.last_received_objects_[0].get();
  auto* non_translated_text =
      fake_controller->fake_overlay_page_.last_received_text_.get();
  EXPECT_TRUE(non_translate_object);
  EXPECT_TRUE(non_translated_text);
  EXPECT_TRUE(kTestOverlayObject->Equals(*non_translate_object));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       IssueTranslateFullPageRequestWithSelectedRegion) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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
      "&lns_mode=un&cs=0&gsc=2&hl=en-US");
  controller->IssueLensRegionRequestForTesting(kTestRegion->Clone(),
                                               /*is_click=*/false);
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Prevent flakiness by flushing the tasks.
  fake_controller->FlushForTesting();

  // After flushing the mojo calls, the data should be present.
  EXPECT_FALSE(
      fake_controller->fake_overlay_page_.last_received_objects_.empty());

  // Only objects should have been sent to the overlay from the full image
  // response.
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
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // There should be no histograms logged.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/0);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Set the full image request to return an error via the search controller.
  auto* fake_controller =
      static_cast<LensSearchControllerFake*>(GetLensSearchController());
  ASSERT_TRUE(fake_controller);
  fake_controller->SetFullImageRequestShouldReturnError();

  // Showing UI should change the state to screenshot and eventually to overlay.
  // When the overlay is bound, it should start the query flow which returns a
  // response for the full image callback.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Verify the error page histogram was not recorded since the result panel is
  // not open.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/0);

  // Side panel is not showing at first.
  EXPECT_FALSE(IsSidePanelOpen());
  EXPECT_FALSE(controller->GetSidePanelWebContentsForTesting());

  // Issuing a request should show the side panel even if navigation is expected
  // to fail.
  controller->IssueTextSelectionRequestForTesting("test query",
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Expect the Lens Overlay results panel to open.
  ASSERT_TRUE(IsLensResultsSidePanelShowing());

  // The recorded histogram should be the start query error.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.SidePanelResultStatus",
      lens::SidePanelResultStatus::kErrorPageShownStartQueryError,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       HandleStartQueryResponseError_Offline) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // There should be no histograms logged.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/0);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Set the full image request to return an error via the search controller.
  auto* fake_controller =
      static_cast<LensSearchControllerFake*>(GetLensSearchController());
  fake_controller->SetFullImageRequestShouldReturnError();

  // Showing UI should change the state to screenshot and eventually to overlay.
  // When the overlay is bound, it should start the query flow which returns a
  // response for the full image callback.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Verify the error page histogram was not recorded since the result panel is
  // not open.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/0);

  // Set the network connection type to being offline.
  auto scoped_mock_network_change_notifier =
      std::make_unique<net::test::ScopedMockNetworkChangeNotifier>();
  scoped_mock_network_change_notifier->mock_network_change_notifier()
      ->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  // Side panel is not showing at first.
  EXPECT_FALSE(IsSidePanelOpen());
  EXPECT_FALSE(controller->GetSidePanelWebContentsForTesting());

  // Issuing a request should show the side panel even if navigation is expected
  // to fail.
  controller->IssueTextSelectionRequestForTesting("test query",
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Expect the Lens Overlay results panel to open.
  ASSERT_TRUE(IsLensResultsSidePanelShowing());

  // The recorded histogram should still be the start query error rather than
  // the network being offline.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.SidePanelResultStatus",
      lens::SidePanelResultStatus::kErrorPageShownStartQueryError,
      /*expected_count=*/1);
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // An after an interaction, the image suggest signals should be set.
  controller->IssueLensRegionRequestForTesting(kTestRegion->Clone(),
                                               /*is_click=*/false);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

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
  ASSERT_TRUE(GetWebView()->GetEnabled());

  // Grab the index of the currently active tab so we can return to it later.
  int active_controller_tab_index =
      browser()->tab_strip_model()->active_index();

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());
  EXPECT_TRUE(controller->GetOverlayWebViewForTesting()->GetVisible());
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->Contains(
      controller->GetOverlayWebViewForTesting()));
  // Tab contents web view should be disabled.
  ASSERT_FALSE(GetWebView()->GetEnabled());

  // Open a side panel to test that the side panel persists between tab
  // switches.
  controller->IssueTextSelectionRequestForTesting("test query",
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

  // Opening a new tab should background the overlay UI.
  WaitForPaint(kDocumentWithNamedElement,
               WindowOpenDisposition::NEW_FOREGROUND_TAB,
               ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
                   ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kBackground; }));
  // Overlay view should never be invisible since it is used across tabs.
  EXPECT_FALSE(controller->GetOverlayViewForTesting()->GetVisible());
  EXPECT_FALSE(controller->GetOverlayWebViewForTesting()->GetVisible());
  EXPECT_TRUE(base::test::RunUntil([&]() { return !IsSidePanelOpen(); }));
  // Tab contents web view should be enabled.
  ASSERT_TRUE(GetWebView()->GetEnabled());

  // Returning back to the previous tab should show the overlay UI again.
  browser()->tab_strip_model()->ActivateTabAt(active_controller_tab_index);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());
  EXPECT_TRUE(controller->GetOverlayWebViewForTesting()->GetVisible());
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->Contains(
      controller->GetOverlayWebViewForTesting()));
  // Side panel should come back when returning to previous tab.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  // Tab contents web view should be disabled.
  ASSERT_FALSE(GetWebView()->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       BackgroundAndForegroundUISidePanelOnly) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  auto* search_controller =
      static_cast<LensSearchControllerFake*>(GetLensSearchController());
  ASSERT_EQ(controller->state(), State::kOff);
  // Tab contents web view should be enabled.
  ASSERT_TRUE(GetWebView()->GetEnabled());

  // Grab the index of the currently active tab so we can return to it later.
  int active_controller_tab_index =
      browser()->tab_strip_model()->active_index();

  // Issue a text search request to open the side panel without the overlay.
  search_controller->IssueTextSearchRequest(
      LensOverlayInvocationSource::kContentAreaContextMenuText, "query", {},
      AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      /*suppress_contextualization=*/false);

  // Wait for the side panel to be visible.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

  // The lens overlay controller should be off.
  ASSERT_EQ(controller->state(), State::kOff);
  // Tab contents web view should be enabled.
  ASSERT_TRUE(GetWebView()->GetEnabled());

  // Opening a new tab should background the lens session.
  WaitForPaint(kDocumentWithNamedElement,
               WindowOpenDisposition::NEW_FOREGROUND_TAB,
               ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
                   ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  // Overlay controller state should remain off.
  ASSERT_EQ(controller->state(), State::kOff);

  // Side panel should not be showing
  EXPECT_TRUE(base::test::RunUntil([&]() { return !IsSidePanelOpen(); }));
  // Tab contents web view should be enabled.
  ASSERT_TRUE(GetWebView()->GetEnabled());

  // Returning back to the previous tab should restore the side panel.
  browser()->tab_strip_model()->ActivateTabAt(active_controller_tab_index);

  // Overlay should still be off.
  ASSERT_EQ(controller->state(), State::kOff);
  // Side panel should come back when returning to previous tab.
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  // Tab contents web view should be enabled.
  ASSERT_TRUE(GetWebView()->GetEnabled());
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       LoadURLInResultsFrame) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Side panel is not showing at first.
  EXPECT_FALSE(IsSidePanelOpen());

  // Open the side panel.
  controller->OpenSidePanelForTesting();

  // Loading a url in the side panel should show the results page.
  const GURL search_url("https://www.google.com/search");
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(
      search_url);

  // Expect the Lens Overlay results panel to open.
  ASSERT_TRUE(IsLensResultsSidePanelShowing());
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanelResultStatusHistogram_ResultShown) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // There should be no histograms logged.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/0);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Side panel is not showing at first.
  EXPECT_FALSE(IsSidePanelOpen());
  EXPECT_FALSE(controller->GetSidePanelWebContentsForTesting());

  // Open the side panel.
  controller->OpenSidePanelForTesting();

  // Loading a url in the side panel should show the side panel even if we
  // expect the navigation to fail.
  const GURL search_url("https://www.google.com/search");
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(
      search_url);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Expect the Lens Overlay results panel to open.
  ASSERT_TRUE(IsLensResultsSidePanelShowing());

  // Verify the histogram was set correctly to `kResultShown`.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Lens.Overlay.SidePanelResultStatus",
                                     lens::SidePanelResultStatus::kResultShown,
                                     /*expected_count=*/1);
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OfflineErrorPageInSidePanel) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // There should be no histograms logged.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/0);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Set the network connection type to being offline.
  auto scoped_mock_network_change_notifier =
      std::make_unique<net::test::ScopedMockNetworkChangeNotifier>();
  scoped_mock_network_change_notifier->mock_network_change_notifier()
      ->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_NONE);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Side panel is not showing at first.
  EXPECT_FALSE(IsSidePanelOpen());
  EXPECT_FALSE(controller->GetSidePanelWebContentsForTesting());

  // Issuing a request should show the side panel even if navigation is expected
  // to fail.
  controller->IssueTextSelectionRequestForTesting("test query",
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Expect the Lens Overlay results panel to open.
  ASSERT_TRUE(IsLensResultsSidePanelShowing());

  // Verify the error page was set correctly.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.SidePanelResultStatus",
      lens::SidePanelResultStatus::kErrorPageShownOffline,
      /*expected_count=*/1);

  // Set the network connection type to being online.
  scoped_mock_network_change_notifier->mock_network_change_notifier()
      ->SetConnectionType(net::NetworkChangeNotifier::CONNECTION_WIFI);

  // Issuing a new request after the network connection is back should show the
  // results page.
  content::TestNavigationObserver observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueTextSelectionRequestForTesting("test query",
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  observer.WaitForNavigationFinished();

  // Verify the error page was set correctly. It should be hidden after a
  // successful navigation.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/2);
  histogram_tester.ExpectBucketCount("Lens.Overlay.SidePanelResultStatus",
                                     lens::SidePanelResultStatus::kResultShown,
                                     /*expected_count=*/1);
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_SameTabSameOriginLinkClick) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Open the side panel.
  controller->OpenSidePanelForTesting();

  // Loading a url in the side panel should show the results page. This needs to
  // be done to set up the WebContentsObserver.
  const GURL search_url("https://www.google.com/search");
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(
      search_url);

  // Expect the Lens Overlay results panel to open.
  EXPECT_TRUE(IsLensResultsSidePanelShowing());
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));
  int tabs = browser()->tab_strip_model()->count();

  // Verify the fake controller exists and reset any loading that was done
  // before as part of setup.
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  ASSERT_TRUE(test_side_panel_coordinator);
  test_side_panel_coordinator->ResetSidePanelTracking();

  // The results frame should be the only child frame of the side panel web
  // contents.
  content::RenderFrameHost* results_frame = content::ChildFrameAt(
      controller->GetSidePanelWebContentsForTesting()->GetPrimaryMainFrame(),
      0);
  EXPECT_TRUE(results_frame);

  // Simulate a same-origin navigation on the results frame.
  const GURL nav_url("https://www.google.com/search?q=apples");
  content::TestNavigationObserver observer(
      controller->GetSidePanelWebContentsForTesting(),
      /*expected_number_of_navigations=*/2);
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kSameTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait for the navigation to finish and the page to finish loading.
  observer.Wait();

  // It should not open a new tab as this is a same-origin navigation.
  EXPECT_EQ(tabs, browser()->tab_strip_model()->count());

  VerifySearchQueryParameters(observer.last_navigation_url());
  VerifyTextQueriesAreEqual(observer.last_navigation_url(), nav_url);

  // Verify the loading state was set correctly.
  // Loading is set to true twice because the URL is originally malformed.
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_true_, 2);
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_false_, 1);

  // We should find that the input text on the searchbox is the same as the text
  // query of the nav_url.
  EXPECT_TRUE(content::EvalJs(
                  controller->GetSidePanelWebContentsForTesting()
                      ->GetPrimaryMainFrame(),
                  content::JsReplace(kCheckSearchboxInput, "apples"),
                  content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES)
                  .ExtractBool());
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(
    LensOverlayControllerBrowserTest,
    SidePanel_UnsupportedSearchLinkClick_ShouldOpenSearchURLInNewTab) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Open the side panel.
  controller->OpenSidePanelForTesting();

  // Loading a url in the side panel should show the results page. This needs to
  // be done to set up the WebContentsObserver.
  const GURL search_url("https://www.google.com/search");
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(
      search_url);

  // Expect the Lens Overlay results panel to open.
  EXPECT_TRUE(IsLensResultsSidePanelShowing());
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));
  int tabs = browser()->tab_strip_model()->count();

  // Verify the fake controller exists and reset any loading that was done
  // before as part of setup.
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  ASSERT_TRUE(test_side_panel_coordinator);
  test_side_panel_coordinator->ResetSidePanelTracking();

  // The results frame should be the only child frame of the side panel web
  // contents.
  content::RenderFrameHost* results_frame = content::ChildFrameAt(
      controller->GetSidePanelWebContentsForTesting()->GetPrimaryMainFrame(),
      0);
  EXPECT_TRUE(results_frame);

  // Simulate a same-origin navigation that should open in a new tab on the
  // results frame.
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  const GURL nav_url("https://www.google.com/search?q=apples&udm=28");
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kSameTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Verify the new tab has the URL.
  content::WebContents* new_tab = add_tab.Wait();
  content::WaitForLoadStop(new_tab);
  EXPECT_EQ(new_tab->GetLastCommittedURL(), nav_url);
  // It should open a new tab as this is a an unsupported search URL for the
  // side panel.
  EXPECT_EQ(tabs + 1, browser()->tab_strip_model()->count());

  // Verify the loading state was not set.
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_true_, 0);
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_false_, 0);
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_SameTabCrossOriginLinkClick) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Open the side panel.
  controller->OpenSidePanelForTesting();

  // Loading a url in the side panel should show the results page. This needs to
  // be done to set up the WebContentsObserver.
  const GURL search_url("https://www.google.com/search");
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(
      search_url);

  // Expect the Lens Overlay results panel to open.
  EXPECT_TRUE(IsLensResultsSidePanelShowing());
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
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  ASSERT_TRUE(test_side_panel_coordinator);
  test_side_panel_coordinator->ResetSidePanelTracking();

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  const GURL nav_url("https://new.domain.com/");
  // Simulate a cross-origin navigation on the results frame.
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kSameTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Verify the new tab has the URL.
  content::WebContents* new_tab = add_tab.Wait();
  content::WaitForLoadStop(new_tab);
  EXPECT_EQ(new_tab->GetLastCommittedURL(), nav_url);

  // Verify the loading state was never set.
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_true_, 0);
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_false_, 0);
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_SearchURLClickWithTextDirective) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "green", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Issuing a searchbox request when the controller is in kOverlay state
  // should result in the state being kLivePageAndResults. This shouldn't
  // change the CONTEXTUAL_SEARCHBOX page classification.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);

  // Expect the Lens Overlay results panel to open.
  EXPECT_TRUE(IsLensResultsSidePanelShowing());
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
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  ASSERT_TRUE(test_side_panel_coordinator);
  test_side_panel_coordinator->ResetSidePanelTracking();

  // Simulate a same-origin navigation on the results frame.
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  const GURL nav_url("https://www.google.com/search?q=apples#:~:text=apple");
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kSameTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Verify the new tab has the URL.
  content::WebContents* new_tab = add_tab.Wait();
  content::WaitForLoadStop(new_tab);
  EXPECT_EQ(new_tab->GetLastCommittedURL(), nav_url);

  // Verify the loading state was never set.
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_true_, 0);
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_false_, 0);

  // Record the text directive result.
  histogram_tester.ExpectTotalCount("Lens.Overlay.TextDirectiveResult", 1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.TextDirectiveResult",
      lens::LensOverlayTextDirectiveResult::kOpenedInNewTab, 1);
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_LinkClickWithTextDirective_TextIsPresent) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());
  int tabs = browser()->tab_strip_model()->count();

  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "green", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Issuing a searchbox request when the controller is in kOverlay state
  // should result in the state being kLivePageAndResults. This shouldn't
  // change the CONTEXTUAL_SEARCHBOX page classification.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);

  // Expect the Lens Overlay results panel to open.
  EXPECT_TRUE(IsLensResultsSidePanelShowing());
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
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  ASSERT_TRUE(test_side_panel_coordinator);
  test_side_panel_coordinator->ResetSidePanelTracking();

  std::string relative_url =
      std::string(kDocumentWithNamedElement) + "#:~:text=select&text=element";
  const GURL nav_url = embedded_test_server()->GetURL(relative_url);

  // There should be no text highlighter manager for the main web contents at
  // this point.
  companion::TextHighlighterManager* manager =
      companion::TextHighlighterManager::GetForPage(browser()
                                                        ->tab_strip_model()
                                                        ->GetActiveTab()
                                                        ->GetContents()
                                                        ->GetPrimaryPage());
  EXPECT_FALSE(manager);

  // Simulate a cross-origin navigation on the results frame.
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kSameTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Wait for the TextHighlighterManager to be created.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    manager =
        companion::TextHighlighterManager::GetForPage(browser()
                                                          ->tab_strip_model()
                                                          ->GetActiveTab()
                                                          ->GetContents()
                                                          ->GetPrimaryPage());
    return manager;
  }));

  // It should not open a new tab as this only renders text highlights.
  EXPECT_EQ(tabs, browser()->tab_strip_model()->count());
  EXPECT_TRUE(manager);
  EXPECT_FALSE(manager->get_text_highlighters_for_testing().empty());
  for (const auto& highlighter : manager->get_text_highlighters_for_testing()) {
    EXPECT_TRUE(highlighter->GetTextDirective() == "select" ||
                highlighter->GetTextDirective() == "element");
  }
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Lens.Overlay.TextDirectiveResult",
               lens::LensOverlayTextDirectiveResult::kFoundOnPage) == 1;
  }));
  histogram_tester.ExpectTotalCount("Lens.Overlay.TextDirectiveResult", 1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.TextDirectiveResult",
      lens::LensOverlayTextDirectiveResult::kFoundOnPage, 1);
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
// TODO(crbug.com/399899383): Disabled due to flakiness on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SidePanel_LinkClickWithTextDirective_TextIsMissing \
  DISABLED_SidePanel_LinkClickWithTextDirective_TextIsMissing
#else
#define MAYBE_SidePanel_LinkClickWithTextDirective_TextIsMissing \
  SidePanel_LinkClickWithTextDirective_TextIsMissing
#endif
IN_PROC_BROWSER_TEST_F(
    LensOverlayControllerBrowserTest,
    MAYBE_SidePanel_LinkClickWithTextDirective_TextIsMissing) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "green", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Issuing a searchbox request when the controller is in kOverlay state
  // should result in the state being kLivePageAndResults. This shouldn't
  // change the CONTEXTUAL_SEARCHBOX page classification.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);

  // Expect the Lens Overlay results panel to open.
  EXPECT_TRUE(IsLensResultsSidePanelShowing());
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
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  ASSERT_TRUE(test_side_panel_coordinator);
  test_side_panel_coordinator->ResetSidePanelTracking();

  std::string relative_url =
      std::string(kDocumentWithNamedElement) + "#:~:text=javascript";
  const GURL nav_url = embedded_test_server()->GetURL(relative_url);

  // Simulate a cross-origin navigation on the results frame.
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kSameTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // There should be no text highlighter manager for the main web contents at
  // this point.
  companion::TextHighlighterManager* manager =
      companion::TextHighlighterManager::GetForPage(browser()
                                                        ->tab_strip_model()
                                                        ->GetActiveTab()
                                                        ->GetContents()
                                                        ->GetPrimaryPage());
  EXPECT_FALSE(manager);

  // Verify the new tab has the URL.
  content::WebContents* new_tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(new_tab));
  EXPECT_EQ(new_tab->GetLastCommittedURL(), nav_url);

  // Verify the loading state was never set.
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_true_, 0);
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_false_, 0);

  // Record the text directive result.
  histogram_tester.ExpectTotalCount("Lens.Overlay.TextDirectiveResult", 1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.TextDirectiveResult",
      lens::LensOverlayTextDirectiveResult::kOpenedInNewTab, 1);
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
// TODO(crbug.com/399899383): Disabled due to flakiness on Mac.
#if BUILDFLAG(IS_MAC)
#define MAYBE_SidePanel_LinkClickWithTextDirective_TextIsIncomplete \
  DISABLED_SidePanel_LinkClickWithTextDirective_TextIsIncomplete
#else
#define MAYBE_SidePanel_LinkClickWithTextDirective_TextIsIncomplete \
  SidePanel_LinkClickWithTextDirective_TextIsIncomplete
#endif
IN_PROC_BROWSER_TEST_F(
    LensOverlayControllerBrowserTest,
    MAYBE_SidePanel_LinkClickWithTextDirective_TextIsIncomplete) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "green", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Issuing a searchbox request when the controller is in kOverlay state
  // should result in the state being kLivePageAndResults. This shouldn't
  // change the CONTEXTUAL_SEARCHBOX page classification.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);

  // Expect the Lens Overlay results panel to open.
  EXPECT_TRUE(IsLensResultsSidePanelShowing());
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
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  ASSERT_TRUE(test_side_panel_coordinator);
  test_side_panel_coordinator->ResetSidePanelTracking();

  std::string relative_url = std::string(kDocumentWithNamedElement) +
                             "#:~:text=select&text=element&text=javascript";
  const GURL nav_url = embedded_test_server()->GetURL(relative_url);

  // Simulate a cross-origin navigation on the results frame.
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kSameTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // There should be no text highlighter manager for the main web contents at
  // this point.
  companion::TextHighlighterManager* manager =
      companion::TextHighlighterManager::GetForPage(browser()
                                                        ->tab_strip_model()
                                                        ->GetActiveTab()
                                                        ->GetContents()
                                                        ->GetPrimaryPage());
  EXPECT_FALSE(manager);

  // Verify the new tab has the URL.
  content::WebContents* new_tab = add_tab.Wait();
  EXPECT_TRUE(content::WaitForLoadStop(new_tab));
  EXPECT_EQ(new_tab->GetLastCommittedURL(), nav_url);

  // Verify the loading state was never set.
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_true_, 0);
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_false_, 0);

  // Record the text directive result.
  histogram_tester.ExpectTotalCount("Lens.Overlay.TextDirectiveResult", 1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.TextDirectiveResult",
      lens::LensOverlayTextDirectiveResult::kOpenedInNewTab, 1);
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_TopLevelSameOriginLinkClick) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Open the side panel.
  controller->OpenSidePanelForTesting();

  // Loading a url in the side panel should show the results page. This needs to
  // be done to set up the WebContentsObserver.
  const GURL search_url("https://www.google.com/search");
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(
      search_url);

  // Expect the Lens Overlay results panel to open.
  EXPECT_TRUE(IsLensResultsSidePanelShowing());
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));
  int tabs = browser()->tab_strip_model()->count();

  // The results frame should be the only child frame of the side panel web
  // contents.
  content::RenderFrameHost* results_frame = content::ChildFrameAt(
      controller->GetSidePanelWebContentsForTesting()->GetPrimaryMainFrame(),
      0);
  const GURL nav_url("https://www.google.com/search?q=apples");
  content::OverrideLastCommittedOrigin(results_frame,
                                       url::Origin::Create(search_url));
  EXPECT_TRUE(results_frame);

  // Verify the fake controller exists and reset any loading that was done
  // before as part of setup.
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  ASSERT_TRUE(test_side_panel_coordinator);
  test_side_panel_coordinator->ResetSidePanelTracking();

  // Simulate a top level same-origin navigation on the results frame.
  content::TestNavigationObserver observer(
      controller->GetSidePanelWebContentsForTesting());
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kTopLevelNavLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));
  observer.WaitForNavigationFinished();

  // It should not open a new tab as this is a same-origin navigation.
  EXPECT_EQ(tabs, browser()->tab_strip_model()->count());

  VerifySearchQueryParameters(observer.last_navigation_url());
  VerifyTextQueriesAreEqual(observer.last_navigation_url(), nav_url);

  // Verify the loading state was set correctly.
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_true_, 1);
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_false_, 0);

  // We should find that the input text on the searchbox is the same as the text
  // query of the nav_url.
  EXPECT_TRUE(content::EvalJs(
                  controller->GetSidePanelWebContentsForTesting()
                      ->GetPrimaryMainFrame(),
                  content::JsReplace(kCheckSearchboxInput, "apples"),
                  content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES)
                  .ExtractBool());
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_NewTabCrossOriginLinkClick) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Open the side panel.
  controller->OpenSidePanelForTesting();

  // Loading a url in the side panel should show the results page. This needs to
  // be done to set up the WebContentsObserver.
  const GURL search_url("https://www.google.com/search");
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(
      search_url);

  // Expect the Lens Overlay results panel to open.
  EXPECT_TRUE(IsLensResultsSidePanelShowing());
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // The results frame should be the only child frame of the side panel web
  // contents.
  content::RenderFrameHost* results_frame = content::ChildFrameAt(
      controller->GetSidePanelWebContentsForTesting()->GetPrimaryMainFrame(),
      0);
  const GURL nav_url("https://new.domain.com/");
  content::OverrideLastCommittedOrigin(results_frame,
                                       url::Origin::Create(search_url));
  EXPECT_TRUE(results_frame);

  // Verify the fake controller exists and reset any loading that was done
  // before as part of setup.
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  ASSERT_TRUE(test_side_panel_coordinator);
  test_side_panel_coordinator->ResetSidePanelTracking();

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
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_true_, 0);
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_false_, 0);
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_NewTabCrossOriginLinkClickFromUntrustedSite) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Open the side panel.
  controller->OpenSidePanelForTesting();

  // Loading a url in the side panel should show the results page. This needs to
  // be done to set up the WebContentsObserver.
  const GURL search_url("https://www.google.com/search");
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(
      search_url);

  // Expect the Lens Overlay results panel to open.
  EXPECT_TRUE(IsLensResultsSidePanelShowing());
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));
  int tabs = browser()->tab_strip_model()->count();

  // The results frame should be the only child frame of the side panel web
  // contents.
  content::RenderFrameHost* results_frame = content::ChildFrameAt(
      controller->GetSidePanelWebContentsForTesting()->GetPrimaryMainFrame(),
      0);
  const GURL nav_url("https://new.domain.com/");
  content::OverrideLastCommittedOrigin(results_frame,
                                       url::Origin::Create(nav_url));
  EXPECT_TRUE(results_frame);

  // Verify the fake controller exists and reset any loading that was done
  // before as part of setup.
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  ASSERT_TRUE(test_side_panel_coordinator);
  test_side_panel_coordinator->ResetSidePanelTracking();

  // Simulate a cross-origin navigation on the results frame.
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kNewTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // It should not open a new tab as the initatior origin should not be
  // considered "trusted".
  EXPECT_EQ(tabs, browser()->tab_strip_model()->count());
  // Verify the loading state was never set.
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_true_, 0);
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_false_, 0);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, CsbToHiddenState) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);

  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "green", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Issuing a searchbox request when the controller is in kOverlay state
  // should result in the state being kHidden. This shouldn't
  // change the CONTEXTUAL_SEARCHBOX page classification.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_OpenInNewTab) {
  base::HistogramTester histogram_tester;
  base::UserActionTester user_action_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Open the side panel.
  controller->OpenSidePanelForTesting();

  // Loading a url in the side panel should show the results page. This needs to
  // be done to set up the WebContentsObserver.
  const GURL search_url("https://www.google.com/search?gsc=2&vsrid=12345");
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(
      search_url);

  // Expect the Lens Overlay results panel to open.
  EXPECT_TRUE(IsLensResultsSidePanelShowing());
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Verify the fake controller exists and reset any loading that was done
  // before as part of setup.
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  ASSERT_TRUE(test_side_panel_coordinator);
  test_side_panel_coordinator->ResetSidePanelTracking();

  ui_test_utils::AllBrowserTabAddedWaiter add_tab;

  // Simulate clicking the open in new tab option.
  SimulateOpenInNewTabButtonClick();

  // Verify the new tab opens to a URL with the same path, no gsc param, and a
  // different vsrid. Other params may be changed or unchanged.
  content::WebContents* new_tab = add_tab.Wait();
  content::WaitForLoadStop(new_tab);
  EXPECT_EQ(new_tab->GetLastCommittedURL().GetPath(), search_url.GetPath());
  std::string gsc_value;
  EXPECT_FALSE(net::GetValueForKeyInQuery(new_tab->GetLastCommittedURL(),
                                          kChromeSidePanelParameterKey,
                                          &gsc_value));
  std::string original_vsrid_value;
  net::GetValueForKeyInQuery(search_url, kLensRequestQueryParameter,
                             &original_vsrid_value);
  std::string new_vsrid_value;
  net::GetValueForKeyInQuery(new_tab->GetLastCommittedURL(),
                             kLensRequestQueryParameter, &new_vsrid_value);
  EXPECT_NE(original_vsrid_value, new_vsrid_value);

  // Verify action recorded.
  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "SidePanel.LensOverlayResults.NewTabButtonClicked"));

  // Verify the loading state was never set.
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_true_, 0);
  EXPECT_EQ(test_side_panel_coordinator->side_panel_loading_set_to_false_, 0);
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanel_OpenInNewTabDisabledForContextualQueries) {
  base::UserActionTester user_action_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Verify page content was included as bytes in the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->last_sent_page_content_payload()
               .content()
               .content_data()
               .size() != 0;
  }));

  // Verify searchbox is in contextual mode.
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);

  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "hello", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Wait for URL to load in side panel.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Verify the fake controller exists and reset any loading that was done
  // before as part of setup.
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  ASSERT_TRUE(test_side_panel_coordinator);
  test_side_panel_coordinator->ResetSidePanelTracking();

  // Should do nothing.
  SimulateOpenInNewTabButtonClick();

  // Verify no action recorded.
  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "SidePanel.LensOverlayResults.NewTabButtonClicked"));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       CsbInputTypeSetsLensSelectionType) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);

  // Issue a regular searchbox request.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "green", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Wait for URL to load in side panel.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Verify the query and params are set.
  auto first_search_query = GetLoadedSearchQuery();
  EXPECT_TRUE(first_search_query);
  EXPECT_EQ(first_search_query->search_query_text_, "green");

  EXPECT_EQ(first_search_query->lens_selection_type_, lens::MULTIMODAL_SEARCH);

  // Issue a zero prefix suggest searchbox request.
  content::TestNavigationObserver second_searchbox_query_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "red", AutocompleteMatchType::Type::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/true, std::map<std::string, std::string>());

  // We can't use content::WaitForLoadStop here since the last navigation is
  // successful.
  second_searchbox_query_observer.WaitForNavigationFinished();

  // Verify the query and params are set.
  auto second_search_query = GetLoadedSearchQuery();
  EXPECT_TRUE(second_search_query);
  EXPECT_EQ(second_search_query->search_query_text_, "red");

  EXPECT_EQ(second_search_query->lens_selection_type_,
            lens::MULTIMODAL_SUGGEST_ZERO_PREFIX);

  // Issue a typeahead suggest searchbox request.
  content::TestNavigationObserver third_searchbox_query_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "blue", AutocompleteMatchType::Type::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  third_searchbox_query_observer.WaitForNavigationFinished();

  // Verify the query and params are set.
  auto third_search_query = GetLoadedSearchQuery();
  EXPECT_TRUE(third_search_query);
  EXPECT_EQ(third_search_query->search_query_text_, "blue");

  EXPECT_EQ(third_search_query->lens_selection_type_,
            lens::MULTIMODAL_SUGGEST_TYPEAHEAD);
}

// TODO THIS SHOULD NOT BE HERE
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       PopAndLoadQueryFromHistory) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Open the side panel.
  controller->OpenSidePanelForTesting();

  // Loading a url in the side panel should show the results page.
  const GURL first_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.menu&q=oranges&lns_fp=1&lns_mode=text"
      "&cs=0&gsc=2&hl=en-US");
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(
      first_search_url);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // The search query history stack should be empty and the currently loaded
  // query should be set.
  EXPECT_TRUE(GetSearchQueryHistory().empty());
  auto loaded_search_query = GetLoadedSearchQuery();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "oranges");
  VerifySearchQueryParameters(loaded_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(loaded_search_query->search_query_url_,
                            first_search_url);
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
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(
      second_search_url);
  observer.Wait();

  // The search query history stack should have 1 entry and the currently loaded
  // query should be set to the new query
  EXPECT_EQ(GetSearchQueryHistory().size(), 1UL);
  loaded_search_query = GetLoadedSearchQuery();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "kiwi");
  VerifySearchQueryParameters(loaded_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(loaded_search_query->search_query_url_,
                            second_search_url);
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
  GetLensOverlaySidePanelCoordinator()->PopAndLoadQueryFromHistory();
  pop_observer.Wait();

  // The search query history stack should be empty and the currently loaded
  // query should be set to the previous query.
  EXPECT_TRUE(GetSearchQueryHistory().empty());
  loaded_search_query = GetLoadedSearchQuery();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "oranges");
  VerifySearchQueryParameters(loaded_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(loaded_search_query->search_query_url_,
                            first_search_url);
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
                       PopAndLoadQueryFromHistoryWithRegionAndTextSelection) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Issuing a text selection request should show the results page.
  const GURL first_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.menu&vsint=CAMiCSoHb3JhbmdlcyoMCgIIBxICCAMYASAC&"
      "q=oranges"
      "&lns_fp=1&lns_mode=text&lns_surface=42&cs=0&gsc=2&hl=en-US");
  controller->IssueTextSelectionRequestForTesting("oranges", 20, 200);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // The search query history stack should be empty and the currently loaded
  // query should be set.
  EXPECT_TRUE(GetSearchQueryHistory().empty());
  auto loaded_search_query = GetLoadedSearchQuery();
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
      "&lns_mode=un&cs=0&gsc=2&hl=en-US");
  // We can't use content::WaitForLoadStop here and below since the last
  // navigation was already successful.
  content::TestNavigationObserver second_search_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueLensRegionRequestForTesting(kTestRegion->Clone(),
                                               /*is_click=*/false);
  second_search_observer.WaitForNavigationFinished();

  // The search query history stack should have 1 entry and the currently loaded
  // region should be set.
  EXPECT_EQ(GetSearchQueryHistory().size(), 1UL);
  loaded_search_query.reset();
  loaded_search_query = GetLoadedSearchQuery();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, std::string());
  EXPECT_FALSE(loaded_search_query->selected_text_);
  EXPECT_FALSE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_TRUE(loaded_search_query->selected_region_);
  EXPECT_EQ(loaded_search_query->lens_selection_type_, lens::REGION_SEARCH);

  // Loading another url in the side panel should update the results page.
  const GURL third_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.menu&vsint=CAMiBioEa2l3aSoKCgIIBxICCAMgAg&q="
      "kiwi&lns_fp=1"
      "&lns_mode=text&lns_surface=42&cs=0&gsc=2&hl=en-US");
  content::TestNavigationObserver third_search_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueTextSelectionRequestForTesting("kiwi", 1, 100);
  third_search_observer.WaitForNavigationFinished();

  // The search query history stack should have 2 entries and the currently
  // loaded query should be set to the new query
  EXPECT_EQ(GetSearchQueryHistory().size(), 2UL);
  loaded_search_query.reset();
  loaded_search_query = GetLoadedSearchQuery();
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
  content::TestNavigationObserver first_pop_observer(
      controller->GetSidePanelWebContentsForTesting());
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  fake_query_controller->ResetTestingState();
  GetLensOverlaySidePanelCoordinator()->PopAndLoadQueryFromHistory();

  // Verify the new interaction request was sent.
  EXPECT_EQ(controller->get_selected_region_for_testing(), kTestRegion);
  EXPECT_FALSE(controller->get_selected_text_for_region());
  EXPECT_EQ(fake_query_controller->last_queried_region(), kTestRegion);
  EXPECT_TRUE(fake_query_controller->last_queried_region_bytes().has_value());
  EXPECT_EQ(fake_query_controller->last_lens_selection_type(),
            lens::REGION_SEARCH);

  first_pop_observer.WaitForNavigationFinished();

  // The search query history stack should have 1 entry and the previously
  // loaded region should be present.
  EXPECT_EQ(GetSearchQueryHistory().size(), 1UL);
  loaded_search_query.reset();
  loaded_search_query = GetLoadedSearchQuery();
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
  GetLensOverlaySidePanelCoordinator()->PopAndLoadQueryFromHistory();
  second_pop_observer.WaitForNavigationFinished();

  // The search query history stack should be empty and the currently loaded
  // query should be set to the original query.
  EXPECT_TRUE(GetSearchQueryHistory().empty());
  loaded_search_query.reset();
  loaded_search_query = GetLoadedSearchQuery();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "oranges");
  url_without_start_time_or_size =
      RemoveStartTimeAndSizeParams(loaded_search_query->search_query_url_);
  VerifySearchQueryParameters(loaded_search_query->search_query_url_);
  VerifyTextQueriesAreEqual(loaded_search_query->search_query_url_,
                            first_search_url);
  EXPECT_TRUE(loaded_search_query->selected_region_thumbnail_uri_.empty());
  EXPECT_FALSE(loaded_search_query->selected_region_);
  EXPECT_TRUE(loaded_search_query->selected_text_);
  EXPECT_EQ(loaded_search_query->lens_selection_type_,
            lens::SELECT_TEXT_HIGHLIGHT);
  EXPECT_EQ(loaded_search_query->selected_text_->first, 20);
  EXPECT_EQ(loaded_search_query->selected_text_->second, 200);
  VerifySearchQueryParameters(second_pop_observer.last_navigation_url());
  VerifyTextQueriesAreEqual(second_pop_observer.last_navigation_url(),
                            first_search_url);
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
  OpenLensOverlayWithPendingRegion(
      LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), CreateNonEmptyBitmap(100, 100));
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  EXPECT_EQ(fake_query_controller->last_lens_selection_type(),
            lens::INJECTED_IMAGE);

  // Loading a url in the side panel should show the results page.
  const GURL first_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.ctxi&q=&lns_fp=1&lns_mode=un"
      "&cs=0&gsc=2&hl=en-US");

  // The search query history stack should be empty and the currently loaded
  // query should be set.
  EXPECT_TRUE(GetSearchQueryHistory().empty());
  auto loaded_search_query = GetLoadedSearchQuery();
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
      "search?source=chrome.cr.ctxi&vsint=CAMiBioEa2l3aSoKCgIIBxICCAMgAg&q="
      "kiwi&lns_fp="
      "1&lns_mode=text&lns_surface=42&cs=0&gsc=2&hl=en-US");
  content::TestNavigationObserver second_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueTextSelectionRequestForTesting("kiwi", 1, 100);
  second_observer.Wait();

  // The search query history stack should have 1 entry and the currently loaded
  // query should be set to the new query
  EXPECT_EQ(GetSearchQueryHistory().size(), 1UL);
  loaded_search_query.reset();
  loaded_search_query = GetLoadedSearchQuery();
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
  fake_query_controller->ResetTestingState();

  content::TestNavigationObserver third_observer(
      controller->GetSidePanelWebContentsForTesting());
  GetLensOverlaySidePanelCoordinator()->PopAndLoadQueryFromHistory();
  third_observer.Wait();

  // Verify the new interaction request was sent.
  EXPECT_EQ(controller->get_selected_region_for_testing(), kTestRegion);
  EXPECT_FALSE(controller->get_selected_text_for_region());
  EXPECT_EQ(fake_query_controller->last_queried_region(), kTestRegion);
  EXPECT_TRUE(fake_query_controller->last_queried_region_bytes());
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes()->width(), 100);
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes()->height(), 100);
  EXPECT_EQ(fake_query_controller->last_lens_selection_type(),
            lens::INJECTED_IMAGE);

  // The search query history stack should be empty and the currently loaded
  // query should be set to the original query.
  EXPECT_EQ(GetSearchQueryHistory().size(), 0UL);
  loaded_search_query.reset();
  loaded_search_query = GetLoadedSearchQuery();
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

// TODO(https://crbug.com/397600510): Disabled due to excessive flakiness.
IN_PROC_BROWSER_TEST_F(
    LensOverlayControllerBrowserTest,
    DISABLED_PopAndLoadQueryFromHistoryWithMultimodalRequest) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  OpenLensOverlayWithPendingRegion(
      LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return GetLoadedSearchQuery().has_value(); }));

  // Loading a url in the side panel should show the results page.
  const GURL first_search_url(
      "https://www.google.com/"
      "search?source=chrome.cr.ctxi&q=&lns_fp=1&lns_mode=un"
      "&cs=0&gsc=2&hl=en-US");

  // The search query history stack should be empty and the currently loaded
  // query should be set.
  EXPECT_TRUE(GetSearchQueryHistory().empty());
  auto loaded_search_query = GetLoadedSearchQuery();
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
      "cs=0&gsc=2&hl=en-US&q=green&lns_mode=mu&lns_fp=1&udm=24");
  // We can't use content::WaitForLoadStop here since the last navigation is
  // successful.
  content::TestNavigationObserver first_searchbox_query_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "green", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());
  first_searchbox_query_observer.Wait();

  // The search query history stack should have 1 entry and the currently loaded
  // query should be set to the new query
  EXPECT_EQ(GetSearchQueryHistory().size(), 1UL);
  loaded_search_query.reset();
  loaded_search_query = GetLoadedSearchQuery();
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
      "cs=0&gsc=2&hl=en-US&q=red&lns_mode=mu&lns_fp=1&udm=24");
  // We can't use content::WaitForLoadStop here since the last navigation is
  // successful.
  content::TestNavigationObserver second_searchbox_query_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "red", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/true, std::map<std::string, std::string>());
  second_searchbox_query_observer.Wait();

  // The search query history stack should have 2 entries and the currently
  // loaded query should be set to the new query
  EXPECT_EQ(GetSearchQueryHistory().size(), 2UL);
  loaded_search_query.reset();
  loaded_search_query = GetLoadedSearchQuery();
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
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  fake_query_controller->ResetTestingState();

  content::TestNavigationObserver pop_observer(
      controller->GetSidePanelWebContentsForTesting());
  GetLensOverlaySidePanelCoordinator()->PopAndLoadQueryFromHistory();

  // Verify the new interaction request was sent.
  EXPECT_EQ(controller->get_selected_region_for_testing(), kTestRegion);
  EXPECT_FALSE(controller->get_selected_text_for_region());
  EXPECT_EQ(fake_query_controller->last_queried_region(), kTestRegion);
  EXPECT_TRUE(fake_query_controller->last_queried_region_bytes());
  UNSAFE_TODO(EXPECT_TRUE(
      memcmp(fake_query_controller->last_queried_region_bytes()->getPixels(),
             initial_bitmap.getPixels(),
             initial_bitmap.computeByteSize()) == 0));
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes()->width(), 100);
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes()->height(), 100);
  EXPECT_EQ(fake_query_controller->last_queried_text(), "green");
  EXPECT_EQ(fake_query_controller->last_lens_selection_type(),
            lens::MULTIMODAL_SEARCH);

  pop_observer.Wait();

  // Popping the query stack again should show the initial query.
  fake_query_controller->ResetTestingState();
  GetLensOverlaySidePanelCoordinator()->PopAndLoadQueryFromHistory();

  // Verify that the last queried data did not contain any query text.
  EXPECT_EQ(controller->get_selected_region_for_testing(), kTestRegion);
  EXPECT_FALSE(controller->get_selected_text_for_region());
  EXPECT_EQ(fake_query_controller->last_queried_region(), kTestRegion);
  EXPECT_TRUE(fake_query_controller->last_queried_region_bytes());
  UNSAFE_TODO(EXPECT_TRUE(
      memcmp(fake_query_controller->last_queried_region_bytes()->getPixels(),
             initial_bitmap.getPixels(),
             initial_bitmap.computeByteSize()) == 0));
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes()->width(), 100);
  EXPECT_EQ(fake_query_controller->last_queried_region_bytes()->height(), 100);
  EXPECT_TRUE(fake_query_controller->last_queried_text().empty());
  EXPECT_EQ(fake_query_controller->last_lens_selection_type(),
            lens::INJECTED_IMAGE);
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       AddQueryToHistoryAfterResize) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Open the side panel.
  controller->OpenSidePanelForTesting();

  // Loading a url in the side panel should show the results page.
  const GURL first_search_url(
      "https://www.google.com/search?q=oranges&gsc=2&hl=en-US");
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(
      first_search_url);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Loading a second url in the side panel should show the results page.
  const GURL second_search_url(
      "https://www.google.com/search?q=kiwi&gsc=2&hl=en-US");
  // We can't use content::WaitForLoadStop here since the last navigation is
  // successful.
  content::TestNavigationObserver observer(
      controller->GetSidePanelWebContentsForTesting());
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(
      second_search_url);
  observer.WaitForNavigationFinished();

  // Make the side panel larger.
  const int increment = -50;
  BrowserView::GetBrowserViewForBrowser(browser())
      ->contents_height_side_panel()
      ->OnResize(increment, true);
  // Popping the query should load the previous query into the results frame.
  content::TestNavigationObserver pop_observer(
      controller->GetSidePanelWebContentsForTesting());
  GetLensOverlaySidePanelCoordinator()->PopAndLoadQueryFromHistory();
  pop_observer.WaitForNavigationFinished();
  // The search query history stack should be empty and the currently loaded
  // query should be set to the previous query.
  EXPECT_TRUE(GetSearchQueryHistory().empty());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       RecordHistogramsShowAndClose) {
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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
  histogram_tester.ExpectTotalCount("Lens.Overlay.ByDocumentType.Html.Invoked",
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
  test_ukm_recorder.ExpectEntryMetric(
      entries[0],
      ukm::builders::Lens_Overlay_SessionEnd::kInvocationDocumentTypeName,
      static_cast<int64_t>(lens::MimeType::kHtml));
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
  OpenLensOverlay(LensOverlayInvocationSource::kToolbar);
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);

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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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

  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  EXPECT_THAT(fake_query_controller->last_user_action(),
              testing::Optional(lens::mojom::UserAction::kRegionSelection));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OverlayClosesIfTabUrlPathChanges) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the Overlay
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OverlayClosesSidePanelBeforeOpening) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the side panel
  auto* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
  side_panel_ui->Show(SidePanelEntry::Id::kBookmarks);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kBookmarks));
  }));

  // Showing UI should eventually result in overlay state. When the overlay is
  // bound, it should start the query flow which returns a response for the
  // interaction data callback.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Side panel should now be closed.
  EXPECT_FALSE(IsSidePanelOpen());
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Grab fake controller to test if notify the overlay of being closed.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_FALSE(fake_controller->fake_overlay_page_.did_notify_overlay_closing_);

  // Open the side panel
  auto* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
  side_panel_ui->Show(SidePanelEntry::Id::kBookmarks);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kBookmarks));
  }));

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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

  // Verify the side panel is open
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

  // Open a different side panel
  browser()->GetFeatures().side_panel_ui()->Show(
      SidePanelEntry::Id::kBookmarks);

  // Overlay should close.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, SidePanelOpen) {
  WaitForPaint();

  // Wait for side panel to fully open.
  browser()->GetFeatures().side_panel_ui()->Show(
      SidePanelEntry::Id::kBookmarks);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return browser()->GetBrowserView().contents_height_side_panel()->state() ==
           SidePanel::State::kOpen;
  }));

  auto* controller = GetLensOverlayController();
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);

  // Overlay should eventually show.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // And the side-panel should be hidden.
  EXPECT_EQ(browser()->GetBrowserView().contents_height_side_panel()->state(),
            SidePanel::State::kClosed);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, FindBarClosesOverlay) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Open the find bar.
  browser()->GetFeatures().GetFindBarController()->Show();

  // Verify the overlay turns off.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, EnterprisePolicy) {
  // The default policy is to allow the feature to be enabled.
  EXPECT_TRUE(browser()
                  ->GetFeatures()
                  .lens_overlay_entry_point_controller()
                  ->IsEnabled());

  // If GenAiDefaultSettings is set, the feature enablement should
  // fallback to GenAiDefaultSettings setting.
  policy::PolicyMap policies;
  policies.Set("GenAiDefaultSettings", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(2), nullptr);
  policy_provider()->UpdateChromePolicy(policies);
  EXPECT_FALSE(browser()
                   ->GetFeatures()
                   .lens_overlay_entry_point_controller()
                   ->IsEnabled());

  policies.Set("LensOverlaySettings", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(1), nullptr);
  policy_provider()->UpdateChromePolicy(policies);
  EXPECT_FALSE(browser()
                   ->GetFeatures()
                   .lens_overlay_entry_point_controller()
                   ->IsEnabled());

  policies.Set("LensOverlaySettings", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(0), nullptr);
  policy_provider()->UpdateChromePolicy(policies);
  EXPECT_TRUE(browser()
                  ->GetFeatures()
                  .lens_overlay_entry_point_controller()
                  ->IsEnabled());
}

class LensOverlayControllerEntrypointsBrowserTest
    : public LensOverlayControllerBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  LensOverlayControllerEntrypointsBrowserTest() = default;
  ~LensOverlayControllerEntrypointsBrowserTest() override = default;

  void SetupFeatureList() override {
    std::vector<base::test::FeatureRefAndParams> enabled_features = {
        {lens::features::kLensOverlay, {}},
        {lens::features::kLensOverlayContextualSearchbox, {}},
        {lens::features::kLensOverlayOmniboxEntryPoint, {}},
        {lens::features::kLensOverlaySurvey, {}},
        {lens::features::kLensOverlaySidePanelOpenInNewTab, {}}};
    if (IsPageActionsMigrationEnabled()) {
      enabled_features.push_back(
          {::features::kPageActionsMigration,
           {
               {::features::kPageActionsMigrationLensOverlay.name, "true"},
           }});
    }
    // TODO(crbug.com/441102004): Update OverlayHidesEntrypoints to support
    //   kAiModeOmniboxEntryPoint.
    feature_list_.InitWithFeaturesAndParameters(
        enabled_features,
        /*disabled_features=*/{omnibox::kAiModeOmniboxEntryPoint,
                               lens::features::kLensSearchZeroStateCsb});
  }

  void VerifyEntrypoints(bool expected_visible) {
    // Verify context menu entrypoint matches expected visibility.
    EXPECT_EQ(expected_visible, GetContextMenu()->IsItemPresent(
                                    IDC_CONTENT_CONTEXT_LENS_REGION_SEARCH));

    // Verify omnibox (location bar) icon matches expected visibility.
    auto* location_bar =
        BrowserView::GetBrowserViewForBrowser(browser())->GetLocationBarView();
    location_bar->omnibox_view()->RequestFocus();
    views::View* omnibox_entrypoint;
    if (IsPageActionMigrated(PageActionIconType::kLensOverlay)) {
      omnibox_entrypoint =
          location_bar->page_action_container()->GetPageActionView(
              kActionSidePanelShowLensOverlayResults);
    } else {
      location_bar->page_action_icon_controller()->UpdateAll();
      omnibox_entrypoint =
          location_bar->page_action_icon_controller()->GetIconView(
              PageActionIconType::kLensOverlay);
    }
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return omnibox_entrypoint->GetVisible() == expected_visible;
    }));

    // Verify three dot menu entrypoint matches expected visibility.
    EXPECT_EQ(expected_visible,
              browser()->command_controller()->IsCommandEnabled(
                  IDC_CONTENT_CONTEXT_LENS_OVERLAY));

    // Verify toolbar entrypoint is always enabled and visible.
    actions::ActionItem* toolbar_entry_point =
        actions::ActionManager::Get().FindAction(
            kActionSidePanelShowLensOverlayResults,
            browser()->browser_actions()->root_action_item());
    EXPECT_TRUE(toolbar_entry_point->GetVisible());
    EXPECT_TRUE(toolbar_entry_point->GetEnabled());
  }

 private:
  bool IsPageActionsMigrationEnabled() const { return GetParam(); }
};

INSTANTIATE_TEST_SUITE_P(All,
                         LensOverlayControllerEntrypointsBrowserTest,
                         ::testing::Values(false, true),
                         [](const ::testing::TestParamInfo<bool>& info) {
                           return info.param ? "PageActionsMigrationEnabled"
                                             : "PageActionsMigrationDisabled";
                         });

IN_PROC_BROWSER_TEST_P(LensOverlayControllerEntrypointsBrowserTest,
                       OverlayHidesEntrypoints) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Verify the entrypoints are enabled.
  VerifyEntrypoints(/*expected_visible=*/true);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify the entrypoints are hidden.
  VerifyEntrypoints(/*expected_visible=*/false);

  // Grab the index of the currently active tab so we can return to it later.
  int active_controller_tab_index =
      browser()->tab_strip_model()->active_index();

  // Switch to a new tab.
  WaitForPaint(kDocumentWithNamedElement,
               WindowOpenDisposition::NEW_FOREGROUND_TAB,
               ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
                   ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kBackground; }));

  // Verify the entrypoints are visible again.
  VerifyEntrypoints(/*expected_visible=*/true);

  // Switch back to the original tab.
  browser()->tab_strip_model()->ActivateTabAt(active_controller_tab_index);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify the entrypoints are hidden again.
  VerifyEntrypoints(/*expected_visible=*/false);

  // Close the overlay.
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  // Verify the entrypoints are visible again.
  VerifyEntrypoints(/*expected_visible=*/true);
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Before showing the results panel, there should be no OnCopyCommand sent to
  // the overlay.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_FALSE(fake_controller->fake_overlay_page_.did_trigger_copy);

  // Send CTRL+C to overlay
  SimulateCtrlCKeyPress(GetOverlayWebContents());
  fake_controller->FlushForTesting();

  // Verify that OnCopyCommand was sent.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_controller->fake_overlay_page_.did_trigger_copy; }));

  // Reset did_trigger_copy.
  fake_controller->fake_overlay_page_.did_trigger_copy = false;

  // Open side panel.
  controller->IssueTextSelectionRequestForTesting("test query",
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

  // Send CTRL+C to side panel
  SimulateCtrlCKeyPress(controller->GetSidePanelWebContentsForTesting());
  fake_controller->FlushForTesting();

  // Verify that OnCopyCommand was sent.
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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
  ASSERT_TRUE(GetWebView()->GetEnabled());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OverlayClosesIfRendererNavigates) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the Overlay
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Force the renderer to navigate cross-origin to change the renderer process.
  // This was previously known to cause a crash (crbug.com/371643466).
  controller->GetOverlayWebViewForTesting()
      ->GetWebContents()
      ->GetController()
      .LoadURL(GURL(kHelloWorldDataUri), content::Referrer(),
               ui::PAGE_TRANSITION_LINK, std::string());

  // Overlay should close
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));
}

// TODO(crbug.com/422501416): Re-enable this test on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_OverlayInBackgroundClosesIfRendererExits \
  DISABLED_OverlayInBackgroundClosesIfRendererExits
#else
#define MAYBE_OverlayInBackgroundClosesIfRendererExits \
  OverlayInBackgroundClosesIfRendererExits
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       MAYBE_OverlayInBackgroundClosesIfRendererExits) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Get the underlying tab before we open a new tab.
  content::WebContents* underlying_tab_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Open the Overlay
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       CorrectAnalyticsAndRequestIdSentWithGen204s) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  auto* fake_search_controller =
      static_cast<LensSearchControllerFake*>(GetLensSearchController());
  ASSERT_TRUE(fake_search_controller);

  // Showing UI should change the state to screenshot and eventually to overlay.
  // When the overlay is bound, it should start the query flow which returns a
  // response for the full image callback.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());

  EXPECT_EQ(fake_query_controller->latency_gen_204_counter(
                lens::LensOverlayGen204Controller::LatencyType::
                    kFullPageObjectsRequestFetchLatency),
            1);

  // Objects request latency should not have an analytics id associated with it.
  EXPECT_FALSE(
      fake_query_controller->last_latency_gen204_analytics_id().has_value());

  // Objects request latency gen204 should have the same vsrid as the actual
  // request.
  EXPECT_THAT(
      fake_query_controller->sent_full_image_request_id(),
      EqualsProto(
          fake_query_controller->last_latency_gen204_request_id().value()));

  std::string encoded_sent_objects_analytics_id = base32::Base32Encode(
      base::as_byte_span(
          fake_query_controller->sent_full_image_request_id().analytics_id()),
      base32::Base32EncodePolicy::OMIT_PADDING);

  // Log a copy text user task completion event.
  controller->RecordUkmAndTaskCompletionForLensOverlayInteractionForTesting(
      lens::mojom::UserAction::kCopyText);

  // The objects request and the task completion gen204 should have the same
  // analytics id and request id.
  EXPECT_EQ(fake_query_controller->last_user_action(),
            lens::mojom::UserAction::kCopyText);
  EXPECT_TRUE(fake_query_controller->last_task_completion_gen204_analytics_id()
                  .has_value());
  EXPECT_EQ(
      fake_query_controller->last_task_completion_gen204_analytics_id().value(),
      encoded_sent_objects_analytics_id);
  EXPECT_THAT(
      fake_query_controller->last_task_completion_gen204_request_id().value(),
      EqualsProto(fake_query_controller->sent_full_image_request_id()));

  // Issue a text selection request and record the task completion.
  controller->IssueTextSelectionRequestForTesting("oranges", 20, 200);
  controller->RecordUkmAndTaskCompletionForLensOverlayInteractionForTesting(
      lens::mojom::UserAction::kTextSelection);

  // The text selection request should trigger the side panel to load new
  // search query.
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // The objects request and the task completion gen204 should have the same
  // analytics id and request id.
  EXPECT_EQ(fake_query_controller->last_user_action(),
            lens::mojom::UserAction::kTextSelection);
  EXPECT_TRUE(fake_query_controller->last_task_completion_gen204_analytics_id()
                  .has_value());
  EXPECT_EQ(
      fake_query_controller->last_task_completion_gen204_analytics_id().value(),
      encoded_sent_objects_analytics_id);

  // Issue a region search request, which should trigger an interaction request.
  // Use a navigation observer to wait for the side panel to load, since
  // WaitForLoadStop only works once.
  content::TestNavigationObserver region_search_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueLensRegionRequestForTesting(kTestRegion->Clone(),
                                               /*is_click=*/false);
  region_search_observer.Wait();

  // The interaction request should have a different analytics id than the
  // objects request.
  std::string encoded_sent_interaction_analytics_id = base32::Base32Encode(
      base::as_byte_span(
          fake_query_controller->sent_interaction_request_id().analytics_id()),
      base32::Base32EncodePolicy::OMIT_PADDING);
  EXPECT_NE(encoded_sent_interaction_analytics_id,
            encoded_sent_objects_analytics_id);

  // Issue a delayed text gleams view start event. Normally, this would be sent
  // as soon as the full image response is received, but it is possible that an
  // interaction request and search page request is sent before the full image
  // response is received, such as for the INJECTED_IMAGE case.
  controller->RecordSemanticEventForTesting(
      lens::mojom::SemanticEvent::kTextGleamsViewStart);

  // There should be a semantic action for text view start, with a different
  // request id than the objects or interaction request, as it instead
  // corresponds to the search request.
  EXPECT_EQ(fake_query_controller->last_semantic_event().value(),
            lens::mojom::SemanticEvent::kTextGleamsViewStart);
  EXPECT_THAT(
      fake_query_controller->last_semantic_event_gen204_request_id().value(),
      testing::Not(
          EqualsProto(fake_query_controller->sent_full_image_request_id())));
  EXPECT_THAT(
      fake_query_controller->last_semantic_event_gen204_request_id().value(),
      testing::Not(
          EqualsProto(fake_query_controller->sent_interaction_request_id())));

  std::string search_url_vsrid;
  EXPECT_TRUE(net::GetValueForKeyInQuery(
      GURL(fake_search_controller->GetLastSearchUrl()),
      kLensRequestQueryParameter, &search_url_vsrid));
  EXPECT_EQ(EncodeRequestId(
                fake_query_controller->last_semantic_event_gen204_request_id()
                    .value()),
            search_url_vsrid);

  // The interaction request and latency gen204 should have the same analytics
  // and request id.
  EXPECT_EQ(fake_query_controller->latency_gen_204_counter(
                lens::LensOverlayGen204Controller::LatencyType::
                    kInteractionRequestFetchLatency),
            1);
  EXPECT_EQ(encoded_sent_interaction_analytics_id,
            fake_query_controller->last_latency_gen204_analytics_id().value());
  EXPECT_THAT(
      fake_query_controller->sent_interaction_request_id(),
      EqualsProto(
          fake_query_controller->last_latency_gen204_request_id().value()));

  // Log a copy text user task completion event.
  controller->RecordUkmAndTaskCompletionForLensOverlayInteractionForTesting(
      lens::mojom::UserAction::kCopyText);

  // The encoded vsrid in the task completion gen204 should match that of
  // the search request.
  EXPECT_EQ(EncodeRequestId(
                fake_query_controller->last_semantic_event_gen204_request_id()
                    .value()),
            search_url_vsrid);

  // Issue a text selection request and record the task completion.
  content::TestNavigationObserver text_selection_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueTextSelectionRequestForTesting("oranges", 20, 200);
  controller->RecordUkmAndTaskCompletionForLensOverlayInteractionForTesting(
      lens::mojom::UserAction::kTextSelection);

  // The text selection request should trigger the side panel to load new
  // search query.
  text_selection_observer.Wait();

  // The encoded vsrid in the task completion gen204 should match that of
  // the search request.
  EXPECT_EQ(fake_query_controller->last_user_action(),
            lens::mojom::UserAction::kTextSelection);
  EXPECT_TRUE(fake_query_controller->last_task_completion_gen204_analytics_id()
                  .has_value());
  EXPECT_EQ(
      fake_query_controller->last_task_completion_gen204_analytics_id().value(),
      encoded_sent_interaction_analytics_id);
  EXPECT_THAT(
      fake_query_controller->sent_interaction_request_id(),
      testing::Not(EqualsProto(
          fake_query_controller->last_task_completion_gen204_request_id()
              .value())));
  EXPECT_EQ(EncodeRequestId(
                fake_query_controller->last_semantic_event_gen204_request_id()
                    .value()),
            search_url_vsrid);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OnScrollToMessage_NonPDF) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Set up the test extension event observer to monitor is the extension event
  // is sent correctly.
  extensions::TestEventRouterObserver observer(
      extensions::EventRouter::Get(browser()->profile()));

  // Call OnScrollToMessage.
  std::vector<std::string> text_fragments = {"text1", "text2"};
  uint32_t page_number = 3;
  int tabs = browser()->tab_strip_model()->count();
  GetLensOverlaySidePanelCoordinator()->SetLatestPageUrlWithResponse(
      GURL("file:///test.pdf"));
  GetLensOverlaySidePanelCoordinator()->OnScrollToMessage(text_fragments,
                                                          page_number);

  // Expect a new tab to be opened.
  EXPECT_EQ(tabs + 1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0u, observer.dispatched_events().size());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       FeedbackRequestedOpensFeedbackUI) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Open the side panel.
  controller->OpenSidePanelForTesting();
  ASSERT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Get the coordinator.
  auto* coordinator = GetLensOverlaySidePanelCoordinator();
  ASSERT_TRUE(coordinator);

  base::HistogramTester histogram_tester;

  ASSERT_FALSE(FeedbackDialog::GetInstanceForTest());
  coordinator->RequestSendFeedback();

// ChromeOS opens its own feedback dialog.
#if !BUILDFLAG(IS_CHROMEOS)
  // Wait for the feedback dialog to appear instead of a new tab.
  ASSERT_TRUE(base::test::RunUntil(
      []() { return FeedbackDialog::GetInstanceForTest() != nullptr; }));
#endif  // !BUILDFLAG(IS_CHROMEOS)

  histogram_tester.ExpectTotalCount("Feedback.RequestSource", 1);
}

class LensOverlayControllerBrowserFullscreenDisabled
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlay,
          {
              {"enable-in-fullscreen", "false"},
          }}},
        {{lens::features::kLensSearchZeroStateCsb}});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserFullscreenDisabled,
                       FullscreenClosesOverlay) {
  WaitForPaint();
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Enter into fullscreen mode.
  FullscreenController* fullscreen_controller = browser()
                                                    ->GetFeatures()
                                                    .exclusive_access_manager()
                                                    ->fullscreen_controller();
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
  FullscreenController* fullscreen_controller = browser()
                                                    ->GetFeatures()
                                                    .exclusive_access_manager()
                                                    ->fullscreen_controller();
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
    lens_search_controller_override_ = UseFakeLensSearchController();
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

  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    auto disabled = PDFExtensionTestBase::GetDisabledFeatures();
    disabled.emplace_back(lens::features::kLensOverlayContextualSearchbox);
    disabled.emplace_back(lens::features::kLensOverlayKeyboardSelection);
    disabled.emplace_back(lens::features::kLensSearchZeroStateCsb);
    return disabled;
  }

  LensSearchController* GetLensSearchController() {
    return LensSearchController::From(browser()->GetActiveTabInterface());
  }

  lens::LensOverlayQueryController* GetLensOverlayQueryController() {
    return GetLensSearchController()->lens_overlay_query_controller();
  }

  lens::LensOverlaySidePanelCoordinator* GetLensOverlaySidePanelCoordinator() {
    return GetLensSearchController()->lens_overlay_side_panel_coordinator();
  }

  LensOverlayController* GetLensOverlayController() {
    return browser()
        ->tab_strip_model()
        ->GetActiveTab()
        ->GetTabFeatures()
        ->lens_overlay_controller();
  }

  void OpenLensOverlay(LensOverlayInvocationSource invocation_source) {
    GetLensSearchController()->OpenLensOverlay(invocation_source);
  }

  void CloseOverlayAndWaitForOff(LensOverlayController* controller,
                                 LensOverlayDismissalSource dismissal_source) {
    // TODO(crbug.com/404941800): This uses a roundabout way to close the UI.
    // It has to go through the LensOverlayController because the search
    // controller doesn't have proper state management. Use search controller
    // directly once it has its own state for properly determining kOff.
    LensSearchController::From(controller->GetTabInterface())
        ->CloseLensAsync(dismissal_source);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return controller->state() == State::kOff; }));
  }

 private:
  ui::UserDataFactory::ScopedOverride lens_search_controller_override_;
};

// Regression test for crbug.com/360710001. Asserts the overlay lens page will
// load in a tab without crashing.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       OverlayWebUILoadsInTab) {
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to the lens overlay WebUI and wait for load to finish.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUILensOverlayUntrustedURL)));
  EXPECT_TRUE(active_contents->GetWebUI()
                  ->GetController()
                  ->GetAs<lens::LensOverlayUntrustedUI>());
}

// Regression test for crbug.com/360710001. Asserts the side panel lens page
// will load in a tab without crashing.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       SidePanelWebUILoadsInTab) {
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Navigate to the lens overlay WebUI and wait for load to finish.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUILensUntrustedSidePanelURL)));
  EXPECT_TRUE(active_contents->GetWebUI()
                  ->GetController()
                  ->GetAs<lens::LensSidePanelUntrustedUI>());
}

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

// TODO(crbug.com/440876016): Re-enable this test
IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFTest,
                       DISABLED_PdfBytesExcludedInRequest) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  // Open the PDF document and wait for it to finish loading.
  const GURL url = embedded_test_server()->GetURL(kPdfDocument);
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify PDF bytes were excluded from the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  EXPECT_THAT(
      lens::Payload(),
      EqualsProto(fake_query_controller->last_sent_page_content_payload()));

  // Histograms shouldn't be recorded if CSB isn't shown in session.
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSearchBox.FocusedInSession",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.ZPS.ShownInSession",
      /*expected_count=*/0);
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_ContextualSearchbox_FocusedInSession::
          kEntryName);
  EXPECT_EQ(0u, entries.size());
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_ContextualSuggest_ZPS_ShownInSession::
          kEntryName);
  EXPECT_EQ(0u, entries.size());
}

IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFTest,
                       OnScrollToMessage_PDFNotOnMainTab) {
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  const GURL expected_file_url =
      GURL("file:///test.pdf#page=3:~:text=text1&text=text2");

  // Open the PDF document and wait for it to finish loading.
  const GURL url = embedded_test_server()->GetURL(kPdfDocument);
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Set up the test extension event observer to monitor is the extension event
  // is sent correctly.
  extensions::TestEventRouterObserver observer(
      extensions::EventRouter::Get(browser()->profile()));

  // Call OnScrollToMessage.
  std::vector<std::string> text_fragments = {"text1", "text2"};
  uint32_t page_number = 3;
  int tabs = browser()->tab_strip_model()->count();
  GetLensOverlaySidePanelCoordinator()->SetLatestPageUrlWithResponse(
      expected_file_url);
  ui_test_utils::AllBrowserTabAddedWaiter add_tab;
  GetLensOverlaySidePanelCoordinator()->OnScrollToMessage(text_fragments,
                                                          page_number);

  // Verify the new tab has the URL.
  content::WebContents* new_tab = add_tab.Wait();
  content::WaitForLoadStop(new_tab);
  EXPECT_EQ(new_tab->GetLastCommittedURL(), expected_file_url);

  // Expect one new tab to have opened.
  EXPECT_EQ(tabs + 1, browser()->tab_strip_model()->count());
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
        ASSERT_TRUE(menu->lens_region_search_controller_started_for_testing());
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
                       {{"send-page-url-for-contextualization", "true"},
                        {"characters-per-page-heuristic", "1"},
                        {"use-pdfs-as-context", "true"},
                        {"use-inner-html-as-context", "true"},
                        {"file-upload-limit-bytes",
                         base::NumberToString(kFileSizeLimitBytes)}}});
    return enabled;
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    return {lens::features::kLensSearchZeroStateCsb};
  }

 protected:
  static constexpr uint32_t kFileSizeLimitBytes = 10000;
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify PDF bytes were included in the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->last_sent_page_content_payload()
               .content()
               .content_data()
               .size() == 1;
  }));
  auto content_data = fake_query_controller->last_sent_page_content_payload()
                          .content()
                          .content_data()[0];
  ASSERT_EQ(content_data.content_type(), lens::ContentData::CONTENT_TYPE_PDF);

  // Verify the searchbox was shown.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_TRUE(fake_controller->fake_overlay_page_
                  .last_received_should_show_contextual_searchbox_);
}

IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFContextualizationTest,
                       PartialPdfIncludedInRequest) {
  // Open the PDF document and wait for it to finish loading.
  const GURL url = embedded_test_server()->GetURL(kPdfDocument);
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_TRUE(controller);
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return controller->state() == State::kOverlay; }));

  // Verify pdf content was included in the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_TRUE(base::test::RunUntil([&] {
    return fake_query_controller->last_sent_partial_content().pages_size() == 1;
  }));
  ASSERT_EQ(
      "this is some text\r\nsome more text",
      fake_query_controller->last_sent_partial_content().pages(0).text_segments(
          0));
}

IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFContextualizationTest,
                       PageUrlIncludedInRequest) {
  // Open the PDF document and wait for it to finish loading.
  const GURL url = embedded_test_server()->GetURL(kPdfDocument);
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_TRUE(controller);
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify PDF bytes were included in the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return fake_query_controller->last_sent_page_url() == url; }));
}

IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFContextualizationTest,
                       CurrentPageIncludedInRequest) {
  // Open the PDF document and wait for it to finish loading.
  const GURL url = embedded_test_server()->GetURL(kPdfDocument);
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_TRUE(controller);
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify PDF page number was included in the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_EQ(0, fake_query_controller->sent_full_image_objects_request()
                   .viewport_request_context()
                   .pdf_page_number());
}

IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFContextualizationTest,
                       PdfBytesInFollowUpRequest) {
  base::HistogramTester histogram_tester;

  const GURL url = embedded_test_server()->GetURL(kPdfDocument);
  LoadPdfGetExtensionHost(url);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Make a searchbox query.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "oranges", AutocompleteMatchType::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      /*additional_query_params=*/{});

  // Verify transitions to live page.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  // Reset mock query controller so we can verify the new request.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  fake_query_controller->ResetTestingState();

  // Change page to new PDF.
  const GURL new_url = embedded_test_server()->GetURL(kPdfDocumentWithForm);
  LoadPdfGetExtensionHost(new_url);

  // Focus the searchbox.
  controller->OnFocusChangedForTesting(true);

  // Issue a new searchbox query.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "oranges", AutocompleteMatchType::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      /*additional_query_params=*/{});

  // Verify bytes was updated.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->last_sent_page_content_payload()
               .content()
               .content_data()
               .size() != 0;
  }));
  ASSERT_EQ(lens::ContentData::CONTENT_TYPE_PDF,
            fake_query_controller->last_sent_page_content_payload()
                .content()
                .content_data()[0]
                .content_type());

  // Recording the histograms is async, so need to wait for it to be recorded
  // before continuing to prevent flakiness.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Lens.Overlay.ByPageContentType.Pdf.PageCount", 1) == 2;
  }));

  // Verify the histogram recorded the new byte size.
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ByPageContentType.Pdf.DocumentSize2",
      /*expected_count=*/2);
  // Verify the histogram two documents of one page.
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ByPageContentType.Pdf.PageCount", /*sample*/ 1,
      /*expected_count=*/2);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.Pdf."
      "TimeFromNavigationToFirstInteraction",
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFContextualizationTest,
                       LargePdfNotIncludedInRequest) {
  base::HistogramTester histogram_tester;

  // Verify the document is over the size limit.
  auto file_size = GetFileSizeForTestDataFile(kPdfDocument12KbFileName);
  ASSERT_TRUE(file_size.has_value());
  ASSERT_GT(file_size.value(), kFileSizeLimitBytes);

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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify PDF bytes were not included in the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  EXPECT_THAT(
      lens::Payload(),
      EqualsProto(fake_query_controller->last_sent_page_content_payload()));

  // Verify the searchbox was shown. The CSB should be shown for all PDFs
  // regardless of size. The server will handle what should be returned for
  // large PDFs.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_TRUE(fake_controller->fake_overlay_page_
                  .last_received_should_show_contextual_searchbox_);
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  // Verify the histogram recorded the searchbox was shown.
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.Pdf.ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ByDocumentType.Pdf.ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFContextualizationTest,
                       QueryFlowRestartsOnSearchboxFocus) {
  base::HistogramTester histogram_tester;

  const GURL url = embedded_test_server()->GetURL(kPdfDocument);
  LoadPdfGetExtensionHost(url);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Grab the fake query controller.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());

  // Verify the current number of requests is as expected. Both requests happen
  // async, so need to wait for both to be sent before continuing to prevent
  // flakiness.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->num_full_image_requests_sent() == 1 &&
           fake_query_controller->num_page_content_update_requests_sent() == 1;
  }));

  // Cause cluster info to expire.
  fake_query_controller->ResetRequestClusterInfoStateForTesting();

  // Reset the fake query controller testing state to prevent dangling the
  // stored page content bytes.
  fake_query_controller->ResetTestingState();

  // Focus the searchbox.
  controller->OnFocusChangedForTesting(true);

  // Issue a query.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "red", AutocompleteMatchType::Type::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/true, std::map<std::string, std::string>());

  // Verify transitions to live page.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  // Cause cluster info to expire again.
  fake_query_controller->ResetRequestClusterInfoStateForTesting();

  // Reset the fake query controller testing state to prevent dangling the
  // stored page content bytes.
  fake_query_controller->ResetTestingState();

  // Focus the searchbox again.
  controller->OnFocusChangedForTesting(true);

  // Verify a new full image and page content request was sent in the live page
  // state. Both requests happen async, so need to wait for both to be sent
  // before continuing to prevent flakiness.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->num_full_image_requests_sent() == 2 &&
           fake_query_controller->num_page_content_update_requests_sent() == 2;
  }));
}

// TODO(crbug.com/423881729): Flaky on Win ASAN
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_Histograms DISABLED_Histograms
#else
#define MAYBE_Histograms Histograms
#endif
IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFContextualizationTest,
                       MAYBE_Histograms) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  base::HistogramTester histogram_tester;

  const GURL url = embedded_test_server()->GetURL(kPdfDocument);
  LoadPdfGetExtensionHost(url);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  controller->OnZeroSuggestShownForTesting();

  // Simulate a zero suggest suggestion being chosen.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "red", AutocompleteMatchType::Type::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/true, std::map<std::string, std::string>());

  // Issuing a search from the overlay state can only be done through the
  // contextual searchbox and should result in a live page with results.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  // Recording the histograms is async, so need to wait for it to be recorded
  // before continuing to prevent flakiness.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Lens.Overlay.ByPageContentType.Pdf.PageCount", 1) == 1;
  }));

  histogram_tester.ExpectUniqueSample("Lens.Overlay.ByDocumentType.Pdf.Invoked",
                                      /*sample*/ true,
                                      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.Pdf.ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ByDocumentType.Pdf.ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ByPageContentType.Pdf.DocumentSize2",
      /*expected_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ByPageContentType.Pdf.PageCount", /*sample*/ 1,
      /*expected_bucket_count=*/1);

  // Verify UKM metrics were recorded.
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_ContextualSearchBox_ShownInSession::
          kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSearchBox_ShownInSession::
          kWasShownName,
      true);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSearchBox_ShownInSession::
          kPageContentTypeName,
      static_cast<int64_t>(lens::MimeType::kPdf));

  // Assert zps used in session metrics get recorded.
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.ZPS.SuggestionUsedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.ZPS.SuggestionUsedInSession", true,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.ZPS.ByPageContentType.Pdf."
      "SuggestionUsedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.ZPS.ByPageContentType.Pdf."
      "SuggestionUsedInSession",
      true, /*expected_count=*/1);
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::
          Lens_Overlay_ContextualSuggest_ZPS_SuggestionUsedInSession::
              kEntryName);
  EXPECT_EQ(1u, entries.size());
  entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::
          Lens_Overlay_ContextualSuggest_ZPS_SuggestionUsedInSession::
              kSuggestionUsedInSessionName,
      true);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::
          Lens_Overlay_ContextualSuggest_ZPS_SuggestionUsedInSession::
              kPageContentTypeName,
      static_cast<int64_t>(lens::MimeType::kPdf));

  // Assert query issued in session metrics get recorded.
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.QueryIssuedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.QueryIssuedInSession", true,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.ByPageContentType.Pdf."
      "QueryIssuedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.ByPageContentType.Pdf."
      "QueryIssuedInSession",
      true, /*expected_count=*/1);
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_ContextualSuggest_QueryIssuedInSession::
          kEntryName);
  EXPECT_EQ(1u, entries.size());
  entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSuggest_QueryIssuedInSession::
          kQueryIssuedInSessionName,
      true);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSuggest_QueryIssuedInSession::
          kPageContentTypeName,
      static_cast<int64_t>(lens::MimeType::kPdf));
}

IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFContextualizationTest,
                       RecordSearchboxFocusedInSessionNavigationHistograms) {
  base::HistogramTester histogram_tester;

  // Load a non PDF.
  const GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  WaitForPaintImpl(browser(), url);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  controller->OnFocusChangedForTesting(true);

  // Make a searchbox query.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "oranges", AutocompleteMatchType::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      /*additional_query_params=*/{});

  // Verify transitions to live page.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  fake_query_controller->ResetTestingState();

  // Change page to a PDF.
  const GURL pdf_url = embedded_test_server()->GetURL(kPdfDocumentWithForm);
  LoadPdfGetExtensionHost(pdf_url);

  // Focus the searchbox.
  controller->OnFocusChangedForTesting(true);

  // Issue a new searchbox query.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "oranges", AutocompleteMatchType::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      /*additional_query_params=*/{});

  // Verify transitions to live page.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->last_sent_page_content_payload()
               .content()
               .content_data()
               .size() == 1;
  }));
  auto content_data = fake_query_controller->last_sent_page_content_payload()
                          .content()
                          .content_data();
  ASSERT_EQ(content_data.size(), 1);
  ASSERT_EQ(content_data[0].content_type(),
            lens::ContentData::CONTENT_TYPE_PDF);

  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);
  // Even after navigation, histograms should still be recorded.
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSearchBox.FocusedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSearchBox.FocusedInSession", true,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.AnnotatedPageContent."
      "FocusedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.AnnotatedPageContent."
      "FocusedInSession",
      true, /*expected_count=*/1);
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFContextualizationTest,
                       SidePanel_SameTabCrossOriginLinkClick_PdfWithFragment) {
  const GURL pdf_url = embedded_test_server()->GetURL(kMultiPagePdf);
  LoadPdfGetExtensionHost(pdf_url);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  EXPECT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  int tab_count = browser()->tab_strip_model()->count();

  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "green", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Issuing a searchbox request when the controller is in kOverlay state
  // should result in the state being kLivePageAndResults. This shouldn't
  // change the CONTEXTUAL_SEARCHBOX page classification.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);

  // Expect the Lens Overlay results panel to open.
  EXPECT_TRUE(browser()->GetFeatures().side_panel_ui()->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kLensOverlayResults)));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Set up the test extension event observer to monitor is the extension event
  // is sent correctly.
  extensions::TestEventRouterObserver observer(
      extensions::EventRouter::Get(browser()->profile()));

  // The results frame should be the only child frame of the side panel web
  // contents.
  content::RenderFrameHost* results_frame = content::ChildFrameAt(
      controller->GetSidePanelWebContentsForTesting()->GetPrimaryMainFrame(),
      0);
  EXPECT_TRUE(results_frame);

  std::string pdf_url_with_fragment = std::string(kMultiPagePdf) + "#page=3";
  const GURL nav_url = embedded_test_server()->GetURL(pdf_url_with_fragment);

  // Simulate a cross-origin navigation on the results frame.
  EXPECT_TRUE(content::ExecJs(
      results_frame, content::JsReplace(kSameTabLinkClickScript, nav_url),
      content::EvalJsOptions::EXECUTE_SCRIPT_NO_RESOLVE_PROMISES));

  // Expect an extension event to be sent to the PDF viewer.
  observer.WaitForEventWithName(
      extensions::api::pdf_viewer_private::OnShouldUpdateViewport::kEventName);
  EXPECT_EQ(1u, observer.dispatched_events().size());
  EXPECT_EQ(tab_count, browser()->tab_strip_model()->count());
}

class LensOverlayControllerBrowserPDFUpdatedContentFieldsTest
    : public LensOverlayControllerBrowserPDFContextualizationTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    auto enabled = PDFExtensionTestBase::GetEnabledFeatures();
    enabled.push_back({lens::features::kLensOverlayContextualSearchbox,
                       {{"send-page-url-for-contextualization", "true"},
                        {"characters-per-page-heuristic", "1"},
                        {"use-pdfs-as-context", "true"},
                        {"use-inner-html-as-context", "true"},
                        {"file-upload-limit-bytes",
                         base::NumberToString(kFileSizeLimitBytes)},
                        {"use-updated-content-fields", "true"}}});
    return enabled;
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    return {lens::features::kLensSearchZeroStateCsb};
  }

 protected:
  static constexpr uint32_t kFileSizeLimitBytes = 10000;
};

IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFUpdatedContentFieldsTest,
                       PartialPdfIncludedInRequest) {
  // Open the PDF document and wait for it to finish loading.
  const GURL url = embedded_test_server()->GetURL(kPdfDocument);
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_TRUE(controller);
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return controller->state() == State::kOverlay; }));

  // Verify pdf content was included in the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_TRUE(base::test::RunUntil([&] {
    return fake_query_controller->last_sent_partial_content().pages_size() == 1;
  }));
  ASSERT_EQ(
      "this is some text\r\nsome more text",
      fake_query_controller->last_sent_partial_content().pages(0).text_segments(
          0));
}

class LensOverlayControllerBrowserPDFIncreaseLimitTest
    : public LensOverlayControllerBrowserPDFContextualizationTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures()
      const override {
    auto enabled = PDFExtensionTestBase::GetEnabledFeatures();
    enabled.push_back({lens::features::kLensOverlayContextualSearchbox,
                       {{"use-pdfs-as-context", "true"},
                        {"characters-per-page-heuristic", "1"},
                        {"use-inner-html-as-context", "true"},
                        {"pdf-text-character-limit", "50"},
                        {"file-upload-limit-bytes",
                         base::NumberToString(kFileSizeLimitBytes)}}});
    return enabled;
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() const override {
    return {lens::features::kLensSearchZeroStateCsb};
  }

 protected:
  static constexpr uint32_t kFileSizeLimitBytes = 200000;
};

IN_PROC_BROWSER_TEST_P(LensOverlayControllerBrowserPDFIncreaseLimitTest,
                       PartialPdfCharacterLimitReached_IncludedInRequest) {
  // Open the PDF document and wait for it to finish loading.
  const GURL url = embedded_test_server()->GetURL(kMultiPagePdf);
  content::RenderFrameHost* extension_host = LoadPdfGetExtensionHost(url);
  ASSERT_TRUE(extension_host);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_TRUE(controller);
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&] { return controller->state() == State::kOverlay; }));

  // Verify the first two pages were sent, excluding the last page of the PDF.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());

  ASSERT_TRUE(base::test::RunUntil([&] {
    return 2 == fake_query_controller->last_sent_partial_content().pages_size();
  }));
  ASSERT_EQ(
      "1 First Section\r\nThis is the first section.\r\n1",
      fake_query_controller->last_sent_partial_content().pages(0).text_segments(
          0));
  ASSERT_EQ(
      "1.1 First Subsection\r\nThis is the first subsection.\r\n2",
      fake_query_controller->last_sent_partial_content().pages(1).text_segments(
          0));
}

// TODO(crbug.com/40268279): Stop testing both modes after OOPIF PDF viewer
// launches.
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(LensOverlayControllerBrowserPDFTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    LensOverlayControllerBrowserPDFContextualizationTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    LensOverlayControllerBrowserPDFUpdatedContentFieldsTest);
INSTANTIATE_FEATURE_OVERRIDE_TEST_SUITE(
    LensOverlayControllerBrowserPDFIncreaseLimitTest);

// Test with --enable-pixel-output-in-tests enabled, required to actually grab
// screenshots for color extraction.
class LensOverlayControllerBrowserWithPixelsTest
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kEnablePixelOutputInTests);
    InProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetupFeatureList() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{}, /*disabled_features=*/{
            lens::features::kLensOverlayVisualSelectionUpdates,
            lens::features::kLensSearchZeroStateCsb});
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify screenshot was captured and stored.
  auto screenshot_bitmap = controller->initial_screenshot();
  EXPECT_TRUE(IsNotEmptyAndNotTransparentBlack(screenshot_bitmap));
  screenshot_bitmap = controller->updated_screenshot();
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify screenshot was captured and stored.
  auto screenshot_bitmap = controller->initial_screenshot();
  EXPECT_TRUE(IsNotEmptyAndNotTransparentBlack(screenshot_bitmap));
  screenshot_bitmap = controller->updated_screenshot();
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify screenshot was captured and stored.
  auto screenshot_bitmap = controller->initial_screenshot();
  EXPECT_TRUE(IsNotEmptyAndNotTransparentBlack(screenshot_bitmap));

  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());

  auto& boxes = fake_query_controller->last_sent_significant_region_boxes();
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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
  OpenLensOverlayWithPendingRegion(
      LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), CreateNonEmptyBitmap(100, 100));
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify inner text was incluced as bytes in the the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->last_sent_page_content_payload()
               .content()
               .content_data()
               .size() == 2;
  }));
  auto content_data = fake_query_controller->last_sent_page_content_payload()
                          .content()
                          .content_data();
  ASSERT_EQ(content_data[0].content_type(),
            lens::ContentData::CONTENT_TYPE_INNER_TEXT);

  // Verify the bytes are actually what we expect them to be.
  ASSERT_EQ("The below are non-ascii characters.\n\nこんにちは thêrē 🐶 ©",
            std::string(content_data[0].data().begin(),
                        content_data[0].data().end()));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       UpdateScreenshotOnSearchboxFocus) {
  base::HistogramTester histogram_tester;
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Make a searchbox query.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "oranges", AutocompleteMatchType::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      /*additional_query_params=*/{});

  // Verify transitions to live page.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  // Reset mock query controller so we can verify the new request.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  fake_query_controller->ResetTestingState();

  // Change page.
  WaitForPaint(kDocumentWithNamedElement);

  // Focus the searchbox.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->OnFocusChangedForTesting(/*focused=*/true);

  // Verify a new full image and page content request was sent in the live page
  // state. Both requests happen async, so need to wait for both to be sent
  // before continuing to prevent flakiness.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->num_full_image_requests_sent() == 2 &&
           fake_query_controller->num_page_content_update_requests_sent() == 2;
  }));

  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->last_sent_page_content_payload()
               .content()
               .content_data()
               .size() != 0;
  }));
  ASSERT_EQ(lens::ContentData::CONTENT_TYPE_INNER_TEXT,
            fake_query_controller->last_sent_page_content_payload()
                .content()
                .content_data()[0]
                .content_type());
  ASSERT_TRUE(fake_query_controller->sent_full_image_objects_request()
                  .has_image_data());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       WebDocumentTypeHistograms) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  base::HistogramTester histogram_tester;

  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Setup fake text in the OCR response. Included 4 words on the DOM, and 1
  // not, to make a similarity score of 0.8. Also include some random characters
  // to make sure they are ignored.
  auto* fake_controller =
      static_cast<LensSearchControllerFake*>(GetLensSearchController());
  fake_controller->SetOcrResponseWords(
      {"The.", "   below   - ", " ,are] ", "RANDOM", "\n\n\nCharacters.\n"});

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify page content was included as bytes in the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->last_sent_page_content_payload()
               .content()
               .content_data()
               .size() != 0;
  }));

  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ByDocumentType.Html.Invoked",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.AnnotatedPageContent."
      "ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ByDocumentType.Html."
      "ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ByPageContentType.PlainText.DocumentSize2",
      /*expected_count=*/1);

  // Verify UKM metrics were recorded.
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_ContextualSearchBox_ShownInSession::
          kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSearchBox_ShownInSession::
          kWasShownName,
      true);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSearchBox_ShownInSession::
          kPageContentTypeName,
      static_cast<int64_t>(lens::MimeType::kAnnotatedPageContent));

  // This histogram is async so run until it is recorded.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount("Lens.Overlay.OcrDomSimilarity",
                                           80) == 1;
  }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       RecordSearchboxFocusedInSessionHistograms) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify inner HTML was included as bytes in the the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->last_sent_page_content_payload()
               .content()
               .content_data()
               .size() == 2;
  }));

  // Simulate the searchbox being focused.
  controller->OnFocusChangedForTesting(true);

  // Close the overlay and assert that each histogram was recorded once and
  // that the searchbox was focused in the session.
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSearchBox.FocusedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSearchBox.FocusedInSession", true,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.AnnotatedPageContent."
      "FocusedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.AnnotatedPageContent."
      "FocusedInSession",
      true, /*expected_count=*/1);
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_ContextualSearchbox_FocusedInSession::
          kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSearchbox_FocusedInSession::
          kFocusedInSessionName,
      true);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSearchbox_FocusedInSession::
          kPageContentTypeName,
      static_cast<int64_t>(lens::MimeType::kAnnotatedPageContent));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       RecordSearchboxNotFocusedInSessionHistograms) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify inner HTML was included as bytes in the the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->last_sent_page_content_payload()
               .content()
               .content_data()
               .size() == 2;
  }));

  // Close the overlay and assert that the histogram was recorded once and
  // that the searchbox was not focused in the session.
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSearchBox.FocusedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSearchBox.FocusedInSession", false,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.AnnotatedPageContent."
      "FocusedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.AnnotatedPageContent."
      "FocusedInSession",
      false, /*expected_count=*/1);
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_ContextualSearchbox_FocusedInSession::
          kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSearchbox_FocusedInSession::
          kFocusedInSessionName,
      false);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSearchbox_FocusedInSession::
          kPageContentTypeName,
      static_cast<int64_t>(lens::MimeType::kAnnotatedPageContent));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       RecordContextualZpsSessionHistograms) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify inner HTML was included as bytes in the the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->last_sent_page_content_payload()
               .content()
               .content_data()
               .size() == 2;
  }));

  controller->OnZeroSuggestShownForTesting();

  // Simulate a zero suggest suggestion being chosen.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "red", AutocompleteMatchType::Type::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/true, std::map<std::string, std::string>());

  // Issuing a search from the overlay state can only be done through the
  // contextual searchbox and should result in a live page with results.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  // Close the overlay and assert that the histogram was recorded once and
  // that zps was shown in the session.
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  // Assert zps shown in session metrics get recorded.
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.ZPS.ShownInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.ZPS.ShownInSession", true,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.ZPS.ByPageContentType."
      "AnnotatedPageContent."
      "ShownInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.ZPS.ByPageContentType."
      "AnnotatedPageContent."
      "ShownInSession",
      true, /*expected_count=*/1);
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_ContextualSuggest_ZPS_ShownInSession::
          kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSuggest_ZPS_ShownInSession::
          kShownInSessionName,
      true);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSuggest_ZPS_ShownInSession::
          kPageContentTypeName,
      static_cast<int64_t>(lens::MimeType::kAnnotatedPageContent));

  // Assert zps used in session metrics get recorded.
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.ZPS.SuggestionUsedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.ZPS.SuggestionUsedInSession", true,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.ZPS.ByPageContentType."
      "AnnotatedPageContent."
      "SuggestionUsedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.ZPS.ByPageContentType."
      "AnnotatedPageContent."
      "SuggestionUsedInSession",
      true, /*expected_count=*/1);
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::
          Lens_Overlay_ContextualSuggest_ZPS_SuggestionUsedInSession::
              kEntryName);
  EXPECT_EQ(1u, entries.size());
  entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::
          Lens_Overlay_ContextualSuggest_ZPS_SuggestionUsedInSession::
              kSuggestionUsedInSessionName,
      true);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::
          Lens_Overlay_ContextualSuggest_ZPS_SuggestionUsedInSession::
              kPageContentTypeName,
      static_cast<int64_t>(lens::MimeType::kAnnotatedPageContent));

  // Assert query issued in session metrics get recorded.
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.QueryIssuedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.QueryIssuedInSession", true,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.ByPageContentType.AnnotatedPageContent."
      "QueryIssuedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.ByPageContentType.AnnotatedPageContent."
      "QueryIssuedInSession",
      true, /*expected_count=*/1);
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_ContextualSuggest_QueryIssuedInSession::
          kEntryName);
  EXPECT_EQ(1u, entries.size());
  entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSuggest_QueryIssuedInSession::
          kQueryIssuedInSessionName,
      true);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSuggest_QueryIssuedInSession::
          kPageContentTypeName,
      static_cast<int64_t>(lens::MimeType::kAnnotatedPageContent));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       RecordContextualZpsNotSelectedInSessionHistograms) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify inner HTML was included as bytes in the the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->last_sent_page_content_payload()
               .content()
               .content_data()
               .size() == 2;
  }));

  controller->OnZeroSuggestShownForTesting();

  // Simulate a manual typed suggestion being entered.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "red", AutocompleteMatchType::Type::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Issuing a search from the overlay state can only be done through the
  // contextual searchbox and should result in a live page with results.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  // Close the overlay and assert that the histogram was recorded once and
  // that zps was shown in the session.
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  // Assert used in session metrics get recorded.
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.ZPS.SuggestionUsedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.ZPS.SuggestionUsedInSession", false,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.ZPS.ByPageContentType."
      "AnnotatedPageContent."
      "SuggestionUsedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.ZPS.ByPageContentType."
      "AnnotatedPageContent."
      "SuggestionUsedInSession",
      false, /*expected_count=*/1);
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::
          Lens_Overlay_ContextualSuggest_ZPS_SuggestionUsedInSession::
              kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::
          Lens_Overlay_ContextualSuggest_ZPS_SuggestionUsedInSession::
              kSuggestionUsedInSessionName,
      false);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::
          Lens_Overlay_ContextualSuggest_ZPS_SuggestionUsedInSession::
              kPageContentTypeName,
      static_cast<int64_t>(lens::MimeType::kAnnotatedPageContent));

  // Assert query issued in session metrics get recorded.
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.QueryIssuedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.QueryIssuedInSession", true,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.ByPageContentType.AnnotatedPageContent."
      "QueryIssuedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.ByPageContentType.AnnotatedPageContent."
      "QueryIssuedInSession",
      true, /*expected_count=*/1);
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_ContextualSuggest_QueryIssuedInSession::
          kEntryName);
  EXPECT_EQ(1u, entries.size());
  entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSuggest_QueryIssuedInSession::
          kQueryIssuedInSessionName,
      true);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSuggest_QueryIssuedInSession::
          kPageContentTypeName,
      static_cast<int64_t>(lens::MimeType::kAnnotatedPageContent));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       RecordContextualZpsNotShownInSessionHistograms) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify inner HTML was included as bytes in the the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->last_sent_page_content_payload()
               .content()
               .content_data()
               .size() == 2;
  }));

  // Close the overlay and assert that the histogram was recorded once and
  // that zps was not shown in the session.
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.ZPS.ShownInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.ZPS.ShownInSession", false,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.ZPS.ByPageContentType."
      "AnnotatedPageContent.ShownInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.ZPS.ByPageContentType."
      "AnnotatedPageContent."
      "ShownInSession",
      false, /*expected_count=*/1);
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_ContextualSuggest_ZPS_ShownInSession::
          kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSuggest_ZPS_ShownInSession::
          kShownInSessionName,
      false);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSuggest_ZPS_ShownInSession::
          kPageContentTypeName,
      static_cast<int64_t>(lens::MimeType::kAnnotatedPageContent));

  // Assert query issued in session metrics get recorded.
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.QueryIssuedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.QueryIssuedInSession", false,
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.ByPageContentType.AnnotatedPageContent."
      "QueryIssuedInSession",
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.ByPageContentType.AnnotatedPageContent."
      "QueryIssuedInSession",
      false, /*expected_count=*/1);
  entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_ContextualSuggest_QueryIssuedInSession::
          kEntryName);
  EXPECT_EQ(1u, entries.size());
  entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSuggest_QueryIssuedInSession::
          kQueryIssuedInSessionName,
      false);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSuggest_QueryIssuedInSession::
          kPageContentTypeName,
      static_cast<int64_t>(lens::MimeType::kAnnotatedPageContent));
}

// TODO - crbug.com/400650442: Deflake and re-enable this test.
IN_PROC_BROWSER_TEST_F(
    LensOverlayControllerBrowserTest,
    DISABLED_RecordQueryIssuesBeforeZpsShownInSessionHistograms) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Show ZPS and issue a query.
  controller->OnZeroSuggestShownForTesting();
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "red", AutocompleteMatchType::Type::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  // Close the overlay.
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  // Verify the initial histogram was recorded.
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.InitialQuery."
      "QueryIssuedInSessionBeforeSuggestShown",
      false,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.InitialQuery.ByPageContentType."
      "AnnotatedPageContent."
      "QueryIssuedInSessionBeforeSuggestShown",
      false,
      /*expected_count=*/1);

  // Verify the follow up histogram was not recorded.
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.FollowUpQuery."
      "QueryIssuedInSessionBeforeSuggestShown",
      /*expected_count=*/0);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSuggest.FollowUpQuery.ByPageContentType."
      "AnnotatedPageContent.QueryIssuedInSessionBeforeSuggestShown",
      /*expected_count=*/0);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Issue a search query before ZPS is shown.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "red", AutocompleteMatchType::Type::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  // Issue a follow up after ZPS is shown.
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  int follow_up_query_issued_count =
      test_side_panel_coordinator->side_panel_loading_set_to_true_;

  controller->OnZeroSuggestShownForTesting();
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "red", AutocompleteMatchType::Type::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Run until the follow up query is issued.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return test_side_panel_coordinator->side_panel_loading_set_to_true_ ==
           follow_up_query_issued_count + 1;
  }));

  // Close the overlay.
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  // Verify the initial histogram was recorded.
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.InitialQuery."
      "QueryIssuedInSessionBeforeSuggestShown",
      true,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.InitialQuery.ByPageContentType."
      "AnnotatedPageContent."
      "QueryIssuedInSessionBeforeSuggestShown",
      true,
      /*expected_count=*/1);

  // Verify the follow up histogram was recorded.
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.FollowUpQuery."
      "QueryIssuedInSessionBeforeSuggestShown",
      false,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.FollowUpQuery.ByPageContentType."
      "AnnotatedPageContent.QueryIssuedInSessionBeforeSuggestShown",
      false,
      /*expected_count=*/1);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Issue a search query.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "red", AutocompleteMatchType::Type::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  // Re-grab the new results side panel coordinator since it was destroyed after
  // the overlay was turned off.
  test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());

  // Issue a follow up before ZPS is shown.
  follow_up_query_issued_count =
      test_side_panel_coordinator->side_panel_loading_set_to_true_;
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "red", AutocompleteMatchType::Type::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Run until the follow up query is issued.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return test_side_panel_coordinator->side_panel_loading_set_to_true_ ==
           follow_up_query_issued_count + 1;
  }));

  // Close the overlay.
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  // Verify the follow up histogram was recorded.
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.FollowUpQuery."
      "QueryIssuedInSessionBeforeSuggestShown",
      true,
      /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.ContextualSuggest.FollowUpQuery.ByPageContentType."
      "AnnotatedPageContent.QueryIssuedInSessionBeforeSuggestShown",
      true,
      /*expected_count=*/1);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       ContextualQueryInBackStackRequest) {
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Make a searchbox query.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "oranges", AutocompleteMatchType::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      /*additional_query_params=*/{});

  // Wait for the side panel to load.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->GetSidePanelWebContentsForTesting(); }));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Verify we entered the contextual searchbox flow.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  // Make another query to build the history stack.
  content::TestNavigationObserver observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "apples", AutocompleteMatchType::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      /*additional_query_params=*/{});

  // Wait for the side panel to load.
  observer.WaitForNavigationFinished();

  // Reset mock query controller so we can verify the new request.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  fake_query_controller->ResetTestingState();

  // Issue a new searchbox query.
  GetLensOverlaySidePanelCoordinator()->PopAndLoadQueryFromHistory();

  // Verify we entered the contextual searchbox flow.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->num_interaction_requests_sent() == 1;
  }));
  ASSERT_EQ(fake_query_controller->num_interaction_requests_sent(), 1);
  ASSERT_EQ(fake_query_controller->last_queried_text(), "oranges");
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       IssueTextSearchRequest_Contextualizes) {
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay by issuing a text search request without suppressing
  // contextualization.
  GetLensSearchController()->IssueTextSearchRequest(
      LensOverlayInvocationSource::kContentAreaContextMenuText, "test",
      /*additional_query_parameters=*/{},
      AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      /*suppress_contextualization=*/false);

  // Wait for the side panel to load.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->GetSidePanelWebContentsForTesting(); }));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Overlay should remain in off state.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));

  // Make another query to build the history stack.
  content::TestNavigationObserver observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "apples", AutocompleteMatchType::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      /*additional_query_params=*/{});

  // Wait for the side panel to load.
  observer.WaitForNavigationFinished();

  // Pop last query from history.
  GetLensOverlaySidePanelCoordinator()->PopAndLoadQueryFromHistory();
  observer.WaitForNavigationFinished();

  // An interaction request should have been sent for each contextualized query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->num_interaction_requests_sent() == 3;
  }));
}

// TODO(crbug.com/413042395): This test is not testing overlay logic, but
// instead the side panel logic. Therefore, this test should be moved to a side
// panel browsertest file.
// TODO(crbug.com/439622878): Test is flaky on Windows.
#if BUILDFLAG(IS_WIN)
#define MAYBE_IssueTextSearchRequest_SuppressesContextualization \
  DISABLED_IssueTextSearchRequest_SuppressesContextualization
#else
#define MAYBE_IssueTextSearchRequest_SuppressesContextualization \
  IssueTextSearchRequest_SuppressesContextualization
#endif
IN_PROC_BROWSER_TEST_F(
    LensOverlayControllerBrowserTest,
    MAYBE_IssueTextSearchRequest_SuppressesContextualization) {
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay by issuing a text search request and suppressing
  // contextualization.
  GetLensSearchController()->IssueTextSearchRequest(
      LensOverlayInvocationSource::kContentAreaContextMenuText, "test",
      /*additional_query_parameters=*/{},
      AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      /*suppress_contextualization=*/true);

  // Wait for the side panel to load.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->GetSidePanelWebContentsForTesting(); }));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Overlay should remain in off state.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));

  // Make another query to build the history stack.
  content::TestNavigationObserver observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "apples", AutocompleteMatchType::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      /*additional_query_params=*/{});

  // Wait for the side panel to load.
  observer.WaitForNavigationFinished();

  // Pop last query from history.
  GetLensOverlaySidePanelCoordinator()->PopAndLoadQueryFromHistory();
  observer.WaitForNavigationFinished();

  // No interaction requests should have been sent as contextualization was
  // suppressed.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_EQ(fake_query_controller->num_interaction_requests_sent(), 0);
}

// This test checks that there is no crash when showing lens overlay when
// screenshot is not available.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       ScreenshotUnavailable) {
  // Wait for the real page to finish painting.
  WaitForPaint();

  // Pretend like a screenshot isn't available.
  auto* fake_controller =
      static_cast<LensOverlayControllerFake*>(GetLensOverlayController());
  fake_controller->is_screenshot_possible_ = false;

  // Show the overlay. State should still be off.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(fake_controller->state(), State::kOff);

  // Background the tab.
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), /*index=*/-1,
                   /*foreground=*/true);

  // There should be two tabs. Close the first tab.
  // Regression testing for crbug.com/373767988. Ensure no crash when
  // closing a tab that wasn't screenshotable.
  ASSERT_EQ(browser()->tab_strip_model()->count(), 2);
  browser()->tab_strip_model()->DetachAndDeleteWebContentsAt(/*index=*/0);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       PageUrlIncludedInFollowUpRequest) {
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Make a searchbox query.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "oranges", AutocompleteMatchType::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      /*additional_query_params=*/{});

  // Verify transitions to live page.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  // Reset mock query controller so we can verify the new request.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  fake_query_controller->ResetTestingState();

  // Change page.
  WaitForPaint(kDocumentWithNamedElement);

  // Focus the searchbox.
  controller->OnFocusChangedForTesting(true);

  // Issue a new searchbox query.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "oranges", AutocompleteMatchType::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      /*additional_query_params=*/{});

  // Verify inner text was updated.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return !fake_query_controller->last_sent_page_url().is_empty();
  }));
  const GURL expected_url =
      embedded_test_server()->GetURL(kDocumentWithNamedElement);
  ASSERT_EQ(expected_url, fake_query_controller->last_sent_page_url());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       NoBytesInNonCsbFollowUpRequest) {
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Issue a text seleciton request to get out of CSB flow.
  controller->IssueTextSelectionRequestForTesting("testing",
                                                  /*selection_start_index=*/10,
                                                  /*selection_end_index=*/16);

  // Verify transitions to live page.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

  // Reset mock query controller so we can verify the new request.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  fake_query_controller->ResetTestingState();

  // Issue a new searchbox query.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "oranges", AutocompleteMatchType::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      /*additional_query_params=*/{});

  // Verify inner text was not updated, signified by the payload being empty.
  EXPECT_THAT(
      lens::Payload(),
      EqualsProto(fake_query_controller->last_sent_page_content_payload()));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, ProtectedPageShows) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // There should be no histograms logged.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/0);

  // Set the contextualization controller to return the page as not context
  // eligible.
  auto* fake_contextualization_controller =
      static_cast<lens::TestLensSearchContextualizationController*>(
          GetLensSearchController()
              ->lens_search_contextualization_controller());
  ASSERT_TRUE(fake_contextualization_controller);
  fake_contextualization_controller->SetContextEligible(true);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  // When the overlay is bound, it should start the query flow which returns a
  // response for the full image callback.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Verify the error page histogram was not recorded since the result panel is
  // not open.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/0);

  // Side panel is not showing at first.
  EXPECT_FALSE(IsSidePanelOpen());
  EXPECT_FALSE(controller->GetSidePanelWebContentsForTesting());

  // Issuing a request should show the side panel even if navigation is expected
  // to fail.
  controller->IssueTextSelectionRequestForTesting("test query",
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Expect the Lens Overlay results panel to open.
  EXPECT_TRUE(IsLensResultsSidePanelShowing());

  // The recorded histogram should be a normal result shown.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount("Lens.Overlay.SidePanelResultStatus",
                                     lens::SidePanelResultStatus::kResultShown,
                                     /*expected_count=*/1);
}

class LensOverlayControllerIframeBrowserTest
    : public LensOverlayControllerBrowserTest {
  void SetUp() override {
    // Register a request handler to close the socket. This should result in an
    // ERR_EMPTY_RESPONSE, which is not a special-cased error. Used for the
    // SidePanelIframeLoadOtherError test case. The `test` query parameter is
    // used to trigger this handler.
    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        [](const net::test_server::HttpRequest& request)
            -> std::unique_ptr<net::test_server::HttpResponse> {
          if (base::StartsWith(request.relative_url, kDocumentWithNamedElement,
                               base::CompareCase::SENSITIVE)) {
            std::string fail_query_param;
            net::GetValueForKeyInQuery(request.GetURL(), "fail",
                                       &fail_query_param);
            if (fail_query_param == "invalid-headers") {
              return std::make_unique<net::test_server::RawHttpResponse>(
                  "invalid-headers", "");
            }
          }
          return nullptr;
        }));

    LensOverlayControllerBrowserTest::SetUp();
  }

 protected:
  void SetupFeatureList() override {
    // Set the results search URL to the test server URL so that the iframe
    // navigations are allowed by the iframe CORS policy and the navigation
    // throttle.
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlay,
          {{"results-search-url", embedded_test_server()
                                      ->GetURL(kDocumentWithNamedElement)
                                      .spec()}}}},
        /*disabled_features=*/{lens::features::kLensSearchZeroStateCsb});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerIframeBrowserTest,
                       SidePanelIframeLoadSuccess) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Open the side panel.
  controller->OpenSidePanelForTesting();
  ASSERT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Navigate the iframe to a successful URL.
  GURL url(embedded_test_server()->GetURL(kDocumentWithNamedElement));
  content::TestNavigationObserver navigation_observer(
      controller->GetSidePanelWebContentsForTesting());
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(url);
  navigation_observer.WaitForNavigationFinished();

  // Check histogram. The enum is defined in the .cc file so we can't reference
  // it directly.
  histogram_tester.ExpectUniqueSample("Lens.Overlay.SidePanel.IframeLoadStatus",
                                      /*IframeLoadStatus::kSuccess=*/0, 1);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerIframeBrowserTest,
                       SidePanelIframeLoadConnectionRefused) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Open the side panel.
  controller->OpenSidePanelForTesting();
  ASSERT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Create a URL and then shut down the server to force a connection refused
  // error when the iframe tries to load the URL.
  GURL url = embedded_test_server()->GetURL(kDocumentWithNamedElement);
  ASSERT_TRUE(embedded_test_server()->ShutdownAndWaitUntilComplete());

  // Navigate the iframe to the connection refused URL.
  content::TestNavigationObserver navigation_observer(
      controller->GetSidePanelWebContentsForTesting());
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(url);
  navigation_observer.WaitForNavigationFinished();

  // Check histogram.
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.SidePanel.IframeLoadStatus",
      /*IframeLoadStatus::kFailedConnectionRefused=*/1, 1);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerIframeBrowserTest,
                       SidePanelIframeLoadOtherError) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Open the side panel.
  controller->OpenSidePanelForTesting();
  ASSERT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Navigate the iframe to the URL with a query parameter. This will trigger
  // the request handler that will close the socket.
  GURL url = net::AppendOrReplaceQueryParameter(
      embedded_test_server()->GetURL(kDocumentWithNamedElement), /*key=*/"fail",
      /*value=*/"invalid-headers");
  content::TestNavigationObserver navigation_observer(
      controller->GetSidePanelWebContentsForTesting());
  GetLensOverlaySidePanelCoordinator()->LoadURLInResultsFrameForTesting(url);
  navigation_observer.WaitForNavigationFinished();

  // Check histogram. The enum is defined in the .cc file so we can't reference
  // it directly.
  histogram_tester.ExpectUniqueSample("Lens.Overlay.SidePanel.IframeLoadStatus",
                                      /*IframeLoadStatus::kFailedOther=*/6, 1);
}

class LensOverlayControllerInnerTextEnabledSmallByteLimitTest
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlayContextualSearchbox,
          {
              {"use-inner-text-as-context", "true"},
              {"use-apc-as-context", "false"},
              {"file-upload-limit-bytes", "10"},
          }}},
        /*disabled_features=*/{lens::features::kLensSearchZeroStateCsb});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerInnerTextEnabledSmallByteLimitTest,
                       InnerTextOverLimitNotIncludedInRequest) {
  base::HistogramTester histogram_tester;

  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify innerText bytes were not included in the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  EXPECT_THAT(
      lens::Payload(),
      EqualsProto(fake_query_controller->last_sent_page_content_payload()));

  // Verify the searchbox was shown. The CSB should be shown even if the inner
  // text is not included in the request. The server will handle what to show in
  // the searchbox based on the page content that is received.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_TRUE(fake_controller->fake_overlay_page_
                  .last_received_should_show_contextual_searchbox_);

  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  // Verify the histogram recorded the searchbox was shown.
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.PlainText."
      "ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ByDocumentType.Html."
      "ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       PageUrlIncludedInRequest) {
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify the page url was included as bytes in the the query.
  const GURL expected_url =
      embedded_test_server()->GetURL(kDocumentWithNonAsciiCharacters);
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->last_sent_page_url() == expected_url;
  }));
}

class LensOverlayControllerApcOnlyTest
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlayContextualSearchbox,
          {
              {"use-inner-text-as-context", "false"},
              {"use-apc-as-context", "true"},
          }},
         {lens::features::kLensSearchProtectedPage, {}}},
        {lens::features::kLensSearchZeroStateCsb});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerApcOnlyTest,
                       RecordSearchboxShownInSessionHistogramsByDocumentType) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  base::HistogramTester histogram_tester;

  // Navigate to an image, and invoke then close the overlay.
  WaitForPaint(kImageFile);
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  // Navigate to a audio file, and invoke then close the overlay.
  WaitForPaint(kAudioFile);
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  // Navigate to a video file, and invoke then close the overlay.
  WaitForPaint(kVideoFile);
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  // Navigate to a JSON file, and invoke then close the overlay.
  WaitForPaint(kJsonFile);
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  // Verify histograms were recorded.
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ByDocumentType.Image.ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ByDocumentType.Video.ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ByDocumentType.Audio.ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ByDocumentType.Json.ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       RecordSearchboxFocusHistograms) {
  base::HistogramTester histogram_tester;

  // Navigate to a webpage, and invoke then close the overlay.
  WaitForPaint();
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Focus the searchbox.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->OnFocusChangedForTesting(/*focused=*/true);

  // Verify histogram was recorded.
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.AnnotatedPageContent."
      "TimeFromInvocationToFirstFocus",
      /*expected_count=*/1);

  // Focusing the searchbox again should not record another histogram.
  fake_controller->OnFocusChangedForTesting(/*focused=*/true);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.AnnotatedPageContent."
      "TimeFromInvocationToFirstFocus",
      /*expected_count=*/1);

  // Make a searchbox query to open the live page and side panel.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "oranges", AutocompleteMatchType::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      /*additional_query_params=*/{});

  // Verify transitions to live page.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  // Change page to new URL.
  WaitForPaint(kDocumentWithImage);

  // Focus the searchbox again and verify histogram was recorded.
  fake_controller->OnFocusChangedForTesting(/*focused=*/true);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.AnnotatedPageContent."
      "TimeFromNavigationToFirstFocus",
      /*expected_count=*/1);

  // Focusing the searchbox again should not record another histogram.
  fake_controller->OnFocusChangedForTesting(/*focused=*/true);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.AnnotatedPageContent."
      "TimeFromNavigationToFirstFocus",
      /*expected_count=*/1);

  // Navigate to a new page and verify histogram was recorded.
  WaitForPaint(kImageFile);
  fake_controller->OnFocusChangedForTesting(/*focused=*/true);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.AnnotatedPageContent."
      "TimeFromNavigationToFirstFocus",
      /*expected_count=*/2);
}

class LensOverlayControllerInnerTextAndApc
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlayContextualSearchbox,
          {
              {"send-page-url-for-contextualization", "true"},
              {"use-inner-text-as-context", "true"},
              {"use-apc-as-context", "true"},
              {"use-updated-content-fields", "true"},
          }},
         {lens::features::kLensSearchProtectedPage, {}}},
        {lens::features::kLensSearchZeroStateCsb});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerInnerTextAndApc,
                       AllBytesInRequest) {
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify inner HTML was included as bytes in the the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  // Run until the page content is sent.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->last_sent_page_content_payload()
               .content()
               .content_data()
               .size() == 2;
  }));

  // Expect the old content data fields to be empty.
  EXPECT_TRUE(fake_query_controller->last_sent_page_content_payload()
                  .content_data()
                  .empty());
  EXPECT_TRUE(fake_query_controller->last_sent_page_content_payload()
                  .content_type()
                  .empty());

  // There should be 2 fields of content data.
  const auto last_sent_content =
      fake_query_controller->last_sent_page_content_payload().content();
  ASSERT_EQ(last_sent_content.content_data().size(), 2);

  // Verify innerText is what we expect it to be.
  EXPECT_EQ(lens::ContentData::CONTENT_TYPE_INNER_TEXT,
            last_sent_content.content_data(0).content_type());
  const auto last_sent_text_bytes = last_sent_content.content_data(0).data();
  ASSERT_EQ(
      "The below are non-ascii characters.\n\nこんにちは thêrē 🐶 ©",
      std::string(last_sent_text_bytes.begin(), last_sent_text_bytes.end()));

  // Verify APC is what we expect it to be.
  EXPECT_EQ(lens::ContentData::CONTENT_TYPE_ANNOTATED_PAGE_CONTENT,
            last_sent_content.content_data(1).content_type());
  const auto last_sent_apc_bytes = last_sent_content.content_data(1).data();
  optimization_guide::proto::AnnotatedPageContent apc;
  ASSERT_TRUE(apc.ParseFromString(
      std::string(last_sent_apc_bytes.begin(), last_sent_apc_bytes.end())));
  EXPECT_EQ("The below are non-ascii characters.", apc.root_node()
                                                       .children_nodes(0)
                                                       .children_nodes(0)
                                                       .content_attributes()
                                                       .text_data()
                                                       .text_content());
  EXPECT_EQ("こんにちは thêrē 🐶 ©", apc.root_node()
                                         .children_nodes(1)
                                         .children_nodes(0)
                                         .content_attributes()
                                         .text_data()
                                         .text_content());

  // Verify the searchbox was shown.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  EXPECT_TRUE(fake_controller->fake_overlay_page_
                  .last_received_should_show_contextual_searchbox_);
}

// TODO(crbug.com/422479353): This test seems to be too slow on Windows ASAN.
#if BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER)
#define MAYBE_PageContentTypeHistograms DISABLED_PageContentTypeHistograms
#else
#define MAYBE_PageContentTypeHistograms PageContentTypeHistograms
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerInnerTextAndApc,
                       MAYBE_PageContentTypeHistograms) {
  ukm::TestAutoSetUkmRecorder test_ukm_recorder;
  base::HistogramTester histogram_tester;

  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Setup fake text in the OCR response. Included 4 words on the DOM, and 1
  // not, to make a similarity score of 0.8. Also include some random characters
  // to make sure they are ignored.
  auto* fake_controller =
      static_cast<LensSearchControllerFake*>(GetLensSearchController());
  fake_controller->SetOcrResponseWords(
      {"The.", "   below   - ", " ,are] ", "RANDOM", "\n\n\nCharacters.\n"});

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  CloseOverlayAndWaitForOff(controller,
                            LensOverlayDismissalSource::kOverlayCloseButton);

  // This histogram is async so run until it is recorded.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount(
               "Lens.Overlay.ByPageContentType.AnnotatedPageContent."
               "DocumentSize2",
               0) == 1;
  }));

  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ByDocumentType.Html.Invoked",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ByPageContentType.AnnotatedPageContent."
      "ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectUniqueSample(
      "Lens.Overlay.ContextualSearchBox.ByDocumentType.Html."
      "ShownInSession",
      /*sample*/ true,
      /*expected_bucket_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ByPageContentType.AnnotatedPageContent.DocumentSize2",
      /*expected_count=*/1);
  histogram_tester.ExpectTotalCount(
      "Lens.Overlay.ByPageContentType.PlainText.DocumentSize2",
      /*expected_count=*/1);

  // Verify UKM metrics were recorded.
  auto entries = test_ukm_recorder.GetEntriesByName(
      ukm::builders::Lens_Overlay_ContextualSearchBox_ShownInSession::
          kEntryName);
  EXPECT_EQ(1u, entries.size());
  auto* entry = entries[0].get();
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSearchBox_ShownInSession::
          kWasShownName,
      true);
  test_ukm_recorder.ExpectEntryMetric(
      entry,
      ukm::builders::Lens_Overlay_ContextualSearchBox_ShownInSession::
          kPageContentTypeName,
      static_cast<int64_t>(lens::MimeType::kAnnotatedPageContent));

  // This histogram is async so run until it is recorded.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return histogram_tester.GetBucketCount("Lens.Overlay.OcrDomSimilarity",
                                           80) == 1;
  }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerInnerTextAndApc,
                       PageNotContextEligibleError) {
  base::HistogramTester histogram_tester;
  WaitForPaint(kDocumentWithNonAsciiCharacters);

  // There should be no histograms logged.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/0);

  // Set the contextualization controller to return the page as not context
  // eligible.
  auto* fake_contextualization_controller =
      static_cast<lens::TestLensSearchContextualizationController*>(
          GetLensSearchController()
              ->lens_search_contextualization_controller());
  ASSERT_TRUE(fake_contextualization_controller);
  fake_contextualization_controller->SetContextEligible(false);

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  // When the overlay is bound, it should start the query flow which returns a
  // response for the full image callback.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Verify the error page histogram was not recorded since the result panel is
  // not open.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/0);

  // Side panel is not showing at first.
  EXPECT_FALSE(IsSidePanelOpen());
  EXPECT_FALSE(controller->GetSidePanelWebContentsForTesting());

  // Issuing a request should show the side panel even if navigation is expected
  // to fail.
  controller->IssueTextSelectionRequestForTesting("test query",
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Expect the Lens Overlay results panel to open.
  EXPECT_TRUE(IsLensResultsSidePanelShowing());

  // No page data or screenshot should have been sent.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  const auto last_sent_content =
      fake_query_controller->last_sent_page_content_payload().content();
  EXPECT_EQ(last_sent_content.content_data().size(), 0);
  EXPECT_TRUE(last_sent_content.webpage_url().empty());
  EXPECT_TRUE(last_sent_content.webpage_title().empty());

  // The recorded histogram should be a protected error page being shown.
  histogram_tester.ExpectTotalCount("Lens.Overlay.SidePanelResultStatus",
                                    /*expected_count=*/1);
  histogram_tester.ExpectBucketCount(
      "Lens.Overlay.SidePanelResultStatus",
      lens::SidePanelResultStatus::kErrorPageShownProtected,
      /*expected_count=*/1);
}

class LensOverlayControllerContextualFeaturesDisabledTest
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        /*disabled_features=*/{
            lens::features::kLensOverlayContextualSearchbox,
            lens::features::kLensSearchZeroStateCsb,
            lens::features::kLensOverlayNonBlockingPrivacyNotice});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerContextualFeaturesDisabledTest,
                       PreselectionToastShows) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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
                       PreselectionToastDisappearsOnSelection) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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

  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  // Must explicitly get preselection bubble from controller.
  ASSERT_EQ(controller->get_preselection_widget_for_testing(), nullptr);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerContextualFeaturesDisabledTest,
                       PreselectionToastOmniboxFocusState) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
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

  // Move focus away from omnibox to the overlay web view.
  controller->GetOverlayWebViewForTesting()->RequestFocus();

  // Widget should be visible when web view receives focus and overlay is open.
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify no bytes were excluded from the query.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  EXPECT_THAT(
      lens::Payload(),
      EqualsProto(fake_query_controller->last_sent_page_content_payload()));
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  auto* bubble_widget = waiter.WaitIfNeededAndGet();
  // Wait for the bubble to become visible.
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();
  ASSERT_TRUE(bubble_widget->IsVisible());

  auto* search_controller = GetLensSearchController();
  ASSERT_TRUE(
      search_controller->get_lens_permission_bubble_controller_for_testing()
          ->HasOpenDialogWidget());

  // Verify attempting to show the UI again does not close the bubble widget.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  ASSERT_TRUE(bubble_widget->IsVisible());
  ASSERT_TRUE(
      search_controller->get_lens_permission_bubble_controller_for_testing()
          ->HasOpenDialogWidget());

  // Simulate click on the accept button.
  auto* bubble_widget_delegate =
      bubble_widget->widget_delegate()->AsBubbleDialogDelegate();
  ClickBubbleDialogButton(bubble_widget_delegate,
                          bubble_widget_delegate->GetOkButton());
  ASSERT_FALSE(
      search_controller->get_lens_permission_bubble_controller_for_testing()
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
  auto screenshot_bitmap = controller->initial_screenshot();
  EXPECT_FALSE(screenshot_bitmap.empty());
  screenshot_bitmap = controller->updated_screenshot();
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  auto* bubble_widget = waiter.WaitIfNeededAndGet();
  // Wait for the bubble to become visible.
  views::test::WidgetVisibleWaiter(bubble_widget).Wait();
  ASSERT_TRUE(bubble_widget->IsVisible());

  auto* search_controller = GetLensSearchController();
  ASSERT_TRUE(
      search_controller->get_lens_permission_bubble_controller_for_testing()
          ->HasOpenDialogWidget());

  // Verify attempting to show the UI again does not close the bubble widget.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  // State should remain off.
  ASSERT_EQ(controller->state(), State::kOff);
  ASSERT_TRUE(bubble_widget->IsVisible());
  ASSERT_TRUE(
      search_controller->get_lens_permission_bubble_controller_for_testing()
          ->HasOpenDialogWidget());

  // Simulate click on the reject button.
  auto* bubble_widget_delegate =
      bubble_widget->widget_delegate()->AsBubbleDialogDelegate();
  ClickBubbleDialogButton(bubble_widget_delegate,
                          bubble_widget_delegate->GetCancelButton());
  ASSERT_FALSE(
      search_controller->get_lens_permission_bubble_controller_for_testing()
          ->HasOpenDialogWidget());

  // Verify sharing the page screenshot is still not permitted.
  ASSERT_FALSE(lens::CanSharePageScreenshotWithLensOverlay(prefs));
}

class LensOverlayControllerOverlaySearchbox
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitWithFeatures(
        /*enabled_features=*/{lens::features::kLensOverlay,
                              lens::features::kLensOverlayContextualSearchbox},
        /*disabled_features=*/{lens::features::kLensSearchZeroStateCsb});
  }

  void VerifyContextualSearchQueryParameters(const GURL& url_to_process) {
    EXPECT_THAT(url_to_process.spec(),
                testing::MatchesRegex(std::string(kResultsSearchBaseUrl) +
                                      ".*source=chrome.cr.menu.*&vit=.*&gsc=2&"
                                      "hl=.*&q=.*&biw=\\d+&bih=\\d+"));
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerOverlaySearchbox,
                       OverlaySearchboxPageClassificationAndState) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  // Searchbox should start in contextual mode if we are in the overlay state.
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);

  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "hello", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Issuing a search from the overlay state can only be done through the
  // contextual searchbox and should result in a live page with results.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));
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
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Wait for the bytes to be uploaded before issuing the request.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return fake_query_controller->last_sent_page_content_payload()
               .content()
               .content_data()
               .size() != 0;
  }));

  // Verify searchbox is in contextual mode.
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);

  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "hello", AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());

  // Wait for the side panel to open.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));
  EXPECT_EQ(controller->GetPageClassificationForTesting(),
            metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX);

  // Wait for URL to load in side panel.
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Verify the query and params are set.
  auto loaded_search_query = GetLoadedSearchQuery();
  EXPECT_TRUE(loaded_search_query);
  EXPECT_EQ(loaded_search_query->search_query_text_, "hello");
  VerifyContextualSearchQueryParameters(loaded_search_query->search_query_url_);
}

class LensOverlayControllerIPHBrowserTest
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlay, {}},
         {feature_engagement::kIPHLensOverlayFeature,
          {
              {"x_url_allow_filters",
               "[\"a.com\",\"b.com\",\"c.com/path\",\"d.com/path\"]"},
              {"x_url_block_filters", "[\"a.com/login\",\"d.com\"]"},
          }}},
        /*disabled_features=*/{});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerIPHBrowserTest,
                       IsUrlEligibleForTutorialIPH) {
  WaitForPaint();

  auto* controller = GetLensOverlayController();
  EXPECT_TRUE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.a.com/")));
  EXPECT_FALSE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.a.com/login/path?key=param")));
  EXPECT_TRUE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.b.com/page?key=param")));
  EXPECT_FALSE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.c.com/")));
  EXPECT_TRUE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.c.com/path/path")));
  // Blocks override allows.
  EXPECT_FALSE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.d.com/path")));
}

class LensOverlayControllerIPHWithPathMatchBrowserTest
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlay, {}},
         {feature_engagement::kIPHLensOverlayFeature,
          {
              {"x_url_allow_filters", "[\"*\"]"},
              {"x_url_block_filters", "[\"a.com/login\",\"d.com\",\"e.edu\"]"},
              {"x_url_path_match_allow_patterns",
               "[\"assignment\",\"homework\"]"},
              {"x_url_path_match_block_patterns", "[\"tutor\"]"},
              {"x_url_forced_allowed_match_patterns", "[\"edu/.+\"]"},
          }}},
        /*disabled_features=*/{});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerIPHWithPathMatchBrowserTest,
                       IsUrlEligibleForTutorialIPH) {
  WaitForPaint();

  auto* controller = GetLensOverlayController();
  // Path must match.
  EXPECT_FALSE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.a.com/")));
  EXPECT_TRUE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.a.com/assignment")));
  EXPECT_TRUE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.a.com/homework")));
  // Match can be in any part of path.
  EXPECT_TRUE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.b.com/1/anassignmentpage/2")));
  EXPECT_TRUE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.c.com/your-homework-problem")));
  // Match is on path, not on domain.
  EXPECT_FALSE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.homework.com/")));
  // Match is on path, not on query.
  EXPECT_FALSE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.c.com/path?assignment=1")));
  EXPECT_FALSE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.c.com/path?query=homework")));
  // Match is on path, not on fragment.
  EXPECT_FALSE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.c.com/path#assignment1")));
  // Block patterns take precedence over allow patterns.
  EXPECT_FALSE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.a.com/tutor/assignment")));
  // x_url_block_filters takes precedence over path matches.
  EXPECT_FALSE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.a.com/login/assignments")));
  EXPECT_FALSE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.d.com/homework")));

  // x_url_forced_allowed_match_patterns is blocked by the block url filters.
  EXPECT_FALSE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.e.edu/tutor")));
  // x_url_forced_allowed_match_patterns is blocked by the block path filters.
  EXPECT_FALSE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.d.edu/tutor")));
  // x_url_forced_allowed_match_patterns skips the allowed path filters.
  EXPECT_TRUE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.d.edu/something")));
  // URL not in x_url_forced_allowed_match_patterns and not in allowed path
  // filters.
  EXPECT_FALSE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.d.edu/")));
  // URL in x_url_forced_allowed_match_patterns and in allowed path filters.
  EXPECT_TRUE(controller->IsUrlEligibleForTutorialIPHForTesting(
      GURL("https://www.d.edu/homework")));
}

class LensOverlayControllerSideBySideBrowserTest
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensOverlay, {{"use-blur", "true"}}},
         {features::kSideBySide, {}}},
        {lens::features::kLensSearchZeroStateCsb});
  }

  bool AreAnyRoundedCornersShowing() {
    const ui::ElementContext context =
        views::ElementTrackerViews::GetContextForView(
            BrowserView::GetBrowserViewForBrowser(browser()));
    views::View* start_corner =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kContentsSeparatorLeadingTopCornerElementId, context);
    views::View* end_corner =
        views::ElementTrackerViews::GetInstance()->GetFirstMatchingView(
            kContentsSeparatorTrailingTopCornerElementId, context);
    return (start_corner && start_corner->GetVisible()) ||
           (end_corner && end_corner->GetVisible());
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerSideBySideBrowserTest,
                       BackgroundBlurNotLiveInitially) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Wait until AddBackgroundBlur is called.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->GetOverlayWebViewForTesting()->layer(); }));

  // In a normal tab, the screenshot is not resized initially, so background
  // image capturing should not have been started.
  EXPECT_FALSE(controller->GetLensOverlayBlurLayerDelegateForTesting()
                   ->IsCapturingBackgroundImageForTesting());
}

// TODO(crbug.com/422479353): Flaky on Linux MSan.
#if BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER)
#define MAYBE_BackgroundBlurLiveInitiallyInSplitTab \
  DISABLED_BackgroundBlurLiveInitiallyInSplitTab
#else
#define MAYBE_BackgroundBlurLiveInitiallyInSplitTab \
  BackgroundBlurLiveInitiallyInSplitTab
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerSideBySideBrowserTest,
                       MAYBE_BackgroundBlurLiveInitiallyInSplitTab) {
  chrome::NewSplitTab(browser(),
                      split_tabs::SplitTabCreatedSource::kToolbarButton);

  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Wait until AddBackgroundBlur is called.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->GetOverlayWebViewForTesting()->layer(); }));

  // In a split tab, the screenshot is initially resized, so background image
  // capturing should have been started.
  EXPECT_TRUE(controller->GetLensOverlayBlurLayerDelegateForTesting()
                  ->IsCapturingBackgroundImageForTesting());
}

// TODO(crbug.com/440147535): Flaky on multiple platforms.
IN_PROC_BROWSER_TEST_F(LensOverlayControllerSideBySideBrowserTest,
                       DISABLED_SidePanelRoundedCornerRegularTab) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Open a side panel.
  controller->IssueTextSelectionRequestForTesting(/*text_query=*/"Apples",
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

  // Expect the side panel rounded corner to exist.
  ASSERT_TRUE(controller->GetOverlayViewForTesting()->layer());
  EXPECT_TRUE(controller->GetOverlayViewForTesting()
                  ->layer()
                  ->GetTargetRoundedCornerRadius()
                  .upper_right() > 0);
  EXPECT_TRUE(AreAnyRoundedCornersShowing());
}

// TODO(crbug.com/446694608): Flaky on Linux.
#if BUILDFLAG(IS_LINUX)
#define MAYBE_SidePanelRoundedCornerSplitTab \
  DISABLED_SidePanelRoundedCornerSplitTab
#else
#define MAYBE_SidePanelRoundedCornerSplitTab SidePanelRoundedCornerSplitTab
#endif
IN_PROC_BROWSER_TEST_F(LensOverlayControllerSideBySideBrowserTest,
                       MAYBE_SidePanelRoundedCornerSplitTab) {
  // Create a new split, after which the second tab should be activated.
  chrome::NewSplitTab(browser(),
                      split_tabs::SplitTabCreatedSource::kToolbarButton);

  WaitForPaint();

  // State should start in off.
  auto* controller_1 = GetLensOverlayController();
  auto* side_panel_coordinator_1 = GetLensOverlaySidePanelCoordinator();
  ASSERT_EQ(controller_1->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller_1->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller_1->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Open a side panel.
  controller_1->IssueTextSelectionRequestForTesting(/*text_query=*/"Apples",
                                                    /*selection_start_index=*/0,
                                                    /*selection_end_index=*/0);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return side_panel_coordinator_1->IsEntryShowing(); }));

  // Expect overlay view's corners to be rounded.
  EXPECT_TRUE(controller_1->GetOverlayViewForTesting()->layer() &&
              controller_1->GetOverlayViewForTesting()
                      ->layer()
                      ->GetTargetRoundedCornerRadius()
                      .upper_right() > 0);
  EXPECT_FALSE(AreAnyRoundedCornersShowing());

  // Switch to the first tab.
  browser()->tab_strip_model()->ActivateTabAt(0);

  WaitForPaint();

  // State should start in off.
  auto* controller_0 = GetLensOverlayController();
  auto* side_panel_coordinator_0 = GetLensOverlaySidePanelCoordinator();
  ASSERT_EQ(controller_0->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller_0->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller_0->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Expect the corners to not be rounded.
  EXPECT_FALSE(controller_0->GetOverlayViewForTesting()->layer() &&
               controller_0->GetOverlayViewForTesting()
                       ->layer()
                       ->GetTargetRoundedCornerRadius()
                       .upper_right() > 0);
  EXPECT_FALSE(AreAnyRoundedCornersShowing());

  // Switch back to the second tab.
  browser()->tab_strip_model()->ActivateTabAt(1);

  // Wait for backgrounded state to be restored.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return side_panel_coordinator_0->IsEntryShowing(); }));

  // Expect overlay view's corners to be rounded.
  EXPECT_TRUE(controller_1->GetOverlayViewForTesting()->layer() &&
              controller_1->GetOverlayViewForTesting()
                      ->layer()
                      ->GetTargetRoundedCornerRadius()
                      .upper_right() > 0);
  EXPECT_FALSE(AreAnyRoundedCornersShowing());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerSideBySideBrowserTest,
                       SidePanelAlignmentChanged) {
  chrome::NewSplitTab(browser(),
                      split_tabs::SplitTabCreatedSource::kToolbarButton);

  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Open a side panel.
  controller->IssueTextSelectionRequestForTesting(/*text_query=*/"Apples",
                                                  /*selection_start_index=*/0,
                                                  /*selection_end_index=*/0);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

  // Expect overlay view's top right corner to be rounded.
  gfx::RoundedCornersF rounded_corners = controller->GetOverlayViewForTesting()
                                             ->layer()
                                             ->GetTargetRoundedCornerRadius();
  EXPECT_TRUE(rounded_corners.upper_right() > 0);
  EXPECT_TRUE(rounded_corners.upper_left() == 0);

  // Change side panel to be left aligned.
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kSidePanelHorizontalAlignment, false);

  // Expect overlay view's top left corner to be rounded.
  rounded_corners = controller->GetOverlayViewForTesting()
                        ->layer()
                        ->GetTargetRoundedCornerRadius();
  EXPECT_TRUE(rounded_corners.upper_right() == 0);
  EXPECT_TRUE(rounded_corners.upper_left() > 0);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       EmptyImagePlusTextQueryTest) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // 1. Initial Image Query
  // Open the overlay with a pre-selected region and image.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  OpenLensOverlayWithPendingRegion(
      LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);

  // Wait for the overlay and side panel to fully load.
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  ASSERT_TRUE(controller->GetSidePanelWebContentsForTesting());
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Check that the loaded query is image-only.
  auto image_only_query = GetLoadedSearchQuery();
  ASSERT_TRUE(image_only_query.has_value());
  EXPECT_TRUE(image_only_query->search_query_text_.empty());
  EXPECT_EQ(image_only_query->lens_selection_type_, lens::INJECTED_IMAGE);
  EXPECT_TRUE(image_only_query->selected_region_);
  EXPECT_FALSE(image_only_query->selected_region_bitmap_.drawsNothing());

  // 2. Add Text Query (Multimodal)
  // Issue a text search request from the search box.
  const std::string test_text = "cats";
  content::TestNavigationObserver multimodal_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, test_text, AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      std::map<std::string, std::string>());
  multimodal_observer.WaitForNavigationFinished();

  // Check that the loaded query is now multimodal.
  auto multimodal_query = GetLoadedSearchQuery();
  ASSERT_TRUE(multimodal_query.has_value());
  EXPECT_EQ(multimodal_query->search_query_text_, test_text);
  EXPECT_EQ(multimodal_query->lens_selection_type_, lens::MULTIMODAL_SEARCH);
  EXPECT_TRUE(multimodal_query->selected_region_);
  EXPECT_FALSE(multimodal_query->selected_region_thumbnail_uri_.empty());

  // 3. Add Empty Text Query (Should revert to Image-Only)
  // Issue an empty text search request from the search box.
  const std::string empty_test_text = std::string();
  GetLensOverlaySidePanelCoordinator()->OnImageQueryWithEmptyText();
  content::TestNavigationObserver empty_observer(
      controller->GetSidePanelWebContentsForTesting());
  GetLensOverlaySidePanelCoordinator()->OnImageQueryWithEmptyText();
  empty_observer.Wait();

  // Check that the loaded query state reverted to image-only.
  auto empty_multimodal_query = GetLoadedSearchQuery();
  ASSERT_TRUE(empty_multimodal_query.has_value());
  EXPECT_TRUE(empty_multimodal_query->search_query_text_.empty());
  EXPECT_EQ(empty_multimodal_query->lens_selection_type_,
            lens::MULTIMODAL_SELECTION_CLEAR);
  EXPECT_TRUE(empty_multimodal_query->selected_region_);
  EXPECT_FALSE(empty_multimodal_query->selected_region_thumbnail_uri_.empty());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       ConsecutiveImageNoTextQueryTest) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // 1. Initial Image Query
  // Open the overlay with a pre-selected region and image.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  OpenLensOverlayWithPendingRegion(
      LensOverlayInvocationSource::kContentAreaContextMenuImage,
      kTestRegion->Clone(), initial_bitmap);

  // Wait for the overlay and side panel to fully load.
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  EXPECT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));
  ASSERT_TRUE(controller->GetSidePanelWebContentsForTesting());
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Verify the side panel is showing Lens results.
  EXPECT_TRUE(IsLensResultsSidePanelShowing());

  // Check that the loaded query is image-only.
  auto image_only_query = GetLoadedSearchQuery();
  ASSERT_TRUE(image_only_query.has_value());
  EXPECT_TRUE(image_only_query->search_query_text_.empty());
  EXPECT_EQ(image_only_query->lens_selection_type_, lens::INJECTED_IMAGE);
  EXPECT_TRUE(image_only_query->selected_region_);
  EXPECT_FALSE(image_only_query->selected_region_bitmap_.drawsNothing());

  // 2. Send the textless image query again
  const std::string empty_test_text = std::string();
  GetLensOverlaySidePanelCoordinator()->OnImageQueryWithEmptyText();
  content::TestNavigationObserver empty_observer(
      controller->GetSidePanelWebContentsForTesting());
  GetLensOverlaySidePanelCoordinator()->OnImageQueryWithEmptyText();
  empty_observer.Wait();

  // Check that the loaded query state reverted to image-only.
  auto empty_multimodal_query = GetLoadedSearchQuery();
  ASSERT_TRUE(empty_multimodal_query.has_value());
  EXPECT_TRUE(empty_multimodal_query->search_query_text_.empty());
  EXPECT_EQ(empty_multimodal_query->lens_selection_type_, lens::INJECTED_IMAGE);
  EXPECT_TRUE(empty_multimodal_query->selected_region_);
  EXPECT_FALSE(empty_multimodal_query->selected_region_thumbnail_uri_.empty());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest,
                       RecordInitializationTimingHistograms) {
  base::HistogramTester histogram_tester;
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // No metrics should be emitted before anything happens.
  histogram_tester.ExpectTotalCount("Lens.Overlay.TimeToScreenshot", 0);
  histogram_tester.ExpectTotalCount("Lens.Overlay.TimeToCreateBitmap", 0);
  histogram_tester.ExpectTotalCount("Lens.Overlay.TimeToGetPageContext", 0);
  histogram_tester.ExpectTotalCount("Lens.Overlay.TimeForPageToBind", 0);
  histogram_tester.ExpectTotalCount("Lens.Overlay.TimeToCloseOpenedSidePanel",
                                    0);

  // Open the side panel so we can test the side panel closing metric.
  auto* const side_panel_ui = browser()->GetFeatures().side_panel_ui();
  side_panel_ui->Show(SidePanelEntry::Id::kBookmarks);
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_ui->IsSidePanelEntryShowing(
        SidePanelEntryKey(SidePanelEntryId::kBookmarks));
  }));

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  histogram_tester.ExpectTotalCount("Lens.Overlay.TimeToScreenshot", 1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.TimeToCreateScreenshotBitmap",
                                    1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.TimeToGetPageContext", 1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.TimeToWebuiBound", 1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.TimeToCloseOpenedSidePanel",
                                    1);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerBrowserTest, ReshowOverlay) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should eventually result in overlay state.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Get the first screenshot.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  SkBitmap first_screenshot =
      fake_controller->fake_overlay_page_.last_received_screenshot_;
  EXPECT_FALSE(first_screenshot.isNull());

  // Make a searchbox query to open the side panel and hide the overlay.
  controller->IssueSearchBoxRequestForTesting(
      kTestTime, "oranges", AutocompleteMatchType::SEARCH_SUGGEST,
      /*is_zero_prefix_suggestion=*/false,
      /*additional_query_params=*/{});

  // Wait for the side panel to load.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->GetSidePanelWebContentsForTesting(); }));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));

  // Verify transitions to hidden state.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));
  EXPECT_FALSE(controller->GetOverlayViewForTesting()->GetVisible());

  // Reset the screenshot in the fake page to verify a new one is sent.
  fake_controller->fake_overlay_page_.last_received_screenshot_.reset();

  // Opening the overlay in the current session should reshow the overlay.
  GetLensSearchController()->OpenLensOverlayInCurrentSession();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());

  // Verify a new screenshot was sent.
  EXPECT_FALSE(
      fake_controller->fake_overlay_page_.last_received_screenshot_.isNull());
}

class LensOverlayControllerZeroStateCsbTest
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitWithFeaturesAndParameters(
        {{lens::features::kLensSearchZeroStateCsb, {}}},
        /*disabled_features=*/{});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerZeroStateCsbTest,
                       OpenLensOverlayWithZeroStateCsbQuery) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to hidden and open the side panel.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);

  // Expect the Lens Overlay results panel to open.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  EXPECT_TRUE(content::WaitForLoadStop(
      GetLensOverlaySidePanelCoordinator()->GetSidePanelWebContents()));
  // Overlay should stay in off state.
  ASSERT_EQ(controller->state(), State::kOff);

  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  EXPECT_TRUE(fake_query_controller->last_queried_region());
  EXPECT_EQ(fake_query_controller->last_queried_region()->box.width(), 1.0);
  EXPECT_EQ(fake_query_controller->last_queried_region()->box.height(), 1.0);
  EXPECT_EQ(fake_query_controller->last_queried_region()->box.x(), 0.5);
  EXPECT_EQ(fake_query_controller->last_queried_region()->box.y(), 0.5);
  EXPECT_EQ(fake_query_controller->last_queried_region()->rotation, 0.0);
  EXPECT_EQ(fake_query_controller->last_lens_selection_type(),
            lens::REGION_SEARCH);
}

class LensOverlayControllerReinvocationBrowserTest
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitWithFeatures(
        {lens::features::kLensOverlay,
         lens::features::kLensOverlayContextualSearchbox,
         lens::features::kLensSearchReinvocationAffordance},
        {lens::features::kLensSearchZeroStateCsb});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerReinvocationBrowserTest,
                       NotifiesSidePanelOfOverlayVisibilityChanges) {
  WaitForPaint();
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay
  // and results.
  SkBitmap initial_bitmap = CreateNonEmptyBitmap(100, 100);
  OpenLensOverlayWithPendingRegion(LensOverlayInvocationSource::kAppMenu,
                                   kTestRegion->Clone(), initial_bitmap);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

  // Expect the Lens Overlay results panel to open.
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  ASSERT_TRUE(test_side_panel_coordinator);

  // Verify overlay showing is sent on initial showing.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return test_side_panel_coordinator->last_is_showing_ &&
           test_side_panel_coordinator->last_is_showing_.value();
  }));

  // Grab the index of the currently active tab so we can return to it later.
  int active_controller_tab_index =
      browser()->tab_strip_model()->active_index();

  // Opening a new tab should background the overlay UI.
  WaitForPaint(kDocumentWithNamedElement,
               WindowOpenDisposition::NEW_FOREGROUND_TAB,
               ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
                   ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kBackground; }));

  // Verify overlay showing is sent on background.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return test_side_panel_coordinator->last_is_showing_ &&
           !test_side_panel_coordinator->last_is_showing_.value();
  }));

  // Returning back to the previous tab should show the overlay UI again.
  browser()->tab_strip_model()->ActivateTabAt(active_controller_tab_index);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

  // Verify overlay showing is sent on foreground.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return test_side_panel_coordinator->last_is_showing_ &&
           test_side_panel_coordinator->last_is_showing_.value();
  }));

  // Request a close via the close button on the overlay UI.
  GetLensSearchController()->HideOverlay(
      lens::LensOverlayDismissalSource::kOverlayCloseButton);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  // Verify overlay showing is sent on hiding.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return test_side_panel_coordinator->last_is_showing_ &&
           !test_side_panel_coordinator->last_is_showing_.value();
  }));

  // Opening the overlay in the current session should reshow the overlay.
  GetLensSearchController()->OpenLensOverlayInCurrentSession();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

  // Verify overlay showing is sent on reshowing.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return test_side_panel_coordinator->last_is_showing_ &&
           test_side_panel_coordinator->last_is_showing_.value();
  }));
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerReinvocationBrowserTest,
                       ScreenshotDataReceivedWithSidePanelOpen) {
  WaitForPaint();

  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Issue a text search request to open the side panel without the overlay.
  GetLensSearchController()->IssueTextSearchRequest(
      LensOverlayInvocationSource::kContentAreaContextMenuText, "query", {},
      AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      /*suppress_contextualization=*/true);

  // Wait for side panel to be visible.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  ASSERT_TRUE(GetLensOverlaySidePanelCoordinator()->IsEntryShowing());

  // Reset the query controller to verify a new request was sent with the
  // screenshot.
  auto* fake_query_controller =
      static_cast<lens::TestLensOverlayQueryController*>(
          GetLensOverlayQueryController());
  fake_query_controller->ResetTestingState();

  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);

  // Showing UI should change the state to overlay and results eventually.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Flush for testing to make sure the mojo call has been processed.
  fake_controller->FlushForTesting();
  EXPECT_FALSE(
      fake_controller->fake_overlay_page_.last_received_screenshot_.empty());
  ASSERT_TRUE(fake_controller->fake_overlay_page_
                  .last_received_is_side_panel_open_.has_value());
  EXPECT_TRUE(fake_controller->fake_overlay_page_
                  .last_received_is_side_panel_open_.value());

  // Verify the content bytes were included in a followup request.
  auto page_content_request_payload =
      fake_query_controller->last_sent_page_content_payload();
  EXPECT_EQ(fake_query_controller->num_page_content_update_requests_sent(), 1);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerReinvocationBrowserTest,
                       FollowUpRegionSearchDoesNotLoadInSidePanel) {
  WaitForPaint();
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Issue a search box request with a vsqid to simulate being in an AIM query.
  // This will set the side_panel_new_tab_url_ in the coordinator after the
  // navigation in the side panel finishes.
  controller->IssueSearchBoxRequestForTesting(
      base::Time::Now(), "first query",
      AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false, {{"vsqid", "fake_vsqid"}});

  // Wait for the side panel to open and load.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));
  auto* side_panel_web_contents =
      controller->GetSidePanelWebContentsForTesting();
  ASSERT_TRUE(side_panel_web_contents);
  EXPECT_TRUE(content::WaitForLoadStop(side_panel_web_contents));
  GURL url_before_region_search =
      side_panel_web_contents->GetLastCommittedURL();

  auto* query_controller = static_cast<lens::TestLensOverlayQueryController*>(
      GetLensOverlayQueryController());
  std::optional<lens::LensOverlayVisualSearchInteractionData> vsint_before;
  ASSERT_TRUE(base::test::RunUntil([&]() {
    vsint_before = query_controller->GetVisualSearchInteractionData();
    return vsint_before.has_value();
  }));
  const std::string vsint_before_string = vsint_before->SerializeAsString();
  query_controller->ResetTestingState();

  // Reshow the overlay to perform a region search.
  GetLensSearchController()->OpenLensOverlayInCurrentSession();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  ASSERT_TRUE(base::test::RunUntil([&]() { return controller->state() == State::kOverlay; }));

  // We need to flush the mojo receiver calls to make sure the screenshot was
  // passed back to the WebUI or else the region selection UI will not render.
  auto* fake_controller = static_cast<LensOverlayControllerFake*>(controller);
  ASSERT_TRUE(fake_controller);
  fake_controller->FlushForTesting();
  ASSERT_TRUE(content::WaitForLoadStop(GetOverlayWebContents()));

  // Simulate a region search.
  controller->IssueLensRegionRequestForTesting(kTestRegion->Clone(),
                                               /*is_click=*/false);

  // Wait for the region search to be processed.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return !query_controller->last_queried_region().is_null(); }));

  // Verify that the side panel web contents has focus.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return side_panel_web_contents->GetRenderWidgetHostView()->HasFocus();
  }));

  // Verify that no navigation occurred and the URL is unchanged.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    auto vsint_after = query_controller->GetVisualSearchInteractionData();
    return url_before_region_search ==
               side_panel_web_contents->GetLastCommittedURL() &&
           vsint_after.has_value() &&
           vsint_before_string != vsint_after->SerializeAsString();
  }));
  EXPECT_EQ(url_before_region_search,
            side_panel_web_contents->GetLastCommittedURL());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerReinvocationBrowserTest,
                       RecordsShownAndInvoked) {
  base::HistogramTester histogram_tester;
  WaitForPaint();
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_EQ(controller->state(), State::kScreenshot);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify histograms were recorded correctly.
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked", 1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Shown", 1);

  // Issue a region search to open the side panel.
  controller->IssueLensRegionRequestForTesting(kTestRegion->Clone(),
                                               /*is_click=*/false);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

  // Expect the Lens Overlay results panel to open.
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  ASSERT_TRUE(test_side_panel_coordinator);
  EXPECT_TRUE(IsLensResultsSidePanelShowing());

  // Request a close via the close button on the overlay UI.
  GetLensSearchController()->HideOverlay(
      lens::LensOverlayDismissalSource::kOverlayCloseButton);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  // Opening the overlay in the current session should reshow the overlay.
  GetLensSearchController()->OpenLensOverlayInCurrentSession();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify histograms were recorded correctly.
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked", 1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Shown", 2);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerReinvocationBrowserTest,
                       RecordsShownAndInvoked_OverlayIniitiallyHidden) {
  base::HistogramTester histogram_tester;
  WaitForPaint();
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Issue a text search request to open the side panel without the overlay.
  GetLensSearchController()->IssueTextSearchRequest(
      LensOverlayInvocationSource::kContentAreaContextMenuText, "query", {},
      AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      /*suppress_contextualization=*/true);

  // Wait for side panel to be visible.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  ASSERT_TRUE(GetLensOverlaySidePanelCoordinator()->IsEntryShowing());

  // Expect the Lens Overlay results panel to open.
  auto* test_side_panel_coordinator =
      static_cast<lens::TestLensOverlaySidePanelCoordinator*>(
          GetLensOverlaySidePanelCoordinator());
  ASSERT_TRUE(test_side_panel_coordinator);

  // Verify histograms were not recorded as the overlay has not been shown yet.
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked", 0);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Shown", 0);

  // Opening the overlay in the current session should reshow the overlay.
  GetLensSearchController()->OpenLensOverlayInCurrentSession();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Verify histograms were recorded correctly.
  histogram_tester.ExpectTotalCount("Lens.Overlay.Invoked", 1);
  histogram_tester.ExpectTotalCount("Lens.Overlay.Shown", 1);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerReinvocationBrowserTest,
                       BackgroundOverlayDuringReshow) {
  WaitForPaint();
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Issue a region search to open the side panel.
  controller->IssueLensRegionRequestForTesting(kTestRegion->Clone(),
                                               /*is_click=*/false);
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));

  // Request a close via the close button on the overlay UI.
  GetLensSearchController()->HideOverlay(
      lens::LensOverlayDismissalSource::kOverlayCloseButton);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kHidden; }));

  // Opening the overlay in the current session should start reshowing the
  // overlay.
  GetLensSearchController()->OpenLensOverlayInCurrentSession();
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kIsReshowing; }));

  // Grab the index of the currently active tab so we can return to it later.
  int active_controller_tab_index =
      browser()->tab_strip_model()->active_index();

  // Opening a new tab should background the overlay UI.
  WaitForPaint(kDocumentWithNamedElement,
               WindowOpenDisposition::NEW_FOREGROUND_TAB,
               ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
                   ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kBackground; }));
  EXPECT_EQ(controller->backgrounded_state_for_testing(), State::kHidden);

  // Returning back to the previous tab should show the side panel, but not the
  // overlay.
  browser()->tab_strip_model()->ActivateTabAt(active_controller_tab_index);
  EXPECT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  EXPECT_EQ(controller->state(), State::kHidden);

  // Opening the overlay in the current session should reshow the overlay again.
  GetLensSearchController()->OpenLensOverlayInCurrentSession();
  ASSERT_TRUE(controller->state() == State::kIsReshowing);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));
  EXPECT_TRUE(controller->GetOverlayViewForTesting()->GetVisible());
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerReinvocationBrowserTest,
                       BackgroundOverlayDuringInitialization) {
  WaitForPaint();
  auto* search_controller = GetLensSearchController();
  auto* controller = GetLensOverlayController();
  ASSERT_TRUE(search_controller->IsOff());
  ASSERT_EQ(controller->state(), State::kOff);

  // Issue a text search request to open the side panel without the overlay.
  search_controller->IssueTextSearchRequest(
      LensOverlayInvocationSource::kContentAreaContextMenuText, "query", {},
      AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED,
      /*is_zero_prefix_suggestion=*/false,
      /*suppress_contextualization=*/true);

  // Wait for side panel to be visible.
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return IsLensResultsSidePanelShowing(); }));
  ASSERT_TRUE(GetLensOverlaySidePanelCoordinator()->IsEntryShowing());
  ASSERT_FALSE(search_controller->IsOff());

  // Opening the overlay in the current session should start showing the
  // overlay. Block binding the overlay to test the initialization path.
  auto* fake_controller =
      static_cast<LensOverlayControllerFake*>(GetLensOverlayController());
  ASSERT_TRUE(fake_controller);
  fake_controller->should_bind_overlay_ = false;
  search_controller->OpenLensOverlayInCurrentSession();
  ASSERT_EQ(controller->state(), State::kScreenshot);

  // Grab the index of the currently active tab so we can return to it later.
  int active_controller_tab_index =
      browser()->tab_strip_model()->active_index();

  // Opening a new tab should background the overlay UI.
  WaitForPaint(kDocumentWithNamedElement,
               WindowOpenDisposition::NEW_FOREGROUND_TAB,
               ui_test_utils::BROWSER_TEST_WAIT_FOR_TAB |
                   ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // The overlay controller should go to kOff state.
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOff; }));

  // Returning back to the previous tab should not show the overlay or side
  // panel UI.
  browser()->tab_strip_model()->ActivateTabAt(active_controller_tab_index);
  EXPECT_FALSE(IsLensResultsSidePanelShowing());
  // Overlay controller state should be kOff.
  EXPECT_EQ(controller->state(), State::kOff);
}

IN_PROC_BROWSER_TEST_F(LensOverlayControllerReinvocationBrowserTest,
                       RegionSearchLoadsInSidePanel) {
  WaitForPaint();
  // State should start in off.
  auto* controller = GetLensOverlayController();
  ASSERT_EQ(controller->state(), State::kOff);

  // Showing UI should change the state to screenshot and eventually to overlay.
  // Open the overlay.
  OpenLensOverlay(LensOverlayInvocationSource::kAppMenu);
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return controller->state() == State::kOverlay; }));

  // Issue a region search. This should trigger a navigation in the side panel.
  controller->IssueLensRegionRequestForTesting(kTestRegion->Clone(),
                                               /*is_click=*/false);
  // Expect the Lens Overlay results panel to open.
  ASSERT_TRUE(browser()->GetFeatures().side_panel_ui()->IsSidePanelEntryShowing(
      SidePanelEntryKey(SidePanelEntryId::kLensOverlayResults)));
  EXPECT_TRUE(content::WaitForLoadStop(
      controller->GetSidePanelWebContentsForTesting()));
  EXPECT_EQ(controller->get_selected_region_for_testing(), kTestRegion);

  // The selection type should be REGION_SEARCH at this point.
  auto* query_controller = static_cast<lens::TestLensOverlayQueryController*>(
      GetLensOverlayQueryController());
  EXPECT_EQ(query_controller->last_lens_selection_type(), lens::REGION_SEARCH);
  query_controller->ResetTestingState();

  // Simulate a region search. This should trigger a navigation in the side
  // panel.
  content::TestNavigationObserver search_observer(
      controller->GetSidePanelWebContentsForTesting());
  controller->IssueLensRegionRequestForTesting(kTestRegion->Clone(),
                                               /*is_click=*/false);

  // Verify that the navigation occurred.
  search_observer.WaitForNavigationFinished();
  EXPECT_EQ(query_controller->last_lens_selection_type(), lens::REGION_SEARCH);
}

class LensOverlayControllerContextualTasksBrowserTest
    : public LensOverlayControllerBrowserTest {
 protected:
  void SetupFeatureList() override {
    feature_list_.InitWithFeatures(
        {lens::features::kLensOverlay,
         lens::features::kLensOverlayContextualSearchbox,
         lens::features::kLensSearchReinvocationAffordance,
         contextual_tasks::kContextualTasks},
        {lens::features::kLensSearchZeroStateCsb});
  }
};

IN_PROC_BROWSER_TEST_F(LensOverlayControllerContextualTasksBrowserTest,
                       EnterprisePolicy) {
  // The default policy is to allow the feature to be enabled.
  EXPECT_TRUE(browser()
                  ->GetFeatures()
                  .lens_overlay_entry_point_controller()
                  ->IsEnabled());

  // Even if the LensOverlaySettings policy is set to disabled, the feature
  // should still be enabled since the enterprise policy for contextual tasks is
  // not set.
  policy::PolicyMap policies;
  policies.Set("LensOverlaySettings", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(1), nullptr);
  policy_provider()->UpdateChromePolicy(policies);
  EXPECT_TRUE(browser()
                  ->GetFeatures()
                  .lens_overlay_entry_point_controller()
                  ->IsEnabled());

  policies.Set("SearchContentSharingSettings", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(1), nullptr);
  policy_provider()->UpdateChromePolicy(policies);
  EXPECT_FALSE(browser()
                   ->GetFeatures()
                   .lens_overlay_entry_point_controller()
                   ->IsEnabled());

  policies.Set("SearchContentSharingSettings", policy::POLICY_LEVEL_MANDATORY,
               policy::POLICY_SCOPE_USER, policy::POLICY_SOURCE_CLOUD,
               base::Value(0), nullptr);
  policy_provider()->UpdateChromePolicy(policies);
  EXPECT_TRUE(browser()
                  ->GetFeatures()
                  .lens_overlay_entry_point_controller()
                  ->IsEnabled());
}
