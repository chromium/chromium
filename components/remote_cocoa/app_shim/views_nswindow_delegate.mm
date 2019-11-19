// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/views_nswindow_delegate.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/threading/thread_task_runner_handle.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_host_helper.h"
#include "components/remote_cocoa/common/native_widget_ns_window_host.mojom.h"

@implementation ViewsNSWindowDelegate

- (instancetype)initWithBridgedNativeWidget:
    (remote_cocoa::NativeWidgetNSWindowBridge*)parent {
  DCHECK(parent);
  if ((self = [super init])) {
    parent_ = parent;
  }
  return self;
}

- (NSCursor*)cursor {
  return cursor_.get();
}

- (void)setCursor:(NSCursor*)newCursor {
  if (cursor_.get() == newCursor)
    return;

  cursor_.reset([newCursor retain]);

  // The window has a tracking rect that was installed in -[BridgedContentView
  // initWithView:] that uses the NSTrackingCursorUpdate option. In the case
  // where the window is the key window, that tracking rect will cause
  // -cursorUpdate: to be sent up the responder chain, which will cause the
  // cursor to be set when the message gets to the NativeWidgetMacNSWindow.
  NSWindow* window = parent_->ns_window();
  [window resetCursorRects];

  // However, if this window isn't the key window, that tracking area will have
  // no effect. This is good if this window is just some top-level window that
  // isn't key, but isn't so good if this window isn't key but is a child window
  // of a window that is key. To handle that case, the case where the
  // -cursorUpdate: message will never be sent, just set the cursor here.
  //
  // Only do this for non-key windows so that there will be no flickering
  // between cursors set here and set elsewhere.
  //
  // (This is a known issue; see https://stackoverflow.com/questions/45712066/.)
  if (![window isKeyWindow]) {
    NSWindow* currentWindow = window;
    // Walk up the window chain. If there is a key window in the window parent
    // chain, then work around the issue and set the cursor.
    while (true) {
      NSWindow* parentWindow = [currentWindow parentWindow];
      if (!parentWindow)
        break;
      currentWindow = parentWindow;
      if ([currentWindow isKeyWindow]) {
        [(newCursor ? newCursor : [NSCursor arrowCursor]) set];
        break;
      }
    }
  }
}

- (void)onWindowOrderChanged:(NSNotification*)notification {
  parent_->OnVisibilityChanged();
}

- (void)onSystemControlTintChanged:(NSNotification*)notification {
  parent_->OnSystemControlTintChanged();
}

- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)returnCode
        contextInfo:(void*)contextInfo {
  [sheet orderOut:nil];
  parent_->OnWindowWillClose();
}

// NSWindowDelegate implementation.

- (void)windowDidFailToEnterFullScreen:(NSWindow*)window {
  // Cocoa should already have sent an (unexpected) windowDidExitFullScreen:
  // notification, and the attempt to get back into fullscreen should fail.
  // Nothing to do except verify |parent_| is no longer trying to fullscreen.
  DCHECK(!parent_->target_fullscreen_state());
}

- (void)windowDidFailToExitFullScreen:(NSWindow*)window {
  // Unlike entering fullscreen, windowDidFailToExitFullScreen: is sent *before*
  // windowDidExitFullScreen:. Also, failing to exit fullscreen just dumps the
  // window out of fullscreen without an animation; still sending the expected,
  // windowDidExitFullScreen: notification. So, again, nothing to do here.
  DCHECK(!parent_->target_fullscreen_state());
}

- (void)windowDidResize:(NSNotification*)notification {
  parent_->OnSizeChanged();
}

- (void)windowDidMove:(NSNotification*)notification {
  // Note: windowDidMove: is sent only once at the end of a window drag. There
  // is also windowWillMove: sent at the start, also once. When the window is
  // being moved by the WindowServer live updates are not provided.
  parent_->OnPositionChanged();
}

- (void)windowDidBecomeKey:(NSNotification*)notification {
  parent_->OnWindowKeyStatusChangedTo(true);
}

- (void)windowDidResignKey:(NSNotification*)notification {
  parent_->OnWindowKeyStatusChangedTo(false);
}

- (BOOL)windowShouldClose:(id)sender {
  bool canWindowClose = true;
  parent_->host()->GetCanWindowClose(&canWindowClose);
  return canWindowClose;
}

- (void)windowWillClose:(NSNotification*)notification {
  NSWindow* window = parent_->ns_window();
  if (NSWindow* sheetParent = [window sheetParent]) {
    // On no! Something called -[NSWindow close] on a sheet rather than calling
    // -[NSWindow endSheet:] on its parent. If the modal session is not ended
    // then the parent will never be able to show another sheet. But calling
    // -endSheet: here will block the thread with an animation, so post a task.
    // Use a block: The argument to -endSheet: must be retained, since it's the
    // window that is closing and -performSelector: won't retain the argument
    // (putting |window| on the stack above causes this block to retain it).
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::BindOnce(base::RetainBlock(^{
          [sheetParent endSheet:window];
        })));
  }
  DCHECK([window isEqual:[notification object]]);
  parent_->OnWindowWillClose();
  // |self| may be deleted here (it's NSObject, so who really knows).
  // |parent_| _will_ be deleted for sure.

  // Note OnWindowWillClose() will clear the NSWindow delegate. That is, |self|.
  // That guarantees that the task possibly-posted above will never call into
  // our -sheetDidEnd:. (The task's purpose is just to unblock the modal session
  // on the parent window.)
  DCHECK(![window delegate]);
}

- (void)windowDidMiniaturize:(NSNotification*)notification {
  parent_->host()->OnWindowMiniaturizedChanged(true);
  parent_->OnVisibilityChanged();
}

- (void)windowDidDeminiaturize:(NSNotification*)notification {
  parent_->host()->OnWindowMiniaturizedChanged(false);
  parent_->OnVisibilityChanged();
}

- (void)windowDidChangeBackingProperties:(NSNotification*)notification {
  parent_->OnBackingPropertiesChanged();
}

- (void)windowWillEnterFullScreen:(NSNotification*)notification {
  parent_->OnFullscreenTransitionStart(true);
}

- (void)windowDidEnterFullScreen:(NSNotification*)notification {
  parent_->OnFullscreenTransitionComplete(true);
}

- (void)windowWillExitFullScreen:(NSNotification*)notification {
  parent_->OnFullscreenTransitionStart(false);
}

- (void)windowDidExitFullScreen:(NSNotification*)notification {
  if (base::mac::IsOS10_12()) {
    // There is a window activation/fullscreen bug present only in macOS 10.12
    // that might cause a security surface to appear over the wrong parent
    // window. As much as this code appears to be a no-op, it is not; it causes
    // AppKit to shuffle all the windows around to properly obey the
    // relationships that they should already be obeying.
    [[NSApp orderedWindows][0] performSelector:@selector(orderFront:)
                                    withObject:self
                                    afterDelay:0];
  }

  parent_->OnFullscreenTransitionComplete(false);
}

// Allow non-resizable windows (without NSResizableWindowMask) to fill the
// screen in fullscreen mode. This only happens when
// -[NSWindow toggleFullscreen:] is called since non-resizable windows have no
// fullscreen button. Without this they would only enter fullscreen at their
// current size.
- (NSSize)window:(NSWindow*)window
    willUseFullScreenContentSize:(NSSize)proposedSize {
  return proposedSize;
}

// Override to correctly position modal dialogs.
- (NSRect)window:(NSWindow*)window
    willPositionSheet:(NSWindow*)sheet
            usingRect:(NSRect)defaultSheetLocation {
  int32_t sheetPositionY = 0;
  parent_->host()->GetSheetOffsetY(&sheetPositionY);
  NSView* view = [window contentView];
  NSPoint pointInView = NSMakePoint(0, NSMaxY([view bounds]) - sheetPositionY);
  NSPoint pointInWindow = [view convertPoint:pointInView toView:nil];

  // As per NSWindowDelegate documentation, the origin indicates the top left
  // point of the host frame in window coordinates. The width changes the
  // animation from vertical to trapezoid if it is smaller than the width of the
  // dialog. The height is ignored but should be set to zero.
  return NSMakeRect(0, pointInWindow.y, NSWidth(defaultSheetLocation), 0);
}

@end
