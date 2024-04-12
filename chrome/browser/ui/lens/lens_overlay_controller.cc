// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/lens/lens_overlay_controller.h"

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/lens/core/mojom/overlay_object.mojom.h"
#include "chrome/browser/lens/lens_overlay/lens_overlay_query_controller.h"
#include "chrome/browser/lens/lens_overlay/lens_overlay_url_builder.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/tabs/tab_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/side_panel/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/common/webui_url_constants.h"
#include "components/lens/lens_features.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents_user_data.h"
#include "content/public/browser/web_ui.h"
#include "net/base/url_util.h"
#include "third_party/lens_server_proto/lens_overlay_service_deps.pb.h"
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

}  // namespace

LensOverlayController::LensOverlayController(tabs::TabModel* tab_model)
    : tab_model_(tab_model) {
  if (tab_model_->contents()) {
    LensOverlayControllerTabLookup::CreateForWebContents(tab_model_->contents(),
                                                         this);
  }

  // Automatically unregisters on destruction.
  tab_model_->owning_model()->AddObserver(this);
  tab_model_observer_.Observe(tab_model);
}

LensOverlayController::~LensOverlayController() {
  CloseUI();
  lens_overlay_query_controller_.reset();
  if (tab_model_->contents()) {
    tab_model_->contents()->RemoveUserData(
        LensOverlayControllerTabLookup::UserDataKey());
  }
}

DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(LensOverlayController, kOverlayId);
DEFINE_CLASS_ELEMENT_IDENTIFIER_VALUE(LensOverlayController,
                                      kOverlaySidePanelWebViewId);

bool LensOverlayController::Enabled() {
  return lens::features::IsLensOverlayEnabled();
}

void LensOverlayController::ShowUI() {
  // If UI is already showing or in the process of showing, do nothing.
  if (state_ != State::kOff) {
    return;
  }

  // The UI should only show if the tab is in the foreground.
  if (tab_model_->owning_model()->GetActiveTab() != tab_model_) {
    return;
  }

  // Begin the process of grabbing a screenshot.
  content::RenderWidgetHostView* view = tab_model_->contents()
                                            ->GetPrimaryMainFrame()
                                            ->GetRenderViewHost()
                                            ->GetWidget()
                                            ->GetView();

  // During initialization and shutdown a capture may not be possible.
  if (!view || !view->IsSurfaceAvailableForCopy()) {
    return;
  }

  // Create the results side panel coordinator when showing the UI if it does
  // not already exist for this tab's web contents.
  if (!results_side_panel_coordinator_) {
    Browser* tab_browser = chrome::FindBrowserWithTab(tab_model_->contents());
    CHECK(tab_browser);
    results_side_panel_coordinator_ =
        std::make_unique<lens::LensOverlaySidePanelCoordinator>(
            tab_browser, this,
            SidePanelUI::GetSidePanelUIForBrowser(tab_browser),
            tab_model_->contents());
  }

  // Create the query controller.
  lens_overlay_query_controller_ =
      std::make_unique<lens::LensOverlayQueryController>(
          base::BindRepeating(&LensOverlayController::HandleStartQueryResponse,
                              weak_factory_.GetWeakPtr()),
          base::BindRepeating(
              &LensOverlayController::HandleInteractionURLResponse,
              weak_factory_.GetWeakPtr()),
          base::BindRepeating(
              &LensOverlayController::HandleInteractionDataResponse,
              weak_factory_.GetWeakPtr()),
          tab_model_->owning_model()->profile());

  state_ = State::kScreenshot;
  view->CopyFromSurface(
      /*src_rect=*/gfx::Rect(), /*output_size=*/gfx::Size(),
      base::BindPostTask(
          base::SequencedTaskRunner::GetCurrentDefault(),
          base::BindOnce(&LensOverlayController::DidCaptureScreenshot,
                         weak_factory_.GetWeakPtr(),
                         ++screenshot_attempt_id_)));
}

void LensOverlayController::CloseUI() {
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

  // A permission prompt may be suspended if the overlay was showing when the
  // permission was queued. Restore the suspended prompt if possible.
  // TODO(b/331940245): Refactor to be decoupled from PermissionPromptFactory
  content::WebContents* contents = tab_model_->contents();
  if (contents) {
    auto* permission_request_manager =
        permissions::PermissionRequestManager::FromWebContents(contents);
    if (permission_request_manager &&
        permission_request_manager->CanRestorePrompt()) {
      permission_request_manager->RestorePrompt();
    }
  }

  results_side_panel_coordinator_.reset();

  // Widget destruction can be asynchronous. We want to synchronously release
  // resources, so we clear the contents view immediately.
  if (overlay_widget_) {
    overlay_widget_->SetContentsView(std::make_unique<views::View>());
  }
  overlay_widget_.reset();
  tab_contents_observer_.reset();

  searchbox_handler_.reset();
  side_panel_receiver_.reset();
  side_panel_page_.reset();
  receiver_.reset();
  page_.reset();
  current_screenshot_.reset();
  lens_overlay_query_controller_.reset();
  // In the future we may want a hibernate state. In this case we would stop
  // showing the UI but persist enough information to defrost the original UI
  // state when the tab is foregrounded.
  state_ = State::kOff;
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

void LensOverlayController::BindOverlay(
    mojo::PendingReceiver<lens::mojom::LensPageHandler> receiver,
    mojo::PendingRemote<lens::mojom::LensPage> page) {
  if (state_ != State::kStartingWebUI) {
    return;
  }
  receiver_.Bind(std::move(receiver));
  page_.Bind(std::move(page));
  base::UmaHistogramBoolean("Desktop.LensOverlay.Shown", true);
  state_ = State::kOverlay;

  lens_overlay_query_controller_->StartQueryFlow(current_screenshot_);
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
    // TODO(b/330204523): Send query to the searchbox.
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
  content::WebContents* active_web_contents = tab_model_->contents();
  overlay_widget_->SetBounds(active_web_contents->GetContainerBounds());
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

void LensOverlayController::SendObjects(
    std::vector<lens::mojom::OverlayObjectPtr> objects) {
  page_->ObjectsReceived(std::move(objects));
}

void LensOverlayController::SendText(lens::mojom::TextPtr text) {
  page_->TextReceived(std::move(text));
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

bool LensOverlayController::IsOverlayShowing() {
  return state_ == State::kStartingWebUI || state_ == State::kOverlay ||
         state_ == State::kOverlayAndResults;
}

void LensOverlayController::OnSidePanelEntryDeregistered() {
  // TODO(b/328296424): Currently, when the lens overlay side panel entry is
  // hidden, the lens overlay can still be present so this is needed. When
  // implementing the change to hide the overlay when the side panel entry is
  // hidden, this will no longer be needed.
  side_panel_page_.reset();
  side_panel_receiver_.reset();
}

void LensOverlayController::IssueTextSelectionRequestForTesting(
    const std::string& text_query) {
  IssueTextSelectionRequest(text_query);
}

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
    lens_overlay_controller_->CloseUIAsync();
  }

 private:
  raw_ptr<LensOverlayController> lens_overlay_controller_;
};

void LensOverlayController::DidCaptureScreenshot(int attempt_id,
                                                 const SkBitmap& bitmap) {
  // While capturing a screenshot the overlay was cancelled. Do nothing.
  if (state_ == State::kOff) {
    return;
  }

  // An id mismatch implies this is not the most recent screenshot attempt.
  if (screenshot_attempt_id_ != attempt_id) {
    return;
  }

  // It is not possible to show the overlay UI if the tab is not associated with
  // a tab strip.
  if (!tab_model_->owning_model()) {
    CloseUI();
    return;
  }

  // The documentation for CopyFromSurface claims that the copy can fail, but
  // without providing information about how this can happen.
  // Supposedly IsSurfaceAvailableForCopy() should guard against this case, but
  // this is a multi-process, multi-threaded environment so there may be a
  // TOCTTOU race condition.
  if (bitmap.drawsNothing()) {
    CloseUI();
    return;
  }

  // Need to store the current screenshot before creating the WebUI, since the
  // WebUI is dependent on the screenshot.
  current_screenshot_ = bitmap;
  ShowOverlayWidget();

  state_ = State::kStartingWebUI;
}

void LensOverlayController::ShowOverlayWidget() {
  CHECK(!overlay_widget_);

  overlay_widget_ = std::make_unique<views::Widget>();
  overlay_widget_->Init(CreateWidgetInitParams());
  overlay_widget_->SetContentsView(CreateViewForOverlay());

  content::WebContents* active_web_contents = tab_model_->contents();
  tab_contents_observer_ = std::make_unique<UnderlyingWebContentsObserver>(
      active_web_contents, this);

  // Stack widget at top.
  gfx::NativeWindow top_level_native_window =
      active_web_contents->GetTopLevelNativeWindow();
  views::Widget* top_level_widget =
      views::Widget::GetWidgetForNativeWindow(top_level_native_window);
  overlay_widget_->StackAboveWidget(top_level_widget);

  overlay_widget_->Show();
}

views::Widget::InitParams LensOverlayController::CreateWidgetInitParams() {
  content::WebContents* active_web_contents = tab_model_->contents();
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = "LensOverlayWidget";
  params.child = true;

  gfx::NativeWindow top_level_native_window =
      active_web_contents->GetTopLevelNativeWindow();
  views::Widget* top_level_widget =
      views::Widget::GetWidgetForNativeWindow(top_level_native_window);
  gfx::NativeView top_level_native_view = top_level_widget->GetNativeView();
  params.parent = top_level_native_view;
  params.layer_type = ui::LAYER_NOT_DRAWN;

  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.bounds = active_web_contents->GetContainerBounds();
  return params;
}

std::unique_ptr<views::View> LensOverlayController::CreateViewForOverlay() {
  CHECK(tab_model_);
  // Create a flex layout host view to make sure the web view covers the entire
  // tab.
  std::unique_ptr<views::FlexLayoutView> host_view =
      std::make_unique<views::FlexLayoutView>();

  // Create the web view that hosts the WebUI.
  std::unique_ptr<views::WebView> web_view =
      std::make_unique<views::WebView>(tab_model_->owning_model()->profile());
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

  // Load the untrusted WebUI into the web view.
  GURL url(chrome::kChromeUILensUntrustedURL);
  web_view->LoadInitialURL(url);

  host_view->AddChildView(std::move(web_view));
  return host_view;
}

void LensOverlayController::OnTabStripModelChanged(
    TabStripModel* tab_strip_model,
    const TabStripModelChange& change,
    const TabStripSelectionChange& selection) {
  if (!selection.active_tab_changed()) {
    return;
  }

  if (selection.new_contents == tab_model_->contents()) {
    TabForegrounded();
    return;
  }

  if (selection.old_contents == tab_model_->contents()) {
    TabBackgrounded();
  }
}

const GURL& LensOverlayController::GetPageURL() const {
  // TODO(b/332787629): Return the URL of the WebContents in the main tab.
  return GURL::EmptyGURL();
}

metrics::OmniboxEventProto::PageClassification
LensOverlayController::GetPageClassification() const {
  // TODO(b/332787629): Return the approrpaite classification:
  // CONTEXTUAL_SEARCHBOX
  // SEARCH_SIDE_PANEL_SEARCHBOX
  // LENS_SIDE_PANEL_SEARCHBOX
  return metrics::OmniboxEventProto::CONTEXTUAL_SEARCHBOX;
}

const std::string& LensOverlayController::GetThumbnail() const {
  // Return the thumbnail data (data:image/) or address (chrome://image/).
  static base::NoDestructor<std::string> thumbnail;
  return *thumbnail;
}

const lens::LensOverlayInteractionResponse&
LensOverlayController::GetLensResponse() const {
  static base::NoDestructor<lens::LensOverlayInteractionResponse> response;
  return *response;
}

void LensOverlayController::OnThumbnailRemoved() const {
  // User removed the thumbnail. Update the state.
}

void LensOverlayController::OnSuggestionAccepted(const GURL& destination_url) {
  // TODO(b/332787629): Append the 'mactx' param.
  GURL url = lens::AppendCommonSearchParametersToURL(destination_url);
  if (side_panel_page_) {
    side_panel_page_->LoadResultsInFrame(url);
  } else {
    pending_side_panel_url_ = std::make_optional<GURL>(url);
  }
}

void LensOverlayController::TabForegrounded() {}

void LensOverlayController::TabBackgrounded() {
  CloseUI();
}

void LensOverlayController::CloseRequestedByOverlay() {
  CloseUIAsync();
}

void LensOverlayController::CloseUIAsync() {
  state_ = State::kClosing;

  // This callback comes from WebUI. CloseUI synchronously destroys the WebUI.
  // Dispatch to avoid re-entrancy.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&LensOverlayController::CloseUI,
                                weak_factory_.GetWeakPtr()));
}

void LensOverlayController::IssueLensRequest(
    lens::mojom::CenterRotatedBoxPtr region) {
  DCHECK(region);
  selected_region_ = region.Clone();
  lens_overlay_query_controller_->SendRegionSearch(region.Clone());
  results_side_panel_coordinator_->RegisterEntryAndShow();
  state_ = State::kOverlayAndResults;
}

void LensOverlayController::IssueObjectSelectionRequest(
    const std::string& object_id) {
  selected_region_.reset();
  lens_overlay_query_controller_->SendObjectSelection(object_id);
  results_side_panel_coordinator_->RegisterEntryAndShow();
  state_ = State::kOverlayAndResults;
}

void LensOverlayController::IssueTextSelectionRequest(
    const std::string& query) {
  selected_region_.reset();

  // TODO(b/330204523): Send query to the searchbox.
  lens_overlay_query_controller_->SendTextOnlyQuery(query);
  results_side_panel_coordinator_->RegisterEntryAndShow();
  state_ = State::kOverlayAndResults;
}

void LensOverlayController::HandleInteractionURLResponse(
    lens::proto::LensOverlayUrlResponse response) {
  if (side_panel_page_) {
    side_panel_page_->LoadResultsInFrame(GURL(response.url()));
  } else {
    pending_side_panel_url_ = std::make_optional<GURL>(response.url());
  }
}

void LensOverlayController::HandleInteractionDataResponse(
    lens::proto::LensOverlayInteractionResponse response) {}

void LensOverlayController::WillRemoveContents(tabs::TabModel* tab,
                                               content::WebContents* contents) {
  contents->RemoveUserData(LensOverlayControllerTabLookup::UserDataKey());
  CloseUI();
}

void LensOverlayController::DidAddContents(tabs::TabModel* tab,
                                           content::WebContents* contents) {
  LensOverlayControllerTabLookup::CreateForWebContents(contents, this);
}
