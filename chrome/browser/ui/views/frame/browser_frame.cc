// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/debug/leak_annotations.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/headless/headless_mode_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/native_browser_frame.h"
#include "chrome/browser/ui/views/frame/native_browser_frame_factory.h"
#include "chrome/browser/ui/views/frame/system_menu_model_builder.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/themes.mojom.h"
#include "ui/color/color_provider_key.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/font_list.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/native_widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/wm/desks/desks_helper.h"
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "components/user_manager/user_manager.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/display/screen.h"
#include "ui/linux/linux_ui.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/titlebar_config.h"
#endif

namespace {

bool IsUsingLinuxSystemTheme(Profile* profile) {
#if BUILDFLAG(IS_LINUX)
  return ThemeServiceFactory::GetForProfile(profile)->UsingSystemTheme();
#else
  return false;
#endif
}

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

////////////////////////////////////////////////////////////////////////////////
// BrowserFrame, public:

BrowserFrame::BrowserFrame(BrowserView* browser_view)
    : native_browser_frame_(nullptr),
      root_view_(nullptr),
      browser_frame_view_(nullptr),
      browser_view_(browser_view) {
  browser_view_->set_frame(this);
  set_is_secondary_widget(false);
  // Don't focus anything on creation, selecting a tab will set the focus.
  set_focus_on_creation(false);
}

BrowserFrame::~BrowserFrame() {}

void BrowserFrame::InitBrowserFrame() {
  native_browser_frame_ =
      NativeBrowserFrameFactory::CreateNativeBrowserFrame(this, browser_view_);
  views::Widget::InitParams params = native_browser_frame_->GetWidgetParams();
  params.name = "BrowserFrame";
  params.delegate = browser_view_;
  params.headless_mode = headless::IsHeadlessMode();

  Browser* browser = browser_view_->browser();
  if (browser->is_type_picture_in_picture()) {
    params.z_order = ui::ZOrderLevel::kFloatingWindow;
    // This doesn't change anything visually, but has the side-effect of keeping
    // the pip window in the tab order.
    params.remove_standard_frame = true;
    params.visible_on_all_workspaces = true;
  }

#if BUILDFLAG(IS_OZONE)
  params.inhibit_keyboard_shortcuts =
      browser->is_type_app() || browser->is_type_app_popup();
#endif

  if (native_browser_frame_->ShouldRestorePreviousBrowserWidgetState()) {
    if (browser->is_type_normal() || browser->is_type_devtools() ||
        browser->is_type_app()) {
      // Typed panel/popup can only return a size once the widget has been
      // created.
      // DevTools counts as a popup, but DevToolsWindow::CreateDevToolsBrowser
      // ensures there is always a size available. Without this, the tools
      // launch on the wrong display and can have sizing issues when
      // repositioned to the saved bounds in Widget::SetInitialBounds.
      chrome::GetSavedWindowBoundsAndShowState(browser, &params.bounds,
                                               &params.show_state);

      params.workspace = browser->initial_workspace();
      if (native_browser_frame_->ShouldUseInitialVisibleOnAllWorkspaces()) {
        params.visible_on_all_workspaces =
            browser->initial_visible_on_all_workspaces_state();
      }
      const base::CommandLine& parsed_command_line =
          *base::CommandLine::ForCurrentProcess();

      if (parsed_command_line.HasSwitch(switches::kWindowWorkspace)) {
        params.workspace =
            parsed_command_line.GetSwitchValueASCII(switches::kWindowWorkspace);
      }
    }
  }

  Init(std::move(params));
  SelectNativeTheme();

  if (!native_browser_frame_->UsesNativeSystemMenu()) {
    DCHECK(non_client_view());
    non_client_view()->set_context_menu_controller(this);
  }
}

int BrowserFrame::GetMinimizeButtonOffset() const {
  return native_browser_frame_->GetMinimizeButtonOffset();
}

gfx::Rect BrowserFrame::GetBoundsForTabStripRegion(
    const gfx::Size& tabstrip_minimum_size) const {
  // This can be invoked before |browser_frame_view_| has been set.
  return browser_frame_view_ ? browser_frame_view_->GetBoundsForTabStripRegion(
                                   tabstrip_minimum_size)
                             : gfx::Rect();
}

gfx::Rect BrowserFrame::GetBoundsForWebAppFrameToolbar(
    const gfx::Size& toolbar_preferred_size) const {
  // This can be invoked before |browser_frame_view_| has been set.
  return browser_frame_view_
             ? browser_frame_view_->GetBoundsForWebAppFrameToolbar(
                   toolbar_preferred_size)
             : gfx::Rect();
}

void BrowserFrame::LayoutWebAppWindowTitle(
    const gfx::Rect& available_space,
    views::Label& window_title_label) const {
  // This can be invoked before |browser_frame_view_| has been set.
  if (browser_frame_view_) {
    browser_frame_view_->LayoutWebAppWindowTitle(available_space,
                                                 window_title_label);
  }
}

int BrowserFrame::GetTopInset() const {
  return browser_frame_view_->GetTopInset(false);
}

int BrowserFrame::GetThemeBackgroundXInset() const {
  return browser_frame_view_->GetThemeBackgroundXInset();
}

void BrowserFrame::UpdateThrobber(bool running) {
  browser_frame_view_->UpdateThrobber(running);
}

BrowserNonClientFrameView* BrowserFrame::GetFrameView() const {
  return browser_frame_view_;
}

bool BrowserFrame::UseCustomFrame() const {
  return native_browser_frame_->UseCustomFrame();
}

bool BrowserFrame::ShouldSaveWindowPlacement() const {
  return native_browser_frame_->ShouldSaveWindowPlacement();
}

bool BrowserFrame::ShouldDrawFrameHeader() const {
  return true;
}

void BrowserFrame::GetWindowPlacement(gfx::Rect* bounds,
                                      ui::WindowShowState* show_state) const {
  return native_browser_frame_->GetWindowPlacement(bounds, show_state);
}

content::KeyboardEventProcessingResult BrowserFrame::PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return native_browser_frame_->PreHandleKeyboardEvent(event);
}

bool BrowserFrame::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  return native_browser_frame_->HandleKeyboardEvent(event);
}

void BrowserFrame::OnBrowserViewInitViewsComplete() {
  browser_frame_view_->OnBrowserViewInitViewsComplete();
}

void BrowserFrame::UserChangedTheme(BrowserThemeChangeType theme_change_type) {
  // kWebAppTheme is triggered by web apps and will only change colors, not the
  // frame type; just refresh the theme on all views in the browser window.
  if (theme_change_type == BrowserThemeChangeType::kWebAppTheme) {
    ThemeChanged();
    return;
  }

  // When the browser theme changes, the NativeTheme may also change.
  // In Incognito, the usage of dark or normal hinges on the browser theme.
  if (theme_change_type == BrowserThemeChangeType::kBrowserTheme)
    SelectNativeTheme();

  if (!RegenerateFrameOnThemeChange(theme_change_type)) {
    // If RegenerateFrame() returns true, ThemeChanged() was implicitly called,
    // so no need to call it explicitly.
    ThemeChanged();
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, views::Widget overrides:

views::internal::RootView* BrowserFrame::CreateRootView() {
  root_view_ = new BrowserRootView(browser_view_, this);
  return root_view_;
}

std::unique_ptr<views::NonClientFrameView>
BrowserFrame::CreateNonClientFrameView() {
  auto browser_frame_view =
      chrome::CreateBrowserNonClientFrameView(this, browser_view_);
  browser_frame_view_ = browser_frame_view.get();
  return browser_frame_view;
}

bool BrowserFrame::GetAccelerator(int command_id,
                                  ui::Accelerator* accelerator) const {
  return browser_view_->GetAccelerator(command_id, accelerator);
}

const ui::ThemeProvider* BrowserFrame::GetThemeProvider() const {
  Browser* browser = browser_view_->browser();
  auto* app_controller = browser->app_controller();
  // Ignore the system theme for web apps with window-controls-overlay as the
  // display_override so the web contents can blend with the overlay by using
  // the developer-provided theme color for a better experience. Context:
  // https://crbug.com/1219073.
  if (app_controller && (!IsUsingLinuxSystemTheme(browser->profile()) ||
                         app_controller->AppUsesWindowControlsOverlay())) {
    return app_controller->GetThemeProvider();
  }
  return &ThemeService::GetThemeProviderForProfile(browser->profile());
}

ui::ColorProviderKey::ThemeInitializerSupplier* BrowserFrame::GetCustomTheme()
    const {
  // Do not return any custom theme if this is an incognito browser.
  if (IsIncognitoBrowser()) {
    return nullptr;
  }

  Browser* browser = browser_view_->browser();
  auto* app_controller = browser->app_controller();
  // Ignore the system theme for web apps with window-controls-overlay as the
  // display_override so the web contents can blend with the overlay by using
  // the developer-provided theme color for a better experience. Context:
  // https://crbug.com/1219073.
  if (app_controller && (!IsUsingLinuxSystemTheme(browser->profile()) ||
                         app_controller->AppUsesWindowControlsOverlay())) {
    return app_controller->GetThemeSupplier();
  }
  return ThemeService::GetThemeSupplierForProfile(browser->profile());
}

void BrowserFrame::OnNativeWidgetWorkspaceChanged() {
  chrome::SaveWindowWorkspace(browser_view_->browser(), GetWorkspace());
  chrome::SaveWindowVisibleOnAllWorkspaces(browser_view_->browser(),
                                           IsVisibleOnAllWorkspaces());
#if BUILDFLAG(IS_LINUX)
  // If the window was sent to a different workspace, prioritize it if
  // it was sent to the current workspace and deprioritize it
  // otherwise.  This is done by MoveBrowsersInWorkspaceToFront()
  // which reorders the browsers such that the ones in the current
  // workspace appear before ones in other workspaces.
  auto workspace = display::Screen::GetScreen()->GetCurrentWorkspace();
  if (!workspace.empty())
    BrowserList::MoveBrowsersInWorkspaceToFront(workspace);
#endif
  Widget::OnNativeWidgetWorkspaceChanged();
}

void BrowserFrame::ShowContextMenuForViewImpl(views::View* source,
                                              const gfx::Point& p,
                                              ui::MenuSourceType source_type) {
  if (chrome::IsRunningInForcedAppMode())
    return;

  // Do not show context menu for Document picture-in-picture browser. Context:
  // http://b/274862709.
  if (browser_view_->browser()->is_type_picture_in_picture()) {
    return;
  }

  // Only show context menu if point is in unobscured parts of browser, i.e.
  // if NonClientHitTest returns :
  // - HTCAPTION: in title bar or unobscured part of tabstrip
  // - HTNOWHERE: as the name implies.
  gfx::Point point_in_view_coords(p);
  views::View::ConvertPointFromScreen(non_client_view(), &point_in_view_coords);
  int hit_test = non_client_view()->NonClientHitTest(point_in_view_coords);
  if (hit_test == HTCAPTION || hit_test == HTNOWHERE) {
    menu_runner_ = std::make_unique<views::MenuRunner>(
        GetSystemMenuModel(),
        views::MenuRunner::HAS_MNEMONICS | views::MenuRunner::CONTEXT_MENU,
        base::BindRepeating(&BrowserFrame::OnMenuClosed,
                            base::Unretained(this)));
    menu_runner_->RunMenuAt(source->GetWidget(), nullptr,
                            gfx::Rect(p, gfx::Size(0, 0)),
                            views::MenuAnchorPosition::kTopLeft, source_type);
  }
}

bool BrowserFrame::IsMenuRunnerRunningForTesting() const {
  return menu_runner_ ? menu_runner_->IsRunning() : false;
}

ui::MenuModel* BrowserFrame::GetSystemMenuModel() {
  // TODO(b/271137301): Refactor this class to remove chromeos specific code to
  // subclasses.
#if BUILDFLAG(IS_CHROMEOS_ASH)
  if (user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->GetLoggedInUsers().size() > 1) {
    // In Multi user mode, the number of users as well as the order of users
    // can change. Coming here we have more than one user and since the menu
    // model contains the user information, it must get updated to show any
    // changes happened since the last invocation.
    menu_model_builder_.reset();
  }
#endif
#if BUILDFLAG(IS_CHROMEOS)
  auto* desks_helper = chromeos::DesksHelper::Get(GetNativeWindow());
  int current_num_desks = desks_helper ? desks_helper->GetNumberOfDesks() : -1;
  if (current_num_desks != num_desks_) {
    // Since the number of desks can change, the model must update to show any
    // changes happened since the last invocation.
    menu_model_builder_.reset();
    num_desks_ = current_num_desks;
  }

  bool is_float_state_type =
      GetNativeWindow()->GetProperty(chromeos::kWindowStateTypeKey) ==
      chromeos::WindowStateType::kFloated;
  if (is_float_state_type != is_float_state_type_) {
    menu_model_builder_.reset();
    is_float_state_type_ = is_float_state_type;
  }
#endif
  if (!menu_model_builder_.get()) {
    menu_model_builder_ = std::make_unique<SystemMenuModelBuilder>(
        browser_view_, browser_view_->browser());
    menu_model_builder_->Init();
  }
  return menu_model_builder_->menu_model();
}

void BrowserFrame::SetTabDragKind(TabDragKind tab_drag_kind) {
  if (tab_drag_kind_ == tab_drag_kind)
    return;

  if (native_browser_frame_)
    native_browser_frame_->TabDraggingKindChanged(tab_drag_kind);

  bool was_dragging_any = tab_drag_kind_ != TabDragKind::kNone;
  bool is_dragging_any = tab_drag_kind != TabDragKind::kNone;
  if (was_dragging_any != is_dragging_any)
    browser_view_->TabDraggingStatusChanged(is_dragging_any);

  tab_drag_kind_ = tab_drag_kind;
}

void BrowserFrame::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  UserChangedTheme(BrowserThemeChangeType::kNativeTheme);
}

ui::ColorProviderKey BrowserFrame::GetColorProviderKey() const {
  auto key = Widget::GetColorProviderKey();

  key.app_controller = browser_view_->browser()->app_controller();

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // ChromeOS SystemWebApps use the OS theme all the time.
  if (ash::IsSystemWebApp(browser_view_->browser())) {
    return key;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // color_mode.
  [this, &key]() {
    // Currently the incognito browser is implemented as unthemed dark mode.
    if (IsIncognitoBrowser()) {
      key.color_mode = ui::ColorProviderKey::ColorMode::kDark;
      return;
    }

    const auto* theme_service =
        ThemeServiceFactory::GetForProfile(browser_view_->browser()->profile());
    const auto browser_color_scheme = theme_service->GetBrowserColorScheme();

    if (browser_color_scheme != ThemeService::BrowserColorScheme::kSystem) {
      key.color_mode =
          browser_color_scheme == ThemeService::BrowserColorScheme::kLight
              ? ui::ColorProviderKey::ColorMode::kLight
              : ui::ColorProviderKey::ColorMode::kDark;
    }
  }();

  const auto* theme_service =
      ThemeServiceFactory::GetForProfile(browser_view_->browser()->profile());

  // is_grayscale.
  // Incognito mode browser should be forced to grayscale.
  key.is_grayscale = IsIncognitoBrowser() ||
                     (theme_service && !theme_service->UsingDeviceTheme() &&
                      theme_service->GetIsGrayscale());

  // user_color.
  [this, &key]() {
    // The grayscale theme also assumes that the baseline palette is used.
    if (key.is_grayscale) {
      // Baseline palette is used when `ColorProviderKey::user_color` is empty.
      key.user_color = absl::nullopt;
      return;
    }

    const auto* theme_service =
        ThemeServiceFactory::GetForProfile(browser_view_->browser()->profile());
    if (!theme_service) {
      return;
    }

    if (theme_service->UsingAutogeneratedTheme()) {
      key.user_color = theme_service->GetAutogeneratedThemeColor();
      return;
    }

    if (theme_service->UsingDeviceTheme()) {
      // Device theme retains the user_color from `Widget`.
      return;
    }

    if (theme_service->GetIsBaseline()) {
      // Baseline theme has to explicitly clear user_color as it might have been
      // set earlier.
      key.user_color = absl::nullopt;
    } else {
      key.user_color = theme_service->GetUserColor();
    }
  }();

  // scheme_variant.
  ui::mojom::BrowserColorVariant color_variant =
      theme_service->GetBrowserColorVariant();
  if (!theme_service->UsingDeviceTheme() &&
      color_variant != ui::mojom::BrowserColorVariant::kSystem) {
    key.scheme_variant = GetSchemeVariant(color_variant);
  }

  // frame_type.
  key.frame_type = UseCustomFrame() ? ui::ColorProviderKey::FrameType::kChromium
                                    : ui::ColorProviderKey::FrameType::kNative;


  return key;
}

void BrowserFrame::OnMenuClosed() {
  menu_runner_.reset();
}

void BrowserFrame::OnTouchUiChanged() {
  client_view()->InvalidateLayout();

  // For standard browser frame, if we do not invalidate the NonClientFrameView
  // the client window bounds will not be properly updated which could cause
  // visual artifacts. See crbug.com/1035959 for details.
  if (non_client_view()->frame_view()) {
    // Note that invalidating a view invalidates all of its ancestors, so it is
    // not necessary to also invalidate the NonClientView or RootView here.
    non_client_view()->frame_view()->InvalidateLayout();
  } else {
    non_client_view()->InvalidateLayout();
  }
  GetRootView()->Layout();
}

void BrowserFrame::SelectNativeTheme() {
  // Select between regular and Linux toolkit themes.
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();

  // Use the regular NativeTheme instance if running incognito mode, regardless
  // of system theme (gtk, qt etc).
  if (IsIncognitoBrowser()) {
    SetNativeTheme(native_theme);
    return;
  }

#if BUILDFLAG(IS_LINUX)
  const auto* linux_ui_theme =
      ui::LinuxUiTheme::GetForWindow(GetNativeWindow());
  // Ignore the system theme for web apps with window-controls-overlay as the
  // display_override so the web contents can blend with the overlay by using
  // the developer-provided theme color for a better experience. Context:
  // https://crbug.com/1219073.
  if (linux_ui_theme && !browser_view_->AppUsesWindowControlsOverlay())
    native_theme = linux_ui_theme->GetNativeTheme();
#endif

  SetNativeTheme(native_theme);
}

bool BrowserFrame::RegenerateFrameOnThemeChange(
    BrowserThemeChangeType theme_change_type) {
  bool need_regenerate = false;
#if BUILDFLAG(IS_LINUX)
  // System and user theme changes can both change frame buttons, so the frame
  // always needs to be regenerated on Linux.
  need_regenerate = true;
#endif

#if BUILDFLAG(IS_WIN)
  // On Windows, DWM transition does not performed for a frame regeneration in
  // fullscreen mode, so do a lighweight theme change to refresh a bookmark bar
  // on new tab. (see crbug/1002480)
  // With Mica, toggling titlebar accent colors in the native theme needs a
  // frame regen to switch between the system-drawn and custom-drawn titlebars.
  need_regenerate |=
      (theme_change_type == BrowserThemeChangeType::kBrowserTheme ||
       SystemTitlebarCanUseMicaMaterial()) &&
      !IsFullscreen();
#else
  need_regenerate |= theme_change_type == BrowserThemeChangeType::kBrowserTheme;
#endif

  if (need_regenerate) {
    // This is a heavyweight theme change that requires regenerating the frame
    // as well as repainting the browser window.
    // No need to call ThemeChanged(). It will be implicitly called by
    // FrameTypeChanged().
    FrameTypeChanged();
    return true;
  }

  return false;
}

bool BrowserFrame::IsIncognitoBrowser() const {
  return browser_view_->browser()->profile()->IsIncognitoProfile();
}
