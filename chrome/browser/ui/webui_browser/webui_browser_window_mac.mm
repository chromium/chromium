// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui_browser/webui_browser_window.h"

#import <AppKit/AppKit.h>

#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/apps/app_shim/app_shim_host_mac.h"
#include "chrome/browser/apps/app_shim/app_shim_manager_mac.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#import "chrome/browser/ui/cocoa/browser_window_command_handler.h"
#import "chrome/browser/ui/cocoa/chrome_command_dispatcher_delegate.h"
#include "components/input/native_web_keyboard_event.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "ui/base/accelerators/global_accelerator_listener/global_accelerator_listener.h"
#include "ui/views/cocoa/native_widget_mac_ns_window_host.h"
#include "ui/views/widget/native_widget_mac.h"

#if !BUILDFLAG(IS_MAC)
#error "This file is macOS-only and should not be included on other platforms."
#endif

namespace {

AppShimHost* GetHostForBrowser(Browser* browser) {
  auto* const shim_manager = apps::AppShimManager::Get();
  if (!shim_manager) {
    return nullptr;
  }
  return shim_manager->GetHostForRemoteCocoaBrowser(browser);
}

bool ShouldHandleKeyboardEvent(const input::NativeWebKeyboardEvent& event) {
  // |event.skip_if_unhandled| is true when it shouldn't be handled by the
  // browser if it was ignored by the renderer. See http://crbug.com/25000.
  if (event.skip_if_unhandled) {
    return false;
  }

  // Ignore synthesized keyboard events. See http://crbug.com/23221.
  if (event.GetType() == input::NativeWebKeyboardEvent::Type::kChar) {
    return false;
  }

  // Do not fire shortcuts on key up, and only forward shortcuts if we have an
  // underlying os event.
  return event.os_event && event.os_event.Get().type == NSEventTypeKeyDown;
}

}  // namespace

bool WebUIBrowserWindow::HandleKeyboardEvent(
    const input::NativeWebKeyboardEvent& event) {
  if (!ShouldHandleKeyboardEvent(event)) {
    return false;
  }

  // On Mac keyboard shortcuts are caught by the menu entries, so the keyboard
  // shortcut events must be redispatched to the system so properly configured
  // menu items can catch the keyboard shortcuts.
  return views::NativeWidgetMacNSWindowHost::GetFromNativeWindow(
             GetNativeWindow())
      ->RedispatchKeyEvent(event.os_event.Get());
}

// Note that the logic here is often derived from BrowserNativeWidgetMac.
class WebUIBrowserNativeWidgetMac : public views::NativeWidgetMac {
 public:
  WebUIBrowserNativeWidgetMac(Browser* browser, views::Widget* widget)
      : NativeWidgetMac(widget), browser_(browser) {}

 private:
  // views::NativeWidgetMac implementation:
  void ValidateUserInterfaceItem(
      int32_t command,
      remote_cocoa::mojom::ValidateUserInterfaceItemResult* result) override {
    // This allows menu items like "Close Tabs" to be enabled when the browser
    // has open tabs, which in turn enables the "Close Tabs" keyboard shortcut.
    result->enable = chrome::IsCommandEnabled(browser_, command);
  }

  // For our ValidateUserInterfaceItem() to be called we must register the
  // CommandDispatcher.
  void OnWindowInitialized() override {
    if (auto* bridge = GetInProcessNSWindowBridge()) {
      bridge->SetCommandDispatcher(
          [[ChromeCommandDispatcherDelegate alloc] init],
          [[BrowserWindowCommandHandler alloc] init]);
    } else {
      if (auto* host = GetHostForBrowser(browser_)) {
        host->GetAppShim()->CreateCommandDispatcherForWidget(
            GetNSWindowHost()->bridged_native_widget_id());
      }
    }
  }

  bool WillExecuteCommand(int32_t command,
                          WindowOpenDisposition window_open_disposition,
                          bool is_before_first_responder) override {
    if (is_before_first_responder) {
      // The specification for this private extensions API is incredibly vague.
      // For now, we avoid triggering chrome commands prior to giving the
      // firstResponder a chance to handle the event.
      if (ui::GlobalAcceleratorListener::GetInstance() &&
          ui::GlobalAcceleratorListener::GetInstance()
              ->IsShortcutHandlingSuspended()) {
        return false;
      }

      // If a command is reserved, then we also have it bypass the main menu.
      // This is based on the rough approximation that reserved commands are
      // also the ones that we want to be quickly repeatable.
      // https://crbug.com/836947.
      // The function IsReservedCommandOrKey does not examine its event argument
      // on macOS.
      input::NativeWebKeyboardEvent dummy_event(
          blink::WebInputEvent::Type::kKeyDown, 0, base::TimeTicks());
      if (!browser_->command_controller()->IsReservedCommandOrKey(
              command, dummy_event)) {
        return false;
      }
    }

    return true;
  }

  bool ExecuteCommand(int32_t command,
                      WindowOpenDisposition window_open_disposition,
                      bool is_before_first_responder) override {
    if (!WillExecuteCommand(command, window_open_disposition,
                            is_before_first_responder)) {
      return false;
    }

    // Actually Execute the specific command with |browser_|.
    // This facilitates commands initiated from keyboard shortcuts.
    chrome::ExecuteCommandWithDisposition(browser_, command,
                                          window_open_disposition);
    return true;
  }

  // views::NativeWidget implementation:
  views::internal::NativeWidgetPrivate* AsNativeWidgetPrivate() override {
    return this;
  }

  void PopulateCreateWindowParams(
      const views::Widget::InitParams& widget_params,
      remote_cocoa::mojom::CreateWindowParams* params) override {
    // Loosely based on BrowserNativeWidgetMac::PopulateCreateWindowParams().
    params->style_mask = NSWindowStyleMaskTitled | NSWindowStyleMaskClosable |
                         NSWindowStyleMaskMiniaturizable |
                         NSWindowStyleMaskResizable |
                         NSWindowStyleMaskFullSizeContentView;
    params->window_class = remote_cocoa::mojom::WindowClass::kBrowser;
    // Ensure tabstrip/profile button are visible.
    params->titlebar_appears_transparent = true;
    params->window_title_hidden = true;
    params->animation_enabled = true;
  }

  raw_ptr<Browser> browser_;
};

views::NativeWidget* WebUIBrowserWindow::CreateNativeWidget() {
  return new WebUIBrowserNativeWidgetMac(browser_.get(), widget_.get());
}
