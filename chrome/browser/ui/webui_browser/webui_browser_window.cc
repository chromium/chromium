// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_window.h"

#include "base/notimplemented.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/accelerator_table.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/exclusive_access/exclusive_access_manager.h"
#include "chrome/browser/ui/exclusive_access/fullscreen_controller.h"
#include "chrome/browser/ui/find_bar/find_bar.h"
#include "chrome/browser/ui/views/find_bar_host.h"
#include "chrome/browser/ui/views/side_panel/side_panel_ui_base.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/webui_browser/webui_browser_client_view.h"
#include "chrome/browser/ui/webui_browser/webui_browser_exclusive_access_context.h"
#include "chrome/browser/ui/webui_browser/webui_browser_extensions_container.h"
#include "chrome/browser/ui/webui_browser/webui_browser_modal_dialog_host.h"
#include "chrome/browser/ui/webui_browser/webui_browser_side_panel_ui.h"
#include "chrome/browser/ui/webui_browser/webui_browser_ui.h"
#include "chrome/browser/ui/webui_browser/webui_browser_web_contents_delegate.h"
#include "chrome/browser/ui/webui_browser/webui_location_bar.h"
#include "chrome/common/chrome_render_frame.mojom.h"
#include "chrome/common/webui_url_constants.h"
#include "components/input/native_web_keyboard_event.h"
#include "components/sharing_message/sharing_dialog_data.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/keyboard_event_processing_result.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/blink/public/common/associated_interfaces/associated_interface_provider.h"
#include "third_party/blink/public/mojom/page/draggable_region.mojom.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_delegate.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#endif  // BUILDFLAG(IS_CHROMEOS)

namespace {

const char* const kWebUIBrowserWindowKey = "__WEBUI_BROWSER_WINDOW__";

// Copied from chrome/browser/ui/views/frame/browser_widget.cc.
bool IsUsingLinuxSystemTheme(Profile* profile) {
#if BUILDFLAG(IS_LINUX)
  return ThemeServiceFactory::GetForProfile(profile)->UsingSystemTheme();
#else
  return false;
#endif
}

// WebShell is the WebContents that hosts the top-chrome WebUI. This UserData
// establishes a link from WebShell to WebUIBrowserWindow.
class WebShellWebContentsUserData : public base::SupportsUserData::Data {
 public:
  constexpr static char Key[] = "webshell-user-data";
  explicit WebShellWebContentsUserData(WebUIBrowserWindow* browser_window)
      : browser_window_(browser_window) {}

  raw_ptr<WebUIBrowserWindow> browser_window_;
};

ui::ColorProviderKey::SchemeVariant GetSchemeVariant(
    ui::mojom::BrowserColorVariant color_variant) {
  using BCV = ui::mojom::BrowserColorVariant;
  using SV = ui::ColorProviderKey::SchemeVariant;
  static constexpr auto kSchemeVariantMap = base::MakeFixedFlatMap<BCV, SV>({
      {BCV::kTonalSpot, SV::kTonalSpot},
      {BCV::kNeutral, SV::kNeutral},
      {BCV::kVibrant, SV::kVibrant},
      {BCV::kExpressive, SV::kExpressive},
  });
  return kSchemeVariantMap.at(color_variant);
}

}  // namespace

class WebUIBrowserWindow::WidgetDelegate : public views::WidgetDelegate {
 public:
  explicit WidgetDelegate(
      WebUIBrowserWindow* window,
      WebUIBrowserWebContentsDelegate* web_contents_delegate);

  views::ClientView* CreateClientView(views::Widget* widget) override;
  std::u16string GetWindowTitle() const override;

 private:
  raw_ptr<WebUIBrowserWindow> browser_window_;
  raw_ptr<WebUIBrowserWebContentsDelegate> web_contents_delegate_;
};

WebUIBrowserWindow::WebUIBrowserWindow(Browser* browser) : browser_(browser) {
  location_bar_ = std::make_unique<WebUILocationBar>(this);
  web_contents_delegate_ =
      std::make_unique<WebUIBrowserWebContentsDelegate>(this);
  widget_delegate_ =
      std::make_unique<WidgetDelegate>(this, web_contents_delegate_.get());
  widget_ = std::make_unique<views::Widget>();
  widget_->AddObserver(this);
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  params.name = "WebUIBrowserWindow";
  params.bounds = gfx::Rect(0, 0, 800, 600);
  params.delegate = widget_delegate_.get();
  params.native_widget = CreateNativeWidget();
  widget_->Init(std::move(params));
  widget_->SetNativeWindowProperty(kWebUIBrowserWindowKey, this);
  widget_->MakeCloseSynchronous(base::BindOnce(
      &WebUIBrowserWindow::OnWindowCloseRequested, base::Unretained(this)));
  auto web_view = std::make_unique<views::WebView>(browser_->profile());

  auto* ui_web_contents = web_view->GetWebContents();
  web_contents_delegate_->SetUIWebContents(ui_web_contents);
  ui_web_contents->SetDelegate(web_contents_delegate_.get());
  ui_web_contents->SetUserData(
      WebShellWebContentsUserData::Key,
      std::make_unique<WebShellWebContentsUserData>(this));

  modal_dialog_host_ = std::make_unique<WebUIBrowserModalDialogHost>(this);
  extensions_container_ =
      std::make_unique<WebUIBrowserExtensionsContainer>(*browser_, *this);

  web_view->LoadInitialURL(GURL(chrome::kChromeUIWebuiBrowserURL));
  web_view_ = widget_->SetClientContentsView(std::move(web_view));
  // Set the ColorProviderSource after attaching to the WebView otherwise
  // attaching overwrites it.
  // TODO(webium): |widget_| should use |this| as its ColorProviderSource as
  // secondary UIs need it to get the correct color mode from their parent
  // widget.
  ui_web_contents->SetColorProviderSource(this);

  widget_->Show();
  // Give our main web contents the focus so that accelerators work.
  ui_web_contents->SetInitialFocus();

  LoadAccelerators();
}

WebUIBrowserWindow::~WebUIBrowserWindow() {
  browser_->GetFeatures().TearDownPreBrowserWindowDestruction();
  web_view_ = nullptr;
  // We want to destroy the extensions container before the `widget_` since
  // it wants to de-register itself for focus stuff.
  extensions_container_.reset();
  widget_->RemoveObserver(this);
  widget_.reset();
}

// static
WebUIBrowserWindow* WebUIBrowserWindow::FromWebShellWebContents(
    content::WebContents* web_contents) {
  WebShellWebContentsUserData* user_data =
      static_cast<WebShellWebContentsUserData*>(
          web_contents->GetUserData(WebShellWebContentsUserData::Key));

  if (!user_data) {
    return nullptr;
  }

  return user_data->browser_window_;
}

// static
WebUIBrowserWindow* WebUIBrowserWindow::FromBrowser(
    BrowserWindowInterface* browser) {
  // This function is implemented based on
  // BrowserView::GetBrowserViewForBrowser(). Please see the comments in that
  // function for the implementation rationale.
  if (!browser->GetWindow() || !browser->GetWindow()->GetNativeWindow()) {
    return nullptr;
  }
  return FromNativeWindow(browser->GetWindow()->GetNativeWindow());
}

// static
WebUIBrowserWindow* WebUIBrowserWindow::FromNativeWindow(
    gfx::NativeWindow window) {
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  return widget ? reinterpret_cast<WebUIBrowserWindow*>(
                      widget->GetNativeWindowProperty(kWebUIBrowserWindowKey))
                : nullptr;
}

void WebUIBrowserWindow::Show() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ShowInactive() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::Hide() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::IsVisible() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::SetBounds(const gfx::Rect& bounds) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::Close() {
  widget_->Close();

  // This will not close the window immediately.
  // Instead, we send the close request to the OS, then the OS notifies us that
  // such a request has been made. This results in the invocation of
  // OnWindowCloseRequested(), which gives unload handlers
  // a chance to stop the closing. When all unload handlers are clear, Close()
  // will be called again. This time the close request will go through and the
  // window will be actually closed.
}

void WebUIBrowserWindow::Activate() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::Deactivate() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::IsActive() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::FlashFrame(bool flash) {
  NOTIMPLEMENTED();
}

ui::ZOrderLevel WebUIBrowserWindow::GetZOrderLevel() const {
  NOTIMPLEMENTED();
  return ui::ZOrderLevel::kNormal;
}

void WebUIBrowserWindow::SetZOrderLevel(ui::ZOrderLevel order) {
  NOTIMPLEMENTED();
}

gfx::NativeWindow WebUIBrowserWindow::GetNativeWindow() const {
#if BUILDFLAG(IS_CHROMEOS)
  // Ash ChromeOS has a UaF on widget's aura::Window during browser shutdown.
  // The window is stored in apps::InstanceRegistry which becomes dangling after
  // the BrowserWindow is destroyed.
  // TODO(webium): Fix ChromeOS. Run WebUIBrowserTest.StartupAndShutdown
  // to verify.
  return gfx::NativeWindow();
#else
  return widget_->GetNativeWindow();
#endif
}

bool WebUIBrowserWindow::IsOnCurrentWorkspace() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsVisibleOnScreen() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::SetTopControlsShownRatio(
    content::WebContents* web_contents,
    float ratio) {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::DoBrowserControlsShrinkRendererSize(
    const content::WebContents* contents) const {
  NOTIMPLEMENTED();
  return false;
}

ui::NativeTheme* WebUIBrowserWindow::GetNativeTheme() {
  return ui::NativeTheme::GetInstanceForNativeUi();
}

const ui::ThemeProvider* WebUIBrowserWindow::GetThemeProvider() const {
  // Copied from BrowserWidget::GetThemeProvider().
  auto* app_controller = browser_->app_controller();
  // Ignore the system theme for web apps with window-controls-overlay as the
  // display_override so the web contents can blend with the overlay by using
  // the developer-provided theme color for a better experience. Context:
  // https://crbug.com/1219073.
  if (app_controller && (!IsUsingLinuxSystemTheme(browser_->profile()) ||
                         app_controller->AppUsesWindowControlsOverlay())) {
    return app_controller->GetThemeProvider();
  }
  return &ThemeService::GetThemeProviderForProfile(browser_->profile());
}

const ui::ColorProvider* WebUIBrowserWindow::GetColorProvider() const {
  return ui::ColorProviderManager::Get().GetColorProviderFor(
      GetColorProviderKey());
}

ui::ColorProviderKey::ThemeInitializerSupplier*
WebUIBrowserWindow::GetThemeInitializerSupplier() const {
  // Do not return any custom theme if this is an incognito browser.
  if (browser_->profile()->IsIncognitoProfile()) {
    return nullptr;
  }

  auto* app_controller = browser_->app_controller();
  // Ignore the system theme for web apps with window-controls-overlay as the
  // display_override so the web contents can blend with the overlay by using
  // the developer-provided theme color for a better experience. Context:
  // https://crbug.com/1219073.
  if (app_controller && (!IsUsingLinuxSystemTheme(browser_->profile()) ||
                         app_controller->AppUsesWindowControlsOverlay())) {
    return app_controller->GetThemeSupplier();
  }
  auto* theme_service = ThemeServiceFactory::GetForProfile(browser_->profile());
  return theme_service->UsingDeviceTheme() ? nullptr
                                           : theme_service->GetThemeSupplier();
}

ui::ColorProviderKey WebUIBrowserWindow::GetColorProviderKey() const {
  auto key = ui::NativeTheme::GetInstanceForNativeUi()->GetColorProviderKey(
      GetThemeInitializerSupplier());

  key.app_controller = browser_->app_controller();

#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS SystemWebApps use the OS theme all the time.
  if (ash::IsSystemWebApp(browser_)) {
    return key;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  const auto* theme_service =
      ThemeServiceFactory::GetForProfile(browser_->profile());
  CHECK(theme_service);

  // Determine appropriate key.color_mode.
  [this, &key, theme_service]() {
    // Currently the incognito browser is implemented as unthemed dark mode.
    if (browser_->profile()->IsIncognitoProfile()) {
      key.color_mode = ui::ColorProviderKey::ColorMode::kDark;
      return;
    }

    const auto browser_color_scheme = theme_service->GetBrowserColorScheme();
    if (browser_color_scheme != ThemeService::BrowserColorScheme::kSystem) {
      key.color_mode =
          browser_color_scheme == ThemeService::BrowserColorScheme::kLight
              ? ui::ColorProviderKey::ColorMode::kLight
              : ui::ColorProviderKey::ColorMode::kDark;
    }
  }();

  // Determine appropriate key.user_color.
  // Device theme retains the user_color from `Widget`.
  if (!theme_service->UsingDeviceTheme()) {
    if (theme_service->UsingAutogeneratedTheme()) {
      key.user_color = theme_service->GetAutogeneratedThemeColor();
    } else if (auto user_color = theme_service->GetUserColor()) {
      key.user_color = user_color;
    }
  }

  // Determine appropriate key.user_color_source.
  if (browser_->profile()->IsIncognitoProfile()) {
    key.user_color_source = ui::ColorProviderKey::UserColorSource::kGrayscale;
  } else if (theme_service->UsingDeviceTheme()) {
    key.user_color_source = ui::ColorProviderKey::UserColorSource::kAccent;
  } else if (theme_service->GetIsGrayscale()) {
    key.user_color_source = ui::ColorProviderKey::UserColorSource::kGrayscale;
  } else if (theme_service->GetIsBaseline()) {
    key.user_color_source = ui::ColorProviderKey::UserColorSource::kBaseline;
  } else {
    CHECK(key.user_color.has_value());
    key.user_color_source = ui::ColorProviderKey::UserColorSource::kAccent;
  }

  // Determine appropriate key.scheme_variant.
  ui::mojom::BrowserColorVariant color_variant =
      theme_service->GetBrowserColorVariant();
  if (!theme_service->UsingDeviceTheme() &&
      color_variant != ui::mojom::BrowserColorVariant::kSystem) {
    key.scheme_variant = GetSchemeVariant(color_variant);
  }

  // Determine appropriate key.frame_type.
  // TODO(webium): special windows might need FrameType::kNative.
  key.frame_type = ui::ColorProviderKey::FrameType::kChromium;

#if BUILDFLAG(IS_WIN)
  if (theme_service && theme_service->UsingDeviceTheme()) {
    key.frame_style = ui::ColorProviderKey::FrameStyle::kSystem;
  }
#endif

  return key;
}

ui::RendererColorMap WebUIBrowserWindow::GetRendererColorMap(
    ui::ColorProviderKey::ColorMode color_mode,
    ui::ColorProviderKey::ForcedColors forced_colors) const {
  auto key = GetColorProviderKey();
  key.color_mode = color_mode;
  key.forced_colors = forced_colors;
  ui::ColorProvider* color_provider =
      ui::ColorProviderManager::Get().GetColorProviderFor(key);
  CHECK(color_provider);
  return ui::CreateRendererColorMap(*color_provider);
}

bool WebUIBrowserWindow::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) const {
  // Search for the accelerator in our table.
  for (const auto& entry : accelerator_table_) {
    if (entry.second == command_id) {
      *accelerator = entry.first;
      return true;
    }
  }
  return false;
}

void WebUIBrowserWindow::OnWidgetBoundsChanged(views::Widget* widget,
                                               const gfx::Rect& new_bounds) {
  DCHECK_EQ(widget, widget_.get());
  if (modal_dialog_host_) {
    modal_dialog_host_->NotifyPositionRequiresUpdate();
  }
}

gfx::Rect WebUIBrowserWindow::GetContentsBoundsInScreen() const {
  ui::TrackedElement* content_region =
      ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
          kContentsContainerViewElementId,
          views::ElementTrackerViews::GetContextForWidget(widget_.get()));
  return content_region->GetScreenBounds();
}

ui::TrackedElement* WebUIBrowserWindow::GetExtensionsMenuButtonAnchor() const {
  return ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
      kExtensionsMenuButtonElementId,
      views::ElementTrackerViews::GetContextForWidget(widget_.get()));
}

void WebUIBrowserWindow::ProcessFullscreen(bool fullscreen) {
  widget_->SetFullscreen(fullscreen);
  browser_->WindowFullscreenStateChanged();

  auto* manager = browser_->GetFeatures().exclusive_access_manager();
  if (!manager) {
    return;
  }

  auto* controller = manager->fullscreen_controller();

  std::optional<webui_browser::mojom::FullscreenContext> context;
  if (fullscreen) {
    if (controller->IsTabFullscreen()) {
      context = webui_browser::mojom::FullscreenContext::kTab;
    } else if (controller->IsFullscreenForBrowser()) {
      context = webui_browser::mojom::FullscreenContext::kBrowser;
    }
  }

  // Notify the WebUI about the fullscreen mode.
  if (webui_browser::mojom::Page* page = GetWebUIBrowserUI()->page()) {
    page->OnFullscreenModeChanged(fullscreen, context);
  }

  controller->FullscreenTransitionCompleted();
}

void WebUIBrowserWindow::DeleteBrowserWindow() {
  delete this;
}

bool WebUIBrowserWindow::FindCommandIdForAccelerator(
    const ui::Accelerator& accelerator,
    int* command_id) const {
  auto iter = accelerator_table_.find(accelerator);
  if (iter == accelerator_table_.end()) {
    return false;
  }

  *command_id = iter->second;
  if (accelerator.IsRepeat() && !IsCommandRepeatable(*command_id)) {
    return false;
  }

  return true;
}

void WebUIBrowserWindow::LoadAccelerators() {
  // Let's fill our own accelerator table.
  const bool is_app_mode = IsRunningInForcedAppMode();
#if BUILDFLAG(IS_CHROMEOS)
  const bool is_captive_portal_signin_window =
      browser_->profile()->IsOffTheRecord() &&
      browser_->profile()->GetOTRProfileID().IsCaptivePortal();
#endif
  const std::vector<AcceleratorMapping> accelerator_list(GetAcceleratorList());
  for (const auto& entry : accelerator_list) {
    // In app mode, only allow accelerators of allowlisted commands to pass
    // through.
    if (is_app_mode && !IsCommandAllowedInAppMode(entry.command_id,
                                                  browser_->is_type_popup())) {
      continue;
    }

#if BUILDFLAG(IS_CHROMEOS)
    if (is_captive_portal_signin_window) {
      int command = entry.command_id;
      // Captive portal signin uses an OTR profile without history.
      if (command == IDC_SHOW_HISTORY) {
        continue;
      }
      // The NewTab command expects navigation to occur in the same browser
      // window. For captive portal signin this is not the case, so hide these
      // to reduce confusion.
      if (command == IDC_NEW_TAB || command == IDC_NEW_TAB_TO_RIGHT ||
          command == IDC_CREATE_NEW_TAB_GROUP) {
        continue;
      }
    }
#endif

    ui::Accelerator accelerator(entry.keycode, entry.modifiers);
    accelerator_table_[accelerator] = entry.command_id;
    accelerator_manager_.Register(
        {accelerator}, ui::AcceleratorManager::kNormalPriority, this);
  }
}

bool WebUIBrowserWindow::AcceleratorPressed(
    const ui::Accelerator& accelerator) {
  int command_id;
  // Though AcceleratorManager should not send unknown |accelerator| to us, it's
  // still possible the command cannot be executed now.
  if (!FindCommandIdForAccelerator(accelerator, &command_id)) {
    return false;
  }

  return chrome::ExecuteCommand(browser_.get(), command_id,
                                accelerator.time_stamp());
}

bool WebUIBrowserWindow::CanHandleAccelerators() const {
  return true;
}

int WebUIBrowserWindow::GetTopControlsHeight() const {
  NOTIMPLEMENTED();
  return 0;
}

void WebUIBrowserWindow::SetTopControlsGestureScrollInProgress(
    bool in_progress) {
  NOTIMPLEMENTED();
}

std::vector<StatusBubble*> WebUIBrowserWindow::GetStatusBubbles() {
  NOTIMPLEMENTED();
  return {};
}

void WebUIBrowserWindow::UpdateTitleBar() {
  widget_->UpdateWindowTitle();
  // TODO(webium): The icon might also need updating.
}

void WebUIBrowserWindow::BookmarkBarStateChanged(
    BookmarkBar::AnimateChangeType change_type) {
  GetWebUIBrowserUI()->BookmarkBarStateChanged(change_type);
}

void WebUIBrowserWindow::TemporarilyShowBookmarkBar(base::TimeDelta duration) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::UpdateDevTools(
    content::WebContents* inspected_web_contents) {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::CanDockDevTools() const {
  // This forces DevTools to open in a new window, which is currently necessary
  // because the code path for a launching docked DevTools requires BrowserView,
  // ContentsContainerView, etc.
  return false;
}

void WebUIBrowserWindow::UpdateLoadingAnimations(bool is_visible) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::SetStarredState(bool is_starred) {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::IsTabModalPopupDeprecated() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::SetIsTabModalPopupDeprecated(
    bool is_tab_modal_popup_deprecated) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::OnActiveTabChanged(content::WebContents* old_contents,
                                            content::WebContents* new_contents,
                                            int index,
                                            int reason) {
  // New tabs have their ColorProviderSource set to the Widget initially.
  // Set the ColorProviderSource back to |this| when they're first activated.
  // This is a no-op if it's already set to |this|.
  new_contents->SetColorProviderSource(this);

  // State of extensions depends on what's active --- e.g. some may be disabled
  // on some URLs.
  extensions_container_->NotifyOfAllActions();

  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::OnTabDetached(content::WebContents* contents,
                                       bool was_active) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ZoomChangedForActiveTab(bool can_show_bubble) {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::ShouldHideUIForFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsFullscreenBubbleVisible() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsForceFullscreen() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::SetForceFullscreen(bool force_fullscreen) {
  NOTIMPLEMENTED();
}

gfx::Size WebUIBrowserWindow::GetContentsSize() const {
  NOTIMPLEMENTED();
  return gfx::Size();
}

void WebUIBrowserWindow::SetContentsSize(const gfx::Size& size) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::UpdatePageActionIcon(PageActionIconType type) {
  NOTIMPLEMENTED();
}

autofill::AutofillBubbleHandler*
WebUIBrowserWindow::GetAutofillBubbleHandler() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUIBrowserWindow::ExecutePageActionIconForTesting(
    PageActionIconType type) {
  NOTIMPLEMENTED();
}

LocationBar* WebUIBrowserWindow::GetLocationBar() const {
  return location_bar_.get();
}

void WebUIBrowserWindow::SetFocusToLocationBar(bool is_user_initiated) {
  if (webui_browser::mojom::Page* page = GetWebUIBrowserUI()->page()) {
    page->SetFocusToLocationBar(is_user_initiated);
  }
}

void WebUIBrowserWindow::UpdateReloadStopState(bool is_loading, bool force) {
  if (webui_browser::mojom::Page* page = GetWebUIBrowserUI()->page()) {
    page->SetReloadStopState(is_loading);
  }
}

void WebUIBrowserWindow::UpdateToolbar(content::WebContents* contents) {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::UpdateToolbarSecurityState() {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::UpdateCustomTabBarVisibility(bool visible,
                                                      bool animate) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::SetDevToolsScrimVisibility(bool visible) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ResetToolbarTabState(content::WebContents* contents) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::FocusToolbar() {
  NOTIMPLEMENTED();
}

ExtensionsContainer* WebUIBrowserWindow::GetExtensionsContainer() {
  return extensions_container_.get();
}

void WebUIBrowserWindow::ToolbarSizeChanged(bool is_animating) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::TabDraggingStatusChanged(bool is_dragging) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::LinkOpeningFromGesture(
    WindowOpenDisposition disposition) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::FocusAppMenu() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::FocusBookmarksToolbar() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::FocusInactivePopupForAccessibility() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::RotatePaneFocus(bool forwards) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::FocusWebContentsPane() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::IsBookmarkBarVisible() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsBookmarkBarAnimating() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsTabStripEditable() const {
  return true;
}

void WebUIBrowserWindow::SetTabStripNotEditableForTesting() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::IsToolbarVisible() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsToolbarShowing() const {
  NOTIMPLEMENTED();
  return false;
}

bool WebUIBrowserWindow::IsLocationBarVisible() const {
  return true;
}

SharingDialog* WebUIBrowserWindow::ShowSharingDialog(
    content::WebContents* contents,
    SharingDialogData data) {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUIBrowserWindow::ShowUpdateChromeDialog() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ShowIntentPickerBubble(
    std::vector<apps::IntentPickerAppInfo> app_info,
    bool show_stay_in_chrome,
    bool show_remember_selection,
    apps::IntentPickerBubbleType bubble_type,
    const std::optional<url::Origin>& initiating_origin,
    IntentPickerResponse callback) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ShowBookmarkBubble(const GURL& url,
                                            bool already_bookmarked) {
  NOTIMPLEMENTED();
}

sharing_hub::ScreenshotCapturedBubble*
WebUIBrowserWindow::ShowScreenshotCapturedBubble(content::WebContents* contents,
                                                 const gfx::Image& image) {
  NOTIMPLEMENTED();
  return nullptr;
}

qrcode_generator::QRCodeGeneratorBubbleView*
WebUIBrowserWindow::ShowQRCodeGeneratorBubble(content::WebContents* contents,
                                              const GURL& url,
                                              bool show_back_button) {
  NOTIMPLEMENTED();
  return nullptr;
}

send_tab_to_self::SendTabToSelfBubbleView*
WebUIBrowserWindow::ShowSendTabToSelfDevicePickerBubble(
    content::WebContents* contents) {
  NOTIMPLEMENTED();
  return nullptr;
}

send_tab_to_self::SendTabToSelfBubbleView*
WebUIBrowserWindow::ShowSendTabToSelfPromoBubble(content::WebContents* contents,
                                                 bool show_signin_button) {
  NOTIMPLEMENTED();
  return nullptr;
}

#if BUILDFLAG(IS_CHROMEOS)
void WebUIBrowserWindow::ToggleMultitaskMenu() {
  NOTIMPLEMENTED();
}
#else
sharing_hub::SharingHubBubbleView* WebUIBrowserWindow::ShowSharingHubBubble(
    share::ShareAttempt attempt) {
  NOTIMPLEMENTED();
  return nullptr;
}
#endif  // BUILDFLAG(IS_CHROMEOS)

ShowTranslateBubbleResult WebUIBrowserWindow::ShowTranslateBubble(
    content::WebContents* contents,
    translate::TranslateStep step,
    const std::string& source_language,
    const std::string& target_language,
    translate::TranslateErrors error_type,
    bool is_user_gesture) {
  NOTIMPLEMENTED();
  return ShowTranslateBubbleResult::kBrowserWindowNotValid;
}

void WebUIBrowserWindow::StartPartialTranslate(
    const std::string& source_language,
    const std::string& target_language,
    const std::u16string& text_selection) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ShowOneClickSigninConfirmation(
    const std::u16string& email,
    base::OnceCallback<void(bool)> confirmed_callback) {
  NOTIMPLEMENTED();
}

DownloadBubbleUIController*
WebUIBrowserWindow::GetDownloadBubbleUIController() {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUIBrowserWindow::ConfirmBrowserCloseWithPendingDownloads(
    int download_count,
    Browser::DownloadCloseType dialog_type,
    base::OnceCallback<void(bool)> callback) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::UserChangedTheme(
    BrowserThemeChangeType theme_change_type) {
  NotifyColorProviderChanged();
  extensions_container_->NotifyOfAllActions();  // Icons may need re-rendering.
}

void WebUIBrowserWindow::ShowAppMenu() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::PreHandleDragUpdate(const content::DropData& drop_data,
                                             const gfx::PointF& point) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::PreHandleDragExit() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::HandleDragEnded() {
  NOTIMPLEMENTED();
}

content::KeyboardEventProcessingResult
WebUIBrowserWindow::PreHandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  if ((event.GetType() != blink::WebInputEvent::Type::kRawKeyDown) &&
      (event.GetType() != blink::WebInputEvent::Type::kKeyUp)) {
    return content::KeyboardEventProcessingResult::NOT_HANDLED;
  }

  ui::Accelerator accelerator =
      ui::GetAcceleratorFromNativeWebKeyboardEvent(event);

  // What we have to do here is as follows:
  // - If the |browser_| is for an app, do nothing.
  // - On CrOS if |accelerator| is deprecated, we allow web contents to consume
  //   it if needed.
  // - If the |browser_| is not for an app, and the |accelerator| is not
  //   associated with the browser (e.g. an Ash shortcut), process it.
  // - If the |browser_| is not for an app, and the |accelerator| is associated
  //   with the browser, and it is a reserved one (e.g. Ctrl+w), process it.
  // - If the |browser_| is not for an app, and the |accelerator| is associated
  //   with the browser, and it is not a reserved one, do nothing.

  if (browser_->is_type_app() || browser_->is_type_app_popup()) {
    // Let all keys fall through to a v1 app's web content, even accelerators.
    // We don't use NOT_HANDLED_IS_SHORTCUT here. If we do that, the app
    // might not be able to see a subsequent Char event. See
    // blink::WidgetBaseInputHandler::HandleInputEvent for details.
    return content::KeyboardEventProcessingResult::NOT_HANDLED;
  }

  int command_id;
  if (!FindCommandIdForAccelerator(accelerator, &command_id)) {
    return content::KeyboardEventProcessingResult::NOT_HANDLED;
  }

  // TODO(webium): Handle shortcuts that are registered in the FocusManager.
  // We handle only browser window shortcuts here. Secondary UIs can
  // registered their own shortcuts (e.g. Ctrl+Enter closes the Find Bar) via
  // FocusManager::RegisterAccelerator().
  if (accelerator_manager_.Process(accelerator)) {
    return content::KeyboardEventProcessingResult::HANDLED;
  }

  // BrowserView does not register RELEASED accelerators. So if we can find the
  // command id from |accelerator_table_|, it must be a keydown event. This
  // DCHECK ensures we won't accidentally return NOT_HANDLED for a later added
  // RELEASED accelerator in BrowserView.
  DCHECK_EQ(event.GetType(), blink::WebInputEvent::Type::kRawKeyDown);
  // |accelerator| is a non-reserved browser shortcut (e.g. Ctrl+f).
  return content::KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT;
}

// These functions have Mac implementations in webui_browser_window_mac.mm.
#if !BUILDFLAG(IS_MAC)
bool WebUIBrowserWindow::HandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  return false;
}

views::NativeWidget* WebUIBrowserWindow::CreateNativeWidget() {
  return nullptr;
}
#endif

std::unique_ptr<FindBar> WebUIBrowserWindow::CreateFindBar() {
  return std::make_unique<FindBarHost>(
      browser_->GetFeatures().find_bar_owner());
}

web_modal::WebContentsModalDialogHost*
WebUIBrowserWindow::GetWebContentsModalDialogHost() {
  return modal_dialog_host_.get();
}

web_modal::WebContentsModalDialogHost*
WebUIBrowserWindow::GetWebContentsModalDialogHostFor(
    content::WebContents* web_contents) {
  return modal_dialog_host_.get();
}

void WebUIBrowserWindow::ShowAvatarBubbleFromAvatarButton(
    bool is_source_accelerator) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::MaybeShowProfileSwitchIPH() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::MaybeShowSupervisedUserProfileSignInIPH() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ShowHatsDialog(
    const std::string& site_id,
    const std::optional<std::string>& hats_histogram_name,
    const std::optional<uint64_t> hats_survey_ukm_id,
    base::OnceClosure success_callback,
    base::OnceClosure failure_callback,
    const SurveyBitsData& product_specific_bits_data,
    const SurveyStringData& product_specific_string_data) {
  NOTIMPLEMENTED();
}

ExclusiveAccessContext* WebUIBrowserWindow::GetExclusiveAccessContext() {
  return browser_->GetFeatures().webui_browser_exclusive_access_context();
}

std::string WebUIBrowserWindow::GetWorkspace() const {
  NOTIMPLEMENTED();
  return std::string();
}

bool WebUIBrowserWindow::IsVisibleOnAllWorkspaces() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::ShowEmojiPanel() {
  NOTIMPLEMENTED();
}

std::unique_ptr<content::EyeDropper> WebUIBrowserWindow::OpenEyeDropper(
    content::RenderFrameHost* frame,
    content::EyeDropperListener* listener) {
  NOTIMPLEMENTED();
  return nullptr;
}

void WebUIBrowserWindow::ShowCaretBrowsingDialog() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::CreateTabSearchBubble(
    tab_search::mojom::TabSearchSection section,
    tab_search::mojom::TabOrganizationFeature organization_feature) {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::CloseTabSearchBubble() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ShowIncognitoClearBrowsingDataDialog() {
  NOTIMPLEMENTED();
}

void WebUIBrowserWindow::ShowIncognitoHistoryDisclaimerDialog() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::IsBorderlessModeEnabled() const {
  NOTIMPLEMENTED();
  return false;
}

void WebUIBrowserWindow::OnWebApiWindowResizableChanged() {
  NOTIMPLEMENTED();
}

bool WebUIBrowserWindow::GetCanResize() {
  return widget_delegate_->CanResize();
}

ui::mojom::WindowShowState WebUIBrowserWindow::GetWindowShowState() const {
  if (IsMaximized()) {
    return ui::mojom::WindowShowState::kMaximized;
  } else if (IsMinimized()) {
    return ui::mojom::WindowShowState::kMinimized;
  } else if (IsFullscreen()) {
    return ui::mojom::WindowShowState::kFullscreen;
  } else {
    return ui::mojom::WindowShowState::kDefault;
  }
}

void WebUIBrowserWindow::ShowChromeLabs() {
  NOTIMPLEMENTED();
}

BrowserView* WebUIBrowserWindow::AsBrowserView() {
  NOTIMPLEMENTED();
  return nullptr;
}

gfx::Rect WebUIBrowserWindow::GetBounds() const {
  return widget_->GetWindowBoundsInScreen();
}

bool WebUIBrowserWindow::IsMaximized() const {
  return widget_->IsMaximized();
}

bool WebUIBrowserWindow::IsMinimized() const {
  return widget_->IsMinimized();
}

bool WebUIBrowserWindow::IsFullscreen() const {
  return widget_->IsFullscreen();
}

gfx::Rect WebUIBrowserWindow::GetRestoredBounds() const {
  NOTIMPLEMENTED();
  return gfx::Rect();
}

ui::mojom::WindowShowState WebUIBrowserWindow::GetRestoredState() const {
  NOTIMPLEMENTED();
  return ui::mojom::WindowShowState::kDefault;
}

void WebUIBrowserWindow::Maximize() {
  widget_->Maximize();
}

void WebUIBrowserWindow::Minimize() {
  widget_->Minimize();
}

void WebUIBrowserWindow::Restore() {
  widget_->Restore();
}

void WebUIBrowserWindow::OnWindowCloseRequested(
    views::Widget::ClosedReason close_reason) {
  // TODO(webium): don't close a window during tab dragging.

  // Give beforeunload handlers, the user, or policy the chance to cancel the
  // close before we hide the window below.
  if (!browser_->HandleBeforeClose()) {
    // Need to reset the synchronous close callback after each Close() call as
    // it's reset once used.  Close() is generally called twice during shutdown.
    widget_->MakeCloseSynchronous(base::BindOnce(
        &WebUIBrowserWindow::OnWindowCloseRequested, base::Unretained(this)));
    return;
  }

  browser_->OnWindowClosing();
  if (!browser_->tab_strip_model()->empty()) {
    // Tab strip isn't empty.  Hide the frame (so it appears to have closed
    // immediately) and close all the tabs, allowing the renderers to shut
    // down. When the tab strip is empty we'll be called back again.
    widget_->Hide();
  }
}

WebUIBrowserWindow::WidgetDelegate::WidgetDelegate(
    WebUIBrowserWindow* window,
    WebUIBrowserWebContentsDelegate* web_contents_delegate)
    : browser_window_(window), web_contents_delegate_(web_contents_delegate) {
  // TODO(webium): May want to override these for Apps or Picture-in-picture.
  SetCanResize(browser_window_->browser_->create_params().can_resize);
  SetCanMaximize(browser_window_->browser_->create_params().can_maximize);
  SetCanFullscreen(browser_window_->browser_->create_params().can_fullscreen);
  SetCanMinimize(true);
}

views::ClientView* WebUIBrowserWindow::WidgetDelegate::CreateClientView(
    views::Widget* widget) {
  return new WebUIBrowserClientView(web_contents_delegate_, widget,
                                    TransferOwnershipOfContentsView());
}

std::u16string WebUIBrowserWindow::WidgetDelegate::GetWindowTitle() const {
  // TODO(webium):  BrowserView::GetWindowTitle() has some magic for media
  // on Mac.
  return browser_window_->browser()->GetWindowTitleForCurrentTab(
      true /* include_app_name */);
}

WebUIBrowserUI* WebUIBrowserWindow::GetWebUIBrowserUI() const {
  return web_contents_delegate_->web_contents()
      ->GetWebUI()
      ->GetController()
      ->GetAs<WebUIBrowserUI>();
}

void WebUIBrowserWindow::ShowSidePanel(SidePanelEntryKey side_panel_entry_key) {
  GetWebUIBrowserUI()->ShowSidePanel(side_panel_entry_key);
}

void WebUIBrowserWindow::CloseSidePanel() {
  GetWebUIBrowserUI()->CloseSidePanel();
}

WebUIBrowserSidePanelUI* WebUIBrowserWindow::GetWebUIBrowserSidePanelUI() {
  return static_cast<WebUIBrowserSidePanelUI*>(
      browser_->browser_window_features()->side_panel_ui());
}
