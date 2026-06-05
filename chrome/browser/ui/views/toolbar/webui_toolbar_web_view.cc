// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/toolbar/webui_toolbar_web_view.h"

#include <limits>
#include <memory>

#include "base/feature_list.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/notimplemented.h"
#include "base/notreached.h"
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
#include "chrome/browser/ui/extensions/extensions_container.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/split_tab_util.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/location_bar/webui_content_setting_image_control.h"
#include "chrome/browser/ui/views/location_bar/webui_location_bar.h"
#include "chrome/browser/ui/views/profiles/profile_menu_coordinator.h"
#include "chrome/browser/ui/views/toolbar/app_menu_control.h"
#include "chrome/browser/ui/views/toolbar/webui_split_tabs_control.h"
#include "chrome/browser/ui/waap/initial_web_ui_manager.h"
#include "chrome/browser/ui/waap/initial_webui_window_metrics_manager.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/browser_controls_adapter_impl.h"
#include "chrome/browser/ui/webui/webui_toolbar/adapters/navigation_controls_state_fetcher_impl.h"
#include "chrome/browser/ui/webui/webui_toolbar/webui_toolbar_extensions_container.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/webui_url_constants.h"
#include "components/ukm/content/source_url_recorder.h"
#include "components/user_education/common/user_education_class_properties.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/browser_accessibility_state.h"
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
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/page/page_zoom.h"
#include "ui/accessibility/ax_enums.mojom-shared.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/mojom/menu_source_type.mojom.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/webview/unhandled_keyboard_event_handler.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace {

using Code = mojo_base::mojom::Code;
using Error = mojo_base::mojom::Error;

constexpr char kHistogramToolbarRenderProcessGone[] =
    "InitialWebUI.Toolbar.RenderProcessGone";
constexpr char kHistogramToolbarRenderProcessGoneExceedingRecoveryLimit[] =
    "InitialWebUI.Toolbar.RenderProcessGoneExceedingRecoveryLimit";

// Helper to determine incremental size of displaying one more button. It
// includes `button_spacing` as long as there's already at least one button
// displayed.
int NextButtonWidth(int button_size,
                    int button_spacing,
                    int num_displayed_buttons) {
  if (num_displayed_buttons == 0) {
    return button_size;
  }
  return button_size + button_spacing;
}

WebUIToolbarUI* GetWebUIToolbarUIFromWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  content::WebUI* web_ui = web_contents->GetWebUI();
  if (!web_ui) {
    return nullptr;
  }
  auto* controller = web_ui->GetController();
  return controller ? controller->GetAs<WebUIToolbarUI>() : nullptr;
}

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

  bool HandleKeyboardEvent(
      content::WebContents* source,
      const input::NativeWebKeyboardEvent& event) override {
    // Used to handle the focus event triggered by keyboard (e.g. cmd+L to focus
    // on the omnibox from macOS). See crbug.com/491963415.
    return unhandled_keyboard_event_handler_.HandleKeyboardEvent(
        event, GetFocusManager());
  }

  std::optional<GURL> ConsumeDroppedUrl(const gfx::PointF& point) {
    std::optional<GURL> url;
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
      // `controller` may be nullptr in unit tests.
      browser_controls_adapter_(
          controller ? std::make_unique<
                           browser_controls_api::BrowserControlsAdapterImpl>(
                           browser,
                           controller)
                     : nullptr),
      icon_table_(this),
      reload_control_(this),
      split_tabs_control_(this),
      home_control_(this),
      app_menu_control_(*this),
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
              toolbar_ui_api::IconHandle(),
              toolbar_ui_api::mojom::SecurityLevel::kNone,
              /*text=*/std::u16string(),
              toolbar_ui_api::mojom::SecurityChipAccessibilityState::New(
                  /*label=*/std::u16string(),
                  /*description=*/std::u16string()),
              /*is_clickable=*/false, /*is_text_dangerous=*/false,
              /*is_visible=*/true),
          /*activity_indicators=*/
          std::vector<toolbar_ui_api::mojom::ContentSettingImageStatePtr>(),
          /*permission_dashboard=*/nullptr);
  last_queued_state_.layout_constants_version = 0;
  last_queued_state_.back_forward_control_state = GetBackForwardState();
  last_queued_state_.app_menu_control_state = app_menu_control_.GetState();
  last_queued_state_.avatar_control_state =
      toolbar_ui_api::mojom::AvatarControlState::New();

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
      if (auto* ui =
              GetWebUIToolbarUIFromWebContents(pre_created_contents.get())) {
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

  content::WebContents* web_contents = web_view->GetWebContents();
  if (web_contents) {
    scoped_accessibility_mode_ =
        content::BrowserAccessibilityState::GetInstance()
            ->CreateScopedModeForWebContents(
                web_contents, ui::AXMode::kNativeAdaptedWebContents);
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

  if (initialization_state_ == InitializationState::kUninitialized) {
    // If the WebUI is in uninitialized state, it must be from the non-preloaded
    // path, we move to the pending state and then load the initial URL.
    CHECK(!is_preloaded_);
    SetInitializationState(InitializationState::kPending);
    web_view_->LoadInitialURL(GURL(chrome::kChromeUIWebUIToolbarURL));
    ApplyInitialSurfaceSyncDeadline();
  } else if (is_preloaded_ &&
             initialization_state_ == InitializationState::kPending) {
    // For preloaded path, apply sync deadline when added to the widget.
    ApplyInitialSurfaceSyncDeadline();
  }

  // Initialize sub-controls exactly once when the view is first added to a
  // widget. Note that this may or may not be called after `OnPageInitialized()`
  // so we can't just rely on the initialization_state_ to decide if we want to
  // initialize these controls.
  if (!sub_controls_initialized_) {
    sub_controls_initialized_ = true;

    // Initialize the split tabs control early to determine its initial
    // visibility state (based on prefs/tab state) before the first layout. This
    // prevents layout shifts that would occur if we waited for
    // OnPageInitialized. This is safe because the split tabs control's Init()
    // doesn't need to push state to the WebUI.
    if (features::IsWebUISplitTabsButtonEnabled()) {
      split_tabs_control_.Init();
    }
    if (features::IsWebUIHomeButtonEnabled()) {
      home_control_.Init();
    }
    if (features::IsWebUIPinnedToolbarActionsEnabled()) {
      pinned_toolbar_actions_.Init();
    }
    if (features::IsWebUIExtensionsContainerEnabled()) {
      extensions_container_ = std::make_unique<WebUIToolbarExtensionsContainer>(
          *browser_, GetWidget(), web_contents()->GetWeakPtr());
      // Register `extensions_container_` as the `ExtensionsContainer` for
      // `browser_`.
      scoped_extensions_container_user_data_ =
          std::make_unique<ui::ScopedUnownedUserData<ExtensionsContainer>>(
              browser_->GetUnownedUserDataHost(), *extensions_container_);
      active_tab_subscription_ = browser_->RegisterActiveTabDidChange(
          base::BindRepeating(&WebUIToolbarWebView::OnActiveTabChanged,
                              base::Unretained(this)));
    }

    // Safe-initialize page-dependent controls if the WebUI finished loading
    // early when the widget was still null during `OnPageInitialized()` due to
    // preloading.
    MaybeInitializePageDependentControls();
  }
}

void WebUIToolbarWebView::OnThemeChanged() {
  views::View::OnThemeChanged();
  avatar_control_.UpdateIcon();
  if (location_bar_) {
    location_bar_->OnThemeChanged();
  }
  if (features::IsWebUIPinnedToolbarActionsEnabled()) {
    pinned_toolbar_actions_.OnThemeChanged();
  }
  if (extensions_container_) {
    // Icons may need re-rendering.
    extensions_container_->NotifyOfAllActions();
  }
}

gfx::Size WebUIToolbarWebView::GetMinimumSize() const {
  // Calculate layout with a width of 0, which will return the minimum size.
  return ComputeLayout(views::SizeBound(0));
}

gfx::Size WebUIToolbarWebView::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  // Calculate layout with unbounded width.
  return ComputeLayout(views::SizeBound());
}

void WebUIToolbarWebView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdateButtonOverflowState();
}

void WebUIToolbarWebView::PreferredSizeChanged() {
  // Overflow state of buttons needs to be recomputed.
  UpdateButtonOverflowState();
  View::PreferredSizeChanged();
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
    case toolbar_ui_api::mojom::ContextMenuType::kAppMenu:
      app_menu_control_.HandleContextMenu(screen_rect, source);
      break;
    case toolbar_ui_api::mojom::ContextMenuType::kUnspecified:
      NOTREACHED() << "Unexpected ContextMenuType::kUnspecified.";
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

void WebUIToolbarWebView::MaybeInitializePageDependentControls() {
  if (!GetWidget() ||
      initialization_state_ != InitializationState::kInitialized) {
    return;
  }
  if (features::IsWebUIReloadButtonEnabled() &&
      !reload_control_.is_initialized()) {
    reload_control_.Init();
  }
  if (features::IsWebUIAvatarButtonEnabled() &&
      !avatar_control_.is_initialized()) {
    avatar_control_.Initialize();
  }
}

void WebUIToolbarWebView::OnPageInitialized() {
  SetInitializationState(InitializationState::kInitialized);
  MaybeInitializePageDependentControls();

  if (auto* manager = InitialWebUIManager::From(browser_)) {
    manager->OnWebUIToolbarLoaded();
  }
}

void WebUIToolbarWebView::InvokePinnedToolbarAction(
    toolbar_ui_api::mojom::PinnedToolbarAction action_id) {
  pinned_toolbar_actions_.Invoke(action_id);
}

base::expected<std::monostate, mojo_base::mojom::ErrorPtr>
WebUIToolbarWebView::OnOmniboxAction(
    toolbar_ui_api::mojom::OmniboxActionPtr action) {
  if (location_bar_) {
    return location_bar_->OnOmniboxAction(std::move(action));
  } else {
    return base::unexpected(mojo_base::mojom::Error::New(
        Code::kFailedPrecondition,
        "WebUIToolbarWebView: null location_bar_ for OnOmniboxAction"));
  }
}

void WebUIToolbarWebView::ShowAvatarMenu() {
  avatar_control_.ButtonPressed(/*is_source_accelerator=*/false);
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

webui_toolbar::IconTable& WebUIToolbarWebView::GetIconTable() {
  return icon_table_;
}

void WebUIToolbarWebView::OnPreferredSizeChanged() {
  PreferredSizeChanged();
}

const toolbar_ui_api::mojom::NavigationControlsState&
WebUIToolbarWebView::GetState() const {
  return last_queued_state_;
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

std::unique_ptr<toolbar_ui_api::IconTableFetcher>
WebUIToolbarWebView::GetIconTableFetcher() {
  return icon_table_.MakeIconTableFetcher();
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
  // This is called every time ToolbarView::Layout() is called, almost always
  // with the same value as before. Best to avoid the expensive
  // PreferredSizeChanged() call if nothing actually changed.
  if (margin == back_button_leading_margin_) {
    return;
  }
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
}

void WebUIToolbarWebView::DidFirstVisuallyNonEmptyPaint() {
  has_finished_first_non_empty_paint_ = true;
  if (did_first_non_empty_paint_callback_) {
    std::move(did_first_non_empty_paint_callback_).Run();
  }

  // Reset infinite deadline for toolbar and main content views.
  if (base::FeatureList::IsEnabled(blink::features::kInitialWebUISurfaceSync)) {
    SetSurfaceSyncDeadline(std::nullopt);
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

bool WebUIToolbarWebView::IsOverflowed(
    ui::ElementIdentifier identifier,
    const views::ProposedLayout* proposed_layout) const {
  // This may be called for elements not handled by the WebUIToolbar, in the
  // case an element is not added to the Views toolbar, either, so default to
  // returning false.
  //
  // The other ToolbarController::WebUIToolbarControllerDelegate methods that
  // take identifiers do not need this check, since they're only called for
  // items that are on the overflow menu, so this must have previously returned
  // true, or there's a Views element that should have short-circuited those
  // calls.
  if (identifier != kToolbarForwardButtonElementId &&
      identifier != kToolbarHomeButtonElementId) {
    return false;
  }

  // If there's no proposed layout, check actual element state.
  if (!proposed_layout) {
    // Note that there's no need to check if the relevant control is pinned. If
    // it's not pinned, `is_overflowed` would have been set to false.
    if (identifier == kToolbarForwardButtonElementId) {
      return forward_control_.is_overflowed();
    } else if (identifier == kToolbarHomeButtonElementId) {
      return home_control_.is_overflowed();
    }
    NOTREACHED();
  }

  // Otherwise, compute putative overflow state of specified element, given
  // WebUI toolbar width in the proposed layout.
  ButtonOverflowInfo button_overflow_info;
  ComputeLayout(proposed_layout->GetLayoutFor(this)->bounds.width(),
                &button_overflow_info);
  if (identifier == kToolbarForwardButtonElementId) {
    return button_overflow_info.is_forward_button_overflowed;
  } else if (identifier == kToolbarHomeButtonElementId) {
    return button_overflow_info.is_home_button_overflowed;
  }
  NOTREACHED();
}

bool WebUIToolbarWebView::IsEnabled(ui::ElementIdentifier identifier) const {
  if (identifier == kToolbarForwardButtonElementId) {
    return forward_control_.is_enabled();
  } else if (identifier == kToolbarHomeButtonElementId) {
    // Home button can't be disabled.
    return true;
  }

  // This should only be called for buttons that are on the overflow menu, and
  // are being handled by the WebUI toolbar.
  NOTREACHED();
}

void WebUIToolbarWebView::OverflowButtonClicked(
    ui::ElementIdentifier identifier) {
  // For better or for worse, ignoring modifiers and instead doing everything in
  // the current tab matches the Views overflow menu behavior.
  if (identifier == kToolbarForwardButtonElementId) {
    browser_controls_adapter_->Forward(WindowOpenDisposition::CURRENT_TAB);
    return;
  } else if (identifier == kToolbarHomeButtonElementId) {
    browser_controls_adapter_->NavigateHome(WindowOpenDisposition::CURRENT_TAB);
    return;
  }
  NOTREACHED();
}

const ui::ColorProvider* WebUIToolbarWebView::GetColorProvider() const {
  return views::View::GetColorProvider();
}

float WebUIToolbarWebView::GetScaleFactor() const {
  if (auto* ui = web_view_->web_contents()->GetWebUI()) {
    return ui->GetDeviceScaleFactor();
  }
  return 1.0f;
}

views::FlexSpecification WebUIToolbarWebView::GetFlexSpecification() {
  return views::FlexSpecification(base::BindRepeating(
      &WebUIToolbarWebView::FlexLayoutRule, base::Unretained(this)));
}

void WebUIToolbarWebView::AdjustForToolbarFocus() {
  if (GetFocusManager()->GetFocusedView() == web_view_) {
    web_view_->web_contents()->FocusThroughTabTraversal(/*reverse=*/false);
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

void WebUIToolbarWebView::SetSurfaceSyncDeadline(
    std::optional<uint32_t> deadline_in_frames) {
  if (auto* rwhv = web_view_->web_contents()->GetRenderWidgetHostView()) {
    rwhv->SetForceSpecifiedDeadline(deadline_in_frames);
  }
  if (auto* browser_view = BrowserView::GetBrowserViewForBrowser(browser_)) {
    if (auto* active_contents = browser_view->GetActiveWebContents()) {
      if (auto* main_rwhv = active_contents->GetRenderWidgetHostView()) {
        main_rwhv->SetForceSpecifiedDeadline(deadline_in_frames);
      }
    }
  }
}

void WebUIToolbarWebView::ApplyInitialSurfaceSyncDeadline() {
  if (base::FeatureList::IsEnabled(blink::features::kInitialWebUISurfaceSync)) {
    size_t frames_param =
        blink::features::kInitialWebUISurfaceSyncDeadlineInFrames.Get();
    uint32_t frames = frames_param == std::numeric_limits<size_t>::max()
                          ? std::numeric_limits<uint32_t>::max()
                          : base::checked_cast<uint32_t>(frames_param);
    SetSurfaceSyncDeadline(frames);
  }
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
  return GetWebUIToolbarUIFromWebContents(web_view_->web_contents());
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

void WebUIToolbarWebView::OnToolbarDropFile(const gfx::PointF& drop_position) {
  if (std::optional<GURL> url = web_view_->ConsumeDroppedUrl(drop_position)) {
    browser_->OpenGURL(*url, WindowOpenDisposition::CURRENT_TAB);
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

void WebUIToolbarWebView::OnAppMenuControlStateChanged(
    toolbar_ui_api::mojom::AppMenuControlStatePtr state) {
  if (*state != *last_queued_state_.app_menu_control_state) {
    last_queued_state_.app_menu_control_state = std::move(state);
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

void WebUIToolbarWebView::OnSelectedKeywordChanged(
    toolbar_ui_api::mojom::SelectedKeywordStatePtr state) {
  // mojo::Equals helps deal with nullability here.
  if (!mojo::Equals(state,
                    last_queued_state_.location_bar_state->selected_keyword)) {
    last_queued_state_.location_bar_state->selected_keyword = std::move(state);
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

void WebUIToolbarWebView::OnLhsChipDrag(
    toolbar_ui_api::mojom::LhsChipIdentifier identifier,
    ui::mojom::DragEventSource source) {
  if (location_bar_) {
    location_bar_->OnLhsChipDrag(identifier, source);
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

void WebUIToolbarWebView::OnAvatarControlStateChanged(
    toolbar_ui_api::mojom::AvatarControlStatePtr state) {
  if (!mojo::Equals(state, last_queued_state_.avatar_control_state)) {
    last_queued_state_.avatar_control_state = std::move(state);
    PostPushNavigationState();
  }
}

void WebUIToolbarWebView::OnFocusRequested(
    toolbar_ui_api::mojom::FocusRequestTarget target) {
  // We need to focus the WebView as well, besides the JS focus.
  web_view_->RequestFocus();
  if (WebUIToolbarUI* web_ui = GetWebUIToolbarUI()) {
    web_ui->OnFocusRequested(target);
  }
}

void WebUIToolbarWebView::OnTouchUiChanged() {
  ++last_queued_state_.layout_constants_version;
  PostPushNavigationState();
}

void WebUIToolbarWebView::OnActiveTabChanged(
    BrowserWindowInterface* browser_interface) {
  if (extensions_container_) {
    // State of extensions depends on what's active --- e.g. some may be
    // disabled on some URLs.
    extensions_container_->NotifyOfAllActions();
  }
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

gfx::Size WebUIToolbarWebView::ComputeLayout(
    views::SizeBound available_width,
    ButtonOverflowInfo* button_overflow_info) const {
  // Add everything that cannot overflow.

  int button_count = 0;
  button_count += features::IsWebUIReloadButtonEnabled();
  button_count += features::IsWebUISplitTabsButtonEnabled() &&
                  split_tabs_control_.IsVisible();
  button_count += features::IsWebUIBackForwardButtonEnabled();
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

  // TODO(crbug.com/517948314): This isn't sizing the forward button correctly.
  if (features::IsWebUIBackForwardButtonEnabled()) {
    width += back_button_leading_margin_;
  }

  // Handle overflowable controls here, with highest priority controls handled
  // first. Unlike the views code, this code does not currently allow the split
  // tab button to overflow, due to issues with relative priorities.
  //
  // TODO(crbug.com/517885636): Allow the split tab button to be hidden..

  if (features::IsWebUIBackForwardButtonEnabled() &&
      forward_control_.IsPinned()) {
    int next_button_width = NextButtonWidth(size, gap, button_count);
    bool is_forward_button_overflowed =
        available_width.is_bounded() &&
        next_button_width + width > available_width.value();
    if (!is_forward_button_overflowed) {
      ++button_count;
      width += next_button_width;
    }
    if (button_overflow_info) {
      button_overflow_info->is_forward_button_overflowed =
          is_forward_button_overflowed;
    }
  }

  if (features::IsWebUIHomeButtonEnabled() && home_control_.IsPinned()) {
    int next_button_width = NextButtonWidth(size, gap, button_count);
    bool is_home_button_overflowed =
        available_width.is_bounded() &&
        next_button_width + width > available_width.value();
    if (!is_home_button_overflowed) {
      ++button_count;
      width += next_button_width;
    }
    if (button_overflow_info) {
      button_overflow_info->is_home_button_overflowed =
          is_home_button_overflowed;
    }
  }

  if (features::IsWebUIPinnedToolbarActionsEnabled()) {
    if (int pinned_width = pinned_toolbar_actions_.GetWidth()) {
      width += !!width * gap;  // Add gap if prior controls.
      width += pinned_width;
    }
  }

  // If there are bounds, there's more space available than `width` and we're
  // displaying the location bar, we want all extra space available. Note that
  // this means anything that's lower priority than the WebUIToolbarWebView that
  // can be hidden may end up hidden, so will likely need to be reworked.
  if (available_width.is_bounded() && width <= available_width.value() &&
      location_bar_) {
    return gfx::Size(available_width.value(), size);
  }

  // Otherwise, return the width of all buttons that we want to display.
  return gfx::Size(width, size);
}

void WebUIToolbarWebView::UpdateButtonOverflowState() {
  // Compute layout to determine which buttons to hide. Ignore the returned
  // Size.
  ButtonOverflowInfo button_overflow_info;
  ComputeLayout(bounds().width(), &button_overflow_info);

  // Safe to call this even if the buttons are not being handled by the WebUI,
  // and this needs to be called even if they aren't pinned, so they correctly
  // track that they have not overflowed.
  forward_control_.SetIsOverflowed(
      button_overflow_info.is_forward_button_overflowed);
  home_control_.SetIsOverflowed(button_overflow_info.is_home_button_overflowed);
}

gfx::Size WebUIToolbarWebView::FlexLayoutRule(const views::View*,
                                              const views::SizeBounds& bounds) {
  return ComputeLayout(bounds.width());
}

BEGIN_METADATA(WebUIToolbarWebView)
END_METADATA
