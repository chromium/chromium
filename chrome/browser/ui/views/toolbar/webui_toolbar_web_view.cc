// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/numerics/safe_conversions.h"
#include "base/state_transitions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/trace_event/named_trigger.h"
#include "chrome/browser/external_protocol/external_protocol_handler.h"
#include "chrome/browser/lifetime/browser_shutdown.h"
#include "chrome/browser/page_load_metrics/page_load_metrics_initialize.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/desktop_browser_window_capabilities.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/split_tab_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/toolbar/webui_split_tabs_control.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher_impl.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/context_menu_params.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

constexpr char kHistogramToolbarRenderProcessGone[] =
    "InitialWebUI.Toolbar.RenderProcessGone";
constexpr char kHistogramToolbarRenderProcessGoneExceedingRecoveryLimit[] =
    "InitialWebUI.Toolbar.RenderProcessGoneExceedingRecoveryLimit";

class WebUIToolbarInternalWebView : public views::WebView {
  METADATA_HEADER(WebUIToolbarInternalWebView, views::WebView)

 public:
  explicit WebUIToolbarInternalWebView(content::BrowserContext* browser_context)
      : views::WebView(browser_context) {}
  ~WebUIToolbarInternalWebView() override = default;

  // views::WebView:
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
};

BEGIN_METADATA(WebUIToolbarInternalWebView)
END_METADATA

}  // namespace

WebUIToolbarWebView::WebUIToolbarWebView(
    BrowserWindowInterface* browser,
    chrome::BrowserCommandController* controller,
    std::unique_ptr<WebUILocationBar> location_bar)
    : browser_(browser),
      controller_(controller),
      reload_control_(this),
      split_tabs_control_(this),
      location_bar_(std::move(location_bar)),
      clock_(base::DefaultTickClock::GetInstance()),
      touch_ui_subscription_(ui::TouchUiController::Get()->RegisterCallback(
          base::BindRepeating(&WebUIToolbarWebView::OnTouchUiChanged,
                              base::Unretained(this)))) {
  base::trace_event::EmitNamedTrigger("webui-toolbar-constructor");
  last_queued_state_.split_tabs_control_state =
      toolbar_ui_api::mojom::SplitTabsControlState::New();
  last_queued_state_.reload_control_state =
      toolbar_ui_api::mojom::ReloadControlState::New();
  last_queued_state_.layout_constants_version = 0;
  if (auto* manager = InitialWebUIWindowMetricsManager::From(browser_)) {
    manager->OnReloadButtonCreated();
  }

  SetLayoutManager(std::make_unique<views::FillLayout>());

  auto web_view =
      std::make_unique<WebUIToolbarInternalWebView>(browser->GetProfile());
  auto* web_contents =
      web_view->GetWebContents(GURL(chrome::kChromeUIWebUIToolbarURL));
  // PLM has to be initialized before loading the URL.
  InitializePageLoadMetricsForWebContents(web_contents);

  web_contents->SetPageBaseBackgroundColor(SK_ColorTRANSPARENT);
  web_contents->SetIgnoreZoomGestures(true);

  // We must save the pointer to the WebView so we can load the URL after the
  // view is added to a widget.
  web_view_ = AddChildView(std::move(web_view));
  Observe(web_contents);

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

  // Ensure the browser window interface is associated with the WebContents
  // before the WebUI acts on it.
  webui::SetBrowserWindowInterface(web_view_->GetWebContents(), browser_);

  web_view_->LoadInitialURL(GURL(chrome::kChromeUIWebUIToolbarURL));

  // Initialize the split tabs control early to determine its initial visibility
  // state (based on prefs/tab state) before the first layout. This prevents
  // layout shifts that would occur if we waited for OnPageInitialized.
  // This is safe because the split tabs control's Init() doesn't need to push
  // state to the WebUI.
  if (features::IsWebUISplitTabsButtonEnabled()) {
    split_tabs_control_.Init();
  }

  // Do NOT call GetWebUIToolbarUI() here as it may be null.
  // The reload_control_ will be initialized once the WebUI is ready.
}

gfx::Size WebUIToolbarWebView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  int button_count = 0;
  button_count += features::IsWebUIReloadButtonEnabled();
  button_count += features::IsWebUISplitTabsButtonEnabled() &&
                  split_tabs_control_.IsVisible();

  const int size = GetLayoutConstant(LayoutConstant::kToolbarButtonHeight);
  int width = button_count * size;
  if (button_count > 0) {
    width += (button_count - 1) *
             GetLayoutConstant(LayoutConstant::kToolbarIconDefaultMargin);
  }

  if (location_bar_) {
    // TODO(http://crbug.com/470042732): Where is the 4px margin from?
    width += 4 + location_bar_->PreferredSize().width();
  }
  return gfx::Size(width, size);
}

void WebUIToolbarWebView::HandleContextMenu(
    toolbar_ui_api::mojom::ContextMenuType menu_type,
    gfx::Point viewport_coordinate_css_pixels,
    ui::mojom::MenuSourceType source) {
  CHECK(web_view_);
  // The coordinates are in CSS pixels relative the viewport origin. We need
  // to multiply by the page scaling factor to convert them to DIPs before we
  // can use them as the offset from the viewport origin to show the menu.
  double page_zoom_scale = blink::ZoomLevelToZoomFactor(
      zoom::ZoomController::GetZoomLevelForWebContents(
          web_view_->web_contents()));
  gfx::Point screen_location = GetBoundsInScreen().origin();
  screen_location +=
      ScaleToRoundedPoint(viewport_coordinate_css_pixels, page_zoom_scale)
          .OffsetFromOrigin();

  switch (menu_type) {
    case toolbar_ui_api::mojom::ContextMenuType::kReload:
      reload_control_.HandleContextMenu(GetWidget(), screen_location, source);
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kSplitTabsAction:
    case toolbar_ui_api::mojom::ContextMenuType::kSplitTabsContext:
      split_tabs_control_.HandleContextMenu(menu_type, screen_location, source);
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kUnspecified:
      NOTREACHED() << "Unexpected ClickDispositionFlag::kUnspecified.";
  }
}

void WebUIToolbarWebView::OnPageInitialized() {
  SetInitializationState(InitializationState::kInitialized);

  if (features::IsWebUIReloadButtonEnabled() &&
      !reload_control_.is_initialized()) {
    reload_control_.Init();
  }

  InitialWebUIManager::From(browser_)->OnWebUIToolbarLoaded();
}

ReloadControl* WebUIToolbarWebView::GetReloadControl() {
  return &reload_control_;
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
  auto* ui = GetWebUIToolbarUI();
  CHECK(ui) << "Could not find the web ui for the toolbar";
  ui->Init(this);
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
  CHECK(transitions->IsTransitionValid(initialization_state_, new_state));
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

void WebUIToolbarWebView::OnTouchUiChanged() {
  ++last_queued_state_.layout_constants_version;
  PostPushNavigationState();
}

void WebUIToolbarWebView::PostPushNavigationState() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(&WebUIToolbarWebView::PushNavigationState,
                                weak_ptr_factory_.GetWeakPtr(),
                                ++current_state_generation_));
}

void WebUIToolbarWebView::PushNavigationState(uint64_t state_generation) {
  if (state_generation == current_state_generation_) {
    if (WebUIToolbarUI* web_ui = GetWebUIToolbarUI()) {
      web_ui->OnNavigationControlsStateChanged(last_queued_state_.Clone());
    }
  }
}

BEGIN_METADATA(WebUIToolbarWebView)
END_METADATA
