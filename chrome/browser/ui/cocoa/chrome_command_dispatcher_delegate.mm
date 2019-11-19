// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/chrome_command_dispatcher_delegate.h"

#include "base/logging.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "components/remote_cocoa/common/native_widget_ns_window_host.mojom.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/base/accelerators/accelerator_manager.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "ui/views/widget/widget.h"

@implementation ChromeCommandDispatcherDelegate

- (BOOL)eventHandledByViewsFocusManager:(NSEvent*)event
                               priority:
                                   (ui::AcceleratorManager::HandlerPriority)
                                       priority {
  NSWindow* window = [event window];
  if (!window)
    return NO;

  // Logic for handling Views windows.
  //
  // There are 3 ways for extensions to register accelerators in Views:
  //  1) As regular extension commands, see ExtensionKeybindingRegistryViews.
  //     This always has high priority.
  //  2) As page/browser popup actions, see
  //     ExtensionActionPlatformDelegateViews. This always has high priority.
  //  3) As a bookmark override. This always has regular priority, and is
  //     actually handled as a special case of the IDC_BOOKMARK_THIS_TAB browser
  //     command. See BookmarkCurrentTabAllowingExtensionOverrides.
  //
  // The only reasonable way to access the registered accelerators for (1) and
  // (2) is to use the FocusManager. That is what we do here. But that will also
  // trigger any other sources of registered accelerators. This is actually
  // desired.
  //
  // Note: FocusManager is also given an opportunity to consume the accelerator
  // in the RenderWidgetHostView event handling path. That logic doesn't trigger
  // when the focused view is not a RenderWidgetHostView, which is why this
  // logic is necessary. Duplicating the logic adds a bit of redundant work,
  // but doesn't cause problems.
  content::NativeWebKeyboardEvent keyboard_event(event);
  ui::Accelerator accelerator =
      ui::GetAcceleratorFromNativeWebKeyboardEvent(keyboard_event);
  auto* bridge =
      remote_cocoa::NativeWidgetNSWindowBridge::GetFromNativeWindow(window);
  bool was_handled = false;
  if (bridge) {
    bridge->host()->HandleAccelerator(
        accelerator,
        priority == ui::AcceleratorManager::HandlerPriority::kHighPriority,
        &was_handled);
  }
  return was_handled;
}

- (ui::PerformKeyEquivalentResult)prePerformKeyEquivalent:(NSEvent*)event
                                                   window:(NSWindow*)window {
  // TODO(erikchen): Detect symbolic hot keys, and force control to be passed
  // back to AppKit so that it can handle it correctly.
  // https://crbug.com/846893.

  NSResponder* responder = [window firstResponder];
  if ([responder conformsToProtocol:@protocol(CommandDispatcherTarget)]) {
    NSObject<CommandDispatcherTarget>* target =
        static_cast<NSObject<CommandDispatcherTarget>*>(responder);
    if ([target isKeyLocked:event]) {
      return ui::PerformKeyEquivalentResult::kUnhandled;
    }
  }

  if ([self eventHandledByViewsFocusManager:event
                                   priority:ui::AcceleratorManager::
                                                kHighPriority]) {
    return ui::PerformKeyEquivalentResult::kHandled;
  }

  // If this keyEquivalent corresponds to a Chrome command, trigger it directly
  // via chrome::ExecuteCommand. We avoid going through the NSMenu for two
  // reasons:
  //  * consistency - some commands are not present in the NSMenu. Furthermore,
  //  the NSMenu's contents can be dynamically updated, so there's no guarantee
  //  that passing the event to NSMenu will even do what we think it will do.
  //  * Avoiding sleeps. By default, the implementation of NSMenu
  //  performKeyEquivalent: has a nested run loop that spins for 100ms. If we
  //  avoid that by spinning our task runner in their private mode, there's a
  //  built in nanosleep. See https://crbug.com/836947#c8.
  //
  // By not passing the event to AppKit, we do lose out on the brief
  // highlighting of the NSMenu.
  CommandForKeyEventResult result = CommandForKeyEvent(event);
  if (result.found()) {
    auto* bridge =
        remote_cocoa::NativeWidgetNSWindowBridge::GetFromNativeWindow(window);
    if (bridge) {
      bool was_executed = false;
      bridge->host()->ExecuteCommand(
          result.chrome_command, WindowOpenDisposition::CURRENT_TAB,
          true /* is_before_first_responder */, &was_executed);
      if (was_executed)
        return ui::PerformKeyEquivalentResult::kHandled;
    }
  }

  return ui::PerformKeyEquivalentResult::kUnhandled;
}

- (ui::PerformKeyEquivalentResult)postPerformKeyEquivalent:(NSEvent*)event
                                                    window:(NSWindow*)window
                                              isRedispatch:(BOOL)isRedispatch {
  if ([self eventHandledByViewsFocusManager:event
                                   priority:ui::AcceleratorManager::
                                                kNormalPriority]) {
    return ui::PerformKeyEquivalentResult::kHandled;
  }

  CommandForKeyEventResult result = CommandForKeyEvent(event);

  if (!result.found() && isRedispatch) {
    result.chrome_command = DelayedWebContentsCommandForKeyEvent(event);
    result.from_main_menu = false;
  }

  if (result.found()) {
    auto* bridge =
        remote_cocoa::NativeWidgetNSWindowBridge::GetFromNativeWindow(window);
    if (bridge) {
      // postPerformKeyEquivalent: is only called on events that are not
      // reserved. We want to bypass the main menu if and only if the event is
      // reserved. As such, we let all events with main menu keyEquivalents be
      // handled by the main menu.
      if (result.from_main_menu) {
        return ui::PerformKeyEquivalentResult::kPassToMainMenu;
      }

      bool was_executed = false;
      bridge->host()->ExecuteCommand(
          result.chrome_command, WindowOpenDisposition::CURRENT_TAB,
          false /* is_before_first_responder */, &was_executed);
      DCHECK(was_executed);
      return ui::PerformKeyEquivalentResult::kHandled;
    }
  }

  return ui::PerformKeyEquivalentResult::kUnhandled;
}

@end  // ChromeCommandDispatchDelegate
