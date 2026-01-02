// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/json/json_reader.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/numerics/checked_math.h"
#include "base/process/kill.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/lens_side_panel.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/task_manager/web_contents_tags.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "chrome/browser/ui/lens/lens_help_menu_utils.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_event_handler.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_languages_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_proto_converter.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"
#include "chrome/browser/ui/lens/lens_overlay_untrusted_ui.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/lens_preselection_bubble.h"
#include "chrome/browser/ui/lens/lens_results_panel_router.h"
#include "chrome/browser/ui/lens/lens_search_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/lens_search_feature_flag_utils.h"
#include "chrome/browser/ui/lens/lens_searchbox_controller.h"
#include "chrome/browser/ui/lens/lens_session_metrics_logger.h"
#include "chrome/browser/ui/lens/page_content_type_conversions.h"
#include "chrome/browser/ui/search/omnibox_utils.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/views/interaction/browser_elements_views.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry.h"
#include "chrome/browser/ui/views/side_panel/side_panel_entry_key.h"
#include "chrome/browser/ui/views/side_panel/side_panel_enums.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/branded_strings.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_metrics.h"
#include "components/lens/lens_overlay_mime_type.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/omnibox/browser/lens_suggest_inputs_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/tabs/public/tab_interface.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/child_process_termination_info.h"
#include "content/public/browser/download_manager.h"
#include "content/public/browser/download_request_utils.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/web_ui.h"
#include "net/base/network_change_notifier.h"
#include "pdf/buildflags.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/widget/native_widget.h"

#if BUILDFLAG(ENABLE_PDF)
#include "components/pdf/browser/pdf_document_helper.h"
#include "pdf/mojom/pdf.mojom.h"
#endif  // BUILDFLAG(ENABLE_PDF)

void* kLensOverlayPreselectionWidgetIdentifier =
    &kLensOverlayPreselectionWidgetIdentifier;

namespace {

// Timeout for the fadeout animation. This is purposely set to be twice the
// duration of the fade out animation on the WebUI JS because there is a delay
// between us notifying the WebUI, and the WebUI receiving our event.
constexpr base::TimeDelta kFadeoutAnimationTimeout = base::Milliseconds(300);

// The amount of time to wait for a reflow after closing the side panel before
// taking a screenshot.
constexpr base::TimeDelta kReflowWaitTimeout = base::Milliseconds(200);

// Copy the objects of a vector into another without transferring
// ownership.
std::vector<lens::mojom::OverlayObjectPtr> CopyObjects(
    const std::vector<lens::mojom::OverlayObjectPtr>& objects) {
  std::vector<lens::mojom::OverlayObjectPtr> objects_copy(objects.size());
  std::transform(
      objects.begin(), objects.end(), objects_copy.begin(),
      [](const lens::mojom::OverlayObjectPtr& obj) { return obj->Clone(); });
  return objects_copy;
}

// Given a BGR bitmap, converts into a RGB bitmap instead. Returns empty bitmap
// if creation fails.
SkBitmap CreateRgbBitmap(const SkBitmap& bgr_bitmap) {
  // Convert bitmap from color type `kBGRA_8888_SkColorType` into a new Bitmap
  // with color type `kRGBA_8888_SkColorType` which will allow the bitmap to
  // render properly in the WebUI.
  sk_sp<SkColorSpace> srgb_color_space =
      bgr_bitmap.colorSpace()->makeSRGBGamma();
  SkImageInfo rgb_info = bgr_bitmap.info()
                             .makeColorType(kRGBA_8888_SkColorType)
                             .makeColorSpace(SkColorSpace::MakeSRGB());
  SkBitmap rgb_bitmap;
  rgb_bitmap.setInfo(rgb_info);
  rgb_bitmap.allocPixels(rgb_info);
  if (rgb_bitmap.writePixels(bgr_bitmap.pixmap())) {
    return rgb_bitmap;
  }

  // Bitmap creation failed.
  return SkBitmap();
}

// Converts a JSON string array to a vector.
std::vector<std::string> JSONArrayToVector(const std::string& json_array) {
  std::optional<base::Value> json_value =
      base::JSONReader::Read(json_array, base::JSON_PARSE_CHROMIUM_EXTENSIONS);

  if (!json_value) {
    return {};
  }

  base::Value::List* entries = json_value->GetIfList();
  if (!entries) {
    return {};
  }

  std::vector<std::string> result;
  result.reserve(entries->size());
  for (const base::Value& entry : *entries) {
    const std::string* filter = entry.GetIfString();
    if (filter) {
      result.emplace_back(*filter);
    }
  }
  return result;
}

LensOverlayController* GetLensOverlayControllerFromTabInterface(
    tabs::TabInterface* tab_interface) {
  return tab_interface
             ? tab_interface->GetTabFeatures()->lens_overlay_controller()
             : nullptr;
}

bool IsVisualSelectionType(lens::LensOverlaySelectionType selection_type) {
  return selection_type == lens::REGION_SEARCH ||
         selection_type == lens::TAP_ON_EMPTY ||
         selection_type == lens::TAP_ON_REGION_GLEAM ||
         selection_type == lens::TAP_ON_OBJECT ||
         selection_type == lens::INJECTED_IMAGE;
}

}  // namespace

class LensOverlayController::UnderlyingWebContentsObserver
    : public content::WebContentsObserver {
 public:
  UnderlyingWebContentsObserver(content::WebContents* web_contents,
                                LensOverlayController* lens_overlay_controller)
      : content::WebContentsObserver(web_contents),
        lens_overlay_controller_(lens_overlay_controller) {}

  ~UnderlyingWebContentsObserver() override = default;

  UnderlyingWebContentsObserver(const UnderlyingWebContentsObserver&) = delete;
  UnderlyingWebContentsObserver& operator=(
      const UnderlyingWebContentsObserver&) = delete;

  // content::WebContentsObserver
  void DidFinishNavigation(
      content::NavigationHandle* navigation_handle) override {
    // If the overlay is off, check if we should display IPH.
    if (lens_overlay_controller_->state() == State::kOff) {
      // Only check IPH eligibility if the navigation changed the primary page.
      if (base::FeatureList::IsEnabled(
              feature_engagement::kIPHLensOverlayFeature) &&
          navigation_handle->IsInPrimaryMainFrame() &&
          !navigation_handle->IsSameDocument() &&
          navigation_handle->HasCommitted()) {
        lens_overlay_controller_->MaybeShowDelayedTutorialIPH(
            navigation_handle->GetURL());
      }
      return;
    }

    // If the overlay is open, check if we should close it.
    bool is_user_reload =
        navigation_handle->GetReloadType() != content::ReloadType::NONE &&
        !navigation_handle->IsRendererInitiated();
    // We don't need to close if:
    //   1) The navigation is not for the main page.
    //   2) The navigation hasn't been committed yet.
    //   3) The URL did not change and the navigation wasn't the user reloading
    //      the page.
    if (!navigation_handle->IsInPrimaryMainFrame() ||
        !navigation_handle->HasCommitted() ||
        (navigation_handle->GetPreviousPrimaryMainFrameURL() ==
             navigation_handle->GetURL() &&
         !is_user_reload)) {
      return;
    }
    if (lens_overlay_controller_->state() == State::kHidden) {
      lens_overlay_controller_->UpdateNavigationMetrics();
      lens_overlay_controller_->NotifyPageContentUpdated();
      return;
    }

    // If the page changes, only the overlay needs to be hidden, possibly
    // leaving the side panel open. The search controller will handle whether
    // the side panel should stay open or the entire session should terminate.
    lens_overlay_controller_->lens_search_controller_->HideOverlay(
        lens::LensOverlayDismissalSource::kPageChanged);
    return;
  }

  void PrimaryMainFrameRenderProcessGone(
      base::TerminationStatus status) override {
    // Exit early if the overlay is off or already closing.
    if (lens_overlay_controller_->state() == State::kOff ||
        lens_overlay_controller_->IsOverlayClosing()) {
      return;
    }

    lens_overlay_controller_->lens_search_controller_->CloseLensSync(
        status == base::TERMINATION_STATUS_NORMAL_TERMINATION
            ? lens::LensOverlayDismissalSource::kPageRendererClosedNormally
            : lens::LensOverlayDismissalSource::
                  kPageRendererClosedUnexpectedly);
  }

 private:
  raw_ptr<LensOverlayController> lens_overlay_controller_;
};

LensOverlayController::LensOverlayController(
    tabs::TabInterface* tab,
    LensSearchController* lens_search_controller,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    ThemeService* theme_service)
    : tab_(tab),
      lens_search_controller_(lens_search_controller),
      variations_client_(variations_client),
      identity_manager_(identity_manager),
      pref_service_(pref_service),
      sync_service_(sync_service),
      theme_service_(theme_service),
      gen204_controller_(
          std::make_unique<lens::LensOverlayGen204Controller>()) {
  InitializeTutorialIPHUrlMatcher();

  // Listen to WebContents events
  tab_contents_observer_ = std::make_unique<UnderlyingWebContentsObserver>(
      tab_->GetContents(), this);
}

LensOverlayController::~LensOverlayController() {
  tab_contents_observer_.reset();
  state_ = State::kOff;
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(LensOverlayController, kOverlayId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(LensOverlayController,
                                      kOverlaySidePanelWebViewId);

// static.
LensOverlayController* LensOverlayController::FromWebUIWebContents(
    content::WebContents* webui_web_contents) {
  return GetLensOverlayControllerFromTabInterface(
      webui::GetTabInterface(webui_web_contents));
}

// static.
LensOverlayController* LensOverlayController::FromTabWebContents(
    content::WebContents* tab_web_contents) {
  return GetLensOverlayControllerFromTabInterface(
      tabs::TabInterface::GetFromContents(tab_web_contents));
}

void LensOverlayController::TriggerOverlayFadeOutAnimation(
    base::OnceClosure callback) {
  if (state_ == State::kOff || IsOverlayClosing()) {
    return;
  }
  state_ = State::kHiding;

  // Notify the overlay so it can do any animations or cleanup. The page_ is not
  // guaranteed to exist if CloseUIAsync is called during the setup process.
  if (page_) {
    page_->NotifyOverlayClosing();
  }

  // Set a short 200ms timeout to give the fade out time to transition.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE, std::move(callback), kFadeoutAnimationTimeout);
}

void LensOverlayController::CloseUI(
    lens::LensOverlayDismissalSource dismissal_source) {
  // Notify the query controller to loose references to this classes data before
  // it gets cleaned up to prevent dangling ptrs. This needs to be done even
  // when the overlay state is kOff because the overlay may have been used for
  // contextual suggestions.
  GetLensOverlayQueryController()->ResetPageContentData();

  if (state_ == State::kOff) {
    return;
  }

  state_ = State::kClosing;

  // Closes preselection toast if it exists.
  ClosePreselectionBubble();

  // A permission prompt may be suspended if the overlay was showing when the
  // permission was queued. Restore the suspended prompt if possible.
  // TODO(crbug.com/331940245): Refactor to be decoupled from
  // PermissionPromptFactory
  content::WebContents* contents = tab_->GetContents();
  CHECK(contents);
  auto* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(contents);
  if (permission_request_manager &&
      permission_request_manager->CanRestorePrompt()) {
    permission_request_manager->RestorePrompt();
  }

  pre_initialization_objects_.reset();
  pre_initialization_text_.reset();

  side_panel_shown_subscription_ = base::CallbackListSubscription();

  // Re-enable mouse and keyboard events to the tab contents web view.
  auto* contents_web_view =
      BrowserElementsViews::From(tab_->GetBrowserWindowInterface())
          ->RetrieveView(kActiveContentsWebViewRetrievalId);
  CHECK(contents_web_view);
  contents_web_view->SetEnabled(true);

  if (overlay_web_view_) {
    // Remove render frame observer.
    overlay_web_view_->GetWebContents()
        ->GetPrimaryMainFrame()
        ->GetProcess()
        ->RemoveObserver(this);
  }

  initialization_data_.reset();

  tab_contents_view_observer_.Reset();
  omnibox_tab_helper_observer_.Reset();
  find_tab_observer_.Reset();
  receiver_.reset();
  page_.reset();
  languages_controller_.reset();
  scoped_tab_modal_ui_.reset();
  pending_region_.reset();
  fullscreen_observation_.Reset();
  immersive_mode_observer_.Reset();
  lens_overlay_blur_layer_delegate_.reset();
  pref_change_registrar_.Reset();
  use_aim_for_visual_search_ = false;

  // Notify the searchbox controller to reset its handlers before the overlay
  // is cleaned up. This is needed to prevent a dangling ptr.
  GetLensSearchboxController()->ResetOverlaySearchboxHandler();

  // Cleanup all of the lens overlay related views. The overlay view is owned by
  // the browser view and is reused for each Lens overlay session. Clean it up
  // so it is ready for the next invocation.
  if (overlay_view_) {
    overlay_view_->RemoveChildViewT(
        std::exchange(preselection_widget_anchor_, nullptr));
    overlay_view_->RemoveChildViewT(std::exchange(overlay_web_view_, nullptr));
    MaybeHideSharedOverlayView();
    overlay_view_ = nullptr;
  }

  lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;

  NotifyIsOverlayShowing(false);
  state_ = State::kOff;

  // Update the entrypoints now that the controller is closed.
  UpdateEntryPointsState();

  if (lens::features::IsLensOverlayNonBlockingPrivacyNoticeEnabled() &&
      !lens::DidUserGrantLensOverlayNeededPermissions(pref_service_) &&
      !user_interacted_without_accepting_privacy_notice) {
    lens::RecordNonBlockingPrivacyNoticeAccepted(
        lens::LensOverlayNonBlockingPrivacyNoticeUserAction::
            kClosedWithoutAccepting,
        invocation_source_);
  }
}

// static
const std::u16string LensOverlayController::GetFilenameForURL(const GURL& url) {
  if (!url.has_host() || url.HostIsIPAddress()) {
    return u"screenshot.png";
  }

  return base::ASCIIToUTF16(
      base::StrCat({"screenshot_", url.GetHost(), ".png"}));
}

void LensOverlayController::BindOverlay(
    mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensPage> page) {
  if (state_ != State::kStartingWebUI) {
    return;
  }
  receiver_.Bind(std::move(receiver));
  page_.Bind(std::move(page));

  InitializeOverlay(/*initialization_data=*/nullptr);
  lens::RecordTimeToWebuiBound(base::TimeTicks::Now() -
                               invocation_time_for_webui_binding_);
}

void LensOverlayController::SetInvocationTimeForWebUIBinding(
    base::TimeTicks invocation_time_for_webui_binding) {
  invocation_time_for_webui_binding_ = invocation_time_for_webui_binding;
}

void LensOverlayController::ClearRegionSelection() {
  if (!IsOverlayActive()) {
    return;
  }
  lens_search_controller_->ClearVisualSelectionThumbnail();
  lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;
  initialization_data_->selected_region_.reset();
  initialization_data_->selected_region_bitmap_.reset();
  page_->ClearRegionSelection();
}

uint64_t LensOverlayController::GetInvocationTimeSinceEpoch() {
  return invocation_time_since_epoch_.InMillisecondsSinceUnixEpoch();
}

lens::LensOverlayBlurLayerDelegate*
LensOverlayController::GetLensOverlayBlurLayerDelegateForTesting() {
  return lens_overlay_blur_layer_delegate_.get();
}

views::View* LensOverlayController::GetOverlayViewForTesting() {
  return overlay_view_.get();
}

views::WebView* LensOverlayController::GetOverlayWebViewForTesting() {
  return overlay_web_view_.get();
}

void LensOverlayController::SendText(lens::mojom::TextPtr text) {
  if (!page_) {
    // Store the text to send once the page is bound.
    pre_initialization_text_ = std::move(text);
    return;
  }
  page_->TextReceived(std::move(text));
}

void LensOverlayController::SendRegionText(lens::mojom::TextPtr text,
                                           bool is_injected_image) {
  if (!page_) {
    return;
  }
  page_->RegionTextReceived(std::move(text), is_injected_image);
}

lens::mojom::OverlayThemePtr LensOverlayController::CreateTheme(
    lens::PaletteId palette_id) {
  CHECK(base::Contains(lens::kPaletteColors, palette_id));
  const auto& palette = lens::kPaletteColors.at(palette_id);
  auto theme = lens::mojom::OverlayTheme::New();
  theme->primary = palette.at(lens::ColorId::kPrimary);
  theme->shader_layer_1 = palette.at(lens::ColorId::kShaderLayer1);
  theme->shader_layer_2 = palette.at(lens::ColorId::kShaderLayer2);
  theme->shader_layer_3 = palette.at(lens::ColorId::kShaderLayer3);
  theme->shader_layer_4 = palette.at(lens::ColorId::kShaderLayer4);
  theme->shader_layer_5 = palette.at(lens::ColorId::kShaderLayer5);
  theme->scrim = palette.at(lens::ColorId::kScrim);
  theme->surface_container_highest_light =
      palette.at(lens::ColorId::kSurfaceContainerHighestLight);
  theme->surface_container_highest_dark =
      palette.at(lens::ColorId::kSurfaceContainerHighestDark);
  theme->selection_element = palette.at(lens::ColorId::kSelectionElement);
  return theme;
}

void LensOverlayController::SendObjects(
    std::vector<lens::mojom::OverlayObjectPtr> objects) {
  if (!page_) {
    // Store the objects to send once the page is bound.
    pre_initialization_objects_ = std::move(objects);
    return;
  }
  page_->ObjectsReceived(std::move(objects));
}

void LensOverlayController::NotifyResultsPanelOpened() {
  if (page_) {
    page_->NotifyResultsPanelOpened();
  }
}

void LensOverlayController::TriggerCopy() {
  // This prevents a race condition where the overlay is closed as a keyboard
  // event is being processed.
  if (!page_) {
    return;
  }
  page_->OnCopyCommand();
}

bool LensOverlayController::IsOverlayShowing() const {
  return state_ == State::kStartingWebUI || state_ == State::kOverlay;
}

bool LensOverlayController::IsOverlayActive() const {
  return IsOverlayShowing() || state_ == State::kHidden ||
         state_ == State::kHiding || state_ == State::kIsReshowing;
}

bool LensOverlayController::HasRegionSelection() const {
  return initialization_data_ &&
         !initialization_data_->selected_region_.is_null();
}

bool LensOverlayController::IsOverlayInitializing() {
  return state_ == State::kStartingWebUI || state_ == State::kScreenshot ||
         state_ == State::kClosingOpenedSidePanel;
}

bool LensOverlayController::IsOverlayClosing() {
  return state_ == State::kClosing;
}

bool LensOverlayController::IsScreenshotPossible(
    content::RenderWidgetHostView* view) {
  return view && view->IsSurfaceAvailableForCopy();
}

tabs::TabInterface* LensOverlayController::GetTabInterface() {
  return tab_;
}

void LensOverlayController::IssueLensRegionRequestForTesting(
    lens::mojom::CenterRotatedBoxPtr region,
    bool is_click) {
  IssueLensRegionRequest(std::move(region), is_click);
}

void LensOverlayController::IssueTextSelectionRequestForTesting(
    const std::string& text_query,
    int selection_start_index,
    int selection_end_index,
    bool is_translate) {
  IssueTextSelectionRequest(text_query, selection_start_index,
                            selection_end_index, is_translate);
}

void LensOverlayController::
    RecordUkmAndTaskCompletionForLensOverlayInteractionForTesting(
        lens::mojom::UserAction user_action) {
  RecordUkmAndTaskCompletionForLensOverlayInteraction(user_action);
}

void LensOverlayController::RecordSemanticEventForTesting(
    lens::mojom::SemanticEvent event) {
  RecordLensOverlaySemanticEvent(event);
}

void LensOverlayController::IssueSearchBoxRequestForTesting(
    base::Time query_start_time,
    const std::string& search_box_text,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion,
    std::map<std::string, std::string> additional_query_params) {
  IssueSearchBoxRequest(query_start_time, search_box_text, match_type,
                        is_zero_prefix_suggestion, additional_query_params,
                        std::nullopt);
}

void LensOverlayController::IssueTranslateSelectionRequestForTesting(
    const std::string& text_query,
    const std::string& content_language,
    int selection_start_index,
    int selection_end_index) {
  IssueTranslateSelectionRequest(text_query, content_language,
                                 selection_start_index, selection_end_index);
}

void LensOverlayController::IssueMathSelectionRequestForTesting(
    const std::string& query,
    const std::string& formula,
    int selection_start_index,
    int selection_end_index) {
  IssueMathSelectionRequest(query, formula, selection_start_index,
                            selection_end_index);
}

void LensOverlayController::IssueTranslateFullPageRequestForTesting(
    const std::string& source_language,
    const std::string& target_language) {
  IssueTranslateFullPageRequest(source_language, target_language);
}

void LensOverlayController::IssueEndTranslateModeRequestForTesting() {
  IssueEndTranslateModeRequest();
}

void LensOverlayController::IssueTranslateFullPageRequest(
    const std::string& source_language,
    const std::string& target_language) {
  MaybeGrantLensOverlayPermissionsForSession();
  // Remove the selection thumbnail, if it exists.
  lens_search_controller_->ClearVisualSelectionThumbnail();
  ClearRegionSelection();
  // Set the coachmark text.
  if (preselection_widget_) {
    // This cast is safe since we know the widget delegate will always be a
    // `lens::LensPreselectionBubble`.
    auto* bubble_view = static_cast<lens::LensPreselectionBubble*>(
        preselection_widget_->widget_delegate());
    bubble_view->SetLabelText(
        IDS_LENS_OVERLAY_INITIAL_TOAST_MESSAGE_SELECT_TEXT);
  }
  // Set the translate options on initialization data in case we need to
  // re-enable translate mode later.
  initialization_data_->translate_options_ =
      lens::TranslateOptions(source_language, target_language);

  GetLensOverlayQueryController()->SendFullPageTranslateQuery(source_language,
                                                              target_language);
  MaybeLaunchSurvey();
}

void LensOverlayController::IssueEndTranslateModeRequest() {
  // Reset the coachmark text back to default.
  if (preselection_widget_) {
    // This cast is safe since we know the widget delegate will always be a
    // `lens::LensPreselectionBubble`.
    auto* bubble_view = static_cast<lens::LensPreselectionBubble*>(
        preselection_widget_->widget_delegate());
    bubble_view->SetLabelText(IDS_LENS_OVERLAY_INITIAL_TOAST_MESSAGE);
  }
  lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;
  initialization_data_->selected_text_.reset();
  initialization_data_->translate_options_.reset();
  GetLensOverlayQueryController()->SendEndTranslateModeQuery();
}

void LensOverlayController::NotifyOverlayInitialized() {
  // Now that the overlay is actually showing, it is safe to start doing a Lens
  // request without showing the page reflowing.
  if (pending_region_) {
    // If there is a pending region (i.e. for image right click)
    // use INJECTED_IMAGE as the selection type.
    IssueLensRequest(/*query_start_time=*/invocation_time_since_epoch_,
                     std::move(pending_region_), lens::INJECTED_IMAGE,
                     pending_region_bitmap_);
    pending_region_bitmap_.reset();
  }
}

void LensOverlayController::CopyText(const std::string& text) {
  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
  clipboard_writer.WriteText(base::UTF8ToUTF16(text));
}

void LensOverlayController::CopyImage(lens::mojom::CenterRotatedBoxPtr region) {
  if (initialization_data_->initial_screenshot_.drawsNothing()) {
    return;
  }

  SkBitmap cropped = lens::CropBitmapToRegion(
      initialization_data_->initial_screenshot_, std::move(region));
  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
  clipboard_writer.WriteImage(cropped);
}

void LensOverlayController::RecordUkmAndTaskCompletionForLensOverlayInteraction(
    lens::mojom::UserAction user_action) {
  ukm::SourceId source_id =
      tab_->GetContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  ukm::builders::Lens_Overlay_Overlay_UserAction(source_id)
      .SetUserAction(static_cast<int64_t>(user_action))
      .Record(ukm::UkmRecorder::Get());
  GetLensQueryFlowRouter()->SendTaskCompletionGen204IfEnabled(user_action);
}

void LensOverlayController::RecordLensOverlaySemanticEvent(
    lens::mojom::SemanticEvent event) {
  GetLensQueryFlowRouter()->SendSemanticEventGen204IfEnabled(event);
}

void LensOverlayController::SaveAsImage(
    lens::mojom::CenterRotatedBoxPtr region) {
  SkBitmap cropped = lens::CropBitmapToRegion(
      initialization_data_->initial_screenshot_, std::move(region));
  const GURL data_url = GURL(webui::GetBitmapDataUrl(cropped));
  content::DownloadManager* download_manager =
      tab_->GetBrowserWindowInterface()->GetProfile()->GetDownloadManager();
  net::NetworkTrafficAnnotationTag traffic_annotation =
      net::DefineNetworkTrafficAnnotation("lens_overlay_save", R"(
      semantics {
        sender: "Lens Overlay"
        description:
          "The user may capture a selection of the current screenshot in the "
          "Lens overlay via a button in the overlay. The resulting image is "
          "saved from a data URL to the disk on the local client."
        trigger: "User clicks 'Save as image' in the Lens Overlay after "
           "activating the Lens Overlay and making a selection on the "
           "screenshot."
        data: "A capture of a portion of a screenshot of the current page."
        destination: LOCAL
        last_reviewed: "2024-08-23"
        user_data {
          type: WEB_CONTENT
        }
        internal {
          contacts {
            owners: "//chrome/browser/ui/lens/OWNERS"
          }
        }
      }
      policy {
        cookies_allowed: NO
        setting:
          "No user-visible setting for this feature. Configured via Finch."
        policy_exception_justification:
          "This is not a network request."
      })");
  std::unique_ptr<download::DownloadUrlParameters> params =
      content::DownloadRequestUtils::CreateDownloadForWebContentsMainFrame(
          overlay_web_view_->GetWebContents(), data_url, traffic_annotation);
  params->set_prompt(true);
  params->set_suggested_name(
      GetFilenameForURL(tab_->GetContents()->GetLastCommittedURL()));
  download_manager->DownloadUrl(std::move(params));
}

void LensOverlayController::MaybeShowTranslateFeaturePromo() {
  auto* tracker = ui::ElementTracker::GetElementTracker();
  translate_button_shown_subscription_ =
      tracker->AddElementShownInAnyContextCallback(
          kLensOverlayTranslateButtonElementId,
          base::BindRepeating(
              &LensOverlayController::TryShowTranslateFeaturePromo,
              weak_factory_.GetWeakPtr()));
}

void LensOverlayController::MaybeCloseTranslateFeaturePromo(
    bool feature_engaged) {
  if (auto* const interface =
          BrowserUserEducationInterface::MaybeGetForWebContentsInTab(
              tab_->GetContents())) {
    if (!interface->IsFeaturePromoActive(
            feature_engagement::kIPHLensOverlayTranslateButtonFeature)) {
      // Do nothing if feature promo is not active.
      return;
    }

    if (feature_engaged) {
      interface->NotifyFeaturePromoFeatureUsed(
          feature_engagement::kIPHLensOverlayTranslateButtonFeature,
          FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
    } else {
      interface->AbortFeaturePromo(
          feature_engagement::kIPHLensOverlayTranslateButtonFeature);
    }
  }
}

void LensOverlayController::FetchSupportedLanguages(
    FetchSupportedLanguagesCallback callback) {
  CHECK(languages_controller_);
  languages_controller_->SendGetSupportedLanguagesRequest(std::move(callback));
}

void LensOverlayController::FinishReshowOverlay() {
  if (state_ != State::kIsReshowing) {
    return;
  }

  if (lens_overlay_blur_layer_delegate_) {
    content::RenderWidgetHost* live_page_widget_host =
        tab_->GetContents()
            ->GetPrimaryMainFrame()
            ->GetRenderViewHost()
            ->GetWidget();
    lens_overlay_blur_layer_delegate_->Show(live_page_widget_host);
  }
  SetOverlayWebViewOpacity(1.0f);
  state_ = State::kOverlay;
}

void LensOverlayController::TryShowTranslateFeaturePromo(
    ui::TrackedElement* element) {
  if (!element) {
    return;
  }

  if (auto* const interface =
          BrowserUserEducationInterface::MaybeGetForWebContentsInTab(
              tab_->GetContents())) {
    interface->MaybeShowFeaturePromo(
        feature_engagement::kIPHLensOverlayTranslateButtonFeature);
  }
}

std::string LensOverlayController::GetInvocationSourceString() {
  return lens::InvocationSourceToString(invocation_source_);
}

content::WebContents*
LensOverlayController::GetSidePanelWebContentsForTesting() {
  return GetLensOverlaySidePanelCoordinator()->GetSidePanelWebContents();
}

const GURL& LensOverlayController::GetPageURLForTesting() {
  return lens_search_controller_->GetPageURL();
}

SessionID LensOverlayController::GetTabIdForTesting() {
  return GetLensSearchboxController()->GetTabId();
}

metrics::OmniboxEventProto::PageClassification
LensOverlayController::GetPageClassificationForTesting() {
  return GetPageClassification();
}

const std::string& LensOverlayController::GetThumbnailForTesting() {
  return GetLensSearchboxController()->GetThumbnail();
}

void LensOverlayController::OnTextModifiedForTesting() {
  GetLensSearchboxController()->OnTextModified();
}

void LensOverlayController::OnThumbnailRemovedForTesting() {
  GetLensSearchboxController()->OnThumbnailRemoved();
}

void LensOverlayController::OnFocusChangedForTesting(bool focused) {
  GetLensSearchboxController()->OnFocusChanged(focused);
}

void LensOverlayController::OnZeroSuggestShownForTesting() {
  OnZeroSuggestShown();
}

void LensOverlayController::OpenSidePanelForTesting() {
  MaybeOpenSidePanel();
}

lens::proto::LensOverlaySuggestInputs
LensOverlayController::GetLensSuggestInputsForTesting() {
  return GetLensSearchboxController()->GetLensSuggestInputs();
}

bool LensOverlayController::IsUrlEligibleForTutorialIPHForTesting(
    const GURL& url) {
  return IsUrlEligibleForTutorialIPH(url);
}

void LensOverlayController::ShowUI(
    lens::LensOverlayInvocationSource invocation_source) {
  // If UI is already showing or in the process of showing, do nothing.
  if (state_ != State::kOff && state_ != State::kHidden) {
    return;
  }

  // The UI should only show if the tab is in the foreground or if the tab web
  // contents is not in a crash state.
  if (!tab_->IsActivated() || tab_->GetContents()->IsCrashed()) {
    return;
  }

  // If a different tab-modal is showing, do nothing.
  if (!tab_->CanShowModalUI()) {
    return;
  }

  if (lens::features::IsLensOverlayNonBlockingPrivacyNoticeEnabled() &&
      !lens::DidUserGrantLensOverlayNeededPermissions(pref_service_)) {
    lens::RecordNonBlockingPrivacyNoticeToBeShown(invocation_source);
  }

  // If the side panel is already showing, then visual selections
  // are considered follow ups that should be fulfilled by AIM.
  use_aim_for_visual_search_ = IsResultsSidePanelShowing();

  // Increment the counter for the number of times the Lens Overlay has been
  // started.
  int lens_overlay_start_count =
      pref_service_->GetInteger(prefs::kLensOverlayStartCount);
  pref_service_->SetInteger(prefs::kLensOverlayStartCount,
                            lens_overlay_start_count + 1);

  // Store reference for later use.
  invocation_source_ = invocation_source;

  Profile* profile =
      Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext());
  auto* const side_panel_ui =
      tab_->GetBrowserWindowInterface()->GetFeatures().side_panel_ui();
  CHECK(side_panel_ui);

  // Create the languages controller.
  languages_controller_ =
      std::make_unique<lens::LensOverlayLanguagesController>(profile);

  // Setup observer to be notified of side panel opens and closes.
  SidePanelEntry::PanelType panel_type =
      GetLensOverlaySidePanelCoordinator()->GetPanelType();
  side_panel_shown_subscription_ = side_panel_ui->RegisterSidePanelShown(
      panel_type,
      base::BindRepeating(&LensOverlayController::OnSidePanelDidOpen,
                          weak_factory_.GetWeakPtr()));

  if (find_in_page::FindTabHelper* const find_tab_helper =
          find_in_page::FindTabHelper::FromWebContents(tab_->GetContents())) {
    find_tab_observer_.Observe(find_tab_helper);
  }

  if (!omnibox_tab_helper_observer_.IsObserving()) {
    if (auto* helper = OmniboxTabHelper::FromWebContents(tab_->GetContents())) {
      omnibox_tab_helper_observer_.Observe(helper);
    }
  }

  // This is safe because we checked if another modal was showing above.
  scoped_tab_modal_ui_ = tab_->ShowModalUI();
  fullscreen_observation_.Observe(tab_->GetBrowserWindowInterface()
                                      ->GetExclusiveAccessManager()
                                      ->fullscreen_controller());

  // The preselection widget can cover top Chrome in immersive fullscreen.
  // Observer the reveal state to hide the widget when top Chrome is shown.
  immersive_mode_observer_.Observe(
      ImmersiveModeController::From(tab_->GetBrowserWindowInterface()));

  pref_change_registrar_.Init(pref_service_);
#if BUILDFLAG(IS_MAC)
  // Add observer to listen for changes in the always show toolbar state,
  // since that requires the preselection bubble to rerender to show properly.
  pref_change_registrar_.Add(
      prefs::kShowFullscreenToolbar,
      base::BindRepeating(
          &LensOverlayController::CloseAndReshowPreselectionBubble,
          base::Unretained(this)));
#endif  // BUILDFLAG(IS_MAC)
  pref_change_registrar_.Add(
      prefs::kSidePanelHorizontalAlignment,
      base::BindRepeating(&LensOverlayController::OnSidePanelAlignmentChanged,
                          base::Unretained(this)));

  NotifyUserEducationAboutOverlayUsed();

  // Establish data required for session metrics.
  invocation_time_ = base::TimeTicks::Now();
  invocation_time_since_epoch_ = base::Time::Now();
  ocr_dom_similarity_recorded_in_session_ = false;

  // This should be the last thing called in ShowUI, so if something goes wrong
  // in capturing the screenshot, the state gets cleaned up correctly.
  if (side_panel_ui->IsSidePanelShowing(panel_type) &&
      !IsResultsSidePanelShowing()) {
    // Close the currently opened side panel synchronously if it's not the Lens
    // panel. Postpone the screenshot for a fixed time to allow reflow.
    state_ = State::kClosingOpenedSidePanel;
    side_panel_ui->Close(panel_type, SidePanelEntryHideReason::kSidePanelClosed,
                         /*suppress_animations=*/true);
    base::SingleThreadTaskRunner::GetCurrentDefault()
        ->PostNonNestableDelayedTask(
            FROM_HERE,
            base::BindOnce(
                &LensOverlayController::FinishedWaitingForReflow,
                weak_factory_.GetWeakPtr(),
                std::make_optional<base::TimeTicks>(base::TimeTicks::Now())),
            kReflowWaitTimeout);
  } else {
    state_ = State::kScreenshot;
    content::RenderWidgetHostView* view = tab_->GetContents()
                                              ->GetPrimaryMainFrame()
                                              ->GetRenderViewHost()
                                              ->GetWidget()
                                              ->GetView();
    // During initialization and shutdown a capture may not be possible.
    if (!IsScreenshotPossible(view)) {
      lens_search_controller_->CloseLensSync(
          lens::LensOverlayDismissalSource::kErrorScreenshotCreationFailed);
      return;
    }

    GetContextualizationController()->StartScreenshotFlow(base::BindOnce(
        &LensOverlayController::OnScreenshotTaken, weak_factory_.GetWeakPtr(),
        std::make_optional<base::TimeTicks>(base::TimeTicks::Now())));
  }
}

void LensOverlayController::IssueTextSearchRequest(
    std::string query_text,
    std::map<std::string, std::string> additional_query_parameters,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion,
    lens::LensOverlayInvocationSource invocation_source) {
  IssueTextSearchRequestInner(
      /*query_start_time=*/base::Time::Now(), query_text,
      additional_query_parameters, match_type, is_zero_prefix_suggestion,
      invocation_source);
}

void LensOverlayController::IssueTextSearchRequestInner(
    base::Time query_start_time,
    std::string query_text,
    std::map<std::string, std::string> additional_query_parameters,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion,
    lens::LensOverlayInvocationSource invocation_source) {
  // Ignore the request if the overlay is off or closing.
  if (IsOverlayClosing()) {
    return;
  }

  // If the overlay is off, turn it on so the request can be fulfilled.
  if (state_ == State::kOff) {
    // TODO(crbug.com/439082713) Decouple the contextualization controller from
    // the overlay controller so that the overlay controller is less dependent
    // on it for proper functioning.

    // If the contextualization controller was already initialized then
    // there is no need to call `StartContextualization` again.
    if (GetContextualizationController()->IsActive()) {
      GetContextualizationController()->TryUpdatePageContextualization(
          base::BindOnce(
              &LensOverlayController::OnPageContextUpdatedForSuggestion,
              weak_factory_.GetWeakPtr(), query_start_time, query_text,
              additional_query_parameters, match_type,
              is_zero_prefix_suggestion, invocation_source));
      return;
    }

    GetContextualizationController()->StartContextualization(
        invocation_source,
        base::BindOnce(
            &LensOverlayController::OnPageContextUpdatedForSuggestion,
            weak_factory_.GetWeakPtr(), query_start_time, query_text,
            additional_query_parameters, match_type, is_zero_prefix_suggestion,
            invocation_source));
    return;
  }

  if (IsOverlayInitializing()) {
    // Hold the request until the overlay has finished initializing.
    pending_contextual_search_request_ =
        base::BindOnce(&LensOverlayController::IssueTextSearchRequestInner,
                       weak_factory_.GetWeakPtr(), query_start_time, query_text,
                       additional_query_parameters, match_type,
                       is_zero_prefix_suggestion, invocation_source);
    return;
  } else if (state_ != State::kOff) {
    // If the state is not off or initializing, the Lens sessions should already
    // be initialized. This means the page could have changed since the last
    // contextualization so it should be updated before issuing the contextual
    // search request.
    GetContextualizationController()->TryUpdatePageContextualization(
        base::BindOnce(
            &LensOverlayController::OnPageContextUpdatedForSuggestion,
            weak_factory_.GetWeakPtr(), query_start_time, query_text,
            additional_query_parameters, match_type, is_zero_prefix_suggestion,
            invocation_source));
    return;
  }

  IssueSearchBoxRequest(query_start_time, query_text,
                        AutocompleteMatch::Type::SEARCH_SUGGEST,
                        /*is_zero_prefix_suggestion=*/false,
                        additional_query_parameters, invocation_source);
}

void LensOverlayController::ShowUIWithPendingRegion(
    lens::LensOverlayInvocationSource invocation_source,
    lens::mojom::CenterRotatedBoxPtr region,
    const SkBitmap& region_bitmap) {
  pending_region_ = std::move(region);
  pending_region_bitmap_ = region_bitmap;
  ShowUI(invocation_source);
  // Overrides value set in ShowUI since invoking lens overlay with a pending
  // region is considered a search.
  GetLensSessionMetricsLogger()->OnSearchPerformed();
}

std::string LensOverlayController::GetVsridForNewTab() {
  return GetLensOverlayQueryController()->GetVsridForNewTab();
}

void LensOverlayController::SetTranslateMode(
    std::optional<lens::TranslateOptions> translate_options) {
  if (!page_) {
    return;
  }

  if (translate_options.has_value()) {
    page_->SetTranslateMode(translate_options->source_language,
                            translate_options->target_language);
  } else {
    // If the overlay was previously in translate mode, send a
    // request to end translate mode so the WebUI can update its state.
    if (initialization_data_->translate_options_.has_value()) {
      IssueEndTranslateModeRequest();
      GetLensOverlaySidePanelCoordinator()->SetSidePanelIsLoadingResults(true);
    }
    // Disable translate mode by setting source and target languages to empty
    // strings. This is a no-op if translate mode is already disabled.
    page_->SetTranslateMode(std::string(), std::string());
  }
  // Store the latest translate options.
  initialization_data_->translate_options_ = translate_options;
}

void LensOverlayController::SetTextSelection(int32_t selection_start_index,
                                             int32_t selection_end_index) {
  page_->SetTextSelection(selection_start_index, selection_end_index);
  initialization_data_->selected_text_ =
      std::make_pair(selection_start_index, selection_end_index);
}

void LensOverlayController::SetPostRegionSelection(
    lens::mojom::CenterRotatedBoxPtr box) {
  page_->SetPostRegionSelection(box->Clone());
  initialization_data_->selected_region_ = std::move(box);
}

void LensOverlayController::SetAdditionalSearchQueryParams(
    std::map<std::string, std::string> additional_search_query_params) {
  initialization_data_->additional_search_query_params_ =
      additional_search_query_params;
}

void LensOverlayController::ClearTextSelection() {
  if (!IsOverlayShowing()) {
    return;
  }
  if (initialization_data_->selected_text_.has_value()) {
    initialization_data_->selected_text_.reset();
    page_->ClearTextSelection();
  }
}

void LensOverlayController::OnSearchboxFocusChanged(bool focused) {
  if (!focused) {
    return;
  }

  if (IsContextualSearchbox()) {
    GetLensSessionMetricsLogger()->OnSearchboxFocused();

    if (state() == State::kHidden) {
      // If the live page is showing and the searchbox becomes focused, showing
      // intent to issue a new query, upload the new page content for
      // contextualization.
      GetContextualizationController()->TryUpdatePageContextualization(
          base::BindOnce(&LensOverlayController::NotifyPageContentUpdated,
                         weak_factory_.GetWeakPtr()));
    }
  }
}

void LensOverlayController::OnZeroSuggestShown() {
  if (!IsContextualSearchbox()) {
    return;
  }

  GetLensSessionMetricsLogger()->OnZeroSuggestShown(
      /*is_initial_query=*/state() == State::kOverlay);
}

void LensOverlayController::IssueLensRequest(
    base::Time query_start_time,
    lens::mojom::CenterRotatedBoxPtr region,
    lens::LensOverlaySelectionType selection_type,
    std::optional<SkBitmap> region_bytes) {
  CHECK(initialization_data_);
  CHECK(region);
  GetLensSearchboxController()->SetSearchboxInputText(std::string());
  initialization_data_->selected_region_ = region.Clone();
  initialization_data_->selected_text_.reset();
  initialization_data_->additional_search_query_params_.clear();
  lens_selection_type_ = selection_type;
  if (region_bytes) {
    initialization_data_->selected_region_bitmap_ = region_bytes.value();
    lens_search_controller_->HandleThumbnailCreatedBitmap(region_bytes.value());
  } else {
    initialization_data_->selected_region_bitmap_.reset();
  }

  if (GetContextualizationController()->GetCurrentPageContextEligibility()) {
    GetLensQueryFlowRouter()->SendRegionSearch(
        query_start_time, region.Clone(), selection_type,
        initialization_data_->additional_search_query_params_, region_bytes,
        invocation_source_);
  }

  if (invocation_source_ ==
      lens::LensOverlayInvocationSource::kContextualTasksComposebox) {
    return;
  }

  MaybeOpenSidePanel();
  GetLensSessionMetricsLogger()->RecordTimeToFirstInteraction(
      lens::LensOverlayFirstInteractionType::kRegionSelect);

  // TODO(crbug.com/428208291): The overlay can be in the hidden state and this
  // could be a query coming from the back stack which would make setting this
  // to kOverlay incorrect. Check if the overlay is currently in that state to
  // determine if this should be set to kOverlay or not. This should be fixed by
  // moving the functionality to make Lens requests to a more appropriate
  // location.
  if (state_ != State::kHidden) {
    state_ = State::kOverlay;
    MaybeLaunchSurvey();
  }
}

void LensOverlayController::IssueMultimodalRequest(
    base::Time query_start_time,
    lens::mojom::CenterRotatedBoxPtr region,
    const std::string& text_query,
    lens::LensOverlaySelectionType selection_type,
    std::optional<SkBitmap> region_bitmap) {
  if (GetContextualizationController()->GetCurrentPageContextEligibility()) {
    GetLensQueryFlowRouter()->SendMultimodalRequest(
        query_start_time, std::move(region), text_query, selection_type,
        initialization_data_->additional_search_query_params_, region_bitmap,
        invocation_source_);
  }

  if (invocation_source_ ==
      lens::LensOverlayInvocationSource::kContextualTasksComposebox) {
    return;
  }
}

void LensOverlayController::IssueSearchBoxRequest(
    base::Time query_start_time,
    const std::string& search_box_text,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion,
    std::map<std::string, std::string> additional_query_params,
    std::optional<lens::LensOverlayInvocationSource> invocation_source) {
  MaybeGrantLensOverlayPermissionsForSession();
  // Log the interaction time here so the time to fetch new page bytes is not
  // intcluded.
  GetLensSessionMetricsLogger()
      ->RecordContextualSearchboxTimeToInteractionAfterNavigation();
  GetLensSessionMetricsLogger()->RecordTimeToFirstInteraction(
      lens::LensOverlayFirstInteractionType::kSearchbox);

  // Do not attempt to contextualize if CSB is disabled, if recontextualization
  // on each query is disabled, if the live page is not being displayed, or if
  // the user is not in the contextual search flow (aka, issues an image request
  // already).
  if (!lens::IsLensOverlayContextualSearchboxEnabled() ||
      !lens::features::ShouldLensOverlayRecontextualizeOnQuery() ||
      state() != State::kHidden || !IsContextualSearchbox()) {
    IssueSearchBoxRequestPart2(query_start_time, search_box_text, match_type,
                               is_zero_prefix_suggestion,
                               additional_query_params, invocation_source);
    return;
  }

  // If contextual searchbox is enabled, make sure the page bytes are current
  // prior to issuing the search box request.
  GetContextualizationController()->TryUpdatePageContextualization(
      base::BindOnce(&LensOverlayController::IssueSearchBoxRequestPart2,
                     weak_factory_.GetWeakPtr(), query_start_time,
                     search_box_text, match_type, is_zero_prefix_suggestion,
                     additional_query_params, invocation_source));
}

void LensOverlayController::IssueContextualTextRequest(
    base::Time query_start_time,
    const std::string& text_query,
    lens::LensOverlaySelectionType selection_type) {
  if (GetContextualizationController()->GetCurrentPageContextEligibility()) {
    lens_selection_type_ = selection_type;
    GetLensQueryFlowRouter()->SendContextualTextQuery(
        query_start_time, text_query, selection_type,
        initialization_data_->additional_search_query_params_,
        invocation_source_);
  }
}

void LensOverlayController::AddOverlayStateToSearchQuery(
    lens::SearchQuery& search_query) {
  if (!initialization_data_) {
    return;
  }

  // In the case where a query was triggered by a selection on the overlay or
  // use of the searchbox, initialization_data_ and
  // additional_search_query_params_ will have already been set. Record that
  // state in a search query struct.
  if (initialization_data_->selected_region_) {
    search_query.selected_region_ =
        initialization_data_->selected_region_->Clone();
  }
  if (!initialization_data_->selected_region_bitmap_.drawsNothing()) {
    search_query.selected_region_bitmap_ =
        initialization_data_->selected_region_bitmap_;
  }
  if (initialization_data_->selected_text_.has_value()) {
    search_query.selected_text_ = initialization_data_->selected_text_.value();
  }
  if (initialization_data_->translate_options_.has_value()) {
    search_query.translate_options_ =
        initialization_data_->translate_options_.value();
  }
  search_query.lens_selection_type_ = lens_selection_type_;
  search_query.additional_search_query_params_ =
      initialization_data_->additional_search_query_params_;
}

LensOverlayController::OverlayInitializationData::OverlayInitializationData(
    const SkBitmap& screenshot,
    SkBitmap rgb_screenshot,
    lens::PaletteId color_palette,
    GURL page_url,
    std::optional<std::string> page_title)
    : initial_screenshot_(screenshot),
      initial_rgb_screenshot_(std::move(rgb_screenshot)),
      updated_screenshot_(screenshot),
      color_palette_(color_palette),
      page_url_(page_url),
      page_title_(page_title) {}
LensOverlayController::OverlayInitializationData::~OverlayInitializationData() =
    default;

void LensOverlayController::CreateInitializationData(
    const SkBitmap& screenshot,
    const std::vector<gfx::Rect>& all_bounds,
    std::optional<uint32_t> pdf_current_page) {
  // Create the new RGB bitmap async to prevent the main thread from blocking on
  // the encoding.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&CreateRgbBitmap, screenshot),
      base::BindOnce(
          &LensOverlayController::ContinueCreateInitializationData,
          weak_factory_.GetWeakPtr(), screenshot, all_bounds, pdf_current_page,
          std::make_optional<base::TimeTicks>(base::TimeTicks::Now())));
}

void LensOverlayController::ContinueCreateInitializationData(
    const SkBitmap& screenshot,
    const std::vector<gfx::Rect>& all_bounds,
    std::optional<uint32_t> pdf_current_page,
    std::optional<base::TimeTicks> screenshot_bitmap_start_time,
    SkBitmap rgb_screenshot) {
  if (screenshot_bitmap_start_time.has_value()) {
    lens::RecordTimeToCreateScreenshotBitmap(
        base::TimeTicks::Now() - screenshot_bitmap_start_time.value());
  }
  if (state_ != State::kStartingWebUI || rgb_screenshot.drawsNothing()) {
    // TODO(b/334185985): Handle case when screenshot RGB encoding fails.
    lens_search_controller_->CloseLensSync(
        lens::LensOverlayDismissalSource::kErrorScreenshotEncodingFailed);
    return;
  }

  // Resolve the color palette based on the vibrant screenshot color.
  lens::PaletteId color_palette = lens::PaletteId::kFallback;
  if (lens::features::IsDynamicThemeDetectionEnabled()) {
    std::vector<SkColor> colors;
    for (const auto& pair : lens::kPalettes) {
      colors.emplace_back(pair.first);
    }
    SkColor screenshot_color = lens::ExtractVibrantOrDominantColorFromImage(
        screenshot, lens::features::DynamicThemeMinPopulationPct());
    SkColor theme_color = lens::FindBestMatchedColorOrTransparent(
        colors, screenshot_color, lens::features::DynamicThemeMinChroma());
    if (theme_color != SK_ColorTRANSPARENT) {
      color_palette = lens::kPalettes.at(theme_color);
    }
  }

  auto initialization_data = std::make_unique<OverlayInitializationData>(
      screenshot, std::move(rgb_screenshot), color_palette,
      lens_search_controller_->GetPageURL(),
      lens_search_controller_->GetPageTitle());
  initialization_data->significant_region_boxes_ =
      ConvertSignificantRegionBoxes(all_bounds);
  initialization_data->last_retrieved_most_visible_page_ = pdf_current_page;

  GetContextualizationController()->GetPageContextualization(base::BindOnce(
      &LensOverlayController::StorePageContentAndContinueInitialization,
      weak_factory_.GetWeakPtr(), std::move(initialization_data),
      std::make_optional<base::TimeTicks>(base::TimeTicks::Now())));
}

void LensOverlayController::StorePageContentAndContinueInitialization(
    std::unique_ptr<OverlayInitializationData> initialization_data,
    std::optional<base::TimeTicks> page_context_start_time,
    std::vector<lens::PageContent> page_contents,
    lens::MimeType primary_content_type,
    std::optional<uint32_t> page_count) {
  if (page_context_start_time.has_value()) {
    lens::RecordTimeToGetPageContext(base::TimeTicks::Now() - invocation_time_);
  }
  initialization_data->page_contents_ = page_contents;
  initialization_data->primary_content_type_ = primary_content_type;
  initialization_data->pdf_page_count_ = page_count;
  InitializeOverlay(std::move(initialization_data));

  // TODO(crbug.com/428208291): Remove this in favor of the contextualization
  // controller correctly setting the page content and recording the document
  // metrics.
  GetContextualizationController()->SetPageContent(page_contents,
                                                   primary_content_type);
  GetContextualizationController()->RecordDocumentMetrics(page_count);
}

std::vector<lens::mojom::CenterRotatedBoxPtr>
LensOverlayController::ConvertSignificantRegionBoxes(
    const std::vector<gfx::Rect>& all_bounds) {
  std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes;
  int max_regions = lens::features::GetLensOverlayMaxSignificantRegions();
  if (max_regions == 0) {
    return significant_region_boxes;
  }
  content::RenderFrameHost* render_frame_host =
      tab_->GetContents()->GetPrimaryMainFrame();
  auto view_bounds = render_frame_host->GetView()->GetViewBounds();
  for (auto& image_bounds : all_bounds) {
    // Check the original area of the images against the minimum area.
    if (image_bounds.width() * image_bounds.height() >=
        lens::features::GetLensOverlaySignificantRegionMinArea()) {
      // We only have bounds for images in the main frame of the tab (i.e. not
      // in iframes), so view bounds are identical to tab bounds and can be
      // used for both parameters.
      significant_region_boxes.emplace_back(
          lens::GetCenterRotatedBoxFromTabViewAndImageBounds(
              view_bounds, view_bounds, image_bounds));
    }
  }
  // If an image is outside the viewpoint, the box will have zero area.
  std::erase_if(significant_region_boxes, [](const auto& box) {
    return box->box.height() == 0 || box->box.width() == 0;
  });
  // Sort by descending area.
  std::sort(significant_region_boxes.begin(), significant_region_boxes.end(),
            [](const auto& box1, const auto& box2) {
              return box1->box.height() * box1->box.width() >
                     box2->box.height() * box2->box.width();
            });
  // Treat negative values of max_regions as no limit.
  if (max_regions > 0 &&
      significant_region_boxes.size() > (unsigned long)max_regions) {
    significant_region_boxes.resize(max_regions);
  }

  return significant_region_boxes;
}

void LensOverlayController::SuppressGhostLoader() {
  if (page_) {
    page_->SuppressGhostLoader();
  }
  GetLensOverlaySidePanelCoordinator()->SuppressGhostLoader();
}

void LensOverlayController::SetLiveBlur(bool enabled) {
  if (!lens_overlay_blur_layer_delegate_) {
    return;
  }

  if (enabled) {
    lens_overlay_blur_layer_delegate_->StartBackgroundImageCapture();
    return;
  }

  lens_overlay_blur_layer_delegate_->StopBackgroundImageCapture();
}

void LensOverlayController::ShowOverlay() {
  auto* contents_web_view =
      BrowserElementsViews::From(tab_->GetBrowserWindowInterface())
          ->RetrieveView(kActiveContentsWebViewRetrievalId);
  CHECK(contents_web_view);

  NotifyIsOverlayShowing(true);
  // If the view already exists, we just need to reshow it.
  if (overlay_view_) {
    // Restore the state to show the overlay.
    overlay_view_->SetVisible(true);
    preselection_widget_anchor_->SetVisible(true);
    overlay_web_view_->SetVisible(true);
    SetOverlayRoundedCorner();

    // Restart the live blur since the view is visible again.
    SetLiveBlur(should_enable_live_blur_on_show_);

    // The overlay needs to be focused on show to immediately begin
    // receiving key events.
    overlay_web_view_->RequestFocus();

    // Disable mouse and keyboard inputs to the tab contents web view. Do this
    // after the overlay takes focus. If it is done before, focus will move from
    // the contents web view to another Chrome UI element before the overlay can
    // take focus.
    contents_web_view->SetEnabled(false);
    return;
  }

  // Create the views that will house our UI.
  overlay_view_ = CreateViewForOverlay();
  overlay_view_->SetVisible(true);
  SetOverlayRoundedCorner();

  // Sanity check that the overlay view is above the contents web view.
  auto* parent_view = overlay_view_->parent();
  views::View* child_contents_view = contents_web_view;
  // TODO(crbug.com/443102583): Remove this block if overlay_view_ ends up
  // getting reparented such that it always shares a parent with
  // contents_web_view.
  if (base::FeatureList::IsEnabled(features::kSideBySide)) {
    // When split view is enabled, there are two additional layers of
    // hierarchy:
    // BrowserView->MultiContentsView->ContentsContainerView->ContentsWebView
    // vs.
    // BrowserView->ContentsWebView
    // Since the overlay view is parented by BrowserView, to properly pass the
    // check below, we should only compare direct children of BrowserView.
    child_contents_view = child_contents_view->parent()->parent();
  }
  CHECK(parent_view->GetIndexOf(overlay_view_) >
        parent_view->GetIndexOf(child_contents_view));

  // Observe the overlay view to handle resizing the background blur layer.
  tab_contents_view_observer_.Observe(overlay_view_);

  // The overlay needs to be focused on show to immediately begin
  // receiving key events.
  CHECK(overlay_web_view_);
  overlay_web_view_->RequestFocus();

  // Disable mouse and keyboard inputs to the tab contents web view. Do this
  // after the overlay takes focus. If it is done before, focus will move from
  // the contents web view to another Chrome UI element before the overlay can
  // take focus.
  contents_web_view->SetEnabled(false);

  // Listen to the render process housing out overlay.
  overlay_web_view_->GetWebContents()
      ->GetPrimaryMainFrame()
      ->GetProcess()
      ->AddObserver(this);
}

void LensOverlayController::MaybeHideSharedOverlayView() {
  if (!overlay_view_) {
    return;
  }
  for (views::View* child : overlay_view_->children()) {
    if (child->GetVisible()) {
      // If any child is visible, it is being used by another tab so do not hide
      // the overlay view.
      return;
    }
  }
  overlay_view_->SetVisible(false);
}

void LensOverlayController::MaybeOpenSidePanel() {
  // If Lens in contextual tasks is enabled, the side panel is opened by the
  // contextual tasks side panel UI service rather than the overlay controller.
  if (lens_search_controller_->should_route_to_contextual_tasks()) {
    return;
  }

  GetLensOverlaySidePanelCoordinator()->RegisterEntryAndShow();
}

void LensOverlayController::InitializeOverlay(
    std::unique_ptr<OverlayInitializationData> initialization_data) {
  // Initialization data is ready.
  if (initialization_data) {
    initialization_data_ = std::move(initialization_data);
  }

  // We can only continue once both the WebUI is bound and the initialization
  // data is processed and ready. If either of those conditions aren't met, we
  // exit early and wait for the other condition to call this method again.
  if (!page_ || !initialization_data_) {
    return;
  }

  // Move the data that was stored prior to initialization into
  // initialization_data_.
  if (pre_initialization_objects_.has_value()) {
    initialization_data_->objects_ =
        std::move(pre_initialization_objects_.value());
    pre_initialization_objects_.reset();
  }
  if (pre_initialization_text_.has_value()) {
    initialization_data_->text_ = std::move(pre_initialization_text_.value());
    pre_initialization_text_.reset();
  }

  InitializeOverlayUI(*initialization_data_);
  base::UmaHistogramBoolean("Lens.Overlay.Shown", true);

  // Show the preselection overlay now that the overlay is initialized and ready
  // to be shown.
  if (!pending_region_ && !IsResultsSidePanelShowing()) {
    ShowPreselectionBubble();
  }

  // Create the blur delegate so it is ready to blur once the view is visible.
  if (lens::features::GetLensOverlayUseBlur()) {
    content::RenderWidgetHost* live_page_widget_host =
        tab_->GetContents()
            ->GetPrimaryMainFrame()
            ->GetRenderViewHost()
            ->GetWidget();
    lens_overlay_blur_layer_delegate_ =
        std::make_unique<lens::LensOverlayBlurLayerDelegate>(
            live_page_widget_host);
  }

  state_ = State::kOverlay;
  lens_search_controller_->NotifyOverlayOpened();

  // Update the entry points state to ensure that the entry points are disabled
  // now that the overlay is showing.
  UpdateEntryPointsState();

  // Only start the query flow again if there is no full image response and the
  // side panel is not open. The side panel being open indicates that a full
  // image response could have been received and not passed to the overlay.
  if (GetLensQueryFlowRouter()->IsOff()) {
    if (!GetContextualizationController()->GetCurrentPageContextEligibility()) {
      initialization_data_->initial_screenshot_ = SkBitmap();
      initialization_data_->page_url_ = GURL();
      initialization_data_->page_title_ = "";
    }

    GetLensQueryFlowRouter()->StartQueryFlow(
        initialization_data_->initial_screenshot_,
        initialization_data_->page_url_, initialization_data_->page_title_,
        std::move(initialization_data_->significant_region_boxes_),
        initialization_data_->page_contents_,
        initialization_data_->primary_content_type_,
        initialization_data_->last_retrieved_most_visible_page_,
        GetUiScaleFactor(), invocation_time_);

#if BUILDFLAG(ENABLE_PDF)
    // TODO(crbug.com/418825720): When StorePageContentAndContinueInitialization
    // is called, the contextualization controller does not update its own
    // contextualization. In this case, the partial PDF text should be sent here
    // if it is available.
    if (initialization_data_->primary_content_type_ == lens::MimeType::kPdf) {
      GetContextualizationController()
          ->FetchVisiblePageIndexAndGetPartialPdfText(
              initialization_data_->pdf_page_count_.value(),
              base::BindOnce(
                  &LensOverlayController::OnPdfPartialPageTextRetrieved,
                  weak_factory_.GetWeakPtr()));
    }
#endif
  } else {
    // If the query flow is already started, update the page content with
    // the new viewport.
    GetContextualizationController()->UpdatePageContext(
        initialization_data_->page_contents_,
        initialization_data_->primary_content_type_,
        initialization_data_->pdf_page_count_,
        initialization_data_->initial_screenshot_,
        initialization_data_->last_retrieved_most_visible_page_);
  }

  // If there is a pending contextual search request, issue it now that the
  // overlay is initialized.
  if (pending_contextual_search_request_) {
    std::move(pending_contextual_search_request_).Run();
  }

  // TODO(b/352622136): We should not start the lens request until the overlay
  // is open to prevent the side panel from opening while the overlay UI is
  // rendering.
  if (pending_region_) {
    // If there is a pending region (i.e. for image right click)
    // use INJECTED_IMAGE as the selection type.
    IssueLensRequest(invocation_time_since_epoch_, std::move(pending_region_),
                     lens::INJECTED_IMAGE, pending_region_bitmap_);
    pending_region_bitmap_.reset();
  }

  GetContextualizationController()->TryCalculateAndRecordOcrDomSimilarity();
}

void LensOverlayController::InitializeOverlayUI(
    const OverlayInitializationData& init_data) {
  // This should only contain LensPage mojo calls and should not affect
  // `state_`.
  CHECK(page_);
  // TODO(b/371593619), it would be more efficent to send all initialization
  // data to the overlay web UI in a single message.
  page_->ThemeReceived(CreateTheme(init_data.color_palette_));

  auto* lens_session_metrics_logger = GetLensSessionMetricsLogger();

  lens_session_metrics_logger->OnContextualSearchboxShown();
  lens_session_metrics_logger->OnInitialPageContentRetrieved(
      /*page_content_type=*/init_data.page_contents_.empty()
          ? lens::MimeType::kUnknown
          : init_data.primary_content_type_);

  // Only show the CSB if the results side panel is not open.
  bool is_side_panel_open = IsResultsSidePanelShowing();
  page_->ShouldShowContextualSearchBox(!is_side_panel_open);
  // If should show CSB, and the CSB viewport thumbnail is enabled, send it now.
  if (lens::features::GetVisualSelectionUpdatesEnableCsbThumbnail()) {
    lens_search_controller_->HandleThumbnailCreatedBitmap(
        init_data.initial_screenshot_);
  }

  // Send the initial document type to the overlay web UI.
  NotifyPageContentUpdated();

  page_->ScreenshotDataReceived(init_data.initial_rgb_screenshot_,
                                is_side_panel_open);
  if (!init_data.objects_.empty()) {
    SendObjects(CopyObjects(init_data.objects_));
  }
  if (init_data.text_) {
    SendText(init_data.text_->Clone());
  }
  if (pending_region_) {
    page_->SetPostRegionSelection(pending_region_->Clone());
  }
  if (lens_search_controller_->IsHandshakeComplete()) {
    // Notify the overlay that it is safe to query autocomplete.
    page_->NotifyHandshakeComplete();
  }
}

bool LensOverlayController::IsContextualSearchbox() {
  return lens_search_controller_->lens_searchbox_controller()
      ->IsContextualSearchbox();
}

raw_ptr<views::View> LensOverlayController::CreateViewForOverlay() {
  // Grab the host view for the overlay which is owned by the browser view.
  auto* const host_view =
      BrowserElementsViews::From(tab_->GetBrowserWindowInterface())
          ->GetView(kLensOverlayViewElementId);
  CHECK(host_view);

  // Setup a preselection anchor view. Usually bubbles are anchored to top
  // chrome, but top chrome is not always visible when our overlay is visible.
  // Instead of anchroing to top chrome, we anchor to this view because 1) it
  // always exists when the overlay exists and 2) it is before the WebView in
  // the view hierarchy and therefore will receive focus first when tabbing from
  // top chrome.
  std::unique_ptr<views::View> anchor_view = std::make_unique<views::View>();
  anchor_view->SetFocusBehavior(views::View::FocusBehavior::NEVER);
  preselection_widget_anchor_ = host_view->AddChildView(std::move(anchor_view));

  // Create the web view.
  std::unique_ptr<views::WebView> web_view = std::make_unique<views::WebView>(
      tab_->GetContents()->GetBrowserContext());
  content::WebContents* web_view_contents = web_view->GetWebContents();
  web_view->SetProperty(views::kElementIdentifierKey, kOverlayId);
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_view_contents, SK_ColorTRANSPARENT);

  // Set the label for the renderer process in Chrome Task Manager.
  task_manager::WebContentsTags::CreateForToolContents(
      web_view_contents, IDS_LENS_OVERLAY_RENDERER_LABEL);

  // As the embedder for the lens overlay WebUI content we must set the
  // appropriate tab interface here.
  webui::SetTabInterface(web_view_contents, GetTabInterface());

  // Set the web contents delegate to this controller so we can handle keyboard
  // events. Allow accelerators (e.g. hotkeys) to work on this web view.
  web_view->set_allow_accelerators(true);
  web_view->GetWebContents()->SetDelegate(this);

  // Load the untrusted WebUI into the web view.
  web_view->LoadInitialURL(GURL(chrome::kChromeUILensOverlayUntrustedURL));

  overlay_web_view_ = host_view->AddChildView(std::move(web_view));
  return host_view;
}

bool LensOverlayController::HandleContextMenu(
    content::RenderFrameHost& render_frame_host,
    const content::ContextMenuParams& params) {
  // We do not want to show the browser context menu on the overlay unless we
  // are in debugging mode. Returning true is equivalent to not showing the
  // context menu.
  return !lens::features::IsLensOverlayDebuggingEnabled();
}

bool LensOverlayController::HandleKeyboardEvent(
    content::WebContents* source,
    const input::NativeWebKeyboardEvent& event) {
  // This can be called before the overlay web view is attached to the overlay
  // view. In that case, the focus manager could be null.
  if (!overlay_web_view_ || !overlay_web_view_->GetFocusManager()) {
    return false;
  }
  return lens_search_controller_->lens_overlay_event_handler()
      ->HandleKeyboardEvent(source, event,
                            overlay_web_view_->GetFocusManager());
}

void LensOverlayController::OnFullscreenStateChanged() {
  // Flag is enabled to allow Lens Overlay in fullscreen no matter what so we
  // can exit early.
  if (lens::features::GetLensOverlayEnableInFullscreen()) {
    return;
  }
  // If there is top chrome we can keep the overlay open.
  if (tab_->GetBrowserWindowInterface()->IsTabStripVisible()) {
    return;
  }
  lens_search_controller_->CloseLensSync(
      lens::LensOverlayDismissalSource::kFullscreened);
}

void LensOverlayController::OnViewBoundsChanged(views::View* observed_view) {
  CHECK(observed_view == overlay_view_);

  // Set our view to the same bounds as the contents web view so it always
  // covers the tab contents.
  if (lens_overlay_blur_layer_delegate_) {
    // Set the blur to have the same bounds as our view, but since it is in our
    // views local coordinate system, the blur should be positioned at (0,0).
    lens_overlay_blur_layer_delegate_->layer()->SetBounds(
        overlay_view_->GetLocalBounds());
  }
}

#if BUILDFLAG(IS_MAC)
void LensOverlayController::OnWidgetActivationChanged(views::Widget* widget,
                                                      bool active) {
  if (active && preselection_widget_) {
    // On Mac, traversing out of the preselection widget into the browser causes
    // the browser to restore its focus to the wrong place. Thus, when entering
    // the preselection widget, make sure to clear out the browser's native
    // focus. This causes the preselection widget to lose activation, so
    // reactivate it manually.
    BrowserElementsViews::From(tab_->GetBrowserWindowInterface())
        ->GetPrimaryWindowWidget()
        ->GetFocusManager()
        ->ClearNativeFocus();
    preselection_widget_->Activate();
  }
}
#endif

void LensOverlayController::OnWidgetDestroying(views::Widget* widget) {
  preselection_widget_ = nullptr;
  preselection_widget_observer_.Reset();
}

void LensOverlayController::OnOmniboxFocusChanged(
    OmniboxFocusState state,
    OmniboxFocusChangeReason reason) {
  if (state_ != State::kOverlay || IsResultsSidePanelShowing()) {
    return;
  }

  if (state == OMNIBOX_FOCUS_NONE) {
    ShowPreselectionBubble();
  } else {
    HidePreselectionBubble();
  }
}

void LensOverlayController::OnFindEmptyText(
    content::WebContents* web_contents) {
  if (state_ == State::kHidden) {
    return;
  }
  lens_search_controller_->CloseLensAsync(
      lens::LensOverlayDismissalSource::kFindInPageInvoked);
}

void LensOverlayController::OnFindResultAvailable(
    content::WebContents* web_contents) {
  if (state_ == State::kHidden) {
    return;
  }
  lens_search_controller_->CloseLensAsync(
      lens::LensOverlayDismissalSource::kFindInPageInvoked);
}

void LensOverlayController::OnImmersiveRevealStarted() {
  // The toolbar has began to reveal. If the overlay is showing, hide the
  // preselection bubble to ensure it doesn't cover with the toolbar UI.
  if (IsOverlayShowing()) {
    HidePreselectionBubble();
  }
}

void LensOverlayController::OnImmersiveRevealEnded() {
  // The toolbar is no longer revealed. If the overlay is showing, reshow the
  // preselection bubble to ensure it doesn't cover with the toolbar UI.
  if (IsOverlayShowing()) {
    ShowPreselectionBubble();
  }
}

void LensOverlayController::OnImmersiveFullscreenEntered() {
  // The browser entered immersive fullscreen. If the overlay is showing, call
  // close and reopen the preselection bubble to ensure it respositions
  // correctly.
  if (IsOverlayShowing()) {
    CloseAndReshowPreselectionBubble();
  }
}

void LensOverlayController::OnImmersiveFullscreenExited() {
  // The browser exited immersive fullscreen. If the overlay is showing, call
  // close and reopen the preselection bubble to ensure it respositions
  // correctly.
  if (IsOverlayShowing()) {
    CloseAndReshowPreselectionBubble();
  }
}

void LensOverlayController::OnHandshakeComplete() {
  CHECK(lens_search_controller_->IsHandshakeComplete());
  // Notify the overlay that the handshake is complete if its initialized.
  if (page_) {
    page_->NotifyHandshakeComplete();
  }
}

metrics::OmniboxEventProto::PageClassification
LensOverlayController::GetPageClassification() const {
  return lens_search_controller_->lens_searchbox_controller()
      ->GetPageClassification();
}

float LensOverlayController::GetUiScaleFactor() {
  int device_scale_factor =
      tab_->GetContents()->GetRenderWidgetHostView()->GetDeviceScaleFactor();
  float page_scale_factor =
      zoom::ZoomController::FromWebContents(tab_->GetContents())
          ->GetZoomPercent() /
      100.0f;
  return device_scale_factor * page_scale_factor;
}

void LensOverlayController::OnSidePanelDidOpen() {
  if (IsResultsSidePanelShowing()) {
    SetOverlayRoundedCorner();
  } else {
    // If a side panel opens that is not ours, we must close the overlay.
    lens_search_controller_->CloseLensSync(
        lens::LensOverlayDismissalSource::kUnexpectedSidePanelOpen);
  }
}

void LensOverlayController::SetOverlayRoundedCorner() {
  CHECK(overlay_view_ && overlay_web_view_);
  if (!base::FeatureList::IsEnabled(features::kSideBySide)) {
    return;
  }

  const bool should_round_corner = IsResultsSidePanelShowing();
  const float radius =
      should_round_corner
          ? overlay_web_view_->GetLayoutProvider()->GetCornerRadiusMetric(
                views::ShapeContextTokens::kContentSeparatorRadius)
          : 0;
  const bool right_aligned =
      pref_service_->GetBoolean(prefs::kSidePanelHorizontalAlignment);
  const gfx::RoundedCornersF radii = gfx::RoundedCornersF{
      right_aligned ? 0 : radius, right_aligned ? radius : 0, 0, 0};

  overlay_web_view_->holder()->SetCornerRadii(radii);

  // If we show the overlay with overlay_view_ being painted to a layer,
  // there is a visual bug where the background is momentarily transparent,
  // causing flickering. When we don't want the corner to be rounded,
  // instead of setting the corner radii to 0, destroy the layer instead.
  // See crbug.com/437355402.
  if (!should_round_corner) {
    overlay_view_->DestroyLayer();
    return;
  }

  overlay_view_->SetPaintToLayer();
  overlay_view_->layer()->SetIsFastRoundedCorner(true);
  overlay_view_->layer()->SetRoundedCornerRadius(radii);
}

void LensOverlayController::FinishedWaitingForReflow(
    std::optional<base::TimeTicks> reflow_start_time) {
  if (state_ == State::kClosingOpenedSidePanel) {
    if (reflow_start_time.has_value()) {
      lens::RecordTimeToCloseOpenedSidePanel(base::TimeTicks::Now() -
                                             reflow_start_time.value());
    }
    // This path is invoked after the user invokes the overlay, but we needed
    // to close the side panel before taking a screenshot. The Side panel is
    // now closed so we can now take the screenshot of the page.
    state_ = State::kScreenshot;
    GetContextualizationController()->StartScreenshotFlow(base::BindOnce(
        &LensOverlayController::OnScreenshotTaken, weak_factory_.GetWeakPtr(),
        std::make_optional(base::TimeTicks::Now())));
  }
}

void LensOverlayController::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  // Exit early if the overlay is already closing.
  if (IsOverlayClosing()) {
    return;
  }

  // The overlay's primary main frame process has exited, either cleanly or
  // unexpectedly. Close the overlay so that the user does not get into a broken
  // state where the overlay cannot be dismissed. Note that RenderProcessExited
  // can be called during the destruction of a frame in the overlay, so it is
  // important to post a task to close the overlay to avoid double-freeing the
  // overlay's frames. See https://crbug.com/371643466.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(
          &LensSearchController::CloseLensSync,
          lens_search_controller_->GetWeakPtr(),
          info.status == base::TERMINATION_STATUS_NORMAL_TERMINATION
              ? lens::LensOverlayDismissalSource::kOverlayRendererClosedNormally
              : lens::LensOverlayDismissalSource::
                    kOverlayRendererClosedUnexpectedly));
}

void LensOverlayController::TabForegrounded(tabs::TabInterface* tab) {
  // Ignore the event if the overlay is not backgrounded.
  if (state_ != State::kBackground) {
    // If the side panel is open without the overlay, exit early to avoid
    // showing the overlay.
    return;
  }

  // If the overlay was backgrounded, restore the previous state.
  if (backgrounded_state_ != State::kHidden) {
    ShowOverlay();
  }
  if (!IsResultsSidePanelShowing() && backgrounded_state_ != State::kHidden) {
    ShowPreselectionBubble();
  }
  if (lens::IsLensOverlayContextualSearchboxEnabled()) {
    SuppressGhostLoader();
  }

  state_ = backgrounded_state_;
  UpdateEntryPointsState();
}

void LensOverlayController::TabWillEnterBackground(tabs::TabInterface* tab) {
  // If the current tab was already backgrounded, do nothing.
  if (state_ == State::kBackground) {
    DCHECK(state_ != State::kBackground) << "State should not be kBackground.";
    return;
  }

  // If the overlay is active, background it.
  if (IsOverlayActive()) {
    const bool is_in_transitional_state =
        state_ == State::kIsReshowing || state_ == State::kHiding;

    // If the overlay is in a transitional state, the state to restore to is
    // kHidden. Otherwise, restore to the current state.
    backgrounded_state_ = is_in_transitional_state ? State::kHidden : state_;

    // If the overlay UI is showing, hide it.
    if (overlay_web_view_ && overlay_web_view_->GetVisible()) {
      HideOverlay();
    }

    state_ = State::kBackground;
    UpdateEntryPointsState();

    // TODO(crbug.com/335516480): Schedule the UI to be suspended.
  }
}

void LensOverlayController::ActivityRequestedByOverlay(
    ui::mojom::ClickModifiersPtr click_modifiers) {
  // The tab is expected to be in the foreground.
  if (!tab_->IsActivated()) {
    return;
  }
  tab_->GetBrowserWindowInterface()->OpenGURL(
      GURL(lens::features::GetLensOverlayActivityURL()),
      ui::DispositionFromClick(
          click_modifiers->middle_button, click_modifiers->alt_key,
          click_modifiers->ctrl_key, click_modifiers->meta_key,
          click_modifiers->shift_key,
          WindowOpenDisposition::NEW_FOREGROUND_TAB));
}

void LensOverlayController::AddBackgroundBlur() {
  // We do not blur unless the overlay is currently active and the blur delegate
  // was created.
  if (!lens_overlay_blur_layer_delegate_ || (state_ != State::kOverlay)) {
    return;
  }

  // Add our blur layer to the view.
  overlay_web_view_->SetPaintToLayer();
  overlay_web_view_->layer()->Add(lens_overlay_blur_layer_delegate_->layer());
  overlay_web_view_->layer()->StackAtBottom(
      lens_overlay_blur_layer_delegate_->layer());
  lens_overlay_blur_layer_delegate_->layer()->SetBounds(
      overlay_web_view_->GetLocalBounds());

  lens_overlay_blur_layer_delegate_->FetchBackgroundImage();
}

void LensOverlayController::CloseRequestedByOverlayCloseButton() {
  lens_search_controller_->HideOverlay(
      lens::LensOverlayDismissalSource::kOverlayCloseButton);
}

void LensOverlayController::CloseRequestedByOverlayBackgroundClick() {
  lens_search_controller_->HideOverlay(
      lens::LensOverlayDismissalSource::kOverlayBackgroundClick);
}

void LensOverlayController::FeedbackRequestedByOverlay() {
  lens::FeedbackRequestedByEvent(tab_, ui::EF_NONE);
}

void LensOverlayController::GetOverlayInvocationSource(
    GetOverlayInvocationSourceCallback callback) {
  std::move(callback).Run(GetInvocationSourceString());
}

void LensOverlayController::InfoRequestedByOverlay(
    ui::mojom::ClickModifiersPtr click_modifiers) {
  // The tab is expected to be in the foreground.
  if (!tab_->IsActivated()) {
    return;
  }
  tab_->GetBrowserWindowInterface()->OpenGURL(
      GURL(lens::features::GetLensOverlayHelpCenterURL()),
      ui::DispositionFromClick(
          click_modifiers->middle_button, click_modifiers->alt_key,
          click_modifiers->ctrl_key, click_modifiers->meta_key,
          click_modifiers->shift_key,
          WindowOpenDisposition::NEW_FOREGROUND_TAB));
}

void LensOverlayController::IssueLensRegionRequest(
    lens::mojom::CenterRotatedBoxPtr region,
    bool is_click) {
  MaybeGrantLensOverlayPermissionsForSession();
  SkBitmap region_bitmap = lens::CropBitmapToRegion(
      initialization_data_->initial_screenshot_, region->Clone());
  IssueLensRequest(/*query_start_time=*/base::Time::Now(), std::move(region),
                   is_click ? lens::TAP_ON_EMPTY : lens::REGION_SEARCH,
                   region_bitmap);
}

void LensOverlayController::IssueLensObjectRequest(
    lens::mojom::CenterRotatedBoxPtr region,
    bool is_mask_click) {
  MaybeGrantLensOverlayPermissionsForSession();
  SkBitmap region_bitmap = lens::CropBitmapToRegion(
      initialization_data_->initial_screenshot_, region->Clone());
  IssueLensRequest(
      /*query_start_time=*/base::Time::Now(), std::move(region),
      is_mask_click ? lens::TAP_ON_REGION_GLEAM : lens::TAP_ON_OBJECT,
      region_bitmap);
}

void LensOverlayController::IssueTextSelectionRequest(const std::string& query,
                                                      int selection_start_index,
                                                      int selection_end_index,
                                                      bool is_translate) {
  MaybeGrantLensOverlayPermissionsForSession();
  initialization_data_->additional_search_query_params_.clear();
  lens_selection_type_ =
      is_translate ? lens::SELECT_TRANSLATED_TEXT : lens::SELECT_TEXT_HIGHLIGHT;

  IssueTextSelectionRequestInner(/*query_start_time=*/base::Time::Now(), query,
                                 selection_start_index, selection_end_index);
}

void LensOverlayController::IssueTranslateSelectionRequest(
    const std::string& query,
    const std::string& content_language,
    int selection_start_index,
    int selection_end_index) {
  MaybeGrantLensOverlayPermissionsForSession();
  initialization_data_->additional_search_query_params_.clear();
  lens::AppendTranslateParamsToMap(
      initialization_data_->additional_search_query_params_, query, "auto");
  lens_selection_type_ = lens::TRANSLATE_CHIP;

  IssueTextSelectionRequestInner(/*query_start_time=*/base::Time::Now(), query,
                                 selection_start_index, selection_end_index);
}

void LensOverlayController::IssueMathSelectionRequest(
    const std::string& query,
    const std::string& formula,
    int selection_start_index,
    int selection_end_index) {
  MaybeGrantLensOverlayPermissionsForSession();
  initialization_data_->additional_search_query_params_.clear();
  lens::AppendStickinessSignalForFormula(
      initialization_data_->additional_search_query_params_, formula);
  lens_selection_type_ = lens::SYMBOLIC_MATH_OBJECT;

  IssueTextSelectionRequestInner(/*query_start_time=*/base::Time::Now(), query,
                                 selection_start_index, selection_end_index);
}

void LensOverlayController::IssueTextSelectionRequestInner(
    base::Time query_start_time,
    const std::string& query,
    int selection_start_index,
    int selection_end_index) {
  initialization_data_->selected_region_.reset();
  initialization_data_->selected_region_bitmap_.reset();
  initialization_data_->selected_text_ =
      std::make_pair(selection_start_index, selection_end_index);

  GetLensSearchboxController()->SetSearchboxInputText(query);
  lens_search_controller_->ClearVisualSelectionThumbnail();

  GetLensQueryFlowRouter()->SendTextOnlyQuery(
      query_start_time, query, lens_selection_type_,
      initialization_data_->additional_search_query_params_,
      invocation_source_);
  MaybeOpenSidePanel();
  GetLensSessionMetricsLogger()->RecordTimeToFirstInteraction(
      lens::LensOverlayFirstInteractionType::kTextSelect);
  state_ = State::kOverlay;
  MaybeLaunchSurvey();
}

void LensOverlayController::ClosePreselectionBubble() {
  if (preselection_widget_) {
    preselection_widget_->Close();
    preselection_widget_ = nullptr;
    preselection_widget_observer_.Reset();
  }
}

void LensOverlayController::ShowPreselectionBubble() {
  // Don't show the preselection bubble if the overlay is not being shown.
  if (IsResultsSidePanelShowing()) {
    return;
  }

#if BUILDFLAG(IS_MAC)
  // On Mac, the kShowFullscreenToolbar pref is used to determine whether the
  // toolbar is always shown. This causes the toolbar to never unreveal, meaning
  // the preselection bubble will never be shown. Check for this case and show
  // the preselection bubble if needed.
  const bool always_show_toolbar =
      pref_service_->GetBoolean(prefs::kShowFullscreenToolbar);
#else
  const bool always_show_toolbar = false;
#endif  // BUILDFLAG(IS_MAC)

  if (!always_show_toolbar &&
      ImmersiveModeController::From(tab_->GetBrowserWindowInterface())
          ->IsRevealed()) {
    // If the immersive mode controller is revealing top chrome, do not show
    // the preselection bubble. The bubble will be shown once the reveal
    // finishes.
    return;
  }

  if (!preselection_widget_) {
    CHECK(preselection_widget_anchor_);
    // Setup the preselection widget.
    preselection_widget_ = views::BubbleDialogDelegateView::CreateBubble(
        std::make_unique<lens::LensPreselectionBubble>(
            weak_factory_.GetWeakPtr(), preselection_widget_anchor_,
            net::NetworkChangeNotifier::IsOffline(),
            /*exit_clicked_callback=*/
            base::BindRepeating(
                &LensSearchController::CloseLensSync,
                lens_search_controller_->GetWeakPtr(),
                lens::LensOverlayDismissalSource::kPreselectionToastExitButton),
            /*on_cancel_callback=*/
            base::BindOnce(&LensSearchController::CloseLensSync,
                           lens_search_controller_->GetWeakPtr(),
                           lens::LensOverlayDismissalSource::
                               kPreselectionToastEscapeKeyPress)));
    preselection_widget_->SetNativeWindowProperty(
        views::kWidgetIdentifierKey,
        const_cast<void*>(kLensOverlayPreselectionWidgetIdentifier));
    preselection_widget_observer_.Observe(preselection_widget_);
    // Setting the parent allows focus traversal out of the preselection widget.
    preselection_widget_->SetFocusTraversableParent(
        preselection_widget_anchor_->GetWidget()->GetFocusTraversable());
    preselection_widget_->SetFocusTraversableParentView(
        preselection_widget_anchor_);
  }

  // When in fullscreen, top Chrome may cover this widget on Mac. Set the
  // z-order to floating UI element to ensure the widget is above the top
  // Chrome. Only do this if immersive mode is enabled to avoid issues with
  // the preselection widget covering other windows.
  if (ImmersiveModeController::From(tab_->GetBrowserWindowInterface())
          ->IsEnabled()) {
    preselection_widget_->SetZOrderLevel(ui::ZOrderLevel::kFloatingUIElement);
  } else {
    preselection_widget_->SetZOrderLevel(ui::ZOrderLevel::kNormal);
  }

  auto* bubble_view = static_cast<lens::LensPreselectionBubble*>(
      preselection_widget_->widget_delegate());
  bubble_view->SetCanActivate(true);

  // The bubble position is dependent on if top chrome is showing. Resize the
  // bubble to ensure the correct position is used.
  bubble_view->SizeToContents();
  // Show inactive so that the overlay remains active.
  preselection_widget_->ShowInactive();
}

void LensOverlayController::CloseAndReshowPreselectionBubble() {
  // If the preselection bubble is already closed, do not reshow it.
  if (!preselection_widget_) {
    return;
  }
  ClosePreselectionBubble();
  ShowPreselectionBubble();
}

void LensOverlayController::HidePreselectionBubble() {
  if (preselection_widget_) {
    // The preselection bubble remains in the browser's focus order even when it
    // is hidden, for example, when another browser tab is active. This means it
    // remains possible for the bubble to be activated by keyboard input i.e.
    // tabbing into the bubble, which unhides the bubble even on a browser tab
    // where the overlay is not being shown. Prevent this by setting the bubble
    // to non-activatable while it is hidden.
    auto* bubble_view = static_cast<lens::LensPreselectionBubble*>(
        preselection_widget_->widget_delegate());
    bubble_view->SetCanActivate(false);

    preselection_widget_->Hide();
  }
}

void LensOverlayController::IssueSearchBoxRequestPart2(
    base::Time query_start_time,
    const std::string& search_box_text,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion,
    std::map<std::string, std::string> additional_query_params,
    std::optional<lens::LensOverlayInvocationSource> invocation_source) {
  // TODO(crbug.com/404941800): Re-add check for state == kOff once the
  // contextualization flow is fully decoupled from the overlay.
  // If the overlay is closing, do not attempt to issue the query.
  if (IsOverlayClosing()) {
    return;
  }
  initialization_data_->additional_search_query_params_ =
      additional_query_params;

  if (initialization_data_->selected_region_.is_null() &&
      GetPageClassification() ==
          metrics::OmniboxEventProto::SEARCH_SIDE_PANEL_SEARCHBOX) {
    // Non-Lens and non-contextual searches should not have a selection type.
    lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;
  } else if (is_zero_prefix_suggestion) {
    lens_selection_type_ = lens::MULTIMODAL_SUGGEST_ZERO_PREFIX;
  } else if (match_type == AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED) {
    lens_selection_type_ = lens::MULTIMODAL_SEARCH;
  } else {
    lens_selection_type_ = lens::MULTIMODAL_SUGGEST_TYPEAHEAD;
  }

  lens::LensOverlayInvocationSource final_source =
      invocation_source.value_or(invocation_source_);

  if (!GetContextualizationController()->GetCurrentPageContextEligibility()) {
    // Do not send any requests if the page is not context eligible.
  } else if (initialization_data_->selected_region_.is_null() &&
             IsContextualSearchbox()) {
    GetLensQueryFlowRouter()->SendContextualTextQuery(
        query_start_time, search_box_text, lens_selection_type_,
        initialization_data_->additional_search_query_params_, final_source);
    GetLensSessionMetricsLogger()->OnContextualSearchboxQueryIssued(
        is_zero_prefix_suggestion,
        /*is_initial_query=*/state_ == State::kOverlay);
  } else if (initialization_data_->selected_region_.is_null()) {
    GetLensQueryFlowRouter()->SendTextOnlyQuery(
        query_start_time, search_box_text, lens_selection_type_,
        initialization_data_->additional_search_query_params_, final_source);
  } else {
    std::optional<SkBitmap> selected_region_bitmap =
        initialization_data_->selected_region_bitmap_.drawsNothing()
            ? std::nullopt
            : std::make_optional<SkBitmap>(
                  initialization_data_->selected_region_bitmap_);
    GetLensQueryFlowRouter()->SendMultimodalRequest(
        query_start_time, initialization_data_->selected_region_.Clone(),
        search_box_text, lens_selection_type_,
        initialization_data_->additional_search_query_params_,
        selected_region_bitmap, final_source);
  }

  // If we are in the zero state, this request must have come from CSB. In that
  // case, hide the overlay to allow live page to show through.
  if (!IsResultsSidePanelShowing() && state_ == State::kOverlay) {
    HideOverlay();
    state_ = overlay_view_ ? State::kHidden : State::kOff;
  }

  // The searchbox text is set once the URL loads in the results frame, however,
  // adding it here allows the user to see the text query in the searchbox while
  // a long query loads.
  GetLensSearchboxController()->SetSearchboxInputText(search_box_text);

  MaybeOpenSidePanel();
  // Only set the side panel to loading if the page is context eligible because
  // otherwise there will be no results to load.
  bool is_context_eligible =
      lens_search_controller_->lens_search_contextualization_controller()
          ->GetCurrentPageContextEligibility();
  GetLensOverlaySidePanelCoordinator()->SetSidePanelIsLoadingResults(
      is_context_eligible);
  GetLensOverlaySidePanelCoordinator()->SetShowProtectedErrorPage(
      !is_context_eligible);

  // Update the entry points state here in case InitializeOverlay() was
  // bypassed by straight to SRP flows.
  UpdateEntryPointsState();

  MaybeLaunchSurvey();
}

void LensOverlayController::HandleStartQueryResponse(
    std::vector<lens::mojom::OverlayObjectPtr> objects,
    lens::mojom::TextPtr text,
    bool is_error) {
  // TODO(crbug.com/404941800): State can be off temporarily if the user
  // contextualizes via an entry point that does not show the overlay UI. Remove
  // the check for kOff after migrating the contextualization flow since this
  // function should not be called when the overlay is not open.
  if (state_ == State::kOff) {
    return;
  }

  // If the side panel is open, then the error page state can change depending
  // on whether the query succeeded or not. If the side panel is not open, the
  // error page state can only change if the query failed since the first side
  // panel navigation will take care of recording whether the result was shown.
  auto* side_panel_coordinator = GetLensOverlaySidePanelCoordinator();
  const bool is_side_panel_open = side_panel_coordinator->IsSidePanelBound();
  if (is_side_panel_open &&
      !side_panel_coordinator->IsShowingProtectedErrorPage()) {
    side_panel_coordinator->MaybeSetSidePanelShowErrorPage(
        is_error,
        is_error
            ? lens::mojom::SidePanelResultStatus::kErrorPageShownStartQueryError
            : lens::mojom::SidePanelResultStatus::kResultShown);
  } else if (!is_side_panel_open && is_error) {
    side_panel_coordinator->MaybeSetSidePanelShowErrorPage(
        /*should_show_error_page=*/true,
        lens::mojom::SidePanelResultStatus::kErrorPageShownStartQueryError);
  }

  if (!objects.empty()) {
    SendObjects(std::move(objects));
  }

  // Text can be null if there was no text within the server response.
  if (!text.is_null()) {
    // If the initialization data is not yet ready, SendText will store the text
    // to be attached when ready.
    if (initialization_data_) {
      initialization_data_->text_ = text.Clone();
    }

    SendText(std::move(text));

    // Try and record the OCR DOM similarity since the OCR text is now
    // available.
    GetContextualizationController()->TryCalculateAndRecordOcrDomSimilarity();
  }
}

void LensOverlayController::HandleInteractionURLResponse(
    lens::proto::LensOverlayUrlResponse response) {
  MaybeOpenSidePanel();
  auto* results_panel_router = GetLensResultsPanelRouter();

  if (use_aim_for_visual_search_ &&
      IsVisualSelectionType(lens_selection_type_) &&
      lens::features::GetEnableLensButtonInSearchbox()) {
    // Focus the side panel contents so the composebox can be properly focused.
    results_panel_router->FocusSearchbox();

    // The latest vsint is stored in the query controller. By returning here,
    // the URL will not be loaded into the side panel. Instead, when the user
    // makes a query in the composebox, the vsint will be sent to the results.
    return;
  }

  // This will become a no-op when the Next panel is enabled (as the
  // `side_panel_page_` in the Lens overlay side panel coordinator will not be
  // bound). In the future, the Lens query controller router (or similar) will
  // notify the side panel that it should load the interaction URL without
  // involving the overlay controller at all.
  auto* results_side_panel_coordinator = GetLensOverlaySidePanelCoordinator();
  results_side_panel_coordinator->SetLatestPageUrlWithResponse(
      GURL(response.page_url()));
  results_side_panel_coordinator->LoadURLInResultsFrame(GURL(response.url()));
}

void LensOverlayController::HandleInteractionResponse(
    lens::mojom::TextPtr text) {
  const bool is_injected_image = lens_selection_type_ == lens::INJECTED_IMAGE;
  SendRegionText(std::move(text), is_injected_image);
}

void LensOverlayController::HandlePageContentUploadProgress(uint64_t position,
                                                            uint64_t total) {
  // If the progress bar is not being shown for this upload, or if this user is
  // not making contextual queries, do not update the progress bar.
  if (!is_upload_progress_bar_shown_ || !IsContextualSearchbox()) {
    return;
  }

  float progress = total > 0 ? static_cast<float>(position) / total : 1.0f;

  // For the first upload handler event received, check if the progress is above
  // the heuristic threshold. If so, do not show the progress bar because it is
  // assumed that the upload will finish quickly, and showing the progress bar
  // would be distracting.
  if (is_first_upload_handler_event_) {
    is_first_upload_handler_event_ = false;
    if (total > 0 &&
        progress > lens::features::GetUploadProgressBarShowHeuristic()) {
      is_upload_progress_bar_shown_ = false;
      return;
    }
  }

  GetLensOverlaySidePanelCoordinator()->SetPageContentUploadProgress(
      total > 0 ? static_cast<float>(position) / total : 1.0f);
}

void LensOverlayController::HideOverlay() {
  // Re-enable mouse and keyboard events to the tab contents web view, and take
  // focus before the overlay view is hidden. If it is done after, focus will
  // move from the overlay view to another Chrome UI element before the contents
  // web view can take focus.
  auto* contents_web_view =
      BrowserElementsViews::From(tab_->GetBrowserWindowInterface())
          ->RetrieveView(kActiveContentsWebViewRetrievalId);
  CHECK(contents_web_view);
  contents_web_view->SetEnabled(true);
  contents_web_view->RequestFocus();

  // Hide the overlay view, but keep the web view attached to the overlay view
  // so that the overlay can be re-shown without creating a new web view.
  if (preselection_widget_anchor_) {
    preselection_widget_anchor_->SetVisible(false);
  }
  if (overlay_web_view_) {
    overlay_web_view_->SetVisible(false);
  }
  MaybeHideSharedOverlayView();

  // Save the current value of whether live blur is enabled so that it can be
  // restored when the overlay is shown again.
  if (lens_overlay_blur_layer_delegate_) {
    should_enable_live_blur_on_show_ =
        lens_overlay_blur_layer_delegate_->IsLiveBlurActive();
  }
  SetLiveBlur(false);
  HidePreselectionBubble();

  NotifyIsOverlayShowing(false);
}

void LensOverlayController::HideOverlayAndSetHiddenState() {
  if (state_ != State::kHiding) {
    return;
  }
  HideOverlay();
  state_ = State::kHidden;
}

void LensOverlayController::ReshowOverlay() {
  // The overlay must be in the kHidden state to be restored properly.
  CHECK(state_ == State::kHidden);
  state_ = State::kIsReshowing;
  use_aim_for_visual_search_ = true;

  // Clear any previous selections to ensure a clean state.
  ClearAllSelections();

  // Update the page contextualization.
  GetContextualizationController()->TryUpdatePageContextualization(
      base::BindOnce(&LensOverlayController::ReshowOverlayPart2,
                     weak_factory_.GetWeakPtr()));
}

void LensOverlayController::MaybeLaunchSurvey() {
  lens_search_controller_->MaybeLaunchSurvey();
}

void LensOverlayController::InitializeTutorialIPHUrlMatcher() {
  if (!base::FeatureList::IsEnabled(
          feature_engagement::kIPHLensOverlayFeature)) {
    return;
  }

  tutorial_iph_url_matcher_ = std::make_unique<url_matcher::URLMatcher>();
  base::MatcherStringPattern::ID id(0);
  url_matcher::util::AddFiltersWithLimit(
      tutorial_iph_url_matcher_.get(), true, &id,
      JSONArrayToVector(
          feature_engagement::kIPHLensOverlayUrlAllowFilters.Get()),
      &iph_url_filters_);
  url_matcher::util::AddFiltersWithLimit(
      tutorial_iph_url_matcher_.get(), false, &id,
      JSONArrayToVector(
          feature_engagement::kIPHLensOverlayUrlBlockFilters.Get()),
      &iph_url_filters_);

  auto force_allow_url_strings = JSONArrayToVector(
      feature_engagement::kIPHLensOverlayUrlForceAllowedUrlMatchPatterns.Get());
  std::vector<base::MatcherStringPattern> force_allow_url_patterns;
  std::vector<const base::MatcherStringPattern*> force_allow_url_pointers;
  force_allow_url_patterns.reserve(force_allow_url_strings.size());
  force_allow_url_pointers.reserve(force_allow_url_strings.size());
  for (const std::string& entry : force_allow_url_strings) {
    force_allow_url_patterns.emplace_back(entry, ++id);
    force_allow_url_pointers.push_back(&force_allow_url_patterns.back());
  }
  forced_url_matcher_ = std::make_unique<url_matcher::RegexSetMatcher>();
  // Pointers will not be referenced after AddPatterns() completes.
  forced_url_matcher_->AddPatterns(force_allow_url_pointers);

  auto allow_strings = JSONArrayToVector(
      feature_engagement::kIPHLensOverlayUrlPathMatchAllowPatterns.Get());
  std::vector<base::MatcherStringPattern> allow_patterns;
  std::vector<const base::MatcherStringPattern*> allow_pointers;
  allow_patterns.reserve(allow_strings.size());
  allow_pointers.reserve(allow_strings.size());
  for (const std::string& entry : allow_strings) {
    allow_patterns.emplace_back(entry, ++id);
    allow_pointers.push_back(&allow_patterns.back());
  }
  page_path_allow_matcher_ = std::make_unique<url_matcher::RegexSetMatcher>();
  // Pointers will not be referenced after AddPatterns() completes.
  page_path_allow_matcher_->AddPatterns(allow_pointers);

  auto block_strings = JSONArrayToVector(
      feature_engagement::kIPHLensOverlayUrlPathMatchBlockPatterns.Get());
  std::vector<base::MatcherStringPattern> block_patterns;
  std::vector<const base::MatcherStringPattern*> block_pointers;
  block_patterns.reserve(block_strings.size());
  block_pointers.reserve(block_strings.size());
  for (const std::string& entry : block_strings) {
    block_patterns.emplace_back(entry, ++id);
    block_pointers.push_back(&block_patterns.back());
  }
  page_path_block_matcher_ = std::make_unique<url_matcher::RegexSetMatcher>();
  // Pointers will not be referenced after AddPatterns() completes.
  page_path_block_matcher_->AddPatterns(block_pointers);
}

void LensOverlayController::MaybeShowDelayedTutorialIPH(const GURL& url) {
  auto* entry_point_controller = tab_->GetBrowserWindowInterface()
                                     ->GetFeatures()
                                     .lens_overlay_entry_point_controller();
  if (!entry_point_controller || !entry_point_controller->IsEnabled()) {
    return;
  }

  // If a tutorial IPH was already queued, cancel it.
  tutorial_iph_timer_.Stop();

  if (IsUrlEligibleForTutorialIPH(url)) {
    tutorial_iph_timer_.Start(
        FROM_HERE, feature_engagement::kIPHLensOverlayDelayTime.Get(),
        base::BindOnce(&LensOverlayController::ShowTutorialIPH,
                       weak_factory_.GetWeakPtr()));
  }
}

void LensOverlayController::UpdateNavigationMetrics() {
  GetLensSessionMetricsLogger()->OnPageNavigation();
}

void LensOverlayController::ClearAllSelections() {
  if (state_ == State::kOff) {
    return;
  }

  if (page_) {
    page_->ClearAllSelections();
  }
  initialization_data_->selected_region_.reset();
  initialization_data_->selected_region_bitmap_.reset();
  initialization_data_->selected_text_.reset();
  if (!IsContextualSearchbox()) {
    lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;
  }
}

void LensOverlayController::HandleRegionBitmapCreated(
    const SkBitmap& region_bitmap) {
  // Do not update the selected region bitmap if the overlay is off or if the
  // region bitmap is already set.
  if (state_ == State::kOff ||
      !initialization_data_->selected_region_bitmap_.drawsNothing()) {
    return;
  }

  initialization_data_->selected_region_bitmap_ = region_bitmap;
}

void LensOverlayController::OnSidePanelAlignmentChanged() {
  if (IsOverlayShowing()) {
    SetOverlayRoundedCorner();
  }
}

bool LensOverlayController::IsUrlEligibleForTutorialIPH(const GURL& url) {
  if (!tutorial_iph_url_matcher_) {
    return false;
  }

  // Check if the URL matches any of the allow filters. If it does not,
  // return false immediately as this should not be a shown match.
  auto matches = tutorial_iph_url_matcher_.get()->MatchURL(url);
  if (!matches.size()) {
    return false;
  }

  // Now that the URL is allowed, check if it matches any of the block filters.
  // If it does, return false as to block this URL from showing the IPH.
  for (auto match : matches) {
    // Blocks take precedence over allows.
    if (!iph_url_filters_[match].allow) {
      return false;
    }
  }

  // Now that the URL is an allowed URL, verify the match is not blocked by
  // the block matcher. If it does contain blocked words in its path, return
  // false to prevent the IPH from being shown.
  if (page_path_block_matcher_ && !page_path_block_matcher_->IsEmpty() &&
      page_path_block_matcher_->Match(url.GetPath(), &matches)) {
    return false;
  }

  // Check if the URL matches any of the forced allowed URLs. If it does, return
  // true as this should be a shown match even if the path does not contain an
  // allowlisted pattern (below).
  if (forced_url_matcher_ && !forced_url_matcher_->IsEmpty() &&
      forced_url_matcher_->Match(url.spec(), &matches)) {
    return true;
  }

  // Finally, check if the URL matches any of the allowed patterns. If it
  // doesn't, return false to prevent the IPH from being shown.
  if (page_path_allow_matcher_ && !page_path_allow_matcher_->IsEmpty() &&
      !page_path_allow_matcher_->Match(url.GetPath(), &matches)) {
    return false;
  }

  // Finally if all checks pass, this must be a valid match. I.e.:
  // 1. The URL matches at least one of the allowed URLs.
  // 2. The URL does not match any of the blocked URLs.
  // 3. The URL does not match any of the block path patterns.
  // 4. The URL matches at least one of the allowed path patterns.
  return true;
}

void LensOverlayController::ShowTutorialIPH() {
  if (auto* user_ed = BrowserUserEducationInterface::From(
          tab_->GetBrowserWindowInterface())) {
    user_ed->MaybeShowFeaturePromo(feature_engagement::kIPHLensOverlayFeature);
  }
}

void LensOverlayController::NotifyUserEducationAboutOverlayUsed() {
  if (auto* user_ed = BrowserUserEducationInterface::From(
          tab_->GetBrowserWindowInterface())) {
    user_ed->NotifyFeaturePromoFeatureUsed(
        feature_engagement::kIPHLensOverlayFeature,
        FeaturePromoFeatureUsedAction::kClosePromoIfPresent);
  }
}

void LensOverlayController::NotifyPageContentUpdated() {
  auto page_content_type = lens::StringMimeTypeToMojoPageContentType(
      tab_->GetContents()->GetContentsMimeType());
  if (page_) {
    page_->PageContentTypeChanged(page_content_type);
  }
  GetLensOverlaySidePanelCoordinator()->NotifyPageContentUpdated();
}

void LensOverlayController::UpdateEntryPointsState() {
  tab_->GetBrowserWindowInterface()
      ->GetFeatures()
      .lens_overlay_entry_point_controller()
      ->UpdateEntryPointsState(
          /*hide_toolbar_entrypoint=*/false);
}

void LensOverlayController::NotifyIsOverlayShowing(bool is_showing) {
  auto* results_panel_router = GetLensResultsPanelRouter();
  if (is_showing) {
    results_panel_router->OnOverlayShown();
  } else {
    results_panel_router->OnOverlayHidden();
  }
}

void LensOverlayController::OnPdfPartialPageTextRetrieved(
    std::vector<std::u16string> pdf_pages_text) {
  initialization_data_->pdf_pages_text_ = std::move(pdf_pages_text);
}

void LensOverlayController::OnPageContextUpdatedForSuggestion(
    base::Time query_start_time,
    std::string query,
    std::map<std::string, std::string> additional_query_parameters,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion,
    lens::LensOverlayInvocationSource invocation_source) {
  // TODO(crbug.com/404941800): Eventually, this should be a CHECK or removed
  // once the contextualization controller is separated from the overlay. For
  // now, this is required to prevent failures when opening the side panel.
  // `initialization_data_` is used in IssueSearchBoxRequestPart2 to determine
  // what type of query to send (contextual or text only).
  if (state_ == State::kOff) {
    initialization_data_ = std::make_unique<OverlayInitializationData>(
        SkBitmap(), SkBitmap(), lens::PaletteId::kFallback,
        lens_search_controller_->GetPageURL(),
        lens_search_controller_->GetPageTitle());
  }

  // TODO(crbug.com/404941800): This flow should not start the overlay once
  // contextualization is separated from the overlay.
  IssueSearchBoxRequest(query_start_time, query, match_type,
                        is_zero_prefix_suggestion, additional_query_parameters,
                        invocation_source);
}

void LensOverlayController::OnScreenshotTaken(
    std::optional<base::TimeTicks> screenshot_start_time,
    const SkBitmap& bitmap,
    const std::vector<gfx::Rect>& all_bounds,
    std::optional<uint32_t> pdf_current_page) {
  if (screenshot_start_time.has_value()) {
    lens::RecordTimeToScreenshot(base::TimeTicks::Now() -
                                 screenshot_start_time.value());
  }

  // While capturing a screenshot the overlay was cancelled. Do nothing.
  if (state_ == State::kOff || IsOverlayClosing()) {
    return;
  }

  // The documentation for CopyFromSurface claims that the copy can fail, but
  // without providing information about how this can happen.
  // Supposedly IsSurfaceAvailableForCopy() should guard against this case, but
  // this is a multi-process, multi-threaded environment so there may be a
  // TOCTTOU race condition.
  if (bitmap.drawsNothing()) {
    lens_search_controller_->CloseLensSync(
        lens::LensOverlayDismissalSource::kErrorScreenshotCreationFailed);
    return;
  }

  // The following two methods happen async to parallelize the two bottlenecks
  // in our invocation flow.
  CreateInitializationData(bitmap, all_bounds, pdf_current_page);
  ShowOverlay();

  state_ = State::kStartingWebUI;
}

void LensOverlayController::ReshowOverlayPart2() {
  if (state_ != State::kIsReshowing) {
    return;
  }

  // Create the new RGB bitmap asynchronously to prevent the main thread from
  // blocking on the encoding.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::USER_BLOCKING},
      base::BindOnce(&CreateRgbBitmap,
                     GetContextualizationController()->viewport_screenshot()),
      base::BindOnce(&LensOverlayController::ReshowOverlayPart3,
                     weak_factory_.GetWeakPtr()));
}

void LensOverlayController::ReshowOverlayPart3(const SkBitmap& rgb_screenshot) {
  if (state_ != State::kIsReshowing) {
    return;
  }

  if (rgb_screenshot.drawsNothing()) {
    lens_search_controller_->CloseLensSync(
        lens::LensOverlayDismissalSource::kErrorScreenshotEncodingFailed);
    return;
  }
  CHECK(initialization_data_);
  initialization_data_->initial_rgb_screenshot_ = rgb_screenshot;
  CHECK(page_);
  page_->OnOverlayReshown(rgb_screenshot);

  if (lens_overlay_blur_layer_delegate_) {
    lens_overlay_blur_layer_delegate_->Hide();
  }

  // Set the overlay web view opacity to near-zero instead of using
  // `SetVisible(false)`. Setting visibility to false prevents animation frames
  // in the WebUI, which causes ghosting of the old screenshot when the view is
  // reshown. Setting opacity instead allows for animation frames in the WebUI
  // to properly hide the background image canvas until the new screenshot can
  // be rendered. The web view opacity is set to 1.0f in
  // `FinishReshowOverlay()`. Note that opacity is set just above `0.f` to pass
  // a DCHECK that exists in `aura::Window` that might otherwise be tripped when
  // setting opacity to 0.f.
  SetOverlayWebViewOpacity(std::nextafter(0.f, 1.f));
  ShowOverlay();
  base::UmaHistogramBoolean("Lens.Overlay.Shown", true);
}

void LensOverlayController::SetOverlayWebViewOpacity(float opacity) {
  if (!overlay_web_view_) {
    return;
  }

  // The web views' holder layer is needed to hide the actual web contents.
  ui::Layer* layer = overlay_web_view_->holder()->GetUILayer();
  if (layer) {
    layer->SetOpacity(opacity);
  }
}

bool LensOverlayController::IsResultsSidePanelShowing() {
  return GetLensResultsPanelRouter()->IsEntryShowing();
}

void LensOverlayController::MaybeGrantLensOverlayPermissionsForSession() {
  if (lens::features::IsLensOverlayNonBlockingPrivacyNoticeEnabled() &&
      !lens::DidUserGrantLensOverlayNeededPermissions(pref_service_)) {
    GetLensOverlayQueryController()->GrantPermissionForSession();
    GetLensQueryFlowRouter()->MaybeResumeQueryFlow();
    user_interacted_without_accepting_privacy_notice = true;
    lens::RecordNonBlockingPrivacyNoticeAccepted(
        lens::LensOverlayNonBlockingPrivacyNoticeUserAction::kLensInteraction,
        invocation_source_);
  }
}

void LensOverlayController::AcceptPrivacyNotice() {
  // Permanently grant permissions, then restart the query flow and upload page
  // content for contextualization.
  lens::GrantLensOverlayNeededPermissions(pref_service_);
  lens::RecordNonBlockingPrivacyNoticeAccepted(
      lens::LensOverlayNonBlockingPrivacyNoticeUserAction::kAccepted,
      invocation_source_);
  GetLensQueryFlowRouter()->MaybeResumeQueryFlow();
  GetLensQueryFlowRouter()->MaybeRestartQueryFlow();
  GetContextualizationController()->TryUpdatePageContextualization(
      base::BindOnce(&LensOverlayController::NotifyPageContentUpdated,
                     weak_factory_.GetWeakPtr()));
}

void LensOverlayController::DismissPrivacyNotice() {
  lens::RecordNonBlockingPrivacyNoticeAccepted(
      lens::LensOverlayNonBlockingPrivacyNoticeUserAction::kDismissed,
      invocation_source_);
}

lens::LensSearchboxController*
LensOverlayController::GetLensSearchboxController() {
  return lens_search_controller_->lens_searchbox_controller();
}

lens::LensOverlaySidePanelCoordinator*
LensOverlayController::GetLensOverlaySidePanelCoordinator() {
  return lens_search_controller_->lens_overlay_side_panel_coordinator();
}

lens::LensResultsPanelRouter*
LensOverlayController::GetLensResultsPanelRouter() {
  return lens_search_controller_->results_panel_router();
}

lens::LensOverlayQueryController*
LensOverlayController::GetLensOverlayQueryController() {
  return lens_search_controller_->lens_overlay_query_controller();
}

lens::LensQueryFlowRouter* LensOverlayController::GetLensQueryFlowRouter() {
  return lens_search_controller_->query_router();
}

lens::LensSearchContextualizationController*
LensOverlayController::GetContextualizationController() {
  return lens_search_controller_->lens_search_contextualization_controller();
}

lens::LensSessionMetricsLogger*
LensOverlayController::GetLensSessionMetricsLogger() {
  return lens_search_controller_->lens_session_metrics_logger();
}
