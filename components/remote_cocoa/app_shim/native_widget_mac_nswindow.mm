// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/native_widget_mac_nswindow.h"

#include "base/debug/dump_without_crashing.h"
#include "base/mac/foundation_util.h"
#include "base/trace_event/trace_event.h"
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
  [self.window performWindowDragWithEvent:event];
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
// The base implementation just tests [self class] == [NSThemeFrame class].
- (BOOL)_shouldFlipTrafficLightsForRTL API_AVAILABLE(macos(10.12)) {
  return [[self window] windowTitlebarLayoutDirection] ==
         NSUserInterfaceLayoutDirectionRightToLeft;
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
  base::scoped_nsobject<CommandDispatcher> _commandDispatcher;
  base::scoped_nsprotocol<id<UserInterfaceItemCommandHandler>> _commandHandler;
  id<WindowTouchBarDelegate> _touchBarDelegate;  // Weak.
  uint64_t _bridgedNativeWidgetId;
  remote_cocoa::NativeWidgetNSWindowBridge* _bridge;
  BOOL _willUpdateRestorableState;
  BOOL _isEnforcingNeverMadeVisible;
}
@synthesize bridgedNativeWidgetId = _bridgedNativeWidgetId;
@synthesize bridge = _bridge;

- (instancetype)initWithContentRect:(NSRect)contentRect
                          styleMask:(NSUInteger)windowStyle
                            backing:(NSBackingStoreType)bufferingType
                              defer:(BOOL)deferCreation {
  DCHECK(NSEqualRects(contentRect, ui::kWindowSizeDeterminedLater));
  if ((self = [super initWithContentRect:ui::kWindowSizeDeterminedLater
                               styleMask:windowStyle
                                 backing:bufferingType
                                   defer:deferCreation])) {
    _commandDispatcher.reset([[CommandDispatcher alloc] initWithOwner:self]);
  }
  return self;
}

// This override helps diagnose lifetime issues in crash stacktraces by
// inserting a symbol on NativeWidgetMacNSWindow and should be kept even if it
// does nothing.
- (void)dealloc {
  if (_isEnforcingNeverMadeVisible) {
    [self removeObserver:self forKeyPath:@"visible"];
  }
  _willUpdateRestorableState = YES;
  [NSObject cancelPreviousPerformRequestsWithTarget:self];
  [super dealloc];
}

- (void)enforceNeverMadeVisible {
  if (_isEnforcingNeverMadeVisible)
    return;
  _isEnforcingNeverMadeVisible = YES;
  [self addObserver:self
         forKeyPath:@"visible"
            options:NSKeyValueObservingOptionNew
            context:nil];
}

- (void)observeValueForKeyPath:(NSString*)keyPath
                      ofObject:(id)object
                        change:(NSDictionary*)change
                       context:(void*)context {
  if ([keyPath isEqual:@"visible"]) {
    DCHECK(_isEnforcingNeverMadeVisible);
    DCHECK_EQ(object, self);
    DCHECK_EQ(context, nil);
    if ([change[NSKeyValueChangeNewKey] boolValue])
      base::debug::DumpWithoutCrashing();
  }
  [super observeValueForKeyPath:keyPath
                       ofObject:object
                         change:change
                        context:context];
}

// Public methods.

- (void)setCommandDispatcherDelegate:(id<CommandDispatcherDelegate>)delegate {
  [_commandDispatcher setDelegate:delegate];
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
  _touchBarDelegate = delegate;
}

// Private methods.

- (ViewsNSWindowDelegate*)viewsNSWindowDelegate {
  return base::mac::ObjCCastStrict<ViewsNSWindowDelegate>([self delegate]);
}

- (BOOL)hasViewsMenuActive {
  bool hasMenuController = false;
  if (_bridge)
    _bridge->host()->GetHasMenuController(&hasMenuController);
  return hasMenuController;
}

- (id<NSAccessibility>)rootAccessibilityObject {
  id<NSAccessibility> obj =
      _bridge ? _bridge->host_helper()->GetNativeViewAccessible() : nil;
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
  if (_bridge)
    _bridge->host()->GetShouldShowWindowTitle(&shouldShowWindowTitle);
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
  if (_bridge)
    _bridge->host()->GetCanWindowBecomeKey(&canBecomeKey);
  return canBecomeKey;
}

- (BOOL)canBecomeMainWindow {
  if (!_bridge)
    return NO;

  // Dialogs and bubbles shouldn't take large shadows away from their parent.
  if (_bridge->parent())
    return NO;

  bool canBecomeKey = NO;
  if (_bridge)
    _bridge->host()->GetCanWindowBecomeKey(&canBecomeKey);
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
  if (_bridge) {
    bool isAlwaysRenderWindowAsKey = NO;
    _bridge->host()->GetAlwaysRenderWindowAsKey(&isAlwaysRenderWindowAsKey);
    if (isAlwaysRenderWindowAsKey)
      return YES;
  }
  return [super hasKeyAppearance];
}

// Override sendEvent to intercept window drag events and allow key events to be
// forwarded to a toolkit-views menu while it is active, and while still
// allowing any native subview to retain firstResponder status.
- (void)sendEvent:(NSEvent*)event {
  // TODO(bokan): Tracing added temporarily to diagnose crbug.com/1039833.
  TRACE_EVENT1("browser", "NSWindow::sendEvent", "WindowNum",
               [self windowNumber]);

  // Let CommandDispatcher check if this is a redispatched event.
  if ([_commandDispatcher preSendEvent:event]) {
    TRACE_EVENT_INSTANT0("browser", "StopSendEvent", TRACE_EVENT_SCOPE_THREAD);
    return;
  }

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
  // TODO(bokan): Tracing added temporarily to diagnose crbug.com/1039833.
  TRACE_EVENT1("browser", "NSWindow::performKeyEquivalent", "WindowNum",
               [self windowNumber]);
  return [_commandDispatcher performKeyEquivalent:event];
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
  return _touchBarDelegate ? [_touchBarDelegate makeTouchBar] : nil;
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
  if (!_bridge)
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
  _bridge->host()->OnWindowStateRestorationDataChanged(
      std::vector<uint8_t>(bytes, bytes + restorableStateData.get().length));
  _willUpdateRestorableState = NO;
}

// AppKit calls -invalidateRestorableState when a property of the window which
// affects its restorable state changes.
- (void)invalidateRestorableState {
  [super invalidateRestorableState];
  if ([self _isConsideredOpenForPersistentState]) {
    if (_willUpdateRestorableState)
      return;
    _willUpdateRestorableState = YES;
    [self performSelectorOnMainThread:@selector(saveRestorableState)
                           withObject:nil
                        waitUntilDone:NO
                                modes:@[ NSDefaultRunLoopMode ]];
  } else if (_willUpdateRestorableState) {
    _willUpdateRestorableState = NO;
    [NSObject cancelPreviousPerformRequestsWithTarget:self];
  }
}

// On newer SDKs, _canMiniaturize respects NSMiniaturizableWindowMask in the
// window's styleMask. Views assumes that Widgets can always be minimized,
// regardless of their window style, so override that behavior here.
- (BOOL)_canMiniaturize {
  return YES;
}

- (BOOL)respondsToSelector:(SEL)aSelector {
  // If this window or its parent does not handle commands, remove it from the
  // chain.
  bool isCommandDispatch =
      aSelector == @selector(commandDispatch:) ||
      aSelector == @selector(commandDispatchUsingKeyModifiers:);
  if (isCommandDispatch && _commandHandler == nil &&
      [_commandDispatcher bubbleParent] == nil) {
    return NO;
  }

  return [super respondsToSelector:aSelector];
}

// CommandDispatchingWindow implementation.

- (void)setCommandHandler:(id<UserInterfaceItemCommandHandler>)commandHandler {
  _commandHandler.reset([commandHandler retain]);
}

- (CommandDispatcher*)commandDispatcher {
  return _commandDispatcher.get();
}

- (BOOL)defaultPerformKeyEquivalent:(NSEvent*)event {
  // TODO(bokan): Tracing added temporarily to diagnose crbug.com/1039833.
  TRACE_EVENT1("browser", "NSWindow::defaultPerformKeyEquivalent", "WindowNum",
               [self windowNumber]);
  return [super performKeyEquivalent:event];
}

- (BOOL)defaultValidateUserInterfaceItem:
    (id<NSValidatedUserInterfaceItem>)item {
  return [super validateUserInterfaceItem:item];
}

- (void)commandDispatch:(id)sender {
  [_commandDispatcher dispatch:sender forHandler:_commandHandler];
}

- (void)commandDispatchUsingKeyModifiers:(id)sender {
  [_commandDispatcher dispatchUsingKeyModifiers:sender
                                     forHandler:_commandHandler];
}

// NSWindow overrides (NSUserInterfaceItemValidations implementation)

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  return [_commandDispatcher validateUserInterfaceItem:item
                                            forHandler:_commandHandler];
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
  if (!_bridge || superFocus != self)
    return superFocus;

  return _bridge->host_helper()->GetNativeViewAccessible();
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
