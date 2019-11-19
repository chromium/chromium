// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/views/frame/browser_frame_mac.h"

#import "base/mac/foundation_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/app_shim/extension_app_shim_handler_mac.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/media/router/media_router_feature.h"
#include "chrome/browser/ui/bookmarks/bookmark_utils.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#import "chrome/browser/ui/cocoa/browser_window_command_handler.h"
#import "chrome/browser/ui/cocoa/chrome_command_dispatcher_delegate.h"
#import "chrome/browser/ui/cocoa/touchbar/browser_window_touch_bar_controller.h"
#include "chrome/browser/ui/views/frame/browser_frame.h"
#include "chrome/browser/ui/views/frame/browser_non_client_frame_view.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#import "components/remote_cocoa/app_shim/window_touch_bar_delegate.h"
#include "components/remote_cocoa/common/application.mojom.h"
#include "components/remote_cocoa/common/native_widget_ns_window.mojom.h"
#include "components/remote_cocoa/common/native_widget_ns_window_host.mojom.h"
#include "components/web_modal/web_contents_modal_dialog_host.h"
#include "content/public/browser/native_web_keyboard_event.h"
#import "ui/base/cocoa/window_size_constants.h"
#include "ui/base/l10n/l10n_util.h"
#import "ui/views/cocoa/native_widget_mac_ns_window_host.h"

namespace {

AppShimHost* GetHostForBrowser(Browser* browser) {
  auto* shim_handler = apps::ExtensionAppShimHandler::Get();
  if (!shim_handler)
    return nullptr;
  return shim_handler->GetHostForBrowser(browser);
}

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
    : views::NativeWidgetMac(browser_frame), browser_view_(browser_view) {}

BrowserFrameMac::~BrowserFrameMac() {
}

BrowserWindowTouchBarController* BrowserFrameMac::GetTouchBarController()
    const {
  return [touch_bar_delegate_ touchBarController];
}

////////////////////////////////////////////////////////////////////////////////
// BrowserFrameMac, views::NativeWidgetMac implementation:

int32_t BrowserFrameMac::SheetOffsetY() {
  // ModalDialogHost::GetDialogPosition() is relative to the host view. In
  // practice, this ends up being the widget's content view.
  web_modal::WebContentsModalDialogHost* dialog_host =
      browser_view_->GetWebContentsModalDialogHost();
  return dialog_host->GetDialogPosition(gfx::Size()).y();
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

void BrowserFrameMac::ValidateUserInterfaceItem(
    int32_t tag,
    remote_cocoa::mojom::ValidateUserInterfaceItemResult* result) {
  Browser* browser = browser_view_->browser();
  if (!chrome::SupportsCommand(browser, tag)) {
    result->enable = false;
    return;
  }

  // Generate return value (enabled state).
  result->enable = chrome::IsCommandEnabled(browser, tag);
  switch (tag) {
    case IDC_CLOSE_TAB:
      // Disable "close tab" if the receiving window is not tabbed.
      // We simply check whether the item has a keyboard shortcut set here;
      // app_controller_mac.mm actually determines whether the item should
      // be enabled.
      result->disable_if_has_no_key_equivalent = true;
      break;
    case IDC_FULLSCREEN: {
      result->new_title.emplace(l10n_util::GetStringUTF16(
          browser->window()->IsFullscreen() ? IDS_EXIT_FULLSCREEN_MAC
                                            : IDS_ENTER_FULLSCREEN_MAC));
      break;
    }
    case IDC_BOOKMARK_THIS_TAB: {
      // Extensions have the ability to hide the bookmark tab menu item.
      // This only affects the bookmark tab menu item under the main menu.
      // The bookmark tab menu item under the app menu has its visibility
      // controlled by AppMenuModel.
      result->new_hidden_state =
          chrome::ShouldRemoveBookmarkThisTabUI(browser->profile());
      break;
    }
    case IDC_BOOKMARK_ALL_TABS: {
      // Extensions have the ability to hide the bookmark all tabs menu
      // item.  This only affects the bookmark page menu item under the main
      // menu.  The bookmark page menu item under the app menu has its
      // visibility controlled by AppMenuModel.
      result->new_hidden_state =
          chrome::ShouldRemoveBookmarkAllTabsUI(browser->profile());
      break;
    }
    case IDC_SHOW_AS_TAB: {
      // Hide this menu option if the window is tabbed or is the devtools
      // window.
      result->new_hidden_state =
          browser->is_type_normal() || browser->is_type_devtools();
      break;
    }
    case IDC_ROUTE_MEDIA: {
      // Hide this menu option if Media Router is disabled.
      result->new_hidden_state =
          !media_router::MediaRouterEnabled(browser->profile());
      break;
    }
    default:
      break;
  }

  // If the item is toggleable, find its toggle state and
  // try to update it.  This is a little awkward, but the alternative is
  // to check after a commandDispatch, which seems worse.
  // On Windows this logic happens in bookmark_bar_view.cc. This simply updates
  // the menu item; it does not display the bookmark bar itself.
  result->set_toggle_state = true;
  switch (tag) {
    default:
      result->set_toggle_state = false;
      break;
    case IDC_SHOW_BOOKMARK_BAR: {
      PrefService* prefs = browser->profile()->GetPrefs();
      result->new_toggle_state =
          prefs->GetBoolean(bookmarks::prefs::kShowBookmarkBar);
      break;
    }
    case IDC_TOGGLE_FULLSCREEN_TOOLBAR: {
      PrefService* prefs = browser->profile()->GetPrefs();
      result->new_toggle_state =
          prefs->GetBoolean(prefs::kShowFullscreenToolbar);
      break;
    }
    case IDC_TOGGLE_JAVASCRIPT_APPLE_EVENTS: {
      PrefService* prefs = browser->profile()->GetPrefs();
      result->new_toggle_state =
          prefs->GetBoolean(prefs::kAllowJavascriptAppleEvents);
      break;
    }
    case IDC_WINDOW_MUTE_SITE: {
      TabStripModel* model = browser->tab_strip_model();
      bool will_mute = model->WillContextMenuMuteSites(model->active_index());
      // Menu items may be validated during browser startup, before the
      // TabStripModel has been populated.
      result->new_toggle_state = !model->empty() && !will_mute;
      break;
    }
    case IDC_WINDOW_PIN_TAB:
      TabStripModel* model = browser->tab_strip_model();
      result->new_toggle_state =
          !model->empty() && !model->WillContextMenuPin(model->active_index());
      break;
  }
}

bool BrowserFrameMac::ExecuteCommand(
    int32_t command,
    WindowOpenDisposition window_open_disposition,
    bool is_before_first_responder) {
  Browser* browser = browser_view_->browser();

  if (is_before_first_responder) {
    // The specification for this private extensions API is incredibly vague.
    // For now, we avoid triggering chrome commands prior to giving the
    // firstResponder a chance to handle the event.
    if (extensions::GlobalShortcutListener::GetInstance()
            ->IsShortcutHandlingSuspended()) {
      return false;
    }

    // If a command is reserved, then we also have it bypass the main menu.
    // This is based on the rough approximation that reserved commands are
    // also the ones that we want to be quickly repeatable.
    // https://crbug.com/836947.
    // The function IsReservedCommandOrKey does not examine its event argument
    // on macOS.
    content::NativeWebKeyboardEvent dummy_event(blink::WebInputEvent::kKeyDown,
                                                0, base::TimeTicks());
    if (!browser->command_controller()->IsReservedCommandOrKey(command,
                                                               dummy_event)) {
      return false;
    }
  }

  chrome::ExecuteCommandWithDisposition(browser, command,
                                        window_open_disposition);
  return true;
}

void BrowserFrameMac::PopulateCreateWindowParams(
    const views::Widget::InitParams& widget_params,
    remote_cocoa::mojom::CreateWindowParams* params) {
  params->style_mask = NSTitledWindowMask | NSClosableWindowMask |
                       NSMiniaturizableWindowMask | NSResizableWindowMask;

  base::scoped_nsobject<NativeWidgetMacNSWindow> ns_window;
  if (browser_view_->IsBrowserTypeNormal() ||
      browser_view_->IsBrowserTypeWebApp()) {
    params->window_class = remote_cocoa::mojom::WindowClass::kBrowser;
    params->style_mask |= NSFullSizeContentViewWindowMask;

    // Ensure tabstrip/profile button are visible.
    params->titlebar_appears_transparent = true;

    // Hosted apps draw their own window title.
    if (browser_view_->IsBrowserTypeWebApp())
      params->window_title_hidden = true;
  } else {
    params->window_class = remote_cocoa::mojom::WindowClass::kDefault;
  }
  params->animation_enabled = true;
}

NativeWidgetMacNSWindow* BrowserFrameMac::CreateNSWindow(
    const remote_cocoa::mojom::CreateWindowParams* params) {
  NativeWidgetMacNSWindow* ns_window = NativeWidgetMac::CreateNSWindow(params);
  if (@available(macOS 10.12.2, *)) {
    touch_bar_delegate_.reset([[BrowserWindowTouchBarViewsDelegate alloc]
        initWithBrowser:browser_view_->browser()
                 window:ns_window]);
    [ns_window setWindowTouchBarDelegate:touch_bar_delegate_.get()];
  }

  return ns_window;
}

remote_cocoa::ApplicationHost*
BrowserFrameMac::GetRemoteCocoaApplicationHost() {
  if (auto* host = GetHostForBrowser(browser_view_->browser()))
    return host->GetRemoteCocoaApplicationHost();
  return nullptr;
}

void BrowserFrameMac::OnWindowInitialized() {
  if (auto* bridge = GetInProcessNSWindowBridge()) {
    bridge->SetCommandDispatcher(
        [[[ChromeCommandDispatcherDelegate alloc] init] autorelease],
        [[[BrowserWindowCommandHandler alloc] init] autorelease]);
  } else {
    if (auto* host = GetHostForBrowser(browser_view_->browser())) {
      host->GetAppShim()->CreateCommandDispatcherForWidget(
          GetNSWindowHost()->bridged_native_widget_id());
    }
  }
}

void BrowserFrameMac::OnWindowDestroying(gfx::NativeWindow native_window) {
  // Clear delegates set in CreateNSWindow() to prevent objects with a reference
  // to |window| attempting to validate commands by looking for a Browser*.
  NativeWidgetMacNSWindow* ns_window =
      base::mac::ObjCCastStrict<NativeWidgetMacNSWindow>(
          native_window.GetNativeNSWindow());
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
  return false;
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
  return GetNSWindowHost()->RedispatchKeyEvent(event.os_event);
}
