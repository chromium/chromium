// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

#include "base/containers/contains.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/lens/core/mojom/geometry.mojom.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/lens/lens_overlay_entry_point_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_event_handler.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_theme_utils.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/lens_permission_bubble_controller.h"
#include "chrome/browser/ui/lens/lens_search_bubble_controller.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/find_in_page/find_tab_helper.h"
#include "components/lens/lens_features.h"
#include "components/lens/lens_overlay_permission_utils.h"
#include "components/permissions/permission_request_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/viz/common/frame_timing_details.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/web_ui.h"
#include "net/base/url_search_params.h"
#include "net/base/url_util.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source_id.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/bubble/bubble_dialog_delegate_view.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/native_widget.h"

void* kLensOverlayPreselectionWidgetIdentifier =
    &kLensOverlayPreselectionWidgetIdentifier;

namespace {

// Timeout for the fadeout animation. This is purposely set to be twice the
// duration of the fade out animation on the WebUI JS because there is a delay
// between us notifying the WebUI, and the WebUI receiving our event.
constexpr base::TimeDelta kFadeoutAnimationTimeout = base::Milliseconds(300);

// The url query param key for the search query.
inline constexpr char kTextQueryParameterKey[] = "q";

// When a WebUIController for lens overlay is created, we need a mechanism to
// glue that instance to the LensOverlayController that spawned it. This class
// is that glue. The lifetime of this instance is scoped to the lifetime of the
// LensOverlayController, which semantically "owns" this instance.
class LensOverlayControllerGlue
    : public content::WebContentsUserData<LensOverlayControllerGlue> {
 public:
  ~LensOverlayControllerGlue() override = default;

  LensOverlayController* controller() { return controller_; }

 private:
  friend WebContentsUserData;

  LensOverlayControllerGlue(content::WebContents* contents,
                            LensOverlayController* controller)
      : content::WebContentsUserData<LensOverlayControllerGlue>(*contents),
        controller_(controller) {}

  // Semantically owns this class.
  raw_ptr<LensOverlayController> controller_;

  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(LensOverlayControllerGlue);

// Allows lookup of a LensOverlayController from a WebContents associated with a
// tab.
class LensOverlayControllerTabLookup
    : public content::WebContentsUserData<LensOverlayControllerTabLookup> {
 public:
  ~LensOverlayControllerTabLookup() override = default;

  LensOverlayController* controller() { return controller_; }

 private:
  friend WebContentsUserData;
  LensOverlayControllerTabLookup(content::WebContents* contents,
                                 LensOverlayController* controller)
      : content::WebContentsUserData<LensOverlayControllerTabLookup>(*contents),
        controller_(controller) {}

  // Semantically owns this class.
  raw_ptr<LensOverlayController> controller_;
  WEB_CONTENTS_USER_DATA_KEY_DECL();
};

WEB_CONTENTS_USER_DATA_KEY_IMPL(LensOverlayControllerTabLookup);

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

// Returns true if the two URLs have the same base url, and the same query
// parameters. This differs from comparing two GURLs using == since this method
// will ensure equivalence even if there are empty query params, viewport
// params, or different query param ordering.
bool AreSearchUrlsEquivalent(const GURL& a, const GURL& b) {
  // Check urls without query and reference (fragment) for equality first.
  GURL::Replacements replacements;
  replacements.ClearRef();
  replacements.ClearQuery();
  if (a.ReplaceComponents(replacements) != b.ReplaceComponents(replacements)) {
    return false;
  }

  // Now, compare each query param individually to ensure equivalence. Remove
  // params that should not contribute to differing search results.
  net::UrlSearchParams a_search_params(
      lens::RemoveIgnoredSearchURLParameters(a));
  net::UrlSearchParams b_search_params(
      lens::RemoveIgnoredSearchURLParameters(b));

  // Sort params so they are in the same order during comparison.
  a_search_params.Sort();
  b_search_params.Sort();

  // Check Search Params for equality
  // All search params, in order, need to have the same keys and the same
  // values.
  return a_search_params.params() == b_search_params.params();
}

}  // namespace

LensOverlayController::LensOverlayController(
    tabs::TabInterface* tab,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    PrefService* pref_service,
    syncer::SyncService* sync_service,
    ThemeService* theme_service)
    : tab_(tab),
      variations_client_(variations_client),
      identity_manager_(identity_manager),
      pref_service_(pref_service),
      sync_service_(sync_service),
      theme_service_(theme_service) {
  LensOverlayControllerTabLookup::CreateForWebContents(tab_->GetContents(),
                                                       this);

  tab_subscriptions_.push_back(tab_->RegisterDidEnterForeground(
      base::BindRepeating(&LensOverlayController::TabForegrounded,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillEnterBackground(
      base::BindRepeating(&LensOverlayController::TabWillEnterBackground,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDiscardContents(
      base::BindRepeating(&LensOverlayController::WillDiscardContents,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillDetach(base::BindRepeating(
      &LensOverlayController::WillDetach, weak_factory_.GetWeakPtr())));
  search_bubble_controller_ =
      std::make_unique<lens::LensSearchBubbleController>(this);
  lens_overlay_event_handler_ =
      std::make_unique<lens::LensOverlayEventHandler>(this);
}

LensOverlayController::~LensOverlayController() {
  // In the event that the tab is being closed or backgrounded, and the window
  // is not closing, TabWillEnterBackground() will be called and the UI will be
  // torn down via CloseUI(). This code path is only relevant for the case where
  // the whole window is being torn down. In that case we need to clear the
  // WebContents::SupportsUserData since it's technically possible for a
  // WebContents to outlive the window, but we do not want to run through the
  // usual teardown since the window is half-destroyed.
  while (!glued_webviews_.empty()) {
    RemoveGlueForWebView(glued_webviews_.front());
  }
  glued_webviews_.clear();
  tab_->GetContents()->RemoveUserData(
      LensOverlayControllerTabLookup::UserDataKey());

  state_ = State::kOff;

  for (Observer& observer : observers_) {
    observer.OnLensOverlayControllerDestroyed();
  }
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(LensOverlayController, kOverlayId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(LensOverlayController,
                                      kOverlaySidePanelWebViewId);

LensOverlayController::SearchQuery::SearchQuery(std::string text_query,
                                                GURL url)
    : search_query_text_(std::move(text_query)),
      search_query_url_(std::move(url)) {}

LensOverlayController::SearchQuery::SearchQuery(const SearchQuery& other) {
  search_query_text_ = other.search_query_text_;
  if (other.selected_region_) {
    selected_region_ = other.selected_region_->Clone();
  }
  selected_region_bitmap_ = other.selected_region_bitmap_;
  selected_region_thumbnail_uri_ = other.selected_region_thumbnail_uri_;
  search_query_url_ = other.search_query_url_;
  selected_text_ = other.selected_text_;
  lens_selection_type_ = other.lens_selection_type_;
}

LensOverlayController::SearchQuery&
LensOverlayController::SearchQuery::operator=(
    const LensOverlayController::SearchQuery& other) {
  search_query_text_ = other.search_query_text_;
  if (other.selected_region_) {
    selected_region_ = other.selected_region_->Clone();
  }
  selected_region_bitmap_ = other.selected_region_bitmap_;
  selected_region_thumbnail_uri_ = other.selected_region_thumbnail_uri_;
  search_query_url_ = other.search_query_url_;
  selected_text_ = other.selected_text_;
  lens_selection_type_ = other.lens_selection_type_;
  return *this;
}

LensOverlayController::SearchQuery::~SearchQuery() = default;

// static
bool LensOverlayController::IsEnabled(Browser* browser) {
  // Exit early if browser is null.
  if (!browser) {
    return false;
  }

  // Feature is disabled via finch.
  if (!lens::features::IsLensOverlayEnabled()) {
    return false;
  }

  // Disable on non-normal windows (those without omnibox and toolbar).
  if (!browser->is_type_normal()) {
    return false;
  }

  // Disable in fullscreen without top-chrome. Need to check that
  // browser->window() exists to avoid to skip this check during initialization.
  // We skip this check since during initialization, it is too early to know if
  // the top-chrome exists or not.
  if (!lens::features::GetLensOverlayEnableInFullscreen() &&
      browser->window() && !browser->IsTabStripVisible()) {
    return false;
  }

  // Lens Overlay is disabled via enterprise policy.
  lens::prefs::LensOverlaySettingsPolicyValue policy_value =
      static_cast<lens::prefs::LensOverlaySettingsPolicyValue>(
          browser->profile()->GetPrefs()->GetInteger(
              lens::prefs::kLensOverlaySettings));
  if (policy_value == lens::prefs::LensOverlaySettingsPolicyValue::kDisabled) {
    return false;
  }

  // Lens Overlay is only enabled if the user's default search engine is Google.
  if (lens::features::IsLensOverlayGoogleDseRequired() &&
      !search::DefaultSearchProviderIsGoogle(browser->profile())) {
    return false;
  }

  // Finally, only enable the overlay if user meets our minimum RAM requirement.
  static int phys_mem_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  return phys_mem_mb > lens::features::GetLensOverlayMinRamMb();
}

void LensOverlayController::ShowUIWithPendingRegion(
    lens::LensOverlayInvocationSource invocation_source,
    const gfx::Rect& tab_bounds,
    const gfx::Rect& view_bounds,
    const gfx::Rect& image_bounds,
    const SkBitmap& region_bitmap) {
  ShowUIWithPendingRegion(invocation_source,
                          lens::GetCenterRotatedBoxFromTabViewAndImageBounds(
                              tab_bounds, view_bounds, image_bounds),
                          region_bitmap);
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
  search_performed_in_session_ = true;
}

void LensOverlayController::ShowUI(
    lens::LensOverlayInvocationSource invocation_source) {
  // If UI is already showing or in the process of showing, do nothing.
  if (state_ != State::kOff) {
    return;
  }

  // The UI should only show if the tab is in the foreground.
  if (!tab_->IsInForeground()) {
    return;
  }

  invocation_source_ = invocation_source;

  // Request user permission before grabbing a screenshot.
  Browser* tab_browser = chrome::FindBrowserWithTab(tab_->GetContents());
  CHECK(tab_browser);
  CHECK(pref_service_);
  if (!lens::CanSharePageScreenshotWithLensOverlay(pref_service_)) {
    if (!permission_bubble_controller_) {
      permission_bubble_controller_ =
          std::make_unique<lens::LensPermissionBubbleController>(
              tab_->GetBrowserWindowInterface(), pref_service_,
              GetInvocationSourceString());
    }
    permission_bubble_controller_->RequestPermission(
        tab_->GetContents(),
        base::BindRepeating(&LensOverlayController::ShowUI,
                            weak_factory_.GetWeakPtr(), invocation_source));
    return;
  }

  // Increment the counter for the number of times the Lens Overlay has been
  // started.
  int lens_overlay_start_count =
      pref_service_->GetInteger(prefs::kLensOverlayStartCount);
  pref_service_->SetInteger(prefs::kLensOverlayStartCount,
                            lens_overlay_start_count + 1);

  // Create the results side panel coordinator when showing the UI if it does
  // not already exist for this tab's web contents.
  if (!results_side_panel_coordinator_) {
    results_side_panel_coordinator_ =
        std::make_unique<lens::LensOverlaySidePanelCoordinator>(this);
  }
  if (lens::features::IsLensOverlaySearchBubbleEnabled()) {
    search_bubble_controller_->Show();
  }

  Profile* profile =
      Profile::FromBrowserContext(tab_->GetContents()->GetBrowserContext());
  // Create the query controller.
  lens_overlay_query_controller_ = CreateLensQueryController(
      base::BindRepeating(&LensOverlayController::HandleStartQueryResponse,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&LensOverlayController::HandleInteractionURLResponse,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&LensOverlayController::HandleInteractionDataResponse,
                          weak_factory_.GetWeakPtr()),
      base::BindRepeating(&LensOverlayController::HandleThumbnailCreated,
                          weak_factory_.GetWeakPtr()),
      variations_client_, identity_manager_, profile, invocation_source,
      lens::LensOverlayShouldUseDarkMode(theme_service_));
  side_panel_coordinator_ =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(tab_browser);
  CHECK(side_panel_coordinator_);

  // Setup observer to be notified of side panel opens and closes.
  side_panel_state_observer_.Observe(side_panel_coordinator_);

  if (find_in_page::FindTabHelper* const find_tab_helper =
          find_in_page::FindTabHelper::FromWebContents(tab_->GetContents())) {
    find_tab_observer_.Observe(find_tab_helper);
  }

  if (auto* helper = OmniboxTabHelper::FromWebContents(tab_->GetContents())) {
    omnibox_tab_helper_observer_.Observe(helper);
  }

  if (side_panel_coordinator_->IsSidePanelShowing()) {
    // Close the currently opened side panel and postpone taking the screenshot
    // until OnSidePanelDidClose
    state_ = State::kClosingOpenedSidePanel;
    side_panel_coordinator_->Close();
  } else {
    CaptureScreenshot();
  }

  scoped_tab_modal_ui_ = tab_->ShowModalUI();
  fullscreen_observation_.Observe(
      tab_browser->exclusive_access_manager()->fullscreen_controller());

  if (!pending_region_) {
    ShowPreselectionBubble();
  }

  // Establish data required for session metrics.
  search_performed_in_session_ = false;
  invocation_time_ = base::TimeTicks::Now();
}

void LensOverlayController::CloseUIAsync(
    lens::LensOverlayDismissalSource dismissal_source) {
  if (state_ == State::kOff || IsOverlayClosing()) {
    return;
  }

  // Notify the overlay so it can do any animations or cleanup. The page_ is not
  // guaranteed to exist if CloseUIAsync is called during the setup process.
  if (page_) {
    page_->NotifyOverlayClosing();
  }

  if (state_ == State::kOverlayAndResults) {
    if (side_panel_coordinator_->GetCurrentEntryId() ==
        SidePanelEntry::Id::kLensOverlayResults) {
      // If a close was triggered while our side panel is showing, instead of
      // just immediately closing the overlay, we close side panel to show a
      // smooth closing animation. Once the side panel deregisters, it will
      // re-call our close method in OnSidePanelDidClose() which will finish the
      // closing process.
      state_ = State::kClosingSidePanel;
      last_dismissal_source_ = dismissal_source;
      side_panel_coordinator_->Close();
      return;
    }
  }

  state_ = State::kClosing;
  // Set a short 200ms timeout to give the fade out time to transition.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&LensOverlayController::CloseUIPart2,
                     weak_factory_.GetWeakPtr(), dismissal_source),
      kFadeoutAnimationTimeout);
}

void LensOverlayController::CloseUISync(
    lens::LensOverlayDismissalSource dismissal_source) {
  if (state_ == State::kOff) {
    return;
  }

  state_ = State::kClosing;
  if (side_panel_coordinator_->GetCurrentEntryId() ==
      SidePanelEntry::Id::kLensOverlayResults) {
    side_panel_state_observer_.Reset();
    side_panel_coordinator_->Close();
  }

  CloseUIPart2(dismissal_source);
}

// static
LensOverlayController* LensOverlayController::GetController(
    content::WebUI* web_ui) {
  return LensOverlayControllerGlue::FromWebContents(web_ui->GetWebContents())
      ->controller();
}

// static
LensOverlayController* LensOverlayController::GetController(
    content::WebContents* tab_contents) {
  auto* glue = LensOverlayControllerTabLookup::FromWebContents(tab_contents);
  return glue ? glue->controller() : nullptr;
}

// static
LensOverlayController*
LensOverlayController::GetControllerFromWebViewWebContents(
    content::WebContents* contents) {
  auto* glue = LensOverlayControllerGlue::FromWebContents(contents);
  return glue ? glue->controller() : nullptr;
}

void LensOverlayController::BindOverlay(
    mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensPage> page) {
  if (state_ != State::kStartingWebUI) {
    return;
  }
  // Initialization data should always exist before binding.
  CHECK(initialization_data_);
  receiver_.Bind(std::move(receiver));
  page_.Bind(std::move(page));

  InitializeOverlayUI(*initialization_data_);
  base::UmaHistogramBoolean("Lens.Overlay.Shown", true);
  state_ = State::kOverlay;

  // Only start the query flow again if we don't already have a full image
  // response.
  if (!initialization_data_->has_full_image_response()) {
    int device_scale_factor =
        tab_->GetContents()->GetRenderWidgetHostView()->GetDeviceScaleFactor();
    float page_scale_factor =
        zoom::ZoomController::FromWebContents(tab_->GetContents())
            ->GetZoomPercent() /
        100.0f;
    // Use std::move because significant_region_boxes_ is only used in this
    // call, which should only occur once in the lifetime of
    // LensOverlayQueryController and thus of LensOverlayController.
    lens_overlay_query_controller_->StartQueryFlow(
        initialization_data_->current_screenshot_,
        initialization_data_->page_url_, initialization_data_->page_title_,
        std::move(initialization_data_->significant_region_boxes_),
        device_scale_factor * page_scale_factor);
  }
  if (pending_region_) {
    // If there is a pending region (i.e. for image right click)
    // use INJECTED_IMAGE as the selection type.
    DoLensRequest(std::move(pending_region_), lens::INJECTED_IMAGE,
                  std::make_optional<SkBitmap>(pending_region_bitmap_));
    pending_region_bitmap_.reset();
  }
}

void LensOverlayController::BindSidePanel(
    mojo::PendingReceiver<lens::mojom::LensSidePanelPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensSidePanelPage> page) {
  // If a side panel was already bound to this overlay controller, then we
  // should reset. This can occur if the side panel is closed and then reopened
  // while the overlay is open.
  side_panel_receiver_.reset();
  side_panel_page_.reset();

  side_panel_receiver_.Bind(std::move(receiver));
  side_panel_page_.Bind(std::move(page));
  if (pending_side_panel_url_.has_value()) {
    side_panel_page_->LoadResultsInFrame(*pending_side_panel_url_);
    pending_side_panel_url_.reset();
  }
  side_panel_page_->SetShowErrorPage(
      pending_side_panel_should_show_error_page_);
}

void LensOverlayController::SetSearchboxHandler(
    std::unique_ptr<RealboxHandler> handler) {
  searchbox_handler_ = std::move(handler);
}

void LensOverlayController::ResetSearchboxHandler() {
  searchbox_handler_.reset();
}

views::View* LensOverlayController::GetOverlayViewForTesting() {
  return overlay_view_.get();
}

views::WebView* LensOverlayController::GetOverlayWebViewForTesting() {
  return overlay_web_view_.get();
}

void LensOverlayController::CreateGlueForWebView(views::WebView* web_view) {
  LensOverlayControllerGlue::CreateForWebContents(web_view->GetWebContents(),
                                                  this);
  glued_webviews_.push_back(web_view);
}

void LensOverlayController::RemoveGlueForWebView(views::WebView* web_view) {
  auto it = std::find(glued_webviews_.begin(), glued_webviews_.end(), web_view);
  if (it != glued_webviews_.end()) {
    web_view->GetWebContents()->RemoveUserData(
        LensOverlayControllerGlue::UserDataKey());
    glued_webviews_.erase(it);
  }
}

void LensOverlayController::SendText(lens::mojom::TextPtr text) {
  page_->TextReceived(std::move(text));
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
  page_->ObjectsReceived(std::move(objects));
}

void LensOverlayController::NotifyResultsPanelOpened() {
  page_->NotifyResultsPanelOpened();
}

void LensOverlayController::TriggerCopyText() {
  page_->TriggerCopyText();
}

bool LensOverlayController::IsOverlayShowing() {
  return state_ == State::kStartingWebUI || state_ == State::kOverlay ||
         state_ == State::kOverlayAndResults ||
         state_ == State::kClosingSidePanel;
}

bool LensOverlayController::IsOverlayClosing() {
  return state_ == State::kClosing || state_ == State::kClosingSidePanel;
}

void LensOverlayController::LoadURLInResultsFrame(const GURL& url) {
  // TODO(b/337114915): If the new URL has a text query parameter and came from
  // the renderer, we need to update the searchbox text.
  if (!IsOverlayShowing()) {
    return;
  }

  if (side_panel_page_) {
    side_panel_page_->LoadResultsInFrame(url);
    return;
  }
  pending_side_panel_url_ = std::make_optional<GURL>(url);
  results_side_panel_coordinator_->RegisterEntryAndShow();
}

void LensOverlayController::SetSearchboxInputText(const std::string& text) {
  if (searchbox_handler_ && searchbox_handler_->IsRemoteBound()) {
    searchbox_handler_->SetInputText(text);
  } else {
    // If the side panel was not bound at the time of request, we store the
    // query as pending to send it to the searchbox on bind.
    pending_text_query_ = text;
  }
}

void LensOverlayController::AddQueryToHistory(std::string query,
                                              GURL search_url) {
  CHECK(initialization_data_);

  // If we are loading the query that was just popped, do not add it to the
  // stack.
  auto loaded_search_query =
      initialization_data_->currently_loaded_search_query_;
  if (loaded_search_query &&
      AreSearchUrlsEquivalent(loaded_search_query->search_query_url_,
                              search_url)) {
    return;
  }

  // A search URL without a Lens mode parameter indicates a click on a related
  // search or other in-SRP refinement. In this case, we should clear all
  // selection and thumbnail state.
  const std::string lens_mode = lens::GetLensModeParameterValue(search_url);
  if (lens_mode.empty()) {
    initialization_data_->selected_region_.reset();
    initialization_data_->selected_region_bitmap_.reset();
    initialization_data_->selected_text_.reset();
    initialization_data_->additional_search_query_params_.clear();
    selected_region_thumbnail_uri_.clear();
    lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;
    page_->ClearAllSelections();
    SetSearchboxThumbnail(std::string());
  }

  // In the case where a query was triggered by a selection on the overlay or
  // use of the searchbox, initialization_data_, additional_search_query_params_
  // and selected_region_thumbnail_uri_ will have already been set. Record
  // that state in a search query struct.
  SearchQuery search_query(query, search_url);
  if (initialization_data_->selected_region_) {
    search_query.selected_region_ =
        initialization_data_->selected_region_->Clone();
  }
  if (!initialization_data_->selected_region_bitmap_.drawsNothing()) {
    search_query.selected_region_bitmap_ =
        initialization_data_->selected_region_bitmap_;
  }
  search_query.selected_region_thumbnail_uri_ = selected_region_thumbnail_uri_;
  if (initialization_data_->selected_text_.has_value()) {
    search_query.selected_text_ = initialization_data_->selected_text_.value();
  }
  search_query.lens_selection_type_ = lens_selection_type_;
  search_query.additional_search_query_params_ =
      initialization_data_->additional_search_query_params_;

  // Add what was the currently loaded search query to the query stack,
  // if it is present.
  if (loaded_search_query) {
    initialization_data_->search_query_history_stack_.push_back(
        loaded_search_query.value());
    side_panel_page_->SetBackArrowVisible(true);
  }

  // Set the currently loaded search query to the one we just created.
  initialization_data_->currently_loaded_search_query_.reset();
  initialization_data_->currently_loaded_search_query_ = search_query;

  // Update searchbox and selection state to match the new query.
  SetSearchboxInputText(query);
}

void LensOverlayController::PopAndLoadQueryFromHistory() {
  if (initialization_data_->search_query_history_stack_.empty()) {
    return;
  }

  // Get the query that we want to load in the results frame and then pop it
  // from the list.
  auto query = initialization_data_->search_query_history_stack_.back();
  initialization_data_->search_query_history_stack_.pop_back();

  if (initialization_data_->search_query_history_stack_.empty()) {
    side_panel_page_->SetBackArrowVisible(false);
  }

  // Clear any active selections on the page and then re-add selections for this
  // query and update the selection, thumbnail and searchbox state.
  CHECK(page_);
  page_->ClearAllSelections();
  if (query.selected_text_.has_value()) {
    page_->SetTextSelection(query.selected_text_->first,
                            query.selected_text_->second);
    initialization_data_->selected_text_ = query.selected_text_.value();
  } else if (query.selected_region_) {
    page_->SetPostRegionSelection(query.selected_region_->Clone());
    initialization_data_->selected_region_ = query.selected_region_->Clone();
    selected_region_thumbnail_uri_ = query.selected_region_thumbnail_uri_;
  }
  initialization_data_->additional_search_query_params_ =
      query.additional_search_query_params_;
  SetSearchboxInputText(query.search_query_text_);
  SetSearchboxThumbnail(query.selected_region_thumbnail_uri_);

  if (query.selected_region_ || !query.selected_region_bitmap_.drawsNothing()) {
    // If the current query has a region or image bytes, we need to send a new
    // interaction request in order to to keep our request IDs in sync with the
    // server. If not, we will receive broken results. Because of this, we also
    // want to modify the currently loaded search query so that we don't get
    // duplicates added to the query history stack.
    initialization_data_->currently_loaded_search_query_.reset();
    if (!initialization_data_->search_query_history_stack_.empty()) {
      auto previous_query =
          initialization_data_->search_query_history_stack_.back();
      initialization_data_->search_query_history_stack_.pop_back();
      initialization_data_->currently_loaded_search_query_ = previous_query;
    }

    // If the query also has text, we should send it as a multimodal query.
    if (query.search_query_text_.empty()) {
      DoLensRequest(
          query.selected_region_->Clone(), query.lens_selection_type_,
          query.selected_region_bitmap_.drawsNothing()
              ? std::nullopt
              : std::make_optional<SkBitmap>(query.selected_region_bitmap_));
    } else {
      // TODO(b/348003311): Add support for sending the selected region bitmap
      // in the multimodal request.
      lens_overlay_query_controller_->SendMultimodalRequest(
          initialization_data_->selected_region_.Clone(),
          query.search_query_text_, query.lens_selection_type_,
          initialization_data_->additional_search_query_params_);
    }
    return;
  }
  // Load the popped query URL in the results frame if it does not need to
  // send image bytes.
  LoadURLInResultsFrame(query.search_query_url_);

  // Set the currently loaded query to the one we just popped.
  initialization_data_->currently_loaded_search_query_.reset();
  initialization_data_->currently_loaded_search_query_ = query;
}

void LensOverlayController::SetSidePanelIsLoadingResults(bool is_loading) {
  if (side_panel_page_) {
    side_panel_page_->SetIsLoadingResults(is_loading);
  }
}

void LensOverlayController::SetSidePanelShowErrorPage(
    bool should_show_error_page) {
  if (side_panel_page_) {
    side_panel_page_->SetShowErrorPage(should_show_error_page);
    return;
  }

  pending_side_panel_should_show_error_page_ = should_show_error_page;
}

void LensOverlayController::OnSidePanelHidden() {
  // If we're already in the process of closing, continue to do so.
  if (state_ == State::kClosingSidePanel) {
    CHECK(last_dismissal_source_.has_value());
    CloseUIPart2(*last_dismissal_source_);
    last_dismissal_source_.reset();
    return;
  }

  // If the tab is not in the foreground, this is not relevant.
  if (!tab_->IsInForeground()) {
    return;
  }

  // The user clicks the close button on the side panel. Begin to close the UI
  // asynchronously.
  CloseUIAsync(lens::LensOverlayDismissalSource::kSidePanelCloseButton);
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
    int selection_end_index) {
  IssueTextSelectionRequest(text_query, selection_start_index,
                            selection_end_index);
}

void LensOverlayController::RecordUkmLensOverlayInteractionForTesting(
    lens::mojom::UserAction user_action) {
  RecordUkmLensOverlayInteraction(user_action);
}

void LensOverlayController::IssueSearchBoxRequestForTesting(
    const std::string& search_box_text,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion,
    std::map<std::string, std::string> additional_query_params) {
  IssueSearchBoxRequest(search_box_text, match_type, is_zero_prefix_suggestion,
                        additional_query_params);
}

void LensOverlayController::IssueTranslateSelectionRequestForTesting(
    const std::string& text_query,
    const std::string& content_language,
    int selection_start_index,
    int selection_end_index) {
  IssueTranslateSelectionRequest(text_query, content_language,
                                 selection_start_index, selection_end_index);
}

void LensOverlayController::CopyText(const std::string& text) {
  ui::ScopedClipboardWriter clipboard_writer(ui::ClipboardBuffer::kCopyPaste);
  clipboard_writer.WriteText(base::UTF8ToUTF16(text));
}

void LensOverlayController::RecordUkmLensOverlayInteraction(
    lens::mojom::UserAction user_action) {
  ukm::SourceId source_id =
      tab_->GetContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  ukm::builders::Lens_Overlay_Overlay_UserAction(source_id)
      .SetUserAction(static_cast<int64_t>(user_action))
      .Record(ukm::UkmRecorder::Get());
}

std::string LensOverlayController::GetInvocationSourceString() {
  switch (invocation_source_) {
    case lens::LensOverlayInvocationSource::kAppMenu:
      return "AppMenu";
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuPage:
      return "ContentAreaContextMenuPage";
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuImage:
      return "ContentAreaContextMenuImage";
    case lens::LensOverlayInvocationSource::kToolbar:
      return "Toolbar";
    case lens::LensOverlayInvocationSource::kFindInPage:
      return "FindInPage";
    case lens::LensOverlayInvocationSource::kOmnibox:
      return "Omnibox";
  }
}

content::WebContents*
LensOverlayController::GetSidePanelWebContentsForTesting() {
  if (!results_side_panel_coordinator_) {
    return nullptr;
  }
  return results_side_panel_coordinator_->GetSidePanelWebContents();
}

const GURL& LensOverlayController::GetPageURLForTesting() {
  return GetPageURL();
}

SessionID LensOverlayController::GetTabIdForTesting() {
  return GetTabId();
}

metrics::OmniboxEventProto::PageClassification
LensOverlayController::GetPageClassificationForTesting() {
  return GetPageClassification();
}

const std::string& LensOverlayController::GetThumbnailForTesting() {
  return GetThumbnail();
}

void LensOverlayController::OnTextModifiedForTesting() {
  OnTextModified();
}

void LensOverlayController::OnThumbnailRemovedForTesting() {
  OnThumbnailRemoved();
}

const lens::proto::LensOverlayInteractionResponse&
LensOverlayController::GetLensResponseForTesting() {
  return GetLensResponse();
}

std::unique_ptr<lens::LensOverlayQueryController>
LensOverlayController::CreateLensQueryController(
    lens::LensOverlayFullImageResponseCallback full_image_callback,
    lens::LensOverlayUrlResponseCallback url_callback,
    lens::LensOverlayInteractionResponseCallback interaction_data_callback,
    lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    Profile* profile,
    lens::LensOverlayInvocationSource invocation_source,
    bool use_dark_mode) {
  return std::make_unique<lens::LensOverlayQueryController>(
      std::move(full_image_callback), std::move(url_callback),
      std::move(interaction_data_callback),
      std::move(thumbnail_created_callback), variations_client,
      identity_manager, profile, invocation_source, use_dark_mode);
}

LensOverlayController::OverlayInitializationData::OverlayInitializationData(
    const SkBitmap& screenshot,
    const std::string& data_uri,
    lens::PaletteId color_palette,
    std::optional<GURL> page_url,
    std::optional<std::string> page_title,
    std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes,
    std::vector<lens::mojom::OverlayObjectPtr> objects,
    lens::mojom::TextPtr text,
    const lens::proto::LensOverlayInteractionResponse& interaction_response,
    lens::mojom::CenterRotatedBoxPtr selected_region)
    : current_screenshot_(screenshot),
      current_screenshot_data_uri_(data_uri),
      color_palette_(color_palette),
      page_url_(page_url),
      page_title_(page_title),
      significant_region_boxes_(std::move(significant_region_boxes)),
      interaction_response_(interaction_response),
      selected_region_(std::move(selected_region)),
      text_(std::move(text)),
      objects_(std::move(objects)) {}
LensOverlayController::OverlayInitializationData::~OverlayInitializationData() =
    default;

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
    bool is_reload =
        navigation_handle->GetReloadType() != content::ReloadType::NONE;
    // We don't need to close if:
    //   1) The navigation is not for the main page.
    //   2) The navigation hasn't been committed yet.
    //   3) The URL did not change and the navigation wasn't the user reloading
    //      the page.
    if (!navigation_handle->IsInPrimaryMainFrame() ||
        !navigation_handle->HasCommitted() ||
        (navigation_handle->GetPreviousPrimaryMainFrameURL() ==
             navigation_handle->GetURL() &&
         !is_reload)) {
      return;
    }

    lens_overlay_controller_->CloseUISync(
        lens::LensOverlayDismissalSource::kPageChanged);
  }

 private:
  raw_ptr<LensOverlayController> lens_overlay_controller_;
};

void LensOverlayController::CaptureScreenshot() {
  // Begin the process of grabbing a screenshot.
  content::RenderWidgetHostView* view = tab_->GetContents()
                                            ->GetPrimaryMainFrame()
                                            ->GetRenderViewHost()
                                            ->GetWidget()
                                            ->GetView();

  // During initialization and shutdown a capture may not be possible.
  if (!view || !view->IsSurfaceAvailableForCopy()) {
    CloseUISync(
        lens::LensOverlayDismissalSource::kErrorScreenshotCreationFailed);
    return;
  }

  state_ = State::kScreenshot;
  // Side panel is now full closed, take screenshot and open overlay.
  view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(),
      base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(
              &LensOverlayController::FetchViewportImageBoundingBoxes,
              weak_factory_.GetWeakPtr())));
}

void LensOverlayController::FetchViewportImageBoundingBoxes(
    const SkBitmap& bitmap) {
  content::RenderFrameHost* render_frame_host =
      tab_->GetContents()->GetPrimaryMainFrame();
  mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame> chrome_render_frame;
  render_frame_host->GetRemoteAssociatedInterfaces()->GetInterface(
      &chrome_render_frame);
  // Bind the InterfacePtr into the callback so that it's kept alive until
  // there's either a connection error or a response.
  auto* frame = chrome_render_frame.get();

  frame->RequestBoundsForAllImagesDiagnostic(base::BindOnce(
      &LensOverlayController::DidCaptureScreenshot, weak_factory_.GetWeakPtr(),
      std::move(chrome_render_frame), ++screenshot_attempt_id_, bitmap));
}

void LensOverlayController::DidCaptureScreenshot(
    mojo::AssociatedRemote<chrome::mojom::ChromeRenderFrame>
        chrome_render_frame,
    int attempt_id,
    const SkBitmap& bitmap,
    const std::vector<gfx::Rect>& all_bounds) {
  // While capturing a screenshot the overlay was cancelled. Do nothing.
  if (state_ == State::kOff || IsOverlayClosing()) {
    return;
  }

  // An id mismatch implies this is not the most recent screenshot attempt.
  if (screenshot_attempt_id_ != attempt_id) {
    return;
  }

  // The documentation for CopyFromSurface claims that the copy can fail, but
  // without providing information about how this can happen.
  // Supposedly IsSurfaceAvailableForCopy() should guard against this case, but
  // this is a multi-process, multi-threaded environment so there may be a
  // TOCTTOU race condition.
  if (bitmap.drawsNothing()) {
    CloseUISync(
        lens::LensOverlayDismissalSource::kErrorScreenshotCreationFailed);
    return;
  }

  // Encode the screenshot so we can transform it into a data URI for the WebUI.
  scoped_refptr<base::RefCountedBytes> data;
  lens::LensOverlayClientLogs client_logs;
  if (!lens::EncodeImage(
          bitmap, lens::features::GetLensOverlayScreenshotRenderQuality(),
          &data, client_logs)) {
    // TODO(b/334185985): Handle case when screenshot data URI encoding fails.
    CloseUISync(
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
        bitmap, lens::features::DynamicThemeMinPopulationPct());
    SkColor theme_color = lens::FindBestMatchedColorOrTransparent(
        colors, screenshot_color, lens::features::DynamicThemeMinChroma());
    if (theme_color != SK_ColorTRANSPARENT) {
      color_palette = lens::kPalettes.at(theme_color);
    }
  }

  content::WebContents* active_web_contents = tab_->GetContents();

  std::optional<GURL> page_url;
  if (lens::CanSharePageURLWithLensOverlay(pref_service_)) {
    page_url = std::make_optional<GURL>(active_web_contents->GetVisibleURL());
  }

  std::optional<std::string> page_title;
  if (lens::CanSharePageTitleWithLensOverlay(sync_service_)) {
    page_title = std::make_optional<std::string>(
        base::UTF16ToUTF8(active_web_contents->GetTitle()));
  }

  initialization_data_ = std::make_unique<OverlayInitializationData>(
      bitmap, webui::MakeDataURIForImage(data->as_vector(), "jpeg"),
      color_palette, page_url, page_title);
  AddBoundingBoxesToInitializationData(all_bounds);

  ShowOverlay();

  for (Observer& observer : observers_) {
    observer.OnLensOverlayDidShow();
  }
  state_ = State::kStartingWebUI;
}

void LensOverlayController::AddBoundingBoxesToInitializationData(
    const std::vector<gfx::Rect>& all_bounds) {
  int max_regions = lens::features::GetLensOverlayMaxSignificantRegions();
  if (max_regions == 0) {
    return;
  }
  content::RenderFrameHost* render_frame_host =
      tab_->GetContents()->GetPrimaryMainFrame();
  auto view_bounds = render_frame_host->GetView()->GetViewBounds();
  std::vector<lens::mojom::CenterRotatedBoxPtr> significant_region_boxes;
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
  initialization_data_->significant_region_boxes_ =
      std::move(significant_region_boxes);
}

void LensOverlayController::ShowOverlay() {
  // Listen to WebContents events
  tab_contents_observer_ = std::make_unique<UnderlyingWebContentsObserver>(
      tab_->GetContents(), this);

  // If the view already exists, we just need to reshow it.
  if (overlay_view_) {
    CHECK(overlay_web_view_);
    CHECK(!overlay_view_->GetVisible());

    overlay_view_->SetVisible(true);

    // The overlay needs to be focused on show to immediately begin
    // receiving key events.
    overlay_web_view_->RequestFocus();
    return;
  }

  // Create the view that will house our UI.
  std::unique_ptr<views::View> host_view = CreateViewForOverlay();

  // Grab the tab contents web view.
  auto* contents_web_view = tab_->GetBrowserWindowInterface()->GetWebView();
  CHECK(contents_web_view);

  // Ensure our view starts with the correct bounds.
  host_view->SetBoundsRect(contents_web_view->GetLocalBounds());

  // Add the view as a child of the view housing the tab contents.
  overlay_view_ = contents_web_view->AddChildView(std::move(host_view));
  tab_contents_view_observer_.Observe(contents_web_view);

  // The overlay needs to be focused on show to immediately begin
  // receiving key events.
  CHECK(overlay_web_view_);
  overlay_web_view_->RequestFocus();

  // Listen to the render process housing out overlay.
  overlay_web_view_->GetWebContents()
      ->GetPrimaryMainFrame()
      ->GetProcess()
      ->AddObserver(this);
}

void LensOverlayController::BackgroundUI() {
  overlay_view_->SetVisible(false);
  HidePreselectionBubble();
  tab_contents_observer_.reset();
  state_ = State::kBackground;

  // TODO(b/335516480): Schedule the UI to be suspended.
}

void LensOverlayController::CloseUIPart2(
    lens::LensOverlayDismissalSource dismissal_source) {
  if (state_ == State::kOff) {
    return;
  }

  // Ensure that this path is not being used to close the overlay if the overlay
  // is currently showing. If the overlay is currently showing, CloseUIAsync
  // should be used instead.
  CHECK(state_ != State::kOverlay);
  CHECK(state_ != State::kOverlayAndResults);

  // TODO(b/331940245): Refactor to be decoupled from permission_prompt_factory
  state_ = State::kClosing;

  // Destroy the glue to avoid UaF. This must be done before destroying
  // `results_side_panel_coordinator_` or `overlay_view_`.
  // This logic results on the assumption that the only way to destroy the
  // instances of views::WebView being glued is through this method. Any changes
  // to this assumption will likely need to restructure the concept of
  // `glued_webviews_`.
  while (!glued_webviews_.empty()) {
    RemoveGlueForWebView(glued_webviews_.front());
  }
  glued_webviews_.clear();

  // Closes lens search bubble if it exists.
  CloseSearchBubble();

  // Closes preselection toast if it exists.
  ClosePreselectionBubble();

  // A permission prompt may be suspended if the overlay was showing when the
  // permission was queued. Restore the suspended prompt if possible.
  // TODO(b/331940245): Refactor to be decoupled from PermissionPromptFactory
  content::WebContents* contents = tab_->GetContents();
  CHECK(contents);
  auto* permission_request_manager =
      permissions::PermissionRequestManager::FromWebContents(contents);
  if (permission_request_manager &&
      permission_request_manager->CanRestorePrompt()) {
    permission_request_manager->RestorePrompt();
  }

  permission_bubble_controller_.reset();
  searchbox_handler_.reset();
  results_side_panel_coordinator_.reset();

  side_panel_state_observer_.Reset();
  side_panel_coordinator_ = nullptr;

  if (overlay_web_view_) {
    // Remove render frame observer.
    overlay_web_view_->GetWebContents()
        ->GetPrimaryMainFrame()
        ->GetProcess()
        ->RemoveObserver(this);
  }

  if (overlay_view_) {
    auto* contents_web_view = tab_->GetBrowserWindowInterface()->GetWebView();
    CHECK(contents_web_view);

    // Remove and delete the overlay view and web view. Not doing so will result
    // in dangling pointers when the browser closes. Note the trailing `T` on
    // the method name -- this removes `overlay_view_` and returns a unique_ptr
    // to it which we then discard.  Without the `T`, it returns nothing and
    // frees nothing. Since technically the views are owned by
    // contents_web_view, we need to release our reference using std::exchange
    // to avoid a dangling pointer which throws an error when DCHECK is on.
    overlay_view_->RemoveChildViewT(std::exchange(overlay_web_view_, nullptr));
    contents_web_view->RemoveChildViewT(std::exchange(overlay_view_, nullptr));
  }
  overlay_web_view_ = nullptr;
  overlay_view_ = nullptr;

  tab_contents_view_observer_.Reset();
  omnibox_tab_helper_observer_.Reset();
  find_tab_observer_.Reset();
  tab_contents_observer_.reset();
  side_panel_receiver_.reset();
  side_panel_page_.reset();
  receiver_.reset();
  page_.reset();
  initialization_data_.reset();
  lens_overlay_query_controller_.reset();
  scoped_tab_modal_ui_.reset();
  pending_side_panel_url_.reset();
  pending_text_query_.reset();
  pending_thumbnail_uri_.reset();
  selected_region_thumbnail_uri_.clear();
  pending_region_.reset();
  fullscreen_observation_.Reset();

  lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;

  for (Observer& observer : observers_) {
    observer.OnLensOverlayDidClose();
  }

  state_ = State::kOff;

  RecordEndOfSessionMetrics(dismissal_source);
}

void LensOverlayController::InitializeOverlayUI(
    const OverlayInitializationData& init_data) {
  // This should only contain LensPage mojo calls and should not affect
  // `state_`.
  CHECK(page_);
  page_->ThemeReceived(CreateTheme(init_data.color_palette_));
  page_->ScreenshotDataUriReceived(init_data.current_screenshot_data_uri_);
  if (!init_data.objects_.empty()) {
    SendObjects(CopyObjects(init_data.objects_));
  }
  if (init_data.text_) {
    SendText(init_data.text_->Clone());
  }
  if (pending_region_) {
    page_->SetPostRegionSelection(pending_region_->Clone());
  }
}

std::unique_ptr<views::View> LensOverlayController::CreateViewForOverlay() {
  // Create a flex layout host view to make sure the web view covers the entire
  // tab.
  std::unique_ptr<views::FlexLayoutView> host_view =
      std::make_unique<views::FlexLayoutView>();

  std::unique_ptr<views::WebView> web_view = std::make_unique<views::WebView>(
      tab_->GetContents()->GetBrowserContext());
  web_view->SetProperty(
      views::kFlexBehaviorKey,
      views::FlexSpecification(views::MinimumFlexSizeRule::kScaleToZero,
                               views::MaximumFlexSizeRule::kUnbounded));
  web_view->SetProperty(views::kElementIdentifierKey, kOverlayId);
  views::WebContentsSetBackgroundColor::CreateForWebContentsWithColor(
      web_view->GetWebContents(), SK_ColorTRANSPARENT);

  // Create glue so that WebUIControllers created by this instance can
  // communicate with this instance.
  CreateGlueForWebView(web_view.get());
  // Set the web contents delegate to this controller so we can handle keyboard
  // events. Allow accelerators (e.g. hotkeys) to work on this web view.
  web_view->set_allow_accelerators(true);
  web_view->GetWebContents()->SetDelegate(this);

  // Load the untrusted WebUI into the web view.
  GURL url(chrome::kChromeUILensUntrustedURL);
  web_view->LoadInitialURL(url);

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
  return lens_overlay_event_handler_->HandleKeyboardEvent(
      source, event, overlay_web_view_->GetFocusManager());
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
  CloseUISync(lens::LensOverlayDismissalSource::kFullscreened);
}

void LensOverlayController::OnViewBoundsChanged(views::View* observed_view) {
  CHECK(observed_view == overlay_view_->parent());
  gfx::Rect bounds = observed_view->GetLocalBounds();
  overlay_view_->SetBoundsRect(bounds);
}

void LensOverlayController::OnWidgetDestroying(views::Widget* widget) {
  preselection_widget_ = nullptr;
}

void LensOverlayController::OnOmniboxFocusChanged(
    OmniboxFocusState state,
    OmniboxFocusChangeReason reason) {
  if (state_ == LensOverlayController::State::kOverlay) {
    if (state == OMNIBOX_FOCUS_NONE) {
      ShowPreselectionBubble();
    } else {
      HidePreselectionBubble();
    }
  }
}

void LensOverlayController::OnFindEmptyText(
    content::WebContents* web_contents) {
  CloseUIAsync(lens::LensOverlayDismissalSource::kFindInPageInvoked);
}

void LensOverlayController::OnFindResultAvailable(
    content::WebContents* web_contents) {
  CloseUIAsync(lens::LensOverlayDismissalSource::kFindInPageInvoked);
}

const GURL& LensOverlayController::GetPageURL() const {
  // TODO(b/335234545): Return the page URL when appropriate.
  return GURL::EmptyGURL();
}

SessionID LensOverlayController::GetTabId() const {
  return sessions::SessionTabHelper::IdForTab(tab_->GetContents());
}

metrics::OmniboxEventProto::PageClassification
LensOverlayController::GetPageClassification() const {
  // TODO(b/335234545): Return CONTEXTUAL_SEARCHBOX when appropriate.
  return selected_region_thumbnail_uri_.empty()
             ? metrics::OmniboxEventProto::SEARCH_SIDE_PANEL_SEARCHBOX
             : metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX;
}

std::string& LensOverlayController::GetThumbnail() {
  return selected_region_thumbnail_uri_;
}

const lens::proto::LensOverlayInteractionResponse&
LensOverlayController::GetLensResponse() const {
  return initialization_data_
             ? initialization_data_->interaction_response_
             : lens::proto::LensOverlayInteractionResponse().default_instance();
}

void LensOverlayController::OnTextModified() {
  if (initialization_data_->selected_text_.has_value()) {
    initialization_data_->selected_text_.reset();
    page_->ClearTextSelection();
  }
}

void LensOverlayController::OnThumbnailRemoved() {
  selected_region_thumbnail_uri_.clear();
  lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;
  initialization_data_->selected_region_.reset();
  initialization_data_->selected_region_bitmap_.reset();
  page_->ClearRegionSelection();
}

void LensOverlayController::OnSuggestionAccepted(
    const GURL& destination_url,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion) {
  std::string query_text = "";
  std::map<std::string, std::string> additional_query_parameters;

  net::QueryIterator query_iterator(destination_url);
  while (!query_iterator.IsAtEnd()) {
    std::string_view key = query_iterator.GetKey();
    std::string_view value = query_iterator.GetUnescapedValue();
    if (kTextQueryParameterKey == key) {
      query_text = value;
    } else {
      additional_query_parameters.insert(std::make_pair(
          query_iterator.GetKey(), query_iterator.GetUnescapedValue()));
    }
    query_iterator.Advance();
  }

  IssueSearchBoxRequest(query_text, match_type, is_zero_prefix_suggestion,
                        additional_query_parameters);
}

void LensOverlayController::OnPageBound() {
  // If the side panel closes before the remote gets bound, searchbox_handler_
  // could become unset. Verify it is set before sending to the side panel.
  if (!searchbox_handler_ || !searchbox_handler_->IsRemoteBound()) {
    return;
  }

  // Send any pending inputs for the searchbox.
  if (pending_text_query_.has_value()) {
    searchbox_handler_->SetInputText(*pending_text_query_);
    pending_text_query_.reset();
  }
  if (pending_thumbnail_uri_.has_value()) {
    searchbox_handler_->SetThumbnail(*pending_thumbnail_uri_);
    pending_thumbnail_uri_.reset();
  }
}

void LensOverlayController::OnSidePanelDidOpen() {
  // If a side panel opens that is not ours, we must close the overlay.
  if (side_panel_coordinator_->GetCurrentEntryId() !=
      SidePanelEntry::Id::kLensOverlayResults) {
    CloseUISync(lens::LensOverlayDismissalSource::kUnexpectedSidePanelOpen);
  }
}

void LensOverlayController::OnSidePanelCloseInterrupted() {
  // If we were waiting for the side panel to close, but another side panel
  // opened in the process, we need to close the overlay to not show next to the
  // unwanted side panel.
  if (state_ == State::kClosingOpenedSidePanel) {
    CloseUISync(lens::LensOverlayDismissalSource::kUnexpectedSidePanelOpen);
  }
}

void LensOverlayController::OnSidePanelDidClose() {
  if (state_ == State::kClosingOpenedSidePanel) {
    // This path is invoked after the user invokes the overlay, but we needed to
    // close the side panel before taking a screenshot. The Side panel is now
    // closed so we can now take the screenshot of the page.
    CaptureScreenshot();
  }
}

void LensOverlayController::RenderProcessExited(
    content::RenderProcessHost* host,
    const content::ChildProcessTerminationInfo& info) {
  // Exit early if the overlay is already closing.
  if (IsOverlayClosing()) {
    return;
  }
  // The renderer has exited unexpectedly. Close the overlay so the user does
  // not get into a broken state.
  CloseUISync(lens::LensOverlayDismissalSource::kRendererClosed);
}

void LensOverlayController::TabForegrounded(tabs::TabInterface* tab) {
  // If the overlay was backgrounded, reshow the overlay view.
  if (state_ == State::kBackground) {
    ShowOverlay();
    state_ = (results_side_panel_coordinator_ &&
              results_side_panel_coordinator_->IsEntryShowing())
                 ? State::kOverlayAndResults
                 : State::kOverlay;
    if (state_ != State::kOverlayAndResults) {
      ShowPreselectionBubble();
    }
  }
}

void LensOverlayController::TabWillEnterBackground(tabs::TabInterface* tab) {
  // If the current tab was already backgrounded, do nothing.
  if (state_ == State::kBackground) {
    return;
  }

  // If the overlay was currently showing, then we should background the UI.
  if (IsOverlayShowing()) {
    BackgroundUI();
    return;
  }

  // This is still possible when the controller is in state kScreenshot and the
  // tab was backgrounded. We should close the UI as the overlay has not been
  // created yet.
  CloseUISync(
      lens::LensOverlayDismissalSource::kTabBackgroundedWhileScreenshotting);
}

void LensOverlayController::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  // Background tab contents discarded.
  CloseUISync(lens::LensOverlayDismissalSource::kTabContentsDiscarded);
  old_contents->RemoveUserData(LensOverlayControllerTabLookup::UserDataKey());
  LensOverlayControllerTabLookup::CreateForWebContents(new_contents, this);
}

void LensOverlayController::WillDetach(
    tabs::TabInterface* tab,
    tabs::TabInterface::DetachReason reason) {
  // When dragging a tab into a new window, all window-specific state must be
  // reset. As this flow is not fully functional, close the overlay regardless
  // of `reason`. https://crbug.com/342921671.
  switch (reason) {
    case tabs::TabInterface::DetachReason::kDelete:
      CloseUISync(lens::LensOverlayDismissalSource::kTabClosed);
      return;
    case tabs::TabInterface::DetachReason::kInsertIntoOtherWindow:
      CloseUISync(lens::LensOverlayDismissalSource::kTabDragNewWindow);
      return;
  }
}

void LensOverlayController::DoLensRequest(
    lens::mojom::CenterRotatedBoxPtr region,
    lens::LensOverlaySelectionType selection_type,
    std::optional<SkBitmap> region_bytes) {
  CHECK(initialization_data_);
  CHECK(region);
  SetSearchboxInputText(std::string());
  initialization_data_->selected_region_ = region.Clone();
  initialization_data_->selected_text_.reset();
  initialization_data_->additional_search_query_params_.clear();
  lens_selection_type_ = selection_type;
  if (region_bytes) {
    initialization_data_->selected_region_bitmap_ = region_bytes.value();
  } else {
    initialization_data_->selected_region_bitmap_.reset();
  }

  // TODO(b/332787629): Append the 'mactx' param.
  lens_overlay_query_controller_->SendRegionSearch(
      region.Clone(), selection_type,
      initialization_data_->additional_search_query_params_, region_bytes);
  results_side_panel_coordinator_->RegisterEntryAndShow();
  RecordTimeToFirstInteraction();
  search_performed_in_session_ = true;
  state_ = State::kOverlayAndResults;
}

void LensOverlayController::ActivityRequestedByOverlay(
    ui::mojom::ClickModifiersPtr click_modifiers) {
  // The tab is expected to be in the foreground.
  if (!tab_->IsInForeground()) {
    return;
  }
  tab_->GetBrowserWindowInterface()->OpenURL(
      GURL(lens::features::GetLensOverlayActivityURL()),
      ui::DispositionFromClick(
          click_modifiers->middle_button, click_modifiers->alt_key,
          click_modifiers->ctrl_key, click_modifiers->meta_key,
          click_modifiers->shift_key,
          WindowOpenDisposition::NEW_FOREGROUND_TAB));
}

void LensOverlayController::AddBackgroundBlur() {
  // We do not blur unless the overlay is currently active.
  if (state_ != State::kOverlay && state_ != State::kOverlayAndResults) {
    return;
  }

  int blur_radius_pixels =
      lens::features::GetLensOverlayLivePageBlurRadiusPixels();
  if (blur_radius_pixels >= 0) {
    // SetBackgroundBlur() multiplies by 3 to convert the given
    // value to a pixel value. Since we are already in pixels, we need to divide
    // by 3 so the blur is as expected.
    overlay_web_view_->holder()->GetUILayer()->SetBackgroundBlur(
        blur_radius_pixels / 3);
  }
}

void LensOverlayController::CloseRequestedByOverlayCloseButton() {
  CloseUIAsync(lens::LensOverlayDismissalSource::kOverlayCloseButton);
}

void LensOverlayController::CloseRequestedByOverlayBackgroundClick() {
  CloseUIAsync(lens::LensOverlayDismissalSource::kOverlayBackgroundClick);
}

void LensOverlayController::FeedbackRequestedByOverlay() {
  Browser* tab_browser = chrome::FindBrowserWithTab(tab_->GetContents());
  if (!tab_browser) {
    return;
  }
  chrome::ShowFeedbackPage(
      tab_browser, feedback::kFeedbackSourceLensOverlay,
      /*description_template=*/std::string(),
      /*description_placeholder_text=*/
      l10n_util::GetStringUTF8(IDS_LENS_SEND_FEEDBACK_PLACEHOLDER),
      /*category_tag=*/"lens_overlay",
      /*extra_diagnostics=*/std::string());
}

void LensOverlayController::GetOverlayInvocationSource(
    GetOverlayInvocationSourceCallback callback) {
  std::move(callback).Run(GetInvocationSourceString());
}

void LensOverlayController::InfoRequestedByOverlay(
    ui::mojom::ClickModifiersPtr click_modifiers) {
  // The tab is expected to be in the foreground.
  if (!tab_->IsInForeground()) {
    return;
  }
  tab_->GetBrowserWindowInterface()->OpenURL(
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
  DoLensRequest(std::move(region),
                is_click ? lens::TAP_ON_EMPTY : lens::REGION_SEARCH,
                std::nullopt);
}

void LensOverlayController::IssueLensObjectRequest(
    lens::mojom::CenterRotatedBoxPtr region,
    bool is_mask_click) {
  DoLensRequest(std::move(region),
                is_mask_click ? lens::TAP_ON_REGION_GLEAM : lens::TAP_ON_OBJECT,
                std::nullopt);
}

void LensOverlayController::IssueTextSelectionRequest(const std::string& query,
                                                      int selection_start_index,
                                                      int selection_end_index) {
  initialization_data_->additional_search_query_params_.clear();

  IssueTextSelectionRequestInner(query, selection_start_index,
                                 selection_end_index);
}

void LensOverlayController::IssueTranslateSelectionRequest(
    const std::string& query,
    const std::string& content_language,
    int selection_start_index,
    int selection_end_index) {
  initialization_data_->additional_search_query_params_.clear();
  lens::AppendTranslateParamsToMap(
      initialization_data_->additional_search_query_params_, query, "auto");

  IssueTextSelectionRequestInner(query, selection_start_index,
                                 selection_end_index);
}

void LensOverlayController::IssueTextSelectionRequestInner(
    const std::string& query,
    int selection_start_index,
    int selection_end_index) {
  initialization_data_->selected_region_.reset();
  initialization_data_->selected_region_bitmap_.reset();
  selected_region_thumbnail_uri_.clear();
  lens_selection_type_ = lens::SELECT_TEXT_HIGHLIGHT;
  initialization_data_->selected_text_ =
      std::make_pair(selection_start_index, selection_end_index);

  SetSearchboxInputText(query);
  SetSearchboxThumbnail(std::string());

  lens_overlay_query_controller_->SendTextOnlyQuery(
      query, lens::TextOnlyQueryType::kLensTextSelection,
      initialization_data_->additional_search_query_params_);
  results_side_panel_coordinator_->RegisterEntryAndShow();
  RecordTimeToFirstInteraction();
  search_performed_in_session_ = true;
  state_ = State::kOverlayAndResults;
}

void LensOverlayController::CloseSearchBubble() {
  search_bubble_controller_->Close();
}

void LensOverlayController::ClosePreselectionBubble() {
  if (preselection_widget_) {
    preselection_widget_->Close();
    preselection_widget_ = nullptr;
    preselection_widget_observer_.Reset();
  }
}

void LensOverlayController::ShowPreselectionBubble() {
  if (!preselection_widget_) {
    preselection_widget_ = views::BubbleDialogDelegateView::CreateBubble(
        std::make_unique<lens::LensPreselectionBubble>(
            tab_->GetBrowserWindowInterface()->TopContainer()));
    preselection_widget_->SetNativeWindowProperty(
        views::kWidgetIdentifierKey,
        const_cast<void*>(kLensOverlayPreselectionWidgetIdentifier));
    preselection_widget_observer_.Observe(preselection_widget_);
  }
  preselection_widget_->Show();
}

void LensOverlayController::HidePreselectionBubble() {
  if (preselection_widget_) {
    preselection_widget_->Hide();
  }
}

void LensOverlayController::IssueSearchBoxRequest(
    const std::string& search_box_text,
    AutocompleteMatchType::Type match_type,
    bool is_zero_prefix_suggestion,
    std::map<std::string, std::string> additional_query_params) {
  initialization_data_->additional_search_query_params_ =
      additional_query_params;

  if (initialization_data_->selected_region_.is_null()) {
    lens_selection_type_ = lens::UNKNOWN_SELECTION_TYPE;
    lens_overlay_query_controller_->SendTextOnlyQuery(
        search_box_text, lens::TextOnlyQueryType::kSearchBoxQuery,
        initialization_data_->additional_search_query_params_);
  } else {
    if (is_zero_prefix_suggestion) {
      lens_selection_type_ = lens::MULTIMODAL_SUGGEST_ZERO_PREFIX;
    } else if (match_type ==
               AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED) {
      lens_selection_type_ = lens::MULTIMODAL_SEARCH;
    } else {
      lens_selection_type_ = lens::MULTIMODAL_SUGGEST_TYPEAHEAD;
    }

    lens_overlay_query_controller_->SendMultimodalRequest(
        initialization_data_->selected_region_.Clone(), search_box_text,
        lens_selection_type_,
        initialization_data_->additional_search_query_params_);
  }
  results_side_panel_coordinator_->RegisterEntryAndShow();
  RecordTimeToFirstInteraction();
  search_performed_in_session_ = true;
  state_ = State::kOverlayAndResults;
}

void LensOverlayController::HandleStartQueryResponse(
    std::vector<lens::mojom::OverlayObjectPtr> objects,
    lens::mojom::TextPtr text,
    bool is_error) {
  CHECK(page_);

  // If the full image response fails, the side panel should show the error page
  // since interaction requests will not work.
  SetSidePanelShowErrorPage(/*should_show_error_page=*/is_error);

  if (!objects.empty()) {
    SendObjects(std::move(objects));
  }

  // Text can be null if there was no text within the server response.
  if (!text.is_null()) {
    SendText(std::move(text));
  }
}

void LensOverlayController::HandleInteractionURLResponse(
    lens::proto::LensOverlayUrlResponse response) {
  LoadURLInResultsFrame(GURL(response.url()));
}

void LensOverlayController::HandleInteractionDataResponse(
    lens::proto::LensOverlayInteractionResponse response) {
  initialization_data_->interaction_response_ = response;
}

void LensOverlayController::HandleThumbnailCreated(
    const std::string& thumbnail_bytes) {
  selected_region_thumbnail_uri_ = webui::MakeDataURIForImage(
      base::as_bytes(base::make_span(thumbnail_bytes)), "jpeg");
  SetSearchboxThumbnail(selected_region_thumbnail_uri_);
}

void LensOverlayController::SetSearchboxThumbnail(
    const std::string& thumbnail_uri) {
  if (searchbox_handler_ && searchbox_handler_->IsRemoteBound()) {
    searchbox_handler_->SetThumbnail(thumbnail_uri);
  } else {
    // If the side panel was not bound at the time of request, we store the
    // thumbnail as pending to send it to the searchbox on bind.
    pending_thumbnail_uri_ = thumbnail_uri;
  }
}

void LensOverlayController::RecordTimeToFirstInteraction() {
  if (search_performed_in_session_) {
    return;
  }
  DCHECK(!invocation_time_.is_null());
  base::TimeDelta time_to_first_interaction =
      base::TimeTicks::Now() - invocation_time_;
  // UMA unsliced TimeToFirstInteraction.
  base::UmaHistogramCustomTimes("Lens.Overlay.TimeToFirstInteraction",
                                time_to_first_interaction,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);
  // UMA TimeToFirstInteraction sliced by entry point.
  const auto sliced_time_to_first_interaction_histogram_name =
      "Lens.Overlay.ByInvocationSource." + GetInvocationSourceString() +
      ".TimeToFirstInteraction";
  base::UmaHistogramCustomTimes(sliced_time_to_first_interaction_histogram_name,
                                time_to_first_interaction,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);
  ukm::SourceId source_id =
      tab_->GetContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  // UKM unsliced TimeToFirstInteraction.
  ukm::builders::Lens_Overlay_TimeToFirstInteraction(source_id)
      .SetAllEntryPoints(time_to_first_interaction.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
  // UKM TimeToFirstInteraction sliced by entry point.
  ukm::builders::Lens_Overlay_TimeToFirstInteraction event(source_id);
  switch (invocation_source_) {
    case lens::LensOverlayInvocationSource::kAppMenu:
      event.SetAppMenu(time_to_first_interaction.InMilliseconds());
      break;
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuPage:
      event.SetContentAreaContextMenuPage(
          time_to_first_interaction.InMilliseconds());
      break;
    case lens::LensOverlayInvocationSource::kContentAreaContextMenuImage:
      // Not recorded since the image menu entry point results in a search
      // without the user having to interact with the overlay. Time to first
      // interaction in this case is essentially zero.
      break;
    case lens::LensOverlayInvocationSource::kToolbar:
      event.SetToolbar(time_to_first_interaction.InMilliseconds());
      break;
    case lens::LensOverlayInvocationSource::kFindInPage:
      event.SetFindInPage(time_to_first_interaction.InMilliseconds());
      break;
    case lens::LensOverlayInvocationSource::kOmnibox:
      event.SetOmnibox(time_to_first_interaction.InMilliseconds());
      break;
  }
  event.Record(ukm::UkmRecorder::Get());
}

void LensOverlayController::RecordEndOfSessionMetrics(
    lens::LensOverlayDismissalSource dismissal_source) {
  // UMA invocation source.
  base::UmaHistogramEnumeration("Lens.Overlay.Invoked", invocation_source_);

  // UMA unsliced Dismissed.
  base::UmaHistogramEnumeration("Lens.Overlay.Dismissed", dismissal_source);

  // UMA unsliced InvocationResultedInSearch.
  base::UmaHistogramBoolean("Lens.Overlay.InvocationResultedInSearch",
                            search_performed_in_session_);

  // UMA InvocationResultedInSearch sliced by entry point.
  const auto sliced_search_performed_histogram_name =
      "Lens.Overlay.ByInvocationSource." + GetInvocationSourceString() +
      ".InvocationResultedInSearch";
  base::UmaHistogramBoolean(sliced_search_performed_histogram_name,
                            search_performed_in_session_);

  // UMA unsliced session duration.
  DCHECK(!invocation_time_.is_null());
  base::TimeDelta session_duration = base::TimeTicks::Now() - invocation_time_;
  base::UmaHistogramCustomTimes("Lens.Overlay.SessionDuration",
                                session_duration,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);

  // UMA session duration sliced by entry point.
  const auto sliced_session_duration_histogram_name =
      "Lens.Overlay.ByInvocationSource." + GetInvocationSourceString() +
      ".SessionDuration";
  base::UmaHistogramCustomTimes(sliced_session_duration_histogram_name,
                                session_duration,
                                /*min=*/base::Milliseconds(1),
                                /*max=*/base::Minutes(10), /*buckets=*/50);

  // UKM session end metrics. Includes invocation source, whether the
  // session resulted in a search, and session duration.
  ukm::SourceId source_id =
      tab_->GetContents()->GetPrimaryMainFrame()->GetPageUkmSourceId();
  ukm::builders::Lens_Overlay_SessionEnd(source_id)
      .SetInvocationSource(static_cast<int64_t>(invocation_source_))
      .SetInvocationResultedInSearch(search_performed_in_session_)
      .SetSessionDuration(session_duration.InMilliseconds())
      .Record(ukm::UkmRecorder::Get());
}
