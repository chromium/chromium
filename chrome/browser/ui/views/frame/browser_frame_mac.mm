// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/frame/browser_frame_mac.h"

#import "base/mac/foundation_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#import "chrome/browser/ui/cocoa/browser_window_command_handler.h"
#import "chrome/browser/ui/cocoa/chrome_command_dispatcher_delegate.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_touch_bar_controller.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/native_web_keyboard_event.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/views_bridge_mac/mojo/bridged_native_widget.mojom.h"
#import "ui/views_bridge_mac/native_widget_mac_nswindow.h"
#import "ui/views_bridge_mac/window_touch_bar_delegate.h"

namespace {

bool ShouldHandleKeyboardEvent(const content::NativeWebKeyboardEvent& event) {
  // |event.skip_in_browser| is true when it shouldn't be handled by the browser
  // if it was ignored by the renderer. See http://crbug.com/25000.
  if (event.skip_in_browser)
    return false;

  // Ignore synthesized keyboard events. See http://crbug.com/23221.
  if (event.GetType() == content::NativeWebKeyboardEvent::kChar)
    return false;

  // If the event was not synthesized it should have an os_event.
  DCHECK(event.os_event);

  // Do not fire shortcuts on key up.
  return [event.os_event type] == NSKeyDown;
}

}  // namespace

// Bridge Obj-C class for WindowTouchBarDelegate and
// BrowserWindowTouchBarController.
API_AVAILABLE(macos(10.12.2))
@interface BrowserWindowTouchBarViewsDelegate
    : NSObject<WindowTouchBarDelegate> {
  Browser* browser_;  // Weak.
  NSWindow* window_;  // Weak.
  base::scoped_nsobject<BrowserWindowTouchBarController> touchBarController_;
}

- (BrowserWindowTouchBarController*)touchBarController;

@end

@implementation BrowserWindowTouchBarViewsDelegate

- (instancetype)initWithBrowser:(Browser*)browser window:(NSWindow*)window {
  if ((self = [super init])) {
    browser_ = browser;
    window_ = window;
  }

  return self;
}

- (BrowserWindowTouchBarController*)touchBarController {
  return touchBarController_.get();
}

- (NSTouchBar*)makeTouchBar API_AVAILABLE(macos(10.12.2)) {
  if (!touchBarController_) {
    touchBarController_.reset([[BrowserWindowTouchBarController alloc]
        initWithBrowser:browser_
                 window:window_]);
  }
  return [touchBarController_ makeTouchBar];
}

@end

BrowserFrameMac::BrowserFrameMac(BrowserFrame* browser_frame,
                                 BrowserView* browser_view)
    : views::NativeWidgetMac(browser_frame),
      browser_view_(browser_view),
      command_dispatcher_delegate_(
          [[ChromeCommandDispatcherDelegate alloc] init]) {}

BrowserFrameMac::~BrowserFrameMac() {
}

BrowserWindowTouchBarController* BrowserFrameMac::GetTouchBarController()
    const {
  return [touch_bar_delegate_ touchBarController];
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameMac, views::NativeWidgetMac implementation:

int BrowserFrameMac::SheetPositionY() {
  web_modal::WebContentsModalDialogHost* dialog_host =
      browser_view_->GetWebContentsModalDialogHost();
  NSView* view = dialog_host->GetHostView().GetNativeNSView();
  // Get the position of the host view relative to the window since
  // ModalDialogHost::GetDialogPosition() is relative to the host view.
  int host_view_y =
      [view convertPoint:NSMakePoint(0, NSHeight([view frame])) toView:nil].y;
  return host_view_y - dialog_host->GetDialogPosition(gfx::Size()).y();
}

void BrowserFrameMac::GetWindowFrameTitlebarHeight(
    bool* override_titlebar_height,
    float* titlebar_height) {
  if (browser_view_ && browser_view_->frame() &&
      browser_view_->frame()->GetFrameView()) {
    *override_titlebar_height = true;
    *titlebar_height =
        browser_view_->GetTabStripHeight() +
        browser_view_->frame()->GetFrameView()->GetTopInset(true);
  } else {
    *override_titlebar_height = false;
    *titlebar_height = 0;
  }
}

void BrowserFrameMac::OnFocusWindowToolbar() {
  chrome::ExecuteCommand(browser_view_->browser(), IDC_FOCUS_TOOLBAR);
}

void BrowserFrameMac::OnWindowFullscreenStateChange() {
  browser_view_->FullscreenStateChanged();
}

void BrowserFrameMac::InitNativeWidget(
    const views::Widget::InitParams& params) {
  views::NativeWidgetMac::InitNativeWidget(params);

  [[GetNativeWindow().GetNativeNSWindow() contentView] setWantsLayer:YES];
}

void BrowserFrameMac::PopulateCreateWindowParams(
    const views::Widget::InitParams& widget_params,
    views_bridge_mac::mojom::CreateWindowParams* params) {
  params->style_mask = NSTitledWindowMask | NSClosableWindowMask |
                       NSMiniaturizableWindowMask | NSResizableWindowMask;

  base::scoped_nsobject<NativeWidgetMacNSWindow> ns_window;
  if (browser_view_->IsBrowserTypeNormal() ||
      browser_view_->IsBrowserTypeHostedApp()) {
    params->window_class = views_bridge_mac::mojom::WindowClass::kBrowser;

    if (@available(macOS 10.10, *))
      params->style_mask |= NSFullSizeContentViewWindowMask;

    // Ensure tabstrip/profile button are visible.
    params->titlebar_appears_transparent = true;

    // Hosted apps draw their own window title.
    if (browser_view_->IsBrowserTypeHostedApp())
      params->window_title_hidden = true;
  } else {
    params->window_class = views_bridge_mac::mojom::WindowClass::kDefault;
  }
  params->animation_enabled = true;
}

NativeWidgetMacNSWindow* BrowserFrameMac::CreateNSWindow(
    const views_bridge_mac::mojom::CreateWindowParams* params) {
  NativeWidgetMacNSWindow* ns_window = NativeWidgetMac::CreateNSWindow(params);
  // TODO(ccameron): Window-level hotkeys need to be wired up across processes.
  // https://crbug.com/895168
  [ns_window setCommandDispatcherDelegate:command_dispatcher_delegate_];
  [ns_window setCommandHandler:[[[BrowserWindowCommandHandler alloc] init]
                                   autorelease]];

  if (@available(macOS 10.12.2, *)) {
    touch_bar_delegate_.reset([[BrowserWindowTouchBarViewsDelegate alloc]
        initWithBrowser:browser_view_->browser()
                 window:ns_window]);
    [ns_window setWindowTouchBarDelegate:touch_bar_delegate_.get()];
  }

  return ns_window;
}

views::BridgeFactoryHost* BrowserFrameMac::GetBridgeFactoryHost() {
  auto* shim_handler = apps::ExtensionAppShimHandler::Get();
  if (shim_handler) {
    apps::AppShimHandler::Host* host =
        shim_handler->FindHostForBrowser(browser_view_->browser());
    if (host)
      return host->GetViewsBridgeFactoryHost();
  }
  return nullptr;
}

void BrowserFrameMac::OnWindowDestroying(gfx::NativeWindow native_window) {
  // Clear delegates set in CreateNSWindow() to prevent objects with a reference
  // to |window| attempting to validate commands by looking for a Browser*.
  NativeWidgetMacNSWindow* ns_window =
      base::mac::ObjCCastStrict<NativeWidgetMacNSWindow>(
          native_window.GetNativeNSWindow());
  [ns_window setCommandHandler:nil];
  [ns_window setCommandDispatcherDelegate:nil];
  [ns_window setWindowTouchBarDelegate:nil];
}

int BrowserFrameMac::GetMinimizeButtonOffset() const {
  NOTIMPLEMENTED();
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameMac, NativeBrowserFrame implementation:

views::Widget::InitParams BrowserFrameMac::GetWidgetParams() {
  views::Widget::InitParams params;
  params.native_widget = this;
  return params;
}

bool BrowserFrameMac::UseCustomFrame() const {
  return false;
}

bool BrowserFrameMac::UsesNativeSystemMenu() const {
  return true;
}

bool BrowserFrameMac::ShouldSaveWindowPlacement() const {
  return true;
}

void BrowserFrameMac::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  return NativeWidgetMac::GetWindowPlacement(bounds, show_state);
}

content::KeyboardEventProcessingResult BrowserFrameMac::PreHandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  // On macOS, all keyEquivalents that use modifier keys are handled by
  // -[CommandDispatcher performKeyEquivalent:]. If this logic is being hit,
  // it means that the event was not handled, so we must return either
  // NOT_HANDLED or NOT_HANDLED_IS_SHORTCUT.
  if (EventUsesPerformKeyEquivalent(event.os_event)) {
    int command_id = CommandForKeyEvent(event.os_event).chrome_command;
    if (command_id == -1)
      command_id = DelayedWebContentsCommandForKeyEvent(event.os_event);
    if (command_id != -1)
      return content::KeyboardEventProcessingResult::NOT_HANDLED_IS_SHORTCUT;
  }

  return content::KeyboardEventProcessingResult::NOT_HANDLED;
}

bool BrowserFrameMac::HandleKeyboardEvent(
    const content::NativeWebKeyboardEvent& event) {
  if (!ShouldHandleKeyboardEvent(event))
    return false;

  // Redispatch the event. If it's a keyEquivalent:, this gives
  // CommandDispatcher the opportunity to finish passing the event to consumers.
  return RedispatchKeyEvent(event.os_event);
}
