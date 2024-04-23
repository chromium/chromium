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
#include "chrome/browser/lens/core/mojom/text.mojom.h"
#include "chrome/browser/lens/lens_overlay/lens_overlay_image_helper.h"
#include "chrome/browser/lens/lens_overlay/lens_overlay_query_controller.h"
#include "chrome/browser/lens/lens_overlay/lens_overlay_url_builder.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/side_panel/side_panel_ui.h"
#include "chrome/browser/ui/views/side_panel/lens/lens_overlay_side_panel_coordinator.h"
#include "chrome/browser/ui/webui/util/image_util.h"
#include "chrome/common/webui_url_constants.h"
#include "components/lens/lens_features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/signin/public/identity_manager/identity_manager.h"
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

}  // namespace

LensOverlayController::LensOverlayController(
    tabs::TabInterface* tab,
    variations::VariationsClient* variations_client,
    signin::IdentityManager* identity_manager)
    : tab_(tab),
      variations_client_(variations_client),
      identity_manager_(identity_manager) {
  if (tab_->GetContents()) {
    LensOverlayControllerTabLookup::CreateForWebContents(tab_->GetContents(),
                                                         this);
  }

  tab_subscriptions_.push_back(tab_->RegisterDidEnterForeground(
      base::BindRepeating(&LensOverlayController::TabForegrounded,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterDidEnterBackground(
      base::BindRepeating(&LensOverlayController::TabBackgrounded,
                          weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterDidAddContents(base::BindRepeating(
      &LensOverlayController::DidAddContents, weak_factory_.GetWeakPtr())));
  tab_subscriptions_.push_back(tab_->RegisterWillRemoveContents(
      base::BindRepeating(&LensOverlayController::WillRemoveContents,
                          weak_factory_.GetWeakPtr())));
}

LensOverlayController::~LensOverlayController() {
  CloseUI();
  lens_overlay_query_controller_.reset();
  if (tab_->GetContents()) {
    tab_->GetContents()->RemoveUserData(
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
  if (!tab_->IsInForeground()) {
    return;
  }

  // Begin the process of grabbing a screenshot.
  content::RenderWidgetHostView* view = tab_->GetContents()
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
    Browser* tab_browser = chrome::FindBrowserWithTab(tab_->GetContents());
    CHECK(tab_browser);
    results_side_panel_coordinator_ =
        std::make_unique<lens::LensOverlaySidePanelCoordinator>(
            tab_browser, this,
            SidePanelUI::GetSidePanelUIForBrowser(tab_browser),
            tab_->GetContents());
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
      variations_client_, identity_manager_);

  state_ = State::kScreenshot;
  scoped_tab_modal_ui_ = tab_->ShowModalUI();

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
  content::WebContents* contents = tab_->GetContents();
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
  initialization_data_.reset();
  lens_overlay_query_controller_.reset();
  scoped_tab_modal_ui_.reset();
  pending_side_panel_url_.reset();
  pending_text_query_.reset();
  pending_thumbnail_uri_.reset();

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
        initialization_data_->current_screenshot_);
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
    signin::IdentityManager* identity_manager) {
  return std::make_unique<lens::LensOverlayQueryController>(
      std::move(full_image_callback), std::move(url_callback),
      std::move(interaction_data_callback),
      std::move(thumbnail_created_callback), variations_client,
      identity_manager);
}

LensOverlayController::OverlayInitializationData::OverlayInitializationData(
    const SkBitmap& screenshot,
    const std::string& data_uri,
    std::vector<lens::mojom::OverlayObjectPtr> objects,
    lens::mojom::TextPtr text,
    const lens::proto::LensOverlayInteractionResponse& interaction_response,
    lens::mojom::CenterRotatedBoxPtr selected_region)
    : current_screenshot_(screenshot),
      current_screenshot_data_uri_(data_uri),
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

  // The documentation for CopyFromSurface claims that the copy can fail, but
  // without providing information about how this can happen.
  // Supposedly IsSurfaceAvailableForCopy() should guard against this case, but
  // this is a multi-process, multi-threaded environment so there may be a
  // TOCTTOU race condition.
  if (bitmap.drawsNothing()) {
    CloseUI();
    return;
  }

  // Encode the screenshot so we can transform it into a data URI for the WebUI.
  scoped_refptr<base::RefCountedBytes> data;
  if (!lens::EncodeImage(
          bitmap, lens::features::GetLensOverlayScreenshotRenderQuality(),
          &data)) {
    // TODO(b/334185985): Handle case when screenshot data URI encoding fails.
    CloseUI();
    return;
  }

  initialization_data_ = std::make_unique<OverlayInitializationData>(
      bitmap, webui::MakeDataURIForImage(data->as_vector(), "jpeg"));

  ShowOverlayWidget();

  state_ = State::kStartingWebUI;
}

void LensOverlayController::ShowOverlayWidget() {
  if (overlay_widget_) {
    CHECK(!overlay_widget_->IsVisible());
    overlay_widget_->Show();
    return;
  }

  overlay_widget_ = std::make_unique<views::Widget>();
  overlay_widget_->Init(CreateWidgetInitParams());
  overlay_widget_->SetContentsView(CreateViewForOverlay());

  content::WebContents* active_web_contents = tab_->GetContents();
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

void LensOverlayController::BackgroundUI() {
  overlay_widget_->Hide();
  state_ = State::kBackground;
  // TODO(b/335516480): Schedule the UI to be suspended.
}

void LensOverlayController::InitializeOverlayUI(
    const OverlayInitializationData& init_data) {
  CHECK(page_);
  page_->ScreenshotDataUriReceived(init_data.current_screenshot_data_uri_);
  if (!init_data.objects_.empty()) {
    SendObjects(CopyObjects(init_data.objects_));
  }
  if (init_data.text_) {
    SendText(init_data.text_->Clone());
  }
}

views::Widget::InitParams LensOverlayController::CreateWidgetInitParams() {
  content::WebContents* active_web_contents = tab_->GetContents();
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

  // Load the untrusted WebUI into the web view.
  GURL url(chrome::kChromeUILensUntrustedURL);
  web_view->LoadInitialURL(url);

  host_view->AddChildView(std::move(web_view));
  return host_view;
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

const lens::proto::LensOverlayInteractionResponse&
LensOverlayController::GetLensResponse() const {
  return initialization_data_
             ? initialization_data_->interaction_response_
             : lens::proto::LensOverlayInteractionResponse().default_instance();
}

void LensOverlayController::OnThumbnailRemoved() const {
  // User removed the thumbnail. Update the state.
}

void LensOverlayController::OnSuggestionAccepted(const GURL& destination_url) {
  std::string query_text = "";
  std::map<std::string, std::string> additional_query_parameters;

  net::QueryIterator query_iterator(destination_url);
  while (!query_iterator.IsAtEnd()) {
    std::string_view key = query_iterator.GetKey();
    std::string_view value = query_iterator.GetValue();
    if (kTextQueryParameterKey == key) {
      query_text = value;
    } else {
      additional_query_parameters.insert(std::make_pair(
          query_iterator.GetKey(), query_iterator.GetUnescapedValue()));
    }
    query_iterator.Advance();
  }

  IssueSearchBoxRequest(query_text, additional_query_parameters);
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

void LensOverlayController::TabForegrounded(tabs::TabInterface* tab) {
  // If the overlay was backgrounded, reshow the overlay widget.
  if (state_ == State::kBackground) {
    ShowOverlayWidget();
    state_ = State::kOverlay;
  }
}

void LensOverlayController::TabBackgrounded(tabs::TabInterface* tab) {
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
  CloseUI();
}

void LensOverlayController::WillRemoveContents(tabs::TabInterface* tab,
                                               content::WebContents* contents) {
  contents->RemoveUserData(LensOverlayControllerTabLookup::UserDataKey());
  CloseUI();
}

void LensOverlayController::DidAddContents(tabs::TabInterface* tab,
                                           content::WebContents* contents) {
  LensOverlayControllerTabLookup::CreateForWebContents(contents, this);
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
  CHECK(initialization_data_);
  CHECK(region);
  initialization_data_->selected_region_ = region.Clone();
  // TODO(b/332787629): Append the 'mactx' param.
  if (initialization_data_->last_search_box_text_) {
    lens_overlay_query_controller_->SendMultimodalRequest(
        region.Clone(), *initialization_data_->last_search_box_text_,
        initialization_data_->additional_search_query_params_);
  } else {
    // TODO(b/335718601): Remove query parameters from region search.
    lens_overlay_query_controller_->SendRegionSearch(
        region.Clone(), initialization_data_->additional_search_query_params_);
  }
  results_side_panel_coordinator_->RegisterEntryAndShow();
  state_ = State::kOverlayAndResults;
}

void LensOverlayController::IssueObjectSelectionRequest(
    const std::string& object_id) {
  initialization_data_->last_search_box_text_.reset();
  // TODO(b/332787629): Append the 'mactx' param.
  initialization_data_->additional_search_query_params_.clear();
  initialization_data_->selected_region_.reset();
  // TODO(b/335718601): Remove query parameters from object selection.
  lens_overlay_query_controller_->SendObjectSelection(
      object_id, initialization_data_->additional_search_query_params_);
  results_side_panel_coordinator_->RegisterEntryAndShow();
  state_ = State::kOverlayAndResults;
}

void LensOverlayController::IssueTextSelectionRequest(
    const std::string& query) {
  initialization_data_->last_search_box_text_.reset();
  initialization_data_->additional_search_query_params_.clear();
  initialization_data_->selected_region_.reset();

  if (searchbox_handler_ && searchbox_handler_->IsRemoteBound()) {
    searchbox_handler_->SetInputText(query);
  } else {
    // If the side panel was not bound at the time of request, we store the
    // query as pending to send it to the searchbox on bind.
    pending_text_query_ = query;
  }

  // TODO(b/332787629): Append the 'mactx' param.
  lens_overlay_query_controller_->SendTextOnlyQuery(
      query, initialization_data_->additional_search_query_params_);
  results_side_panel_coordinator_->RegisterEntryAndShow();
  state_ = State::kOverlayAndResults;
}

void LensOverlayController::IssueSearchBoxRequest(
    const std::string& search_box_text,
    std::map<std::string, std::string> additional_query_params) {
  // TODO(b/332787629): Append the 'mactx' param.
  initialization_data_->last_search_box_text_ =
      std::make_optional<std::string>(search_box_text);
  initialization_data_->additional_search_query_params_ =
      additional_query_params;
  if (initialization_data_->selected_region_.is_null()) {
    lens_overlay_query_controller_->SendTextOnlyQuery(
        search_box_text, initialization_data_->additional_search_query_params_);
  } else {
    lens_overlay_query_controller_->SendMultimodalRequest(
        initialization_data_->selected_region_.Clone(), search_box_text,
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
  if (side_panel_page_) {
    side_panel_page_->LoadResultsInFrame(GURL(response.url()));
  } else {
    pending_side_panel_url_ = std::make_optional<GURL>(response.url());
  }
}

void LensOverlayController::HandleInteractionDataResponse(
    lens::proto::LensOverlayInteractionResponse response) {
  initialization_data_->interaction_response_ = response;
}

void LensOverlayController::HandleThumbnailCreated(
    const std::string& thumbnail_bytes) {
  const std::string data_uri = webui::MakeDataURIForImage(
      base::as_bytes(base::make_span(thumbnail_bytes)), "jpeg");

  if (searchbox_handler_ && searchbox_handler_->IsRemoteBound()) {
    searchbox_handler_->SetThumbnail(data_uri);
  } else {
    // If the side panel was not bound at the time of request, we store the
    // thumbnail as pending to send it to the searchbox on bind.
    pending_thumbnail_uri_ = data_uri;
  }
}
