// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"

#include <memory>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/numerics/safe_conversions.h"
#include "base/state_transitions.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/named_trigger.h"
#include "base/types/expected.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/split_tab_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/webui_content_setting_image_control.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/toolbar/webui_split_tabs_control.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher_impl.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/drop_data.h"
#include "content/public/common/result_codes.h"
#include "mojo/public/mojom/base/error.mojom.h"
#include "net/base/filename_util.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

using Code = mojo_base::mojom::Code;
using Error = mojo_base::mojom::Error;

constexpr char kHistogramToolbarRenderProcessGone[] =
    "InitialWebUI.Toolbar.RenderProcessGone";
constexpr char kHistogramToolbarRenderProcessGoneExceedingRecoveryLimit[] =
    "InitialWebUI.Toolbar.RenderProcessGoneExceedingRecoveryLimit";

}  // namespace

class WebUIToolbarInternalWebView : public views::WebView {
  METADATA_HEADER(WebUIToolbarInternalWebView, views::WebView)

 public:
  explicit WebUIToolbarInternalWebView(content::BrowserContext* browser_context)
      : views::WebView(browser_context) {}
  ~WebUIToolbarInternalWebView() override = default;

  // views::WebView:
  void PreHandleDragUpdate(const content::DropData& drop_data,
                           const gfx::PointF& client_pt) override {
    if (!drop_data.filenames.empty()) {
      cached_dragged_file_path_ = drop_data.filenames.front().path;
      cached_dragged_file_position_ = client_pt;
    }
  }

  void RendererUnresponsive(
      content::WebContents* source,
      content::RenderWidgetHost* render_widget_host,
      base::RepeatingClosure hang_monitor_restarter) override {
    // TODO(crbug.com/475397687): Consider using a more aggressive timeout to
    // trigger this.
    if (features::kWebUIReloadButtonRestartUnresponsive.Get()) {
      // Force shutting down the renderer process when the WebUI toolbar
      // is unresponsive. It will be restarted by the WebUIToolbarWebView.
      if (auto* process = render_widget_host->GetProcess()) {
        process->Shutdown(content::RESULT_CODE_KILLED);
      }
      return;
    }

    views::WebView::RendererUnresponsive(source, render_widget_host,
                                         std::move(hang_monitor_restarter));
  }

  void OnFocus() override {
    // The default OnFocus() implementation calls WebContents::Focus(), which
    // restores focus to the last focused element. If the focus was
    // never established, WebContents::Focus() will focus the <body>, which
    // doesn't show a focus ring.
    views::WebView::OnFocus();

    // For a programmatic focus (kDirectFocusChange), focuses the first
    // focusable element in the HTML document by calling
    // WebContents::FocusThroughTabTraversal().
    if (GetFocusManager()->focus_change_reason() ==
            views::FocusManager::FocusChangeReason::kDirectFocusChange &&
        IsWebContentsAlive()) {
      web_contents()->FocusThroughTabTraversal(/*reverse=*/false);
    }
  }

  bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    // Used to handle the focus event triggered by keyboard (e.g. cmd+L to focus
    // on the omnibox from macOS). See crbug.com/491963415.
    return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, GetFocusManager());
  }

  std::optional<GURL> ConsumeDroppedUrl(const gfx::PointF& point) {
    std::optional<GURL> url = std::nullopt;
    if (cached_dragged_file_position_.has_value() &&
        point == *cached_dragged_file_position_ &&
        cached_dragged_file_path_.has_value()) {
      url = net::FilePathToFileURL(*cached_dragged_file_path_);
    }
    cached_dragged_file_path_.reset();
    cached_dragged_file_position_.reset();
    return url;
  }

 private:
  // A handler to handle unhandled keyboard messages coming back from the
  // renderer process.
  views::UnhandledKeyboardEventHandler unhandled_keyboard_event_handler_;
  std::optional<base::FilePath> cached_dragged_file_path_;
  std::optional<gfx::PointF> cached_dragged_file_position_;
};

BEGIN_METADATA(WebUIToolbarInternalWebView)
END_METADATA

WebUIToolbarWebView::WebUIToolbarWebView(
    BrowserWindowInterface* browser,
    chrome::BrowserCommandController* controller,
    std::unique_ptr<WebUILocationBar> location_bar)
    : browser_(browser),
      controller_(controller),
      reload_control_(this),
      split_tabs_control_(this),
      home_control_(this),
      avatar_control_(this, browser->GetBrowserForMigrationOnly()),
      location_bar_(std::move(location_bar)),
      back_control_(this, BackForwardButton::Direction::kBack),
      forward_control_(this, BackForwardButton::Direction::kForward),
      pinned_toolbar_actions_(this),
      clock_(base::DefaultTickClock::GetInstance()),
      touch_ui_subscription_(ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&WebUIToolbarWebView::OnTouchUiChanged,
                              base::Unretained(this)))) {
  base::trace_event::EmitNamedTrigger("webui-toolbar-constructor");
  last_queued_state_.split_tabs_control_state =
      toolbar_ui_api::mojom::SplitTabsControlState::New();
  last_queued_state_.reload_control_state =
      toolbar_ui_api::mojom::ReloadControlState::New();
  last_queued_state_.home_control_state =
      toolbar_ui_api::mojom::HomeControlState::New();
  last_queued_state_.location_bar_state =
      toolbar_ui_api::mojom::LocationBarState::New();
  last_queued_state_.location_bar_state->omnibox_view_state =
      toolbar_ui_api::mojom::OmniboxViewState::New();
  last_queued_state_.location_bar_state->location_bar_flags =
      toolbar_ui_api::mojom::LocationBarFlags::New();
  last_queued_state_.location_bar_state->lhs_chips_state =
      toolbar_ui_api::mojom::LhsChipsState::New(
          toolbar_ui_api::mojom::SecurityChipState::New(
              toolbar_ui_api::mojom::SecurityChipIcon::kHttp,
              toolbar_ui_api::mojom::SecurityLevel::kNone, std::u16string(),
              /*is_clickable=*/false, /*is_text_dangerous=*/false,
              /*is_visible=*/true),
          std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr>(),
          /*permission_dashboard=*/nullptr);
  last_queued_state_.layout_constants_version = 0;
  last_queued_state_.back_forward_control_state = GetBackForwardState();

  if (auto* manager = InitialWebUIWindowMetricsManager::From(browser_)) {
    manager->OnReloadButtonCreated();
  }

  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto web_view =
      std::make_unique<WebUIToolbarInternalWebView>(browser->GetProfile());
  std::unique_ptr<content::WebContents> pre_created_contents;

  if (auto* manager = InitialWebUIManager::From(browser)) {
    pre_created_contents = manager->TakeToolbarContents();
  }
  if (pre_created_contents) {
    is_preloaded_ = true;
    SetInitializationState(InitializationState::kPending);
    // When preload is not enabled, the `WebUIToolbarUI` init is done in
    // `WebUIToolbarWebView::DidFinishNavigation()`. Here since the
    // `WebContents` is pre-created, it might finish navigation before we
    // install the observer, so we have to manually init the `WebUIToolbarUI`.
    if (!pre_created_contents->IsLoading() &&
        pre_created_contents->GetController().GetLastCommittedEntry()) {
      if (auto* ui = GetWebUIToolbarUI()) {
        ui->Init(this);
      }
    }
    Observe(pre_created_contents.get());
    web_view->SetOwnedWebContents(std::move(pre_created_contents));
  } else {
    content::WebContents* web_contents =
        web_view->GetWebContents(GURL(chrome::kChromeUIWebUIToolbarURL));
    Observe(web_contents);
    InitialWebUIManager::ConfigureToolbarWebContents(web_contents, browser);
  }

  // We must save the pointer to the WebView so we can load the URL after the
  // view is added to a widget.
  web_view_ = AddChildView(std::move(web_view));

  // The accessibility and tooltip attributes are handled by the WebUI.
  SetProperty(views::kElementIdentifierKey, kWebUIToolbarElementIdentifier);
}

WebUIToolbarWebView::~WebUIToolbarWebView() = default;

void WebUIToolbarWebView::AddedToWidget() {
  CHECK(web_view_);

  // If initialization has already started or completed, do not run it again.
  if (initialization_state_ != InitializationState::kUninitialized) {
    return;
  }

  SetInitializationState(InitializationState::kPending);

  if (!is_preloaded_) {
    web_view_->LoadInitialURL(GURL(chrome::kChromeUIWebUIToolbarURL));
  }

  // Initialize the split tabs control early to determine its initial visibility
  // state (based on prefs/tab state) before the first layout. This prevents
  // layout shifts that would occur if we waited for OnPageInitialized.
  // This is safe because the split tabs control's Init() doesn't need to push
  // state to the WebUI.
  if (features::IsWebUISplitTabsButtonEnabled()) {
    split_tabs_control_.Init();
  }

  if (features::IsWebUIHomeButtonEnabled()) {
    home_control_.Init();
  }

  if (features::IsWebUIPinnedToolbarActionsEnabled()) {
    pinned_toolbar_actions_.Init();
  }

  // Do NOT call GetWebUIToolbarUI() here as it may be null.
  // The reload_control_ will be initialized once the WebUI is ready.
}

void WebUIToolbarWebView::OnThemeChanged() {
  views::View::OnThemeChanged();
  avatar_control_.UpdateIcon();
}

gfx::Size WebUIToolbarWebView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  int button_count = 0;
  button_count += features::IsWebUIReloadButtonEnabled();
  button_count += features::IsWebUISplitTabsButtonEnabled() &&
                  split_tabs_control_.IsVisible();
  button_count += features::IsWebUIBackForwardButtonEnabled();
  button_count += features::IsWebUIBackForwardButtonEnabled() &&
                  forward_control_.IsPinned();
  button_count +=
      features::IsWebUIHomeButtonEnabled() && home_control_.IsPinned();
  button_count += features::IsWebUIAvatarButtonEnabled();

  const int size = GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
  const int gap = GetLayoutConstant(LayoutConstant::kToolbarIconDefaultMargin);
  int width = button_count * size;
  if (button_count > 0) {
    width += (button_count - 1) * gap;
  }

  if (location_bar_) {
    // TODO(http://crbug.com/470042732): Where is the 4px margin from?
    width += 4 + location_bar_->PreferredSize().width();
  }

  if (features::IsWebUIBackForwardButtonEnabled()) {
    width += back_button_leading_margin_;
  }

  if (features::IsWebUIPinnedToolbarActionsEnabled()) {
    if (int pinned_width = pinned_toolbar_actions_.GetWidth()) {
      width += !!width * gap;  // Add gap if prior controls.
      width += pinned_width;
    }
  }

  return gfx::Size(width, size);
}

void WebUIToolbarWebView::HandleContextMenu(
    toolbar_ui_api::mojom::ContextMenuType menu_type,
    const gfx::RectF& bounds_in_css_pixels,
    ui::mojom::MenuSourceType source) {
  CHECK(web_view_);
  // The coordinates are in CSS pixels relative the viewport origin. We need
  // to multiply by the page scaling factor to convert them to DIPs before we
  // can use them as the bounding rectangle relative to the viewport origin to
  // show the menu.
  double page_zoom_scale = blink::ZoomLevelToZoomFactor(
      zoom::ZoomController::GetZoomLevelForWebContents(
          web_view_->web_contents()));
  gfx::Rect screen_rect = gfx::ToEnclosingRect(
      gfx::ScaleRect(bounds_in_css_pixels, page_zoom_scale));

  // Add the offset of the WebView's top-left corner in screen coordinates to
  // convert the relative rect to an absolute screen rect.
  screen_rect.Offset(GetBoundsInScreen().origin().OffsetFromOrigin());

  switch (menu_type) {
    case toolbar_ui_api::mojom::ContextMenuType::kBack:
      back_control_.HandleContextMenu(GetWidget(), screen_rect, source);
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kForward:
      forward_control_.HandleContextMenu(GetWidget(), screen_rect, source);
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kReload:
      reload_control_.HandleContextMenu(GetWidget(), screen_rect, source);
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kSplitTabsAction:
    case toolbar_ui_api::mojom::ContextMenuType::kSplitTabsContext:
      split_tabs_control_.HandleContextMenu(menu_type, screen_rect, source);
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kHome:
      home_control_.HandleContextMenu(screen_rect, source);
      break;
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionNewIncognitoWindow:
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionShowPasswordsBubbleOrPage:
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionShowPaymentsBubbleOrPage:
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionShowAddressesBubbleOrPage:
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowBookmarks:
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowReadingList:
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowHistoryCluster:
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionShowDownloads:
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionClearBrowsingData:
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionPrint:
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowLensOverlayResults:
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionShowTranslate:
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionQrCodeGenerator:
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionRouteMedia:
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowReadAnything:
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionCopyUrl:
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionSendTabToSelf:
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionTaskManager:
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionDevTools:
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionTabSearch:
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowContextualTasks:
    case toolbar_ui_api::mojom::ContextMenuType::kPinnedActionSidePanelShowLens:
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowAboutThisSite:
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowCustomizeChrome:
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowShoppingInsights:
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowMerchantTrust:
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSendSharedTabGroupFeedback:
    case toolbar_ui_api::mojom::ContextMenuType::
        kPinnedActionSidePanelShowComments:
      pinned_toolbar_actions_.HandleContextMenu(menu_type, screen_rect, source);
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kUnspecified:
      NOTREACHED() << "Unexpected ClickDispositionFlag::kUnspecified.";
  }
}

void WebUIToolbarWebView::ShowContentSettingsBubble(
    ::toolbar_ui_api::mojom::ContentSettingImageType type,
    toolbar_ui_api::ToolbarUIService::ShowContentSettingsBubbleCallback
        callback) {
  if (location_bar_) {
    location_bar_->content_setting_image_control().ShowContentSettingsBubble(
        type, std::move(callback));
  } else {
    std::move(callback).Run(base::unexpected(Error::New(
        Code::kFailedPrecondition,
        base::StringPrintf("WebUIToolbarWebView: cannot create bubble without "
                           "location_bar_ for type: %d",
                           static_cast<int32_t>(type)))));
  }
}

void WebUIToolbarWebView::OnPageInitialized() {
  SetInitializationState(InitializationState::kInitialized);

  if (features::IsWebUIReloadButtonEnabled() &&
      !reload_control_.is_initialized()) {
    reload_control_.Init();
  }
  if (features::IsWebUIAvatarButtonEnabled() &&
      !avatar_control_.is_initialized()) {
    avatar_control_.Initialize();
  }

  InitialWebUIManager::From(browser_)->OnWebUIToolbarLoaded();
}

void WebUIToolbarWebView::InvokePinnedToolbarAction(
    toolbar_ui_api::mojom::PinnedToolbarAction action_id) {
  pinned_toolbar_actions_.Invoke(action_id);
}

void WebUIToolbarWebView::OnOmniboxAction(
    toolbar_ui_api::mojom::OmniboxActionPtr action) {
  if (location_bar_) {
    location_bar_->OnOmniboxAction(std::move(action));
  }
}

ReloadControl* WebUIToolbarWebView::GetReloadControl() {
  return &reload_control_;
}

AvatarToolbarButtonInterface*
WebUIToolbarWebView::GetAvatarToolbarButtonInterface() {
  return &avatar_control_;
}

BrowserWindowInterface* WebUIToolbarWebView::GetBrowser() {
  return browser_;
}

chrome::BrowserCommandController* WebUIToolbarWebView::GetCommandController() {
  return controller_;
}

views::View* WebUIToolbarWebView::GetView() {
  return this;
}

void WebUIToolbarWebView::AnnounceAlert(const std::u16string& announcement) {
  GetViewAccessibility().AnnounceAlert(announcement);
}

void WebUIToolbarWebView::OnPreferredSizeChanged() {
  PreferredSizeChanged();
}

const std::vector<toolbar_ui_api::mojom::PinnedToolbarActionStatePtr>&
WebUIToolbarWebView::GetPinnedToolbarActionsState() const {
  return last_queued_state_.pinned_toolbar_actions_state;
}

browser_controls_api::BrowserControlsService::BrowserControlsServiceDelegate*
WebUIToolbarWebView::GetBrowserControlsDelegate() {
  return this;
}

toolbar_ui_api::ToolbarUIService::ToolbarUIServiceDelegate*
WebUIToolbarWebView::GetToolbarUIServiceDelegate() {
  return this;
}

std::unique_ptr<toolbar_ui_api::NavigationControlsStateFetcher>
WebUIToolbarWebView::GetNavigationControlsStateFetcher() {
  return std::make_unique<toolbar_ui_api::NavigationControlsStateFetcherImpl>(
      base::BindRepeating(&WebUIToolbarWebView::GetNavigationControlsState,
                          base::Unretained(this)));
}

CommandUpdater* WebUIToolbarWebView::GetCommandUpdater() {
  return browser_->GetFeatures().browser_command_controller();
}

toolbar_ui_api::mojom::NavigationControlsStatePtr
WebUIToolbarWebView::GetNavigationControlsState() {
  return last_queued_state_.Clone();
}

void WebUIToolbarWebView::DidStartNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame()) {
    return;
  }
  // Transition back to pending if we are already initialized (e.g. reload).
  if (initialization_state_ == InitializationState::kInitialized) {
    SetInitializationState(InitializationState::kPending);
  }
}

void WebUIToolbarWebView::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }

  // Explicitly do another fetch to check if browser is in a shutdown state.
  auto* bwi = webui::GetBrowserWindowInterface(web_view_->GetWebContents());
  auto shutting_down = bwi == nullptr;
  if (shutting_down) {
    LOG(WARNING) << "browser is shutting down, aborting Init()";
    return;
  }
  auto* ui = GetWebUIToolbarUI();
  CHECK(ui) << "Could not find the web ui for the toolbar";
  ui->Init(this);
}

void WebUIToolbarWebView::SetBackButtonLeadingMargin(int margin) {
  back_button_leading_margin_ = margin;
  OnBackForwardStateChanged();
  PreferredSizeChanged();
}

void WebUIToolbarWebView::SetBackForwardEnabled(int command_id, bool enabled) {
  if (command_id == IDC_BACK) {
    back_control_.SetEnabled(enabled);
  } else {
    forward_control_.SetEnabled(enabled);
  }
}

void WebUIToolbarWebView::SetForwardVisible(bool visible) {
  forward_control_.SetIsPinned(visible);
  PreferredSizeChanged();
}

void WebUIToolbarWebView::DidFirstVisuallyNonEmptyPaint() {
  has_finished_first_non_empty_paint_ = true;
  if (did_first_non_empty_paint_callback_) {
    std::move(did_first_non_empty_paint_callback_).Run();
  }
}

void WebUIToolbarWebView::PrimaryMainFrameRenderProcessGone(
    base::TerminationStatus status) {
  // TODO(crbug.com/474228715): update the enum definition from
  // //base/process/kill.h to be a enum class, and add IFTTT lint.
  base::UmaHistogramEnumeration(kHistogramToolbarRenderProcessGone, status,
                                base::TERMINATION_STATUS_MAX_ENUM);

  // No recovery if it's a normal termination.
  if (status == base::TERMINATION_STATUS_NORMAL_TERMINATION ||
      status == base::TERMINATION_STATUS_STILL_RUNNING) {
    return;
  }

  // Do not recover if the browser is shutting down.
  if (browser_shutdown::IsTryingToQuit()) {
    return;
  }

  // Do not recover if the browser is closing.
  if (browser_->capabilities() &&
      browser_->capabilities()->IsAttemptingToCloseBrowser()) {
    return;
  }

  // Reset the crash count if when the reset interval is reached.
  if (clock_->NowTicks() - last_crash_time_ >=
      features::kWebUIReloadButtonCrashRecoverResetInterval.Get()) {
    crash_count_ = 0;
  }

  last_crash_time_ = clock_->NowTicks();

  if (++crash_count_ <=
      base::checked_cast<uint32_t>(
          features::kWebUIReloadButtonMaxCrashRecoveryTimes.Get())) {
    // TODO(crbug.com/474228715): keep the previous rendered pixels when the
    // WebUI toolbar crashed so there is no visual glitch during recovery.

    // PostTask to avoid re-entrancy into RenderProcessHost during its death.
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &WebUIToolbarWebView::RecoverFromRendererCrashOrUnresponsiveness,
            weak_ptr_factory_.GetWeakPtr()));
  } else {
    // TODO(crbug.com/474228715): if the crash_count exceeds the threshold, we
    // should consider fall back to the C++ view.
    base::UmaHistogramBoolean(
        kHistogramToolbarRenderProcessGoneExceedingRecoveryLimit, true);
    base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(
            &WebUIToolbarWebView::RecoverFromRendererCrashOrUnresponsiveness,
            weak_ptr_factory_.GetWeakPtr()),
        features::kWebUIReloadButtonCrashRecoverRetryInterval.Get());
  }
}

void WebUIToolbarWebView::RecoverFromRendererCrashOrUnresponsiveness() {
  CHECK(web_view_);
  // Note that in some cases the WebView might have been recovered already (e.g.
  // when the user triggers a reload from the devtools), however we will just
  // continue with the reload anyway.
  web_view_->web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                                    /*check_for_repost=*/false);
}

void WebUIToolbarWebView::SetInitializationState(
    InitializationState new_state) {
  using State = WebUIToolbarWebView::InitializationState;
  static const base::NoDestructor<base::StateTransitions<State>> transitions(
      base::StateTransitions<State>({
          {State::kUninitialized, {State::kPending}},
          {State::kPending, {State::kInitialized}},
          {State::kInitialized, {State::kPending}},
      }));
  CHECK(transitions->IsTransitionValid(initialization_state_, new_state))
      << "from " << static_cast<int>(initialization_state_) << " to "
      << static_cast<int>(new_state);
  initialization_state_ = new_state;
}

void WebUIToolbarWebView::SetDidFirstNonEmptyPaintCallbackForTesting(
    base::OnceClosure callback) {
  if (callback.is_null()) {
    return;
  }
  if (has_finished_first_non_empty_paint_) {
    std::move(callback).Run();
    return;
  }
  did_first_non_empty_paint_callback_ = std::move(callback);
}

void WebUIToolbarWebView::SetTickClockForTesting(const base::TickClock* clock) {
  clock_ = clock;
}

views::WebView* WebUIToolbarWebView::GetWebViewForTesting() {
  return web_view_;
}

WebUIToolbarUI* WebUIToolbarWebView::GetWebUIToolbarUI() {
  content::WebUI* web_ui = web_view_->web_contents()->GetWebUI();
  if (!web_ui) {
    return nullptr;
  }
  auto* controller = web_ui->GetController();
  return controller ? controller->GetAs<WebUIToolbarUI>() : nullptr;
}

void WebUIToolbarWebView::PermitLaunchUrl() {
  ExternalProtocolHandler::PermitLaunchUrl();
}

void WebUIToolbarWebView::OnHomeButtonDropUrl(const GURL& url) {
  home_control_.OnHomeButtonDropUrl(url);
}

void WebUIToolbarWebView::OnHomeButtonDropFile(
    const gfx::PointF& drop_position) {
  if (std::optional<GURL> url = web_view_->ConsumeDroppedUrl(drop_position)) {
    home_control_.OnHomeButtonDropUrl(*url);
  }
}

void WebUIToolbarWebView::OnReloadControlStateChanged(
    toolbar_ui_api::mojom::ReloadControlStatePtr state) {
  if (*state != *last_queued_state_.reload_control_state) {
    last_queued_state_.reload_control_state = std::move(state);
    PostPushNavigationState();
  }
}

void WebUIToolbarWebView::OnSplitTabsControlStateChanged(
    toolbar_ui_api::mojom::SplitTabsControlStatePtr state) {
  if (*state != *last_queued_state_.split_tabs_control_state) {
    last_queued_state_.split_tabs_control_state = std::move(state);
    PostPushNavigationState();
  }
}

void WebUIToolbarWebView::OnBackForwardStateChanged() {
  auto state = GetBackForwardState();
  if (*state != *last_queued_state_.back_forward_control_state) {
    last_queued_state_.back_forward_control_state = std::move(state);
    PostPushNavigationState();
  }
}

void WebUIToolbarWebView::OnHomeControlStateChanged(
    toolbar_ui_api::mojom::HomeControlStatePtr state) {
  if (*state != *last_queued_state_.home_control_state) {
    last_queued_state_.home_control_state = std::move(state);
    PostPushNavigationState();
  }
}

void WebUIToolbarWebView::OnOmniboxViewStateChanged(
    toolbar_ui_api::mojom::OmniboxViewStatePtr state) {
  if (*state != *last_queued_state_.location_bar_state->omnibox_view_state) {
    last_queued_state_.location_bar_state->omnibox_view_state =
        std::move(state);
    PostPushNavigationState();
  }
}

void WebUIToolbarWebView::OnLocationBarFlagsChanged(
    toolbar_ui_api::mojom::LocationBarFlagsPtr state) {
  if (*state != *last_queued_state_.location_bar_state->location_bar_flags) {
    last_queued_state_.location_bar_state->location_bar_flags =
        std::move(state);
    PostPushNavigationState();
  }
}

void WebUIToolbarWebView::OnLhsChipsStateChanged(
    toolbar_ui_api::mojom::LhsChipsStatePtr state) {
  if (!mojo::Equals(state,
                    last_queued_state_.location_bar_state->lhs_chips_state)) {
    last_queued_state_.location_bar_state->lhs_chips_state = std::move(state);
    PostPushNavigationState();
  }
}

void WebUIToolbarWebView::OnLhsChipMousePressed(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (location_bar_) {
    location_bar_->OnLhsChipMousePressed(identifier);
  }
}

void WebUIToolbarWebView::OnLhsChipClicked(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier,
    bool is_mouse_interaction) {
  if (location_bar_) {
    location_bar_->OnLhsChipClicked(identifier, is_mouse_interaction);
  }
}

void WebUIToolbarWebView::OnLhsChipPointerEntered(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (location_bar_) {
    location_bar_->OnLhsChipPointerEntered(identifier);
  }
}

void WebUIToolbarWebView::OnLhsChipPointerExited(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (location_bar_) {
    location_bar_->OnLhsChipPointerExited(identifier);
  }
}

void WebUIToolbarWebView::OnLhsChipExpandAnimationEnded(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (location_bar_) {
    location_bar_->OnLhsChipExpandAnimationEnded(identifier);
  }
}

void WebUIToolbarWebView::OnLhsChipCollapseAnimationEnded(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier) {
  if (location_bar_) {
    location_bar_->OnLhsChipCollapseAnimationEnded(identifier);
  }
}

void WebUIToolbarWebView::OnPinnedToolbarActionsStateChanged(
    std::vector<toolbar_ui_api::mojom::PinnedToolbarActionStatePtr> state) {
  if (!mojo::Equals(state, last_queued_state_.pinned_toolbar_actions_state)) {
    last_queued_state_.pinned_toolbar_actions_state = std::move(state);
    PostPushNavigationState();
  }
}

void WebUIToolbarWebView::OnContentSettingChanged(
    std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr> state) {
  if (!mojo::Equals(state, last_queued_state_.location_bar_state
                               ->content_setting_image_states)) {
    last_queued_state_.location_bar_state->content_setting_image_states =
        std::move(state);
    PostPushNavigationState();
  }
}

void WebUIToolbarWebView::OnTouchUiChanged() {
  ++last_queued_state_.layout_constants_version;
  PostPushNavigationState();
}

void WebUIToolbarWebView::PostPushNavigationState() {
  // The toolbar is implemented by many individual elements that all update
  // their state separately. To avoid significant visual flicker caused by
  // repeated state pushes that only update individual elements, we delay
  // the actual push until later by posting it here in the hopes that other
  // controls will complete their state changes before we do the actual push.
  // This way the eventual push updates all controls synchronously without
  // inter-element flicker.

  // We want to delay the actual monolithic state push until all controls
  // have had a chance to update their state. We do this by cancelling any
  // pending state pushes here.
  state_push_weak_ptr_factory_.InvalidateWeakPtrs();

  // Then waiting to perform the push of the latest state until the last posted
  // invocation of PushNavigationState from here. Note that this post may also
  // get cancelled if later updates trickle in.  If the state gets modified
  // after this post, there is a fair chance that there may still be other
  // pending tasks to further update the state, so we keep cancelling pending
  // posts and issuing a later post of PushNavigationState().
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WebUIToolbarWebView::PushNavigationState,
                                state_push_weak_ptr_factory_.GetWeakPtr()));
}

void WebUIToolbarWebView::PushNavigationState() {
  if (WebUIToolbarUI* web_ui = GetWebUIToolbarUI()) {
    web_ui->OnNavigationControlsStateChanged(last_queued_state_);
  }
}

toolbar_ui_api::mojom::BackForwardControlStatePtr
WebUIToolbarWebView::GetBackForwardState() const {
  auto state = toolbar_ui_api::mojom::BackForwardControlState::New();
  state->back_button_state = back_control_.GetButtonState();
  state->forward_button_state = forward_control_.GetButtonState();
  state->back_button_leading_margin = back_button_leading_margin_;
  return state;
}

BEGIN_METADATA(WebUIToolbarWebView)
END_METADATA
