// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/frame/browser_frame.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/debug/leak_annotations.h"
#include "base/i18n/rtl.h"
#include "build/build_config.h"
#include "chrome/browser/app_mode/app_mode_utils.h"
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
#include "chrome/common/chrome_switches.h"
#include "ui/base/hit_test.h"
#include "ui/base/material_design/material_design_controller.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/font_list.h"
#include "ui/native_theme/native_theme_dark_aura.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/widget/native_widget.h"

#if defined(OS_CHROMEOS)
#include "components/user_manager/user_manager.h"
#endif

#if defined(USE_X11)
#include "ui/views/widget/desktop_aura/x11_desktop_handler.h"
#endif

namespace {

bool IsUsingGtkTheme(Profile* profile) {
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  return ThemeServiceFactory::GetForProfile(profile)->UsingSystemTheme();
#else
  return false;
#endif
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
  md_observer_.Add(ui::MaterialDesignController::GetInstance());
}

BrowserFrame::~BrowserFrame() {}

void BrowserFrame::InitBrowserFrame() {
  native_browser_frame_ =
      NativeBrowserFrameFactory::CreateNativeBrowserFrame(this, browser_view_);
  views::Widget::InitParams params = native_browser_frame_->GetWidgetParams();
  params.name = "BrowserFrame";
  params.delegate = browser_view_;
  if (browser_view_->browser()->is_type_normal() ||
      browser_view_->browser()->is_type_devtools()) {
    // Typed panel/popup can only return a size once the widget has been
    // created.
    // DevTools counts as a popup, but DevToolsWindow::CreateDevToolsBrowser
    // ensures there is always a size available. Without this, the tools
    // launch on the wrong display and can have sizing issues when
    // repositioned to the saved bounds in Widget::SetInitialBounds.
    chrome::GetSavedWindowBoundsAndShowState(browser_view_->browser(),
                                             &params.bounds,
                                             &params.show_state);

    params.workspace = browser_view_->browser()->initial_workspace();
    const base::CommandLine& parsed_command_line =
        *base::CommandLine::ForCurrentProcess();

    if (parsed_command_line.HasSwitch(switches::kWindowWorkspace)) {
      params.workspace =
          parsed_command_line.GetSwitchValueASCII(switches::kWindowWorkspace);
    }
  }

  Init(std::move(params));

  if (!native_browser_frame_->UsesNativeSystemMenu()) {
    DCHECK(non_client_view());
    non_client_view()->set_context_menu_controller(this);
  }
}

int BrowserFrame::GetMinimizeButtonOffset() const {
  return native_browser_frame_->GetMinimizeButtonOffset();
}

gfx::Rect BrowserFrame::GetBoundsForTabStripRegion(
    const views::View* tabstrip) const {
  // This can be invoked before |browser_frame_view_| has been set.
  return browser_frame_view_
             ? browser_frame_view_->GetBoundsForTabStripRegion(tabstrip)
             : gfx::Rect();
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

bool BrowserFrame::ShouldUseTheme() const {
  // Browser windows are always themed (including popups).
  if (!web_app::AppBrowserController::IsForWebAppBrowser(
          browser_view_->browser())) {
    return true;
  }

  // The system GTK theme should always be respected if the user has opted to
  // use it.
  if (IsUsingGtkTheme(browser_view_->browser()->profile()))
    return true;

  // Hosted apps on non-GTK use default colors.
  return false;
}

///////////////////////////////////////////////////////////////////////////////
// BrowserFrame, views::Widget overrides:

views::internal::RootView* BrowserFrame::CreateRootView() {
  root_view_ = new BrowserRootView(browser_view_, this);
  return root_view_;
}

views::NonClientFrameView* BrowserFrame::CreateNonClientFrameView() {
  browser_frame_view_ =
      chrome::CreateBrowserNonClientFrameView(this, browser_view_);
  return browser_frame_view_;
}

bool BrowserFrame::GetAccelerator(int command_id,
                                  ui::Accelerator* accelerator) const {
  return browser_view_->GetAccelerator(command_id, accelerator);
}

const ui::ThemeProvider* BrowserFrame::GetThemeProvider() const {
  Browser* browser = browser_view_->browser();
  Profile* profile = browser->profile();
  return ShouldUseTheme()
             ? &ThemeService::GetThemeProviderForProfile(profile)
             : &ThemeService::GetDefaultThemeProviderForProfile(profile);
}

const ui::NativeTheme* BrowserFrame::GetNativeTheme() const {
  if (browser_view_->browser()->profile()->IsIncognitoProfile() &&
      ThemeServiceFactory::GetForProfile(browser_view_->browser()->profile())
          ->UsingDefaultTheme()) {
    return ui::NativeThemeDarkAura::instance();
  }
  return views::Widget::GetNativeTheme();
}

void BrowserFrame::OnNativeWidgetWorkspaceChanged() {
  chrome::SaveWindowWorkspace(browser_view_->browser(), GetWorkspace());
#if defined(USE_X11)
  BrowserList::MoveBrowsersInWorkspaceToFront(
      views::X11DesktopHandler::get()->GetWorkspace());
#endif
  Widget::OnNativeWidgetWorkspaceChanged();
}

void BrowserFrame::PropagateNativeThemeChanged() {
  // Instead of immediately propagating the native theme change to the root
  // view, use BrowserView's custom handling, which may regenerate the frame
  // first, then propgate the theme change to the root view automatically.
  browser_view_->UserChangedTheme(BrowserThemeChangeType::kNativeTheme);
}

void BrowserFrame::ShowContextMenuForViewImpl(views::View* source,
                                              const gfx::Point& p,
                                              ui::MenuSourceType source_type) {
  if (chrome::IsRunningInForcedAppMode())
    return;

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

ui::MenuModel* BrowserFrame::GetSystemMenuModel() {
#if defined(OS_CHROMEOS)
  if (user_manager::UserManager::IsInitialized() &&
      user_manager::UserManager::Get()->GetLoggedInUsers().size() > 1) {
    // In Multi user mode, the number of users as well as the order of users
    // can change. Coming here we have more than one user and since the menu
    // model contains the user information, it must get updated to show any
    // changes happened since the last invocation.
    menu_model_builder_.reset();
  }
#endif
  if (!menu_model_builder_.get()) {
    menu_model_builder_.reset(
        new SystemMenuModelBuilder(browser_view_, browser_view_->browser()));
    menu_model_builder_->Init();
  }
  return menu_model_builder_->menu_model();
}

void BrowserFrame::OnMenuClosed() {
  menu_runner_.reset();
}

void BrowserFrame::OnTouchUiChanged() {
  client_view()->InvalidateLayout();
  non_client_view()->InvalidateLayout();
  GetRootView()->Layout();
}
