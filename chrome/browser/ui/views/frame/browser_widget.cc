// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_widget.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/containers/fixed_flat_map.h"
#include "base/debug/leak_annotations.h"
#include "base/functional/bind.h"
#include "base/i18n/rtl.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/custom_theme_supplier.h"
#include "chrome/browser/themes/theme_service.h"
#include "chrome/browser/themes/theme_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window_state.h"
#include "chrome/browser/ui/views/frame/browser_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_native_widget.h"
#include "chrome/browser/ui/views/frame/browser_native_widget_factory.h"
#include "chrome/browser/ui/views/frame/browser_root_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/frame/system_menu_model_builder.h"
#include "chrome/browser/ui/views/frame/top_container_view.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "ui/base/hit_test.h"
#include "ui/base/mojom/menu_source_type.mojom-forward.h"
#include "ui/base/mojom/themes.mojom.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/color/color_provider_key.h"
#include "ui/events/event_handler.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/native_widget.h"

#if BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chromeos/ui/base/window_properties.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/wm/desks/desks_helper.h"
#include "components/user_manager/user_manager.h"
#include "ui/aura/window.h"
#endif

#if BUILDFLAG(IS_LINUX)
#include "ui/display/screen.h"
#include "ui/linux/linux_ui.h"
#endif

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/mica_titlebar.h"
#endif

namespace {

// Helper to track whether a ThemeChange event has been received by the widget.
class ThemeChangedObserver : public views::WidgetObserver {
 public:
  explicit ThemeChangedObserver(views::Widget* widget) {
    widget_observation.Observe(widget);
  }
  ThemeChangedObserver(const ThemeChangedObserver&) = delete;
  ThemeChangedObserver& operator=(const ThemeChangedObserver&) = delete;
  ~ThemeChangedObserver() override = default;

  // views::WidgetObserver:
  void OnWidgetThemeChanged(views::Widget* widget) override {
    theme_changed_ = true;
  }

  bool theme_changed() const { return theme_changed_; }

 private:
  bool theme_changed_ = false;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation{this};
};

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
// BrowserWidget, public:

BrowserWidget::BrowserWidget(BrowserView* browser_view)
    : browser_native_widget_(nullptr),
      root_view_(nullptr),
      browser_frame_view_(nullptr),
      browser_view_(browser_view) {
  set_is_secondary_widget(false);
  // Don't focus anything on creation, selecting a tab will set the focus.
  set_focus_on_creation(false);
}

BrowserWidget::~BrowserWidget() {
  set_widget_closed();

  // Synchronously destroy owned Widgets, which are typically owned by the
  // BrowserWidget's NativeWidget, to mitigate the risk of dangling pointers to
  // Browser and related objects during destruction.
  views::Widget::ForEachOwnedWidget(GetNativeView(),
                                    [this](views::Widget* widget) {
                                      if (widget != this) {
                                        widget->CloseNow();
                                      }
                                    });

  // Invoke the pre-window-destruction lifecycle hook before the owned
  // BrowserView is destroyed.
  // Do this here and not in ~BrowserView() as BrowserWindowFeatures may attempt
  // to read state on the BrowserWidget as they undergo destruction, and
  // BrowserWidget state is destroyed at the end of this scope.
  browser_view_->browser()->GetFeatures().TearDownPreBrowserWindowDestruction();
}

void BrowserWidget::InitBrowserWidget() {
  browser_native_widget_ =
      BrowserNativeWidgetFactory::CreateBrowserNativeWidget(this,
                                                            browser_view_);
  views::Widget::InitParams params = browser_native_widget_->GetWidgetParams(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET);
  params.name = "BrowserWidget";
  params.delegate = browser_view_;

  Browser* browser = browser_view_->browser();
  if (browser->is_type_picture_in_picture()) {
    params.z_order = ui::ZOrderLevel::kFloatingWindow;
    params.visible_on_all_workspaces = true;
#if !BUILDFLAG(IS_WIN)
    // This has the side-effect of keeping the pip window in the tab order.
    //
    // On all platforms, except for Windows, this doesn't change anything
    // visually. If this is set for the Windows platform, the UI will be
    // affected. Specifically, the title bar will not render correctly, see
    // https://crbug.com/1456231 for more details.
    params.remove_standard_frame = true;
#endif  // !BUILDFLAG(IS_WIN)
  }

#if BUILDFLAG(IS_OZONE)
  params.inhibit_keyboard_shortcuts =
      browser->is_type_app() || browser->is_type_app_popup();

  params.session_data = browser->platform_session_data();
#endif

  if (browser_native_widget_->ShouldRestorePreviousBrowserWidgetState()) {
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
      if (browser_native_widget_->ShouldUseInitialVisibleOnAllWorkspaces()) {
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

#if BUILDFLAG(IS_LINUX)
  SelectNativeTheme();
#else
  SetNativeTheme(ui::NativeTheme::GetInstanceForNativeUi());
#endif

  if (!browser_native_widget_->UsesNativeSystemMenu()) {
    DCHECK(non_client_view());
    non_client_view()->set_context_menu_controller(this);
  }
}

BrowserFrameView* BrowserWidget::GetFrameView() const {
  return browser_frame_view_;
}

bool BrowserWidget::ShouldSaveWindowPlacement() const {
  return browser_native_widget_ &&
         browser_native_widget_->ShouldSaveWindowPlacement();
}

void BrowserWidget::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::mojom::WindowShowState* show_state) const {
  if (browser_native_widget_) {
    browser_native_widget_->GetWindowPlacement(bounds, show_state);
  }
}

content::KeyboardEventProcessingResult BrowserWidget::PreHandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  return browser_native_widget_
             ? browser_native_widget_->PreHandleKeyboardEvent(event)
             : content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool BrowserWidget::HandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  return browser_native_widget_ &&
         browser_native_widget_->HandleKeyboardEvent(event);
}

void BrowserWidget::UserChangedTheme(BrowserThemeChangeType theme_change_type) {
  // kWebAppTheme is triggered by web apps and will only change colors, not the
  // frame type; just refresh the theme on all views in the browser window.
  if (theme_change_type == BrowserThemeChangeType::kWebAppTheme) {
    ThemeChanged();
    return;
  }

  // RegenerateFrameOnThemeChange() may or may not result in an implicit call to
  // ThemeChanged(), regardless of whether the frame was regenerated or not.
  // Ensure that ThemeChanged() is called for this Widget if no implicit call
  // occurred.
  // TODO(crbug.com/40280130): The entire theme propagation system needs to be
  // moved to scheduling theme changes rather than synchronously demanding a
  // ThemeChange() event take place. This will reduce a ton of churn resulting
  // from independent clients increasingly issuing theme change requests.
  ThemeChangedObserver theme_changed_observer(this);
  RegenerateFrameOnThemeChange(theme_change_type);

  if (theme_change_type == BrowserThemeChangeType::kBrowserTheme) {
    // When the browser theme changes, the NativeTheme may also change.
    SelectNativeTheme();

    // Browser theme changes are directly observed by the BrowserWidget. However
    // the other Widgets in the frame's hierarchy may inherit this new theme
    // information in their ColorProviderKeys and thus should also be forwarded
    // theme change notifications.
    Widget::Widgets widgets = GetAllOwnedWidgets(GetNativeView());
    for (Widget* widget : widgets) {
      widget->ThemeChanged();
    }
  }

  if (!theme_changed_observer.theme_changed()) {
    ThemeChanged();
  }
}

///////////////////////////////////////////////////////////////////////////////
// BrowserWidget, views::Widget overrides:

views::internal::RootView* BrowserWidget::CreateRootView() {
  root_view_ = new BrowserRootView(browser_view_, this);
  return root_view_;
}

std::unique_ptr<views::FrameView> BrowserWidget::CreateFrameView() {
  auto browser_frame_view = chrome::CreateBrowserFrameView(this, browser_view_);
  browser_frame_view_ = browser_frame_view.get();
  return browser_frame_view;
}

bool BrowserWidget::GetAccelerator(int command_id,
                                   ui::Accelerator* accelerator) const {
  return browser_view_->GetAccelerator(command_id, accelerator);
}

const ui::ThemeProvider* BrowserWidget::GetThemeProvider() const {
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

ui::ColorProviderKey::ThemeInitializerSupplier* BrowserWidget::GetCustomTheme()
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
  auto* theme_service = ThemeServiceFactory::GetForProfile(browser->profile());
  return theme_service->UsingDeviceTheme() ? nullptr
                                           : theme_service->GetThemeSupplier();
}

void BrowserWidget::OnNativeWidgetWorkspaceChanged() {
  chrome::SaveWindowWorkspace(browser_view_->browser(), GetWorkspace());
  chrome::SaveWindowVisibleOnAllWorkspaces(browser_view_->browser(),
                                           IsVisibleOnAllWorkspaces());
#if BUILDFLAG(IS_LINUX)
  // If the window was sent to a different workspace, prioritize it if
  // it was sent to the current workspace and deprioritize it
  // otherwise.  This is done by MoveBrowsersInWorkspaceToFront()
  // which reorders the browsers such that the ones in the current
  // workspace appear before ones in other workspaces.
  auto workspace = display::Screen::Get()->GetCurrentWorkspace();
  if (!workspace.empty()) {
    BrowserList::MoveBrowsersInWorkspaceToFront(workspace);
  }
#endif
  Widget::OnNativeWidgetWorkspaceChanged();
}

void BrowserWidget::OnNativeWidgetDestroyed() {
  browser_native_widget_ = nullptr;
  Browser* const browser = browser_view_->browser();

  // Current expectations are that the Browser is destroyed synchronously when
  // its NativeWidget is destroyed. Prepare Browser and request synchronous
  // destruction here.
  // TODO(crbug.com/413168662): Once clients have been migrated away from
  // closing Browsers via their NativeWidgets explore removing this completely.
  browser->set_force_skip_warning_user_on_close(true);
  browser->OnWindowClosing();
  Widget::OnNativeWidgetDestroyed();
  browser->SynchronouslyDestroyBrowser();
}

void BrowserWidget::ShowContextMenuForViewImpl(
    views::View* source,
    const gfx::Point& p,
    ui::mojom::MenuSourceType source_type) {
  if (IsRunningInForcedAppMode()) {
    return;
  }

  // Do not show context menu for Document picture-in-picture browser. Context:
  // http://b/274862709.
  if (browser_view_->browser()->is_type_picture_in_picture()) {
    return;
  }

  // Don't show a menu if a tab drag is active. https://crbug.com/1517709
  if (tab_drag_kind_ != TabDragKind::kNone) {
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
        base::BindRepeating(&BrowserWidget::OnMenuClosed,
                            base::Unretained(this)));
    menu_runner_->RunMenuAt(source->GetWidget(), nullptr,
                            gfx::Rect(p, gfx::Size(0, 0)),
                            views::MenuAnchorPosition::kTopLeft, source_type);
    base::RecordAction(base::UserMetricsAction("SystemContextMenu_Opened"));
  }
}

bool BrowserWidget::IsMenuRunnerRunningForTesting() const {
  return menu_runner_ ? menu_runner_->IsRunning() : false;
}

ui::MenuModel* BrowserWidget::GetSystemMenuModel() {
  // TODO(b/271137301): Refactor this class to remove chromeos specific code to
  // subclasses.
  if (!menu_model_builder_.get()) {
    menu_model_builder_ = std::make_unique<SystemMenuModelBuilder>(
        browser_view_, browser_view_->browser());
  }
  menu_model_builder_->Init();
  return menu_model_builder_->menu_model();
}

void BrowserWidget::SetTabDragKind(TabDragKind tab_drag_kind) {
  if (tab_drag_kind_ == tab_drag_kind) {
    return;
  }

  if (browser_native_widget_) {
    browser_native_widget_->TabDraggingKindChanged(tab_drag_kind);
  }

  bool was_dragging_any = tab_drag_kind_ != TabDragKind::kNone;
  bool is_dragging_any = tab_drag_kind != TabDragKind::kNone;
  if (was_dragging_any != is_dragging_any) {
    browser_view_->TabDraggingStatusChanged(is_dragging_any);
  }

  tab_drag_kind_ = tab_drag_kind;
}

void BrowserWidget::OnNativeThemeUpdated(ui::NativeTheme* observed_theme) {
  UserChangedTheme(BrowserThemeChangeType::kNativeTheme);
}

ui::ColorProviderKey BrowserWidget::GetColorProviderKey() const {
  auto key = Widget::GetColorProviderKey();

  key.app_controller = browser_view_->browser()->app_controller();

#if BUILDFLAG(IS_CHROMEOS)
  // ChromeOS SystemWebApps use the OS theme all the time.
  if (ash::IsSystemWebApp(browser_view_->browser())) {
    return key;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)

  const auto* theme_service =
      ThemeServiceFactory::GetForProfile(browser_view_->browser()->profile());
  CHECK(theme_service);

  // color_mode.
  [this, &key, theme_service]() {
    // Currently the incognito browser is implemented as unthemed dark mode.
    if (IsIncognitoBrowser()) {
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

  // user_color.
  // Device theme retains the user_color from `Widget`.
  if (!theme_service->UsingDeviceTheme()) {
    if (theme_service->UsingPolicyTheme()) {
      // For policy themes, use the policy color directly since it's not stored
      // in the autogenerated theme preference.
      key.user_color = theme_service->GetPolicyThemeColor();
    } else if (theme_service->UsingAutogeneratedTheme()) {
      key.user_color = theme_service->GetAutogeneratedThemeColor();
    } else if (auto user_color = theme_service->GetUserColor()) {
      key.user_color = user_color;
    }
  }

  // user_color_source.
  if (IsIncognitoBrowser()) {
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

  // scheme_variant.
  ui::mojom::BrowserColorVariant color_variant =
      theme_service->GetBrowserColorVariant();
  if (!theme_service->UsingDeviceTheme() &&
      color_variant != ui::mojom::BrowserColorVariant::kSystem) {
    key.scheme_variant = GetSchemeVariant(color_variant);
  }

  // frame_type.
  const bool use_custom_frame =
      browser_native_widget_ && browser_native_widget_->UseCustomFrame();
  key.frame_type = use_custom_frame ? ui::ColorProviderKey::FrameType::kChromium
                                    : ui::ColorProviderKey::FrameType::kNative;
#if BUILDFLAG(IS_WIN)
  if (theme_service && theme_service->UsingDeviceTheme() && use_custom_frame) {
    key.frame_style = ui::ColorProviderKey::FrameStyle::kSystem;
  }
#endif

  return key;
}

void BrowserWidget::OnMenuClosed() {
  menu_runner_.reset();
}

void BrowserWidget::SelectNativeTheme() {
#if BUILDFLAG(IS_LINUX)
  // Use the regular NativeTheme instance if running incognito mode, regardless
  // of system theme (gtk, qt etc).
  ui::NativeTheme* native_theme = ui::NativeTheme::GetInstanceForNativeUi();
  if (IsIncognitoBrowser()) {
    SetNativeTheme(native_theme);
    return;
  }

  // Ignore the system theme for web apps with window-controls-overlay as the
  // display_override so the web contents can blend with the overlay by using
  // the developer-provided theme color for a better experience. Context:
  // https://crbug.com/1219073.
  const auto* linux_ui_theme =
      ui::LinuxUiTheme::GetForWindow(GetNativeWindow());
  SetNativeTheme(linux_ui_theme &&
                         !browser_view_->AppUsesWindowControlsOverlay()
                     ? linux_ui_theme->GetNativeTheme()
                     : native_theme);
#endif
}

void BrowserWidget::OnTouchUiChanged() {
  client_view()->InvalidateLayout();

  // For standard browser frame, if we do not invalidate the FrameView
  // the client window bounds will not be properly updated which could cause
  // visual artifacts. See crbug.com/1035959 for details.
  if (non_client_view()->frame_view()) {
    // Note that invalidating a view invalidates all of its ancestors, so it is
    // not necessary to also invalidate the NonClientView or RootView here.
    non_client_view()->frame_view()->InvalidateLayout();
  } else {
    non_client_view()->InvalidateLayout();
  }
  GetRootView()->InvalidateLayout();
}

bool BrowserWidget::RegenerateFrameOnThemeChange(
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
    // Calling FrameTypeChanged() may or may not result in an implicit call to
    // ThemeChanged().
    FrameTypeChanged();
    return true;
  }

  return false;
}

bool BrowserWidget::IsIncognitoBrowser() const {
  return browser_view_->browser()->profile()->IsIncognitoProfile();
}
