// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/chrome_command_dispatcher_delegate.h"

#include "base/logging.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/extensions/global_shortcut_listener.h"
#include "chrome/browser/global_keyboard_shortcuts_mac.h"
#include "chrome/browser/ui/browser_command_controller.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "ui/content_accelerators/accelerator_util.h"
#include "ui/views/widget/widget.h"

@implementation ChromeCommandDispatcherDelegate

- (void)executeChromeCommandBypassingMainMenu:(int)command
                                      browser:(Browser*)browser {
  chrome::ExecuteCommand(browser, command);
}

- (BOOL)eventHandledByExtensionCommand:(NSEvent*)event
                              priority:(ui::AcceleratorManager::HandlerPriority)
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
  //     actually handled as a special case of the IDC_BOOKMARK_PAGE browser
  //     command. See BookmarkCurrentPageAllowingExtensionOverrides.
  //
  // The only reasonable way to access the registered accelerators for (1) and
  // (2) is to use the FocusManager. That is what we do here. But that will also
  // trigger any other sources of registered accelerators. This is actually
  // desired.
  //
  // TODO(erikchen): Once we no longer support Cocoa, we should rename this
  // method to be eventHandledByViewsFocusManager.
  //
  // Note: FocusManager is also given an opportunity to consume the accelerator
  // in the RenderWidgetHostView event handling path. That logic doesn't trigger
  // when the focused view is not a RenderWidgetHostView, which is why this
  // logic is necessary. Duplicating the logic adds a bit of redundant work,
  // but doesn't cause problems.
  content::NativeWebKeyboardEvent keyboard_event(event);
  ui::Accelerator accelerator =
      ui::GetAcceleratorFromNativeWebKeyboardEvent(keyboard_event);
  if (views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window)) {
    if (priority == ui::AcceleratorManager::HandlerPriority::kHighPriority) {
      if (!widget->GetFocusManager()->HasPriorityHandler(accelerator)) {
        return NO;
      }
    }
    return widget->GetFocusManager()->ProcessAccelerator(accelerator);
  }

  return NO;
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
    if ([target isKeyLocked:event])
      return ui::PerformKeyEquivalentResult::kUnhandled;
  }

  if ([self eventHandledByExtensionCommand:event
                                  priority:ui::AcceleratorManager::
                                               kHighPriority]) {
    return ui::PerformKeyEquivalentResult::kHandled;
  }

  // The specification for this private extensions API is incredibly vague. For
  // now, we avoid triggering chrome commands prior to giving the firstResponder
  // a chance to handle the event.
  if (extensions::GlobalShortcutListener::GetInstance()
          ->IsShortcutHandlingSuspended()) {
    return ui::PerformKeyEquivalentResult::kUnhandled;
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
    Browser* browser = chrome::FindBrowserWithWindow(window);
    if (browser &&
        browser->command_controller()->IsReservedCommandOrKey(
            result.chrome_command, content::NativeWebKeyboardEvent(event))) {
      // If a command is reserved, then we also have it bypass the main menu.
      // This is based on the rough approximation that reserved commands are
      // also the ones that we want to be quickly repeatable.
      // https://crbug.com/836947.
      chrome::ExecuteCommand(browser, result.chrome_command);
      return ui::PerformKeyEquivalentResult::kHandled;
    }
  }

  return ui::PerformKeyEquivalentResult::kUnhandled;
}

- (ui::PerformKeyEquivalentResult)postPerformKeyEquivalent:(NSEvent*)event
                                                    window:(NSWindow*)window
                                              isRedispatch:(BOOL)isRedispatch {
  if ([self eventHandledByExtensionCommand:event
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
    Browser* browser = chrome::FindBrowserWithWindow(window);
    if (browser) {
      // postPerformKeyEquivalent: is only called on events that are not
      // reserved. We want to bypass the main menu if and only if the event is
      // reserved. As such, we let all events with main menu keyEquivalents be
      // handled by the main menu.
      if (result.from_main_menu) {
        return ui::PerformKeyEquivalentResult::kPassToMainMenu;
      }

      chrome::ExecuteCommand(browser, result.chrome_command);
      return ui::PerformKeyEquivalentResult::kHandled;
    }
  }

  return ui::PerformKeyEquivalentResult::kUnhandled;
}

@end  // ChromeCommandDispatchDelegate
