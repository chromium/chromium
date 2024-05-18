// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/lens/lens_overlay_image_helper.h"
#include "chrome/browser/ui/lens/lens_overlay_permission_utils.h"
#include "chrome/browser/ui/lens/lens_overlay_query_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/lens/lens_overlay_url_builder.h"
#include "chrome/browser/ui/lens/lens_permission_bubble_controller.h"
#include "chrome/browser/ui/lens/lens_search_bubble_controller.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/side_panel/side_panel.h"
#include "chrome/browser/ui/views/side_panel/side_panel_coordinator.h"
#include "chrome/browser/ui/views/side_panel/side_panel_util.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/lens/lens_features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/viz/common/frame_timing_details.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/web_ui.h"
#include "net/base/url_util.h"
#include "third_party/lens_server_proto/lens_overlay_selection_type.pb.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/window_open_disposition_utils.h"
#include "ui/compositor/compositor.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/webview/web_contents_set_background_color.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/widget/widget.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/core/window_properties.h"

#if defined(USE_AURA)
#include "ui/aura/window.h"
#include "ui/wm/core/window_util.h"
#endif

namespace {

// The radius of the blur to use for the underlying tab contents.
constexpr int kBlurRadiusPixels = 200;

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

gfx::Rect ComputeOverlayBounds(content::WebContents* contents) {
  auto bounds = contents->GetContainerBounds();
#if BUILDFLAG(IS_CHROMEOS_ASH)
  gfx::NativeWindow top_level_native_window =
      contents->GetTopLevelNativeWindow();
  if (!top_level_native_window->GetProperty(wm::kUsesScreenCoordinatesKey)) {
    wm::ConvertRectFromScreen(top_level_native_window, &bounds);
  }
#endif
  return bounds;
}

gfx::NativeView TopLevelNativeView(content::WebContents* contents) {
  gfx::NativeWindow top_level_native_window =
      contents->GetTopLevelNativeWindow();
  views::Widget* top_level_widget =
      views::Widget::GetWidgetForNativeWindow(top_level_native_window);
  return top_level_widget->GetNativeView();
}

// TODO(b/341360519): Move this to a separate file.
bool SidePanelShouldUseDarkMode(ThemeService* theme_service) {
  if (!lens::features::UseBrowserDarkModeSettingForLensOverlay()) {
    return false;
  }
  ThemeService::BrowserColorScheme color_scheme =
      theme_service->GetBrowserColorScheme();
  return color_scheme == ThemeService::BrowserColorScheme::kSystem
             ? ui::NativeTheme::GetInstanceForNativeUi()->ShouldUseDarkColors()
             : color_scheme == ThemeService::BrowserColorScheme::kDark;
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

#if BUILDFLAG(IS_MAC)
  corner_radii_ = {0, 0, 10, 10};
#endif
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
  if (other.search_query_region_) {
    search_query_region_ = other.search_query_region_->Clone();
  }
  search_query_region_thumbnail_ = other.search_query_region_thumbnail_;
  search_query_url_ = other.search_query_url_;
  selected_text_ = other.selected_text_;
}

LensOverlayController::SearchQuery&
LensOverlayController::SearchQuery::operator=(
    const LensOverlayController::SearchQuery& other) {
  search_query_text_ = other.search_query_text_;
  if (other.search_query_region_) {
    search_query_region_ = other.search_query_region_->Clone();
  }
  search_query_region_thumbnail_ = other.search_query_region_thumbnail_;
  search_query_url_ = other.search_query_url_;
  selected_text_ = other.selected_text_;
  return *this;
}

LensOverlayController::SearchQuery::~SearchQuery() = default;

// static
bool LensOverlayController::IsEnabled(Profile* profile) {
  if (!lens::features::IsLensOverlayEnabled()) {
    return false;
  }

  if (lens::features::IsLensOverlayGoogleDseRequired() &&
      !search::DefaultSearchProviderIsGoogle(profile)) {
    return false;
  }

  static int phys_mem_mb = base::SysInfo::AmountOfPhysicalMemoryMB();
  return phys_mem_mb > lens::features::GetLensOverlayMinRamMb();
}

void LensOverlayController::ShowUIWithPendingRegion(
    lens::LensOverlayInvocationSource invocation_source,
    lens::mojom::CenterRotatedBoxPtr region) {
  pending_region_ = std::move(region);
  ShowUI(invocation_source);
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

  // Request user permission before grabbing a screenshot.
  Browser* tab_browser = chrome::FindBrowserWithTab(tab_->GetContents());
  CHECK(tab_browser);
  CHECK(pref_service_);
  if (!lens::CanSharePageScreenshotWithLensOverlay(pref_service_)) {
    if (!permission_bubble_controller_) {
      permission_bubble_controller_ =
          std::make_unique<lens::LensPermissionBubbleController>(
              tab_->GetBrowserWindowInterface(), pref_service_);
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
        std::make_unique<lens::LensOverlaySidePanelCoordinator>(
            tab_browser, this,
            SidePanelUI::GetSidePanelUIForBrowser(tab_browser),
            tab_->GetContents());
  }
  if (lens::features::IsLensOverlaySearchBubbleEnabled()) {
    lens::LensSearchBubbleController::GetOrCreateForBrowser(tab_browser)
        ->Show();
  }

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
      variations_client_, identity_manager_, invocation_source,
      SidePanelShouldUseDarkMode(theme_service_));

  side_panel_coordinator_ =
      SidePanelUtil::GetSidePanelCoordinatorForBrowser(tab_browser);
  CHECK(side_panel_coordinator_);

  // Setup observer to be notified of side panel opens and closes.
  side_panel_state_observer_.Observe(side_panel_coordinator_);

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
  base::UmaHistogramEnumeration("Lens.Overlay.Invoked", invocation_source);
}

void LensOverlayController::CloseUIAsync(
    lens::LensOverlayDismissalSource dismissal_source) {
  if (state_ == State::kOff || IsOverlayClosing()) {
    return;
  }

  // If the tab is in the background, the async processes needed if the callback
  // is coming from the WebUI don't apply and we can call CloseUI directly.
  if (!tab_->IsInForeground()) {
    state_ = State::kClosing;
    CloseUIPart2(dismissal_source);
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
      base::BindOnce(&LensOverlayController::UnblurBackgroundAndContinueClose,
                     weak_factory_.GetWeakPtr(), dismissal_source),
      kFadeoutAnimationTimeout);
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
  base::UmaHistogramBoolean("Desktop.LensOverlay.Shown", true);
  state_ = State::kOverlay;

  // Only start the query flow again if we don't already have a full image
  // response.
  if (!initialization_data_->has_full_image_response()) {
    lens_overlay_query_controller_->StartQueryFlow(
        initialization_data_->current_screenshot_,
        initialization_data_->page_url_, initialization_data_->page_title_);
  }
  if (pending_region_) {
    IssueLensRequest(std::move(pending_region_));
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
}

void LensOverlayController::SetSearchboxHandler(
    std::unique_ptr<RealboxHandler> handler) {
  searchbox_handler_.reset();
  searchbox_handler_ = std::move(handler);
}

void LensOverlayController::ResetSearchboxHandler() {
  searchbox_handler_.reset();
}

views::Widget* LensOverlayController::GetOverlayWidgetForTesting() {
  return overlay_widget_.get();
}

void LensOverlayController::ResetUIBounds() {
  content::WebContents* active_web_contents = tab_->GetContents();
  overlay_widget_->SetBounds(ComputeOverlayBounds(active_web_contents));
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

void LensOverlayController::SendObjects(
    std::vector<lens::mojom::OverlayObjectPtr> objects) {
  page_->ObjectsReceived(std::move(objects));
}

void LensOverlayController::NotifyResultsPanelOpened() {
  UpdateCornerRadiusForSidePanel();

  page_->NotifyResultsPanelOpened();
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
      lens::RemoveUrlViewportParams(loaded_search_query->search_query_url_) ==
          lens::RemoveUrlViewportParams(search_url)) {
    return;
  }

  // Create the search query struct.
  SearchQuery search_query(std::move(query), std::move(search_url));
  if (initialization_data_->selected_region_) {
    search_query.search_query_region_ =
        initialization_data_->selected_region_->Clone();
    search_query.search_query_region_thumbnail_ = thumbnail_uri_;
  }
  if (initialization_data_->selected_text_.has_value()) {
    search_query.selected_text_ = initialization_data_->selected_text_.value();
  }

  // Add the last loaded search query to the query stack if it is present.
  if (loaded_search_query) {
    initialization_data_->search_query_history_stack_.push_back(
        loaded_search_query.value());
    side_panel_page_->SetBackArrowVisible(true);
  }

  // Set the currently loaded search query to the one we just created.
  initialization_data_->currently_loaded_search_query_ = search_query;
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
  // query.
  CHECK(page_);
  page_->ClearAllSelections();
  if (query.selected_text_.has_value()) {
    page_->SetTextSelection(query.selected_text_->first,
                            query.selected_text_->second);
  } else if (query.search_query_region_) {
    page_->SetPostRegionSelection(query.search_query_region_->Clone());
  }

  // Update the searchbox state and the results frame URL. After, set the
  // currently loaded query to the one we just popped.
  SetSearchboxInputText(query.search_query_text_);
  SetSearchboxThumbnail(query.search_query_region_thumbnail_);
  LoadURLInResultsFrame(query.search_query_url_);
  initialization_data_->currently_loaded_search_query_ = query;
}

void LensOverlayController::SetSidePanelIsLoadingResults(bool is_loading) {
  if (side_panel_page_) {
    side_panel_page_->SetIsLoadingResults(is_loading);
  }
}

void LensOverlayController::OnSidePanelEntryDeregistered() {
  if (state_ == State::kClosingSidePanel) {
    CHECK(last_dismissal_source_.has_value());
    UnblurBackgroundAndContinueClose(*last_dismissal_source_);
    last_dismissal_source_.reset();
    return;
  }
  CloseUIAsync(lens::LensOverlayDismissalSource::kSidePanelCloseButton);
}

void LensOverlayController::IssueTextSelectionRequestForTesting(
    const std::string& text_query,
    int selection_start_index,
    int selection_end_index) {
  IssueTextSelectionRequest(text_query, selection_start_index,
                            selection_end_index);
}

void LensOverlayController::IssueTranslateSelectionRequestForTesting(
    const std::string& text_query,
    const std::string& content_language,
    int selection_start_index,
    int selection_end_index) {
  IssueTranslateSelectionRequest(text_query, content_language,
                                 selection_start_index, selection_end_index);
}

content::WebContents*
LensOverlayController::GetSidePanelWebContentsForTesting() {
  if (!results_side_panel_coordinator_) {
    return nullptr;
  }
  return results_side_panel_coordinator_->GetSidePanelWebContents();
}

std::unique_ptr<lens::LensOverlayQueryController>
LensOverlayController::CreateLensQueryController(
    lens::LensOverlayFullImageResponseCallback full_image_callback,
    lens::LensOverlayUrlResponseCallback url_callback,
    lens::LensOverlayInteractionResponseCallback interaction_data_callback,
    lens::LensOverlayThumbnailCreatedCallback thumbnail_created_callback,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager,
    lens::LensOverlayInvocationSource invocation_source,
    bool use_dark_mode) {
  return std::make_unique<lens::LensOverlayQueryController>(
      std::move(full_image_callback), std::move(url_callback),
      std::move(interaction_data_callback),
      std::move(thumbnail_created_callback), variations_client,
      identity_manager, invocation_source, use_dark_mode);
}

LensOverlayController::OverlayInitializationData::OverlayInitializationData(
    const SkBitmap& screenshot,
    const std::string& data_uri,
    std::optional<GURL> page_url,
    std::optional<std::string> page_title,
    std::vector<lens::mojom::OverlayObjectPtr> objects,
    lens::mojom::TextPtr text,
    const lens::proto::LensOverlayInteractionResponse& interaction_response,
    lens::mojom::CenterRotatedBoxPtr selected_region)
    : current_screenshot_(screenshot),
      current_screenshot_data_uri_(data_uri),
      page_url_(page_url),
      page_title_(page_title),
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
  void FrameSizeChanged(content::RenderFrameHost* render_frame_host,
                        const gfx::Size& frame_size) override {
    // We only care to resize the overlay when it's visible to the user.
    if (lens_overlay_controller_->IsOverlayShowing()) {
      lens_overlay_controller_->ResetUIBounds();
    }
  }

  // content::WebContentsObserver
  void PrimaryPageChanged(content::Page& page) override {
    lens_overlay_controller_->CloseUIAsync(
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
    CloseUIAsync(
        lens::LensOverlayDismissalSource::kErrorScreenshotCreationFailed);
  }

  state_ = State::kScreenshot;
  // Side panel is now full closed, take screenshot and open overlay.
  view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(),
      base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&LensOverlayController::DidCaptureScreenshot,
                         weak_factory_.GetWeakPtr(),
                         ++screenshot_attempt_id_)));
}

void LensOverlayController::DidCaptureScreenshot(int attempt_id,
                                                 const SkBitmap& bitmap) {
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
    CloseUIAsync(
        lens::LensOverlayDismissalSource::kErrorScreenshotCreationFailed);
    return;
  }

  // Encode the screenshot so we can transform it into a data URI for the WebUI.
  scoped_refptr<base::RefCountedBytes> data;
  if (!lens::EncodeImage(
          bitmap, lens::features::GetLensOverlayScreenshotRenderQuality(),
          &data)) {
    // TODO(b/334185985): Handle case when screenshot data URI encoding fails.
    CloseUIAsync(
        lens::LensOverlayDismissalSource::kErrorScreenshotEncodingFailed);
    return;
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
      bitmap, webui::MakeDataURIForImage(data->as_vector(), "jpeg"), page_url,
      page_title);

  ShowOverlayWidget();

  state_ = State::kStartingWebUI;
}

void LensOverlayController::ShowOverlayWidget() {
  content::WebContents* active_web_contents = tab_->GetContents();

  if (overlay_widget_) {
    CHECK(overlay_web_view_);
    CHECK(!overlay_widget_->IsVisible());

    // If the tab has moved windows, the widget must be reparented.
    if (overlay_widget_window_session_id_.value() !=
        tab_->GetBrowserWindowInterface()->GetSessionID()) {
      views::Widget::ReparentNativeView(
          overlay_widget_->GetNativeView(),
          TopLevelNativeView(tab_->GetContents()));
    }

    // Regardless, reset state associated with the parent widget.
    tab_contents_observer_ = std::make_unique<UnderlyingWebContentsObserver>(
        active_web_contents, this);
    overlay_widget_window_session_id_.emplace(
        tab_->GetBrowserWindowInterface()->GetSessionID());
    ResetUIBounds();

    overlay_widget_->Show();

    // The overlay needs to be focused on show to immediately begin
    // receiving key events.
    overlay_web_view_->RequestFocus();
    return;
  }

  overlay_widget_ = std::make_unique<views::Widget>();
  overlay_widget_->Init(CreateWidgetInitParams());
  overlay_widget_->SetContentsView(CreateViewForOverlay());
  overlay_widget_window_session_id_.emplace(
      tab_->GetBrowserWindowInterface()->GetSessionID());

  tab_contents_observer_ = std::make_unique<UnderlyingWebContentsObserver>(
      active_web_contents, this);

  // Stack widget at top.
  gfx::NativeWindow top_level_native_window =
      active_web_contents->GetTopLevelNativeWindow();
  views::Widget* top_level_widget =
      views::Widget::GetWidgetForNativeWindow(top_level_native_window);
  overlay_widget_->StackAboveWidget(top_level_widget);

  overlay_widget_->Show();
  // The overlay needs to be focused on show to immediately begin
  // receiving key events.
  CHECK(overlay_web_view_);
  overlay_web_view_->RequestFocus();
}

void LensOverlayController::BackgroundUI() {
  RemoveBackgroundBlur();
  overlay_widget_->Hide();
  tab_contents_observer_.reset();
  state_ = State::kBackground;
  // TODO(b/335516480): Schedule the UI to be suspended.
}

void LensOverlayController::SetWebViewCornerRadii(
    const gfx::RoundedCornersF& radii) {
  CHECK(overlay_web_view_);
  views::NativeViewHost* host = overlay_web_view_->holder();
  DCHECK(host);

  host->SetCornerRadii(radii);
}

void LensOverlayController::UpdateCornerRadiusForSidePanel() {
  Browser* tab_browser = chrome::FindBrowserWithTab(tab_->GetContents());
  CHECK(tab_browser);
  auto* browser_view = BrowserView::GetBrowserViewForBrowser(tab_browser);
  CHECK(browser_view);

  bool is_right_aligned = browser_view->unified_side_panel()->IsRightAligned();
  const float corner_radius =
      browser_view->GetLayoutProvider()->GetCornerRadiusMetric(
          views::ShapeContextTokens::kSidePanelPageContentRadius);
  if (is_right_aligned) {
    corner_radii_.set_upper_right(corner_radius);
    corner_radii_.set_lower_right(0);
  } else {
    corner_radii_.set_upper_left(corner_radius);
    corner_radii_.set_lower_left(0);
  }
  SetWebViewCornerRadii(corner_radii_);
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
  // `results_side_panel_coordinator_` or `overlay_widget_`.
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
  results_side_panel_coordinator_.reset();

  side_panel_state_observer_.Reset();
  side_panel_coordinator_ = nullptr;

  // Widget destruction can be asynchronous. We want to synchronously release
  // resources, so we clear the contents view immediately.
  overlay_web_view_ = nullptr;
  if (overlay_widget_) {
    overlay_widget_->SetContentsView(std::make_unique<views::View>());
  }
  overlay_widget_.reset();
  overlay_widget_window_session_id_.reset();
  tab_contents_observer_.reset();

  searchbox_handler_.reset();
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
  thumbnail_uri_.clear();
  pending_region_.reset();
  fullscreen_observation_.Reset();

  state_ = State::kOff;

  base::UmaHistogramEnumeration("Lens.Overlay.Dismissed", dismissal_source);
}

void LensOverlayController::UnblurBackgroundAndContinueClose(
    lens::LensOverlayDismissalSource dismissal_source) {
  if (state_ != State::kClosing && state_ != State::kClosingSidePanel) {
    return;
  }

  // If we are not in a closing flow, do nothing.
  CHECK(state_ == State::kClosing || state_ == State::kClosingSidePanel);

  // To avoid flickering, we need to remove the background blur and wait for a
  // paint before closing the rest of the overlay.
  RemoveBackgroundBlur();

  // This callback can come from the WebUI. CloseUI synchronously destroys the
  // WebUI. Therefore it is important to dispatch to the call to CloseUIAsync to
  // avoid re-entrancy.
  auto* ui_layer_compositor = tab_->GetBrowserWindowInterface()
                                  ->GetWebView()
                                  ->holder()
                                  ->GetUILayer()
                                  ->GetCompositor();
  ui_layer_compositor->RequestSuccessfulPresentationTimeForNextFrame(
      base::BindOnce(&LensOverlayController::OnBackgroundUnblurred,
                     weak_factory_.GetWeakPtr(), dismissal_source));
}

void LensOverlayController::OnBackgroundUnblurred(
    lens::LensOverlayDismissalSource dismissal_source,
    const viz::FrameTimingDetails& details) {
  // We only finish the closing process once the background has been unblurred.
  CloseUIPart2(dismissal_source);
}

void LensOverlayController::InitializeOverlayUI(
    const OverlayInitializationData& init_data) {
  // This should only contain LensPage mojo calls and should not affect
  // `state_`.
  CHECK(page_);
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

views::Widget::InitParams LensOverlayController::CreateWidgetInitParams() {
  content::WebContents* active_web_contents = tab_->GetContents();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "LensOverlayWidget";
  params.child = true;

  params.parent = TopLevelNativeView(active_web_contents);
  params.layer_type = ui::LAYER_NOT_DRAWN;

  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.bounds = ComputeOverlayBounds(active_web_contents);
  return params;
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
  // We do not want to show the browser context menu on the overlay.
  return true;
}

bool LensOverlayController::HandleKeyboardEvent(
    content::WebContents* source,
    const content::NativeWebKeyboardEvent& event) {
  return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
      event, overlay_web_view_->GetFocusManager());
}

void LensOverlayController::OnFullscreenStateChanged() {
  CloseUIAsync(lens::LensOverlayDismissalSource::kFullscreened);
}

const GURL& LensOverlayController::GetPageURL() const {
  content::WebContents* contents = tab_->GetContents();
  return contents ? contents->GetVisibleURL() : GURL::EmptyGURL();
}

metrics::OmniboxEventProto::PageClassification
LensOverlayController::GetPageClassification() const {
  // TODO(b/332787629): Return CONTEXTUAL_SEARCHBOX when appropriate.
  return thumbnail_uri_.empty()
             ? metrics::OmniboxEventProto::SEARCH_SIDE_PANEL_SEARCHBOX
             : metrics::OmniboxEventProto::LENS_SIDE_PANEL_SEARCHBOX;
}

std::string& LensOverlayController::GetThumbnail() {
  return thumbnail_uri_;
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
  thumbnail_uri_.clear();
  initialization_data_->selected_region_.reset();
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
    CloseUIAsync(lens::LensOverlayDismissalSource::kUnexpectedSidePanelOpen);
  }
}

void LensOverlayController::OnSidePanelCloseInterrupted() {
  // If we were waiting for the side panel to close, but another side panel
  // opened in the process, we need to close the overlay to not show next to the
  // unwanted side panel.
  if (state_ == State::kClosingOpenedSidePanel) {
    CloseUIAsync(lens::LensOverlayDismissalSource::kUnexpectedSidePanelOpen);
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

void LensOverlayController::TabForegrounded(tabs::TabInterface* tab) {
  // If the overlay was backgrounded, reshow the overlay widget.
  if (state_ == State::kBackground) {
    ShowOverlayWidget();
    state_ = State::kOverlay;

    // Show after moving to kOverlay state.
    AddBackgroundBlur();
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
  CloseUIAsync(
      lens::LensOverlayDismissalSource::kTabBackgroundedWhileScreenshotting);
}

void LensOverlayController::WillDiscardContents(
    tabs::TabInterface* tab,
    content::WebContents* old_contents,
    content::WebContents* new_contents) {
  // Background tab contents discarded.
  CloseUIAsync(lens::LensOverlayDismissalSource::kTabContentsDiscarded);
  old_contents->RemoveUserData(LensOverlayControllerTabLookup::UserDataKey());
  LensOverlayControllerTabLookup::CreateForWebContents(new_contents, this);
}

void LensOverlayController::RemoveBackgroundBlur() {
  auto* ui_layer =
      tab_->GetBrowserWindowInterface()->GetWebView()->holder()->GetUILayer();
  ui_layer->SetClipRect(gfx::Rect());
  ui_layer->SetLayerBlur(0);
}

void LensOverlayController::AddBackgroundBlur() {
  // We do not blur unless the overlay is currently active.
  if (state_ != State::kOverlay && state_ != State::kOverlayAndResults) {
    return;
  }
  // Blur the original web contents. This should be done after the overlay
  // widget is showing and the screenshot is rendered so the user cannot see the
  // live page get blurred. SetLayerBlur() multiplies by 3 to convert the given
  // value to a pixel value. Since we are already in pixels, we need to divide
  // by 3 so the blur is as expected.
  CHECK(tab_->IsInForeground());
  auto* ui_layer =
      tab_->GetBrowserWindowInterface()->GetWebView()->holder()->GetUILayer();

#if BUILDFLAG(IS_MAC)
  // This fixes an issue on Mac where the blur will leak beyond the webpage
  // and into the toolbar. Setting a clip rect forces the mask to not
  // overflow. Clipping the rect breaks on linux, so gating the change to MacOS
  // until a fix to cc allows for a universal solution. See b/328294684.
  gfx::Rect web_contents_rect = tab_->GetContents()->GetContainerBounds();
  ui_layer->SetClipRect(gfx::Rect(0, kBlurRadiusPixels - 2,
                                  web_contents_rect.width(),
                                  web_contents_rect.height()));
#endif  // BUILDFLAG(IS_MAC)
  ui_layer->SetLayerBlur(kBlurRadiusPixels / 3);
}

void LensOverlayController::CloseRequestedByOverlayCloseButton() {
  CloseUIAsync(lens::LensOverlayDismissalSource::kOverlayCloseButton);
}

void LensOverlayController::CloseRequestedByOverlayBackgroundClick() {
  CloseUIAsync(lens::LensOverlayDismissalSource::kOverlayBackgroundClick);
}

void LensOverlayController::CloseRequestedByOverlayEscapeKeyPress() {
  CloseUIAsync(lens::LensOverlayDismissalSource::kEscapeKeyPress);
}

void LensOverlayController::CloseRequestedBySidePanelEscapeKeyPress() {
  CloseUIAsync(lens::LensOverlayDismissalSource::kEscapeKeyPress);
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

void LensOverlayController::InfoRequestedByOverlay(
    ui::mojom::ClickModifiersPtr click_modifiers) {
  // The tab is expected to be in the foreground.
  CHECK(tab_->IsInForeground());
  tab_->GetBrowserWindowInterface()->OpenURL(
      GURL(lens::features::GetLensOverlayHelpCenterURL()),
      ui::DispositionFromClick(
          click_modifiers->middle_button, click_modifiers->alt_key,
          click_modifiers->ctrl_key, click_modifiers->meta_key,
          click_modifiers->shift_key,
          WindowOpenDisposition::NEW_BACKGROUND_TAB));
}

void LensOverlayController::IssueLensRequest(
    lens::mojom::CenterRotatedBoxPtr region) {
  CHECK(initialization_data_);
  CHECK(region);
  SetSearchboxInputText(std::string());
  initialization_data_->selected_region_ = region.Clone();
  initialization_data_->selected_text_.reset();
  // TODO(b/332787629): Append the 'mactx' param.
  // TODO(b/335718601): Remove query parameters from region search.
  lens_overlay_query_controller_->SendRegionSearch(
      region.Clone(), initialization_data_->additional_search_query_params_);
  results_side_panel_coordinator_->RegisterEntryAndShow();
  state_ = State::kOverlayAndResults;
}

void LensOverlayController::IssueObjectSelectionRequest(
    const std::string& object_id) {
  SetSearchboxInputText(std::string());
  // TODO(b/332787629): Append the 'mactx' param.
  initialization_data_->additional_search_query_params_.clear();
  initialization_data_->selected_region_.reset();
  initialization_data_->selected_text_.reset();
  // TODO(b/335718601): Remove query parameters from object selection.
  lens_overlay_query_controller_->SendObjectSelection(
      object_id, initialization_data_->additional_search_query_params_);
  results_side_panel_coordinator_->RegisterEntryAndShow();
  state_ = State::kOverlayAndResults;
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
      initialization_data_->additional_search_query_params_, query,
      content_language);

  IssueTextSelectionRequestInner(query, selection_start_index,
                                 selection_end_index);
}

void LensOverlayController::IssueTextSelectionRequestInner(
    const std::string& query,
    int selection_start_index,
    int selection_end_index) {
  initialization_data_->selected_region_.reset();
  thumbnail_uri_.clear();
  initialization_data_->selected_text_ =
      std::make_pair(selection_start_index, selection_end_index);

  SetSearchboxInputText(query);
  SetSearchboxThumbnail(std::string());

  // TODO(b/332787629): Append the 'mactx' param.
  lens_overlay_query_controller_->SendTextOnlyQuery(
      query, initialization_data_->additional_search_query_params_);
  results_side_panel_coordinator_->RegisterEntryAndShow();
  state_ = State::kOverlayAndResults;
}

void LensOverlayController::CloseSearchBubble() {
  if (Browser* tab_browser = chrome::FindBrowserWithTab(tab_->GetContents())) {
    if (auto* controller =
            lens::LensSearchBubbleController::FromBrowser(tab_browser)) {
      controller->Close();
    }
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
    lens_overlay_query_controller_->SendTextOnlyQuery(
        search_box_text, initialization_data_->additional_search_query_params_);
  } else {
    lens::LensOverlaySelectionType multimodal_selection_type;
    if (is_zero_prefix_suggestion) {
      multimodal_selection_type = lens::MULTIMODAL_SUGGEST_ZERO_PREFIX;
    } else if (match_type ==
               AutocompleteMatchType::Type::SEARCH_WHAT_YOU_TYPED) {
      multimodal_selection_type = lens::MULTIMODAL_SEARCH;
    } else {
      multimodal_selection_type = lens::MULTIMODAL_SUGGEST_TYPEAHEAD;
    }

    lens_overlay_query_controller_->SendMultimodalRequest(
        initialization_data_->selected_region_.Clone(), search_box_text,
        multimodal_selection_type,
        initialization_data_->additional_search_query_params_);
  }
  results_side_panel_coordinator_->RegisterEntryAndShow();
  state_ = State::kOverlayAndResults;
}

void LensOverlayController::HandleStartQueryResponse(
    std::vector<lens::mojom::OverlayObjectPtr> objects,
    lens::mojom::TextPtr text) {
  CHECK(page_);
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
  thumbnail_uri_ = webui::MakeDataURIForImage(
      base::as_bytes(base::make_span(thumbnail_bytes)), "jpeg");
  SetSearchboxThumbnail(thumbnail_uri_);
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
