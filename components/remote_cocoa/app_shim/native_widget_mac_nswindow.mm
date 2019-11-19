// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"

#include "base/mac/foundation_util.h"
#import "base/mac/sdk_forward_declarations.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_host_helper.h"
#import "components/remote_cocoa/app_shim/views_nswindow_delegate.h"
#import "components/remote_cocoa/app_shim/window_touch_bar_delegate.h"
#include "components/remote_cocoa/common/native_widget_ns_window_host.mojom.h"
#import "ui/base/cocoa/user_interface_item_command_handler.h"
#import "ui/base/cocoa/window_size_constants.h"

@interface NSWindow (Private)
+ (Class)frameViewClassForStyleMask:(NSWindowStyleMask)windowStyle;
- (BOOL)hasKeyAppearance;
- (long long)_resizeDirectionForMouseLocation:(CGPoint)location;
- (BOOL)_isConsideredOpenForPersistentState;

// Available in later point releases of 10.10. On 10.11+, use the public
// -performWindowDragWithEvent: instead.
- (void)beginWindowDragWithEvent:(NSEvent*)event;
@end

@interface NativeWidgetMacNSWindow () <NSKeyedArchiverDelegate>
- (ViewsNSWindowDelegate*)viewsNSWindowDelegate;
- (BOOL)hasViewsMenuActive;
- (id<NSAccessibility>)rootAccessibilityObject;

// Private API on NSWindow, determines whether the title is drawn on the title
// bar. The title is still visible in menus, Expose, etc.
- (BOOL)_isTitleHidden;
@end

// Use this category to implement mouseDown: on multiple frame view classes
// with different superclasses.
@interface NSView (CRFrameViewAdditions)
- (void)cr_mouseDownOnFrameView:(NSEvent*)event;
@end

@implementation NSView (CRFrameViewAdditions)
// If a mouseDown: falls through to the frame view, turn it into a window drag.
- (void)cr_mouseDownOnFrameView:(NSEvent*)event {
  if ([self.window _resizeDirectionForMouseLocation:event.locationInWindow] !=
      -1)
    return;
  if (@available(macOS 10.11, *))
    [self.window performWindowDragWithEvent:event];
  else if ([self.window
               respondsToSelector:@selector(beginWindowDragWithEvent:)])
    [self.window beginWindowDragWithEvent:event];
  else
    NOTREACHED();
}
@end

@implementation NativeWidgetMacNSWindowTitledFrame
- (void)mouseDown:(NSEvent*)event {
  if (self.window.isMovable)
    [self cr_mouseDownOnFrameView:event];
  [super mouseDown:event];
}
- (BOOL)usesCustomDrawing {
  return NO;
}
@end

@implementation NativeWidgetMacNSWindowBorderlessFrame
- (void)mouseDown:(NSEvent*)event {
  [self cr_mouseDownOnFrameView:event];
  [super mouseDown:event];
}
- (BOOL)usesCustomDrawing {
  return NO;
}
@end

@implementation NativeWidgetMacNSWindow {
 @private
  base::scoped_nsobject<CommandDispatcher> commandDispatcher_;
  base::scoped_nsprotocol<id<UserInterfaceItemCommandHandler>> commandHandler_;
  id<WindowTouchBarDelegate> touchBarDelegate_;  // Weak.
  uint64_t bridgedNativeWidgetId_;
  remote_cocoa::NativeWidgetNSWindowBridge* bridge_;
  BOOL willUpdateRestorableState_;
}
@synthesize bridgedNativeWidgetId = bridgedNativeWidgetId_;
@synthesize bridge = bridge_;

- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSUInteger)windowStyle
                            backing:(NSBackingStoreType)bufferingType
                              defer:(BOOL)deferCreation {
  DCHECK(NSEqualRects(contentRect, ui::kWindowSizeDeterminedLater));
  if ((self = [super initWithContentRect:ui::kWindowSizeDeterminedLater
                               styleMask:windowStyle
                                 backing:bufferingType
                                   defer:deferCreation])) {
    commandDispatcher_.reset([[CommandDispatcher alloc] initWithOwner:self]);
  }
  return self;
}

// This override helps diagnose lifetime issues in crash stacktraces by
// inserting a symbol on NativeWidgetMacNSWindow and should be kept even if it
// does nothing.
- (void)dealloc {
  willUpdateRestorableState_ = YES;
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  [super dealloc];
}

// Public methods.

- (void)setCommandDispatcherDelegate:(id<CommandDispatcherDelegate>)delegate {
  [commandDispatcher_ setDelegate:delegate];
}

- (void)sheetDidEnd:(NSWindow*)sheet
         returnCode:(NSInteger)returnCode
        contextInfo:(void*)contextInfo {
  // Note NativeWidgetNSWindowBridge may have cleared [self delegate], in which
  // case this will no-op. This indirection is necessary to handle AppKit
  // invoking this selector via a posted task. See https://crbug.com/851376.
  [[self viewsNSWindowDelegate] sheetDidEnd:sheet
                                 returnCode:returnCode
                                contextInfo:contextInfo];
}

- (void)setWindowTouchBarDelegate:(id<WindowTouchBarDelegate>)delegate {
  touchBarDelegate_ = delegate;
}

// Private methods.

- (ViewsNSWindowDelegate*)viewsNSWindowDelegate {
  return base::mac::ObjCCastStrict<ViewsNSWindowDelegate>([self delegate]);
}

- (BOOL)hasViewsMenuActive {
  bool hasMenuController = false;
  if (bridge_)
    bridge_->host()->GetHasMenuController(&hasMenuController);
  return hasMenuController;
}

- (id<NSAccessibility>)rootAccessibilityObject {
  id<NSAccessibility> obj =
      bridge_ ? bridge_->host_helper()->GetNativeViewAccessible() : nil;
  // We should like to DCHECK that the object returned implemements the
  // NSAccessibility protocol, but the NSAccessibilityRemoteUIElement interface
  // does not conform.
  // TODO(https://crbug.com/944698): Create a sub-class that does.
  return obj;
}

// NSWindow overrides.

+ (Class)frameViewClassForStyleMask:(NSWindowStyleMask)windowStyle {
  if (windowStyle & NSWindowStyleMaskTitled) {
    if (Class customFrame = [NativeWidgetMacNSWindowTitledFrame class])
      return customFrame;
  } else if (Class customFrame =
                 [NativeWidgetMacNSWindowBorderlessFrame class]) {
    return customFrame;
  }
  return [super frameViewClassForStyleMask:windowStyle];
}

- (BOOL)_isTitleHidden {
  bool shouldShowWindowTitle = YES;
  if (bridge_)
    bridge_->host()->GetShouldShowWindowTitle(&shouldShowWindowTitle);
  return !shouldShowWindowTitle;
}

// The base implementation returns YES if the window's frame view is a custom
// class, which causes undesirable changes in behavior. AppKit NSWindow
// subclasses are known to override it and return NO.
- (BOOL)_usesCustomDrawing {
  return NO;
}

// Ignore [super canBecome{Key,Main}Window]. The default is NO for windows with
// NSBorderlessWindowMask, which is not the desired behavior.
// Note these can be called via -[NSWindow close] while the widget is being torn
// down, so check for a delegate.
- (BOOL)canBecomeKeyWindow {
  bool canBecomeKey = NO;
  if (bridge_)
    bridge_->host()->GetCanWindowBecomeKey(&canBecomeKey);
  return canBecomeKey;
}

- (BOOL)canBecomeMainWindow {
  if (!bridge_)
    return NO;

  // Dialogs and bubbles shouldn't take large shadows away from their parent.
  if (bridge_->parent())
    return NO;

  bool canBecomeKey = NO;
  if (bridge_)
    bridge_->host()->GetCanWindowBecomeKey(&canBecomeKey);
  return canBecomeKey;
}

// Lets the traffic light buttons on the parent window keep their active state.
- (BOOL)hasKeyAppearance {
  // Note that this function is called off of the main thread. In such cases,
  // it is not safe to access the mojo interface or the ui::Widget, as they are
  // not reentrant.
  // https://crbug.com/941506.
  if (![NSThread isMainThread])
    return [super hasKeyAppearance];
  if (bridge_) {
    bool isAlwaysRenderWindowAsKey = NO;
    bridge_->host()->GetAlwaysRenderWindowAsKey(&isAlwaysRenderWindowAsKey);
    if (isAlwaysRenderWindowAsKey)
      return YES;
  }
  return [super hasKeyAppearance];
}

// Override sendEvent to intercept window drag events and allow key events to be
// forwarded to a toolkit-views menu while it is active, and while still
// allowing any native subview to retain firstResponder status.
- (void)sendEvent:(NSEvent*)event {
  // Let CommandDispatcher check if this is a redispatched event.
  if ([commandDispatcher_ preSendEvent:event])
    return;

  NSEventType type = [event type];

  // Draggable regions only respond to left-click dragging, but the system will
  // still suppress right-clicks in a draggable region. Forwarding right-clicks
  // allows the underlying views to respond to right-click to potentially bring
  // up a frame context menu.
  if (type == NSRightMouseDown) {
    if ([[self contentView] hitTest:event.locationInWindow] == nil) {
      [[self contentView] rightMouseDown:event];
      return;
    }
  } else if (type == NSRightMouseUp) {
    if ([[self contentView] hitTest:event.locationInWindow] == nil) {
      [[self contentView] rightMouseUp:event];
      return;
    }
  } else if ([self hasViewsMenuActive]) {
    // Send to the menu, after converting the event into an action message using
    // the content view.
    if (type == NSKeyDown) {
      [[self contentView] keyDown:event];
      return;
    } else if (type == NSKeyUp) {
      [[self contentView] keyUp:event];
      return;
    }
  }

  [super sendEvent:event];
}

// Override window order functions to intercept other visibility changes. This
// is needed in addition to the -[NSWindow display] override because Cocoa
// hardly ever calls display, and reports -[NSWindow isVisible] incorrectly
// when ordering in a window for the first time.
- (void)orderWindow:(NSWindowOrderingMode)orderingMode
         relativeTo:(NSInteger)otherWindowNumber {
  [super orderWindow:orderingMode relativeTo:otherWindowNumber];
  [[self viewsNSWindowDelegate] onWindowOrderChanged:nil];
}

// NSResponder implementation.

- (BOOL)performKeyEquivalent:(NSEvent*)event {
  return [commandDispatcher_ performKeyEquivalent:event];
}

- (void)cursorUpdate:(NSEvent*)theEvent {
  // The cursor provided by the delegate should only be applied within the
  // content area. This is because we rely on the contentView to track the
  // mouse cursor and forward cursorUpdate: messages up the responder chain.
  // The cursorUpdate: isn't handled in BridgedContentView because views-style
  // SetCapture() conflicts with the way tracking events are processed for
  // the view during a drag. Since the NSWindow is still in the responder chain
  // overriding cursorUpdate: here handles both cases.
  if (!NSPointInRect([theEvent locationInWindow], [[self contentView] frame])) {
    [super cursorUpdate:theEvent];
    return;
  }

  NSCursor* cursor = [[self viewsNSWindowDelegate] cursor];
  if (cursor)
    [cursor set];
  else
    [super cursorUpdate:theEvent];
}

- (NSTouchBar*)makeTouchBar API_AVAILABLE(macos(10.12.2)) {
  return touchBarDelegate_ ? [touchBarDelegate_ makeTouchBar] : nil;
}

// Called when the window is the delegate of the archiver passed to
// |-encodeRestorableStateWithCoder:|, below. It prevents the archiver from
// trying to encode the window or an NSView, say, to represent the first
// responder. When AppKit calls |-encodeRestorableStateWithCoder:|, it
// accomplishes the same thing by passing a custom coder.
- (id)archiver:(NSKeyedArchiver*)archiver willEncodeObject:(id)object {
  if (object == self)
    return nil;
  if ([object isKindOfClass:[NSView class]])
    return nil;
  return object;
}

- (void)saveRestorableState {
  if (!bridge_)
    return;
  if (![self _isConsideredOpenForPersistentState])
    return;
  base::scoped_nsobject<NSMutableData> restorableStateData(
      [[NSMutableData alloc] init]);
  base::scoped_nsobject<NSKeyedArchiver> encoder([[NSKeyedArchiver alloc]
      initForWritingWithMutableData:restorableStateData]);
  encoder.get().delegate = self;
  [self encodeRestorableStateWithCoder:encoder];
  [encoder finishEncoding];

  auto* bytes = static_cast<uint8_t const*>(restorableStateData.get().bytes);
  bridge_->host()->OnWindowStateRestorationDataChanged(
      std::vector<uint8_t>(bytes, bytes + restorableStateData.get().length));
  willUpdateRestorableState_ = NO;
}

// AppKit calls -invalidateRestorableState when a property of the window which
// affects its restorable state changes.
- (void)invalidateRestorableState {
  [super invalidateRestorableState];
  if ([self _isConsideredOpenForPersistentState]) {
    if (willUpdateRestorableState_)
      return;
    willUpdateRestorableState_ = YES;
    [self performSelectorOnMainThread:@selector(saveRestorableState)
                           withObject:nil
                        waitUntilDone:NO
                                modes:@[ NSDefaultRunLoopMode ]];
  } else if (willUpdateRestorableState_) {
    willUpdateRestorableState_ = NO;
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
  }
}

// On newer SDKs, _canMiniaturize respects NSMiniaturizableWindowMask in the
// window's styleMask. Views assumes that Widgets can always be minimized,
// regardless of their window style, so override that behavior here.
- (BOOL)_canMiniaturize {
  return YES;
}

// CommandDispatchingWindow implementation.

- (void)setCommandHandler:(id<UserInterfaceItemCommandHandler>)commandHandler {
  commandHandler_.reset([commandHandler retain]);
}

- (CommandDispatcher*)commandDispatcher {
  return commandDispatcher_.get();
}

- (BOOL)defaultPerformKeyEquivalent:(NSEvent*)event {
  return [super performKeyEquivalent:event];
}

- (BOOL)defaultValidateUserInterfaceItem:
    (id<NSValidatedUserInterfaceItem>)item {
  return [super validateUserInterfaceItem:item];
}

- (void)commandDispatch:(id)sender {
  [commandDispatcher_ dispatch:sender forHandler:commandHandler_];
}

- (void)commandDispatchUsingKeyModifiers:(id)sender {
  [commandDispatcher_ dispatchUsingKeyModifiers:sender
                                     forHandler:commandHandler_];
}

// NSWindow overrides (NSUserInterfaceItemValidations implementation)

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  return [commandDispatcher_ validateUserInterfaceItem:item
                                            forHandler:commandHandler_];
}

// NSWindow overrides (NSAccessibility informal protocol implementation).

- (id)accessibilityFocusedUIElement {
  if (![self delegate])
    return [super accessibilityFocusedUIElement];

  // The SDK documents this as "The deepest descendant of the accessibility
  // hierarchy that has the focus" and says "if a child element does not have
  // the focus, either return self or, if available, invoke the superclass's
  // implementation."
  // The behavior of NSWindow is usually to return null, except when the window
  // is first shown, when it returns self. But in the second case, we can
  // provide richer a11y information by reporting the views::RootView instead.
  // Additionally, if we don't do this, VoiceOver reads out the partial a11y
  // properties on the NSWindow and repeats them when focusing an item in the
  // RootView's a11y group. See http://crbug.com/748221.
  id superFocus = [super accessibilityFocusedUIElement];
  if (!bridge_ || superFocus != self)
    return superFocus;

  return bridge_->host_helper()->GetNativeViewAccessible();
}

- (NSString*)accessibilityTitle {
  // Check when NSWindow is asked for its title to provide the title given by
  // the views::RootView (and WidgetDelegate::GetAccessibleWindowTitle()). For
  // all other attributes, use what NSWindow provides by default since diverging
  // from NSWindow's behavior can easily break VoiceOver integration.
  NSString* viewsValue = self.rootAccessibilityObject.accessibilityTitle;
  return viewsValue ? viewsValue : [super accessibilityTitle];
}

@end
