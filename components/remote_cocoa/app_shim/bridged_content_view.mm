// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/remote_cocoa/app_shim/bridged_content_view.h"

#include <limits>

#import "base/apple/foundation_util.h"
#include "base/apple/owned_objc.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/strings/sys_string_conversions.h"
#import "components/remote_cocoa/app_shim/drag_drop_client.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "components/remote_cocoa/app_shim/native_widget_ns_window_host_helper.h"
#include "components/remote_cocoa/common/native_widget_ns_window_host.mojom.h"
#import "ui/base/cocoa/appkit_utils.h"
#include "ui/base/cocoa/find_pasteboard.h"
#include "ui/base/cocoa/tracking_area.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/dragdrop/os_exchange_data_provider_mac.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_edit_commands.h"
#include "ui/base/ime/text_input_client.h"
#import "ui/events/cocoa/cocoa_event_utils.h"
#include "ui/events/event_utils.h"
#include "ui/events/keycodes/dom/dom_code.h"
#import "ui/events/keycodes/keyboard_code_conversion_mac.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/decorated_text.h"
#import "ui/gfx/decorated_text_mac.h"
#include "ui/gfx/geometry/rect.h"
#import "ui/gfx/mac/coordinate_conversion.h"
#include "ui/gfx/scoped_ns_graphics_context_save_gstate_mac.h"

namespace {

NSString* const kFullKeyboardAccessChangedNotification =
    @"com.apple.KeyboardUIModeDidChange";

// Convert a |point| in |source_window|'s AppKit coordinate system (origin at
// the bottom left of the window) to |target_window|'s content rect, with the
// origin at the top left of the content area.
// If |source_window| is nil, |point| will be treated as screen coordinates.
gfx::Point MovePointToWindow(const NSPoint& point,
                             NSWindow* source_window,
                             NSWindow* target_window) {
  NSPoint point_in_screen =
      source_window ? [source_window convertPointToScreen:point] : point;
  NSPoint point_in_window =
      [target_window convertPointFromScreen:point_in_screen];

  NSRect content_rect =
      [target_window contentRectForFrameRect:[target_window frame]];
  return gfx::Point(point_in_window.x,
                    NSHeight(content_rect) - point_in_window.y);
}

// Convert a |point| in |source_window|'s AppKit coordinate system (origin at
// the bottom left of the window) to |target_view|'s coordinate, with the
// origin at the top left of the view.
// If |source_window| is nil, |point| will be treated as screen coordinates.
gfx::Point MovePointToView(const NSPoint& point,
                           NSWindow* source_window,
                           NSView* target_view) {
  NSPoint point_in_screen =
      source_window ? [source_window convertPointToScreen:point] : point;
  NSPoint point_in_window =
      [target_view.window convertPointFromScreen:point_in_screen];

  NSPoint point_in_view = [target_view convertPoint:point_in_window
                                           fromView:nil];
  return gfx::Point(point_in_window.x,
                    NSHeight([target_view frame]) - point_in_view.y);
}

// Some keys are silently consumed by -[NSView interpretKeyEvents:]
// They should not be processed as accelerators.
// See comments at |keyDown:| for details.
bool ShouldIgnoreAcceleratorWithMarkedText(NSEvent* event) {
  ui::KeyboardCode key = ui::KeyboardCodeFromNSEvent(event);
  switch (key) {
    // http://crbug.com/883952: Kanji IME completes composition and dismisses
    // itself.
    case ui::VKEY_RETURN:
    // Kanji IME: select candidate words.
    // Pinyin IME: change tone.
    case ui::VKEY_TAB:
    // Dismiss IME.
    case ui::VKEY_ESCAPE:
    // http://crbug.com/915924: Pinyin IME selects candidate.
    case ui::VKEY_LEFT:
    case ui::VKEY_RIGHT:
    case ui::VKEY_UP:
    case ui::VKEY_DOWN:
    case ui::VKEY_PRIOR:
    case ui::VKEY_NEXT:
      return true;
    default:
      return false;
  }
}

ui::TextEditCommand GetTextEditCommandForMenuAction(SEL action) {
  if (action == @selector(undo:))
    return ui::TextEditCommand::UNDO;
  if (action == @selector(redo:))
    return ui::TextEditCommand::REDO;
  if (action == @selector(cut:))
    return ui::TextEditCommand::CUT;
  if (action == @selector(copy:))
    return ui::TextEditCommand::COPY;
  if (action == @selector(paste:))
    return ui::TextEditCommand::PASTE;
  if (action == @selector(pasteAndMatchStyle:))
    return ui::TextEditCommand::PASTE;
  if (action == @selector(selectAll:))
    return ui::TextEditCommand::SELECT_ALL;
  return ui::TextEditCommand::INVALID_COMMAND;
}

}  // namespace

@interface BridgedContentView ()

// Dispatch |event| to |bridge_|'s host.
- (void)dispatchKeyEvent:(ui::KeyEvent*)event;

// Returns true if active menu controller corresponds to this widget. Note that
// this will synchronously call into the browser process.
- (BOOL)hasActiveMenuController;

// Dispatch |event| to |menu_controller| and return true if |event| is
// swallowed.
- (BOOL)dispatchKeyEventToMenuController:(ui::KeyEvent*)event;

// Passes |event| to the InputMethod for dispatch.
- (void)handleKeyEvent:(ui::KeyEvent*)event;

// Allows accelerators to be handled at different points in AppKit key event
// dispatch. Checks for an unhandled event passed in to -keyDown: and passes it
// to the Widget for processing. Returns YES if the Widget handles it.
- (BOOL)handleUnhandledKeyDownAsKeyEvent;

// Handles an NSResponder Action Message by mapping it to a corresponding text
// editing command from ui_strings.grd and, when not being sent to a
// TextInputClient, the keyCode that toolkit-views expects internally.
// For example, moveToLeftEndOfLine: would pass ui::VKEY_HOME in non-RTL locales
// even though the Home key on Mac defaults to scrollToBeginningOfDocument:.
// This approach also allows action messages a user
// may have remapped in ~/Library/KeyBindings/DefaultKeyBinding.dict to be
// catered for.
// Note: default key bindings in Mac can be read from StandardKeyBinding.dict
// which lives in /System/Library/Frameworks/AppKit.framework/Resources. Do
// `plutil -convert xml1 -o StandardKeyBinding.xml StandardKeyBinding.dict` to
// get something readable.
- (void)handleAction:(ui::TextEditCommand)command
             keyCode:(ui::KeyboardCode)keyCode
             domCode:(ui::DomCode)domCode
          eventFlags:(int)eventFlags;

// ui::EventLocationFromNative() assumes the event hit the contentView.
// Adjust |event| if that's not the case (e.g. for reparented views).
- (void)adjustUiEventLocation:(ui::LocatedEvent*)event
              fromNativeEvent:(NSEvent*)nativeEvent;

// Notification handler invoked when the Full Keyboard Access mode is changed.
- (void)onFullKeyboardAccessModeChanged:(NSNotification*)notification;

// Helper method which forwards |text| to the active menu or |textInputClient_|.
- (void)insertTextInternal:(id)text;

// Returns the native Widget's drag drop client. Possibly null.
- (remote_cocoa::DragDropClient*)dragDropClient NS_RETURNS_INNER_POINTER;

// Returns true if there exists a ui::TextInputClient for the currently focused
// views::View.
- (BOOL)hasTextInputClient;

// Returns true if there exists a ui::TextInputClient for the currently focused
// views::View and that client is right-to-left.
- (BOOL)isTextRTL;

// Menu action handlers.
- (void)undo:(id)sender;
- (void)redo:(id)sender;
- (void)cut:(id)sender;
- (void)copy:(id)sender;
- (void)paste:(id)sender;
- (void)pasteAndMatchStyle:(id)sender;
- (void)selectAll:(id)sender;

@end

@implementation BridgedContentView

@synthesize bridge = _bridge;
@synthesize drawMenuBackgroundForBlur = _drawMenuBackgroundForBlur;
@synthesize keyDownEventForTesting = _keyDownEvent;

- (instancetype)initWithBridge:(remote_cocoa::NativeWidgetNSWindowBridge*)bridge
                        bounds:(gfx::Rect)bounds {
  // To keep things simple, assume the origin is (0, 0) until there exists a use
  // case for something other than that.
  DCHECK(bounds.origin().IsOrigin());
  NSRect initialFrame = NSMakeRect(0, 0, bounds.width(), bounds.height());
  if ((self = [super initWithFrame:initialFrame tracking:NO])) {
    _bridge = bridge;

    // Get notified whenever Full Keyboard Access mode is changed.
    [[NSDistributedNotificationCenter defaultCenter]
        addObserver:self
           selector:@selector(onFullKeyboardAccessModeChanged:)
               name:kFullKeyboardAccessChangedNotification
             object:nil];

    // Initialize the focus manager with the correct keyboard accessibility
    // setting.
    [self updateFullKeyboardAccess];
    [self registerForDraggedTypes:ui::OSExchangeDataProviderMac::
                                      SupportedPasteboardTypes()];
  }
  return self;
}

- (ui::TextInputClient*)textInputClientForTesting {
  return _bridge ? _bridge->host_helper()->GetTextInputClient() : nullptr;
}

- (BOOL)hasTextInputClient {
  bool hasTextInputClient = NO;
  if (_bridge)
    _bridge->text_input_host()->HasClient(&hasTextInputClient);
  return hasTextInputClient;
}

- (BOOL)isTextRTL {
  bool isRTL = NO;
  if (_bridge)
    _bridge->text_input_host()->IsRTL(&isRTL);
  return isRTL;
}

- (void)dealloc {
  // By the time |self| is dealloc'd, it should never be in an NSWindow, and it
  // should never be the current input context.
  DCHECK_EQ(nil, [self window]);
}

- (void)clearView {
  _bridge = nullptr;
  [[NSDistributedNotificationCenter defaultCenter] removeObserver:self];
  if (_cursorTrackingArea.get()) {
    [_cursorTrackingArea.get() clearOwner];
    [self removeTrackingArea:_cursorTrackingArea.get()];
    _cursorTrackingArea.reset(nil);
  }
}

- (bool)needsUpdateWindows {
  // If |self| was being used for the input context, and would now report a
  // different input context, manually invoke [NSApp updateWindows]. This is
  // necessary because AppKit holds on to a raw pointer to a NSTextInputContext
  // (which may have been the one returned by [self inputContext]) that is only
  // updated by -updateWindows. And although AppKit invokes that on each
  // iteration through most runloop modes, it does not call it when running
  // NSEventTrackingRunLoopMode, and not _within_ a run loop iteration, where
  // the inputContext may change before further event processing.
  NSTextInputContext* current = [NSTextInputContext currentInputContext];
  if (!current)
    return false;

  NSTextInputContext* newContext = [self inputContext];
  // If the newContext is non-nil, then it can only be [super inputContext]. So
  // the input context is either not changing, or it was not from |self|. In
  // both cases, there's no need to call -updateWindows.
  if (newContext) {
    DCHECK_EQ(newContext, [super inputContext]);
    return false;
  }

  return current == [super inputContext];
}

// If |point| is classified as a draggable background (HTCAPTION), return nil so
// that it can lead to a window drag or double-click in the title bar. Dragging
// could be optimized by telling the window server which regions should be
// instantly draggable without asking (tracked at https://crbug.com/830962).
- (NSView*)hitTest:(NSPoint)point {
  gfx::Point flippedPoint(point.x, NSHeight(self.superview.bounds) - point.y);
  bool isDraggableBackground = false;
  _bridge->host()->GetIsDraggableBackgroundAt(flippedPoint,
                                              &isDraggableBackground);
  if (isDraggableBackground)
    return nil;
  return [super hitTest:point];
}

- (void)processCapturedMouseEvent:(NSEvent*)theEvent {
  if (!_bridge)
    return;

  NSWindow* source = [theEvent window];
  NSWindow* target = [self window];
  DCHECK(target);
  if (ui::PlatformEventSource::ShouldIgnoreNativePlatformEvents())
    return;

  BOOL isScrollEvent = [theEvent type] == NSEventTypeScrollWheel;

  // If it's the view's window, process normally.
  if ([target isEqual:source]) {
    if (isScrollEvent) {
      [self scrollWheel:theEvent];
    } else {
      [self mouseEvent:theEvent];
      if ([theEvent type] == NSEventTypeLeftMouseUp)
        [self handleLeftMouseUp:theEvent];
    }
    return;
  }

  gfx::Point event_location;
  if (remote_cocoa::IsNSToolbarFullScreenWindow(target)) {
    // We are in immersive fullscreen. This event is generated from the overlay
    // window which sits atop the toolbar. Convert the event location to the
    // content view coordinate, which should have the same bounds as the overlay
    // window.
    // This is to handle the case that `target` may contain the titlebar which
    // the overlay window does not contain. Without this, buttons in the toolbar
    // are not clickable when the titlebar is revealed.
    event_location = MovePointToView(theEvent.locationInWindow, source, self);
  } else {
    event_location =
        MovePointToWindow(theEvent.locationInWindow, source, target);
  }
  [self updateTooltipIfRequiredAt:event_location];

  if (isScrollEvent) {
    auto event =
        std::make_unique<ui::ScrollEvent>(base::apple::OwnedNSEvent(theEvent));
    event->set_location(event_location);
    _bridge->host()->OnScrollEvent(std::move(event));
  } else {
    auto event =
        std::make_unique<ui::MouseEvent>(base::apple::OwnedNSEvent(theEvent));
    event->set_location(event_location);
    _bridge->host()->OnMouseEvent(std::move(event));
  }
}

- (void)updateTooltipIfRequiredAt:(const gfx::Point&)locationInContent {
  DCHECK(_bridge);
  __weak BridgedContentView* weakSelf = self;
  _bridge->host()->GetTooltipTextAt(
      locationInContent,
      base::BindOnce(
          [](__weak BridgedContentView* weakView,
             const std::u16string& newTooltipText) {
            if (!weakView) {
              return;
            }
            __strong BridgedContentView* strongSelf = weakView;
            if (newTooltipText != strongSelf->_lastTooltipText) {
              strongSelf->_lastTooltipText = newTooltipText;
              [strongSelf setToolTipAtMousePoint:base::SysUTF16ToNSString(
                                                     newTooltipText)];
            }
          },
          weakSelf));
}

- (void)updateFullKeyboardAccess {
  if (!_bridge)
    return;
  _bridge->host()->SetKeyboardAccessible([NSApp isFullKeyboardAccessEnabled]);
}

- (void)updateCursorTrackingArea {
  if (_cursorTrackingArea.get()) {
    [_cursorTrackingArea.get() clearOwner];
    [self removeTrackingArea:_cursorTrackingArea.get()];
    _cursorTrackingArea.reset(nil);
  }
  if (![self window])
    return;

  NSTrackingAreaOptions options =
      NSTrackingMouseMoved | NSTrackingCursorUpdate | NSTrackingInVisibleRect |
      NSTrackingMouseEnteredAndExited;
  if ([[self window] level] == kCGFloatingWindowLevel) {
    // Floating windows should know when the mouse enters or leaves, regardless
    // of the first responder, window, or application status.
    // https://crbug.com/1214013
    options |= NSTrackingActiveAlways;
  } else {
    // Apple's documentation says that NSTrackingActiveAlways is incompatible
    // with NSTrackingCursorUpdate, so use NSTrackingActiveInActiveApp for all
    // other levels. See history from  https://crrev.com/314660
    options |= NSTrackingActiveInActiveApp | NSTrackingCursorUpdate;
  }

  CrTrackingArea* trackingArea = [[CrTrackingArea alloc] initWithRect:NSZeroRect
                                                              options:options
                                                                owner:self
                                                             userInfo:nil];
  _cursorTrackingArea.reset(trackingArea);
  [self addTrackingArea:_cursorTrackingArea.get()];
}

// BridgedContentView private implementation.

- (void)dispatchKeyEvent:(ui::KeyEvent*)event {
  if (_bridge)
    _bridge->host_helper()->DispatchKeyEvent(event);
}

- (BOOL)hasActiveMenuController {
  bool hasMenuController = false;
  if (_bridge)
    _bridge->host()->GetHasMenuController(&hasMenuController);
  return hasMenuController;
}

- (BOOL)dispatchKeyEventToMenuController:(ui::KeyEvent*)event {
  if (_bridge)
    return _bridge->host_helper()->DispatchKeyEventToMenuController(event);
  return false;
}

- (void)handleKeyEvent:(ui::KeyEvent*)event {
  DCHECK(event);
  if ([self dispatchKeyEventToMenuController:event])
    return;

  [self dispatchKeyEvent:event];
}

- (BOOL)handleUnhandledKeyDownAsKeyEvent {
  if (!_hasUnhandledKeyDownEvent)
    return NO;

  ui::KeyEvent event((base::apple::OwnedNSEvent(_keyDownEvent)));
  [self handleKeyEvent:&event];
  _hasUnhandledKeyDownEvent = NO;
  return event.handled();
}

- (void)handleAction:(ui::TextEditCommand)command
             keyCode:(ui::KeyboardCode)keyCode
             domCode:(ui::DomCode)domCode
          eventFlags:(int)eventFlags {
  if (!_bridge)
    return;

  // Always propagate the shift modifier if present. Shift doesn't always alter
  // the command selector, but should always be passed along. Control and Alt
  // have different meanings on Mac, so they do not propagate automatically.
  if ([_keyDownEvent modifierFlags] & NSEventModifierFlagShift)
    eventFlags |= ui::EF_SHIFT_DOWN;

  // Generate a synthetic event with the keycode toolkit-views expects.
  ui::KeyEvent event(ui::EventType::kKeyPressed, keyCode, domCode, eventFlags);

  // If the current event is a key event, assume it's the event that led to this
  // edit command and attach it. Note that it isn't always the case that the
  // current event is that key event, especially in tests which use synthetic
  // key events!
  if (NSApp.currentEvent.type == NSEventTypeKeyDown ||
      NSApp.currentEvent.type == NSEventTypeKeyUp) {
    event.SetNativeEvent(base::apple::OwnedNSEvent(NSApp.currentEvent));
  }

  if ([self dispatchKeyEventToMenuController:&event])
    return;

  // If there's an active TextInputClient, schedule the editing command to be
  // performed.
  bool out_enabled = false;
  if (_bridge &&
      _bridge->text_input_host()->IsTextEditCommandEnabled(command,
                                                           &out_enabled) &&
      out_enabled) {
    _bridge->text_input_host()->SetTextEditCommandForNextKeyEvent(command);
  }

  [self dispatchKeyEvent:&event];
}

- (void)adjustUiEventLocation:(ui::LocatedEvent*)event
              fromNativeEvent:(NSEvent*)nativeEvent {
  if ([nativeEvent window] && [[self window] contentView] != self) {
    NSPoint p = [self convertPoint:[nativeEvent locationInWindow] fromView:nil];
    event->set_location(gfx::Point(p.x, NSHeight([self frame]) - p.y));
  }
}

- (void)onFullKeyboardAccessModeChanged:(NSNotification*)notification {
  DCHECK([[notification name]
      isEqualToString:kFullKeyboardAccessChangedNotification]);
  [self updateFullKeyboardAccess];
}

- (void)insertTextInternal:(id)text {
  if (!_bridge)
    return;

  if ([text isKindOfClass:[NSAttributedString class]])
    text = [text string];

  bool isCharacterEvent = _keyDownEvent && [text length] == 1;

  // For some reason, shift-enter (not shift-return) results in the insertion of
  // a string with a single character, U+0003, END OF TEXT. Continuing on this
  // route will result in a forged event that loses the shift modifier.
  // Therefore, early return. When -keyDown: resumes, it will use a ui::KeyEvent
  // constructor that works. See https://crbug.com/1188713#c4 for the analysis.
  if (isCharacterEvent && [text characterAtIndex:0] == 0x0003)
    return;

  // Pass "character" events to the View hierarchy. Cases this handles (non-
  // exhaustive)-
  //    - Space key press on controls. Unlike Tab and newline which have
  //      corresponding action messages, an insertText: message is generated for
  //      the Space key (insertText:replacementRange: when there's an active
  //      input context).
  //    - Menu mnemonic selection.
  // Note we create a custom character ui::KeyEvent (and not use the
  // ui::KeyEvent(NSEvent*) constructor) since we can't just rely on the event
  // key code to get the actual characters from the ui::KeyEvent. This for
  // example is necessary for menu mnemonic selection of non-latin text.

  // Don't generate a key event when there is marked composition text. These key
  // down events should be consumed by the IME and not reach the Views layer.
  // For example, on pressing Return to commit composition text, if we passed a
  // synthetic key event to the View hierarchy, it will have the effect of
  // performing the default action on the current dialog. We do not want this
  // when there is marked text (Return should only confirm the IME).

  // However, IME for phonetic languages such as Korean do not always _mark_
  // text when a composition is active. For these, correct behaviour is to
  // handle the final -keyDown: that caused the composition to be committed, but
  // only _after_ the sequence of insertText: messages coming from IME have been
  // sent to the TextInputClient. Detect this by comparing to -[NSEvent
  // characters]. Note we do not use -charactersIgnoringModifiers: so that,
  // e.g., ß (Alt+s) will match mnemonics with ß rather than s.
  bool isFinalInsertForKeyEvent =
      isCharacterEvent && [text isEqualToString:[_keyDownEvent characters]];

  // Also note that a single, non-IME key down event can also cause multiple
  // insertText:replacementRange: action messages being generated from within
  // -keyDown:'s call to -interpretKeyEvents:. One example, on pressing Alt+e,
  // the accent (´) character is composed via setMarkedText:. Now on pressing
  // the character 'r', two insertText:replacementRange: action messages are
  // generated with the text value of accent (´) and 'r' respectively. The key
  // down event will have characters field of length 2. The first of these
  // insertText messages won't generate a KeyEvent since there'll be active
  // marked text. However, a KeyEvent will be generated corresponding to 'r'.

  // Currently there seems to be no use case to pass non-character events routed
  // from insertText: handlers to the View hierarchy.
  if (isFinalInsertForKeyEvent && ![self hasMarkedText]) {
    int flags = ui::EF_NONE;
    if ([_keyDownEvent isARepeat])
      flags |= ui::EF_IS_REPEAT;
    ui::KeyEvent charEvent = ui::KeyEvent::FromCharacter(
        [text characterAtIndex:0], ui::KeyboardCodeFromNSEvent(_keyDownEvent),
        ui::DomCodeFromNSEvent(_keyDownEvent), flags);
    [self handleKeyEvent:&charEvent];
    _hasUnhandledKeyDownEvent = NO;
    if (charEvent.handled())
      return;
  }

  // Forward the |text| to |textInputClient_| if no menu is active.
  if ([self hasTextInputClient] && ![self hasActiveMenuController]) {
    // Note we don't check isFinalInsertForKeyEvent, nor use |keyDownEvent_|
    // to generate the synthetic ui::KeyEvent since:  For composed text,
    // [keyDownEvent_ characters] might not be the same as |text|. This is
    // because |keyDownEvent_| will correspond to the event that caused the
    // composition text to be confirmed, say, Return key press.
    _bridge->text_input_host()->InsertText(base::SysNSStringToUTF16(text),
                                           isCharacterEvent);
    // Suppress accelerators that may be bound to this key, since it inserted
    // text instead. But note that IME may follow with -insertNewLine:, which
    // will resurrect the keyEvent for accelerator handling.
    _hasUnhandledKeyDownEvent = NO;
  }
}

- (remote_cocoa::DragDropClient*)dragDropClient {
  return _bridge ? _bridge->drag_drop_client() : nullptr;
}

- (void)undo:(id)sender {
  [self handleAction:ui::TextEditCommand::UNDO
             keyCode:ui::VKEY_Z
             domCode:ui::DomCode::US_Z
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)redo:(id)sender {
  [self handleAction:ui::TextEditCommand::REDO
             keyCode:ui::VKEY_Z
             domCode:ui::DomCode::US_Z
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)cut:(id)sender {
  [self handleAction:ui::TextEditCommand::CUT
             keyCode:ui::VKEY_X
             domCode:ui::DomCode::US_X
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)copy:(id)sender {
  [self handleAction:ui::TextEditCommand::COPY
             keyCode:ui::VKEY_C
             domCode:ui::DomCode::US_C
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)paste:(id)sender {
  [self handleAction:ui::TextEditCommand::PASTE
             keyCode:ui::VKEY_V
             domCode:ui::DomCode::US_V
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)pasteAndMatchStyle:(id)sender {
  [self handleAction:ui::TextEditCommand::PASTE
             keyCode:ui::VKEY_V
             domCode:ui::DomCode::US_V
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)selectAll:(id)sender {
  [self handleAction:ui::TextEditCommand::SELECT_ALL
             keyCode:ui::VKEY_A
             domCode:ui::DomCode::US_A
          eventFlags:ui::EF_CONTROL_DOWN];
}

// BaseView implementation.

// Don't use tracking areas from BaseView. BridgedContentView's tracks
// NSTrackingCursorUpdate and Apple's documentation suggests it's incompatible.
- (void)enableTracking {
}

// Translates the location of |theEvent| to toolkit-views coordinates and passes
// the event to NativeWidgetMac for handling.
- (void)mouseEvent:(NSEvent*)theEvent {
  if (!_bridge)
    return;

  DCHECK([theEvent type] != NSEventTypeScrollWheel);
  auto event =
      std::make_unique<ui::MouseEvent>(base::apple::OwnedNSEvent(theEvent));
  [self adjustUiEventLocation:event.get() fromNativeEvent:theEvent];

  // Aura updates tooltips with the help of aura::Window::AddPreTargetHandler().
  // Mac hooks in here.
  [self updateTooltipIfRequiredAt:event->location()];
  _bridge->host()->OnMouseEvent(std::move(event));
}

- (void)forceTouchEvent:(NSEvent*)theEvent {
  if (ui::ForceClickInvokesQuickLook())
    [self quickLookWithEvent:theEvent];
}

// NSView implementation.

// Refuse first responder so that clicking a blank area of the view don't take
// first responder away from another view. This does not prevent the view
// becoming first responder via -[NSWindow makeFirstResponder:] when invoked
// during Init or by FocusManager.
//
// The condition is to work around an AppKit quirk. When a window is being
// ordered front, if its current first responder returns |NO| for this method,
// it resigns it if it can find another responder in the key loop that replies
// |YES|.
- (BOOL)acceptsFirstResponder {
  return self.window.firstResponder == self;
}

// This undocumented method determines which parts of the view prevent
// server-side window dragging (i.e. aren't draggable without asking the app
// first). Since Views decides click-by-click whether to handle an event, the
// whole view is off limits but, since the view's content is rendered out of
// process and the view is locally transparent, AppKit won't guess that.
- (NSRect)_opaqueRectForWindowMoveWhenInTitlebar {
  return self.bounds;
}

- (BOOL)becomeFirstResponder {
  BOOL result = [super becomeFirstResponder];
  if (result && _bridge)
    _bridge->host()->OnIsFirstResponderChanged(true);
  return result;
}

- (BOOL)resignFirstResponder {
  BOOL result = [super resignFirstResponder];
  if (result && _bridge)
    _bridge->host()->OnIsFirstResponderChanged(false);
  return result;
}

- (void)viewDidMoveToWindow {
  // When this view is added to a window, AppKit calls setFrameSize before it is
  // added to the window, so the behavior in setFrameSize is not triggered.
  NSWindow* window = [self window];
  if (window) {
    [self setFrameSize:NSZeroSize];
    [self updateCursorTrackingArea];
  }
}

- (void)setFrameSize:(NSSize)newSize {
  // The size passed in here does not always use
  // -[NSWindow contentRectForFrameRect]. The following ensures that the
  // contentView for a frameless window can extend over the titlebar of the new
  // window containing it, since AppKit requires a titlebar to give frameless
  // windows correct shadows and rounded corners.
  NSWindow* window = [self window];
  if (window && [window contentView] == self) {
    newSize = [window contentRectForFrameRect:[window frame]].size;
    // Ensure that the window geometry be updated on the host side before the
    // view size is updated.
    // TODO(ccameron): Consider updating the view size and window size and
    // position together in UpdateWindowGeometry.
    // https://crbug.com/875776, https://crbug.com/875731
    if (_bridge)
      _bridge->UpdateWindowGeometry();
  }

  [super setFrameSize:newSize];

  if (_bridge)
    _bridge->host()->OnViewSizeChanged(
        gfx::Size(newSize.width, newSize.height));
}

- (BOOL)isOpaque {
  return _bridge ? !_bridge->is_translucent_window() : NO;
}

// To maximize consistency with the Cocoa browser (mac_views_browser=0), accept
// mouse clicks immediately so that clicking on Chrome from an inactive window
// will allow the event to be processed, rather than merely activate the window.
- (BOOL)acceptsFirstMouse:(NSEvent*)theEvent {
  return YES;
}

// NSDraggingDestination protocol overrides.

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender {
  return [self draggingUpdated:sender];
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender {
  remote_cocoa::DragDropClient* client = [self dragDropClient];
  return client ? client->DragUpdate(sender) : ui::DragDropTypes::DRAG_NONE;
}

- (void)draggingExited:(id<NSDraggingInfo>)sender {
  remote_cocoa::DragDropClient* client = [self dragDropClient];
  if (client)
    client->DragExit();
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender {
  remote_cocoa::DragDropClient* client = [self dragDropClient];
  return client && client->Drop(sender) != NSDragOperationNone;
}

- (NSTextInputContext*)inputContext {
  if (!_bridge)
    return nil;
  bool hasTextInputContext = false;
  _bridge->text_input_host()->HasInputContext(&hasTextInputContext);
  return hasTextInputContext ? [super inputContext] : nil;
}

// NSResponder implementation.

- (BOOL)_wantsKeyDownForEvent:(NSEvent*)event {
  // This is a SPI that AppKit apparently calls after |performKeyEquivalent:|
  // returned NO. If this function returns |YES|, Cocoa sends the event to
  // |keyDown:| instead of doing other things with it. Ctrl-tab will be sent
  // to us instead of doing key view loop control, ctrl-left/right get handled
  // correctly, etc.
  // (However, there are still some keys that Cocoa swallows, e.g. the key
  // equivalent that Cocoa uses for toggling the input language. In this case,
  // that's actually a good thing, though -- see http://crbug.com/26115 .)
  return YES;
}

- (void)keyDown:(NSEvent*)theEvent {
  BOOL hadMarkedTextAtKeyDown = [self hasMarkedText];

  // Convert the event into an action message, according to OSX key mappings.
  _keyDownEvent = theEvent;
  _hasUnhandledKeyDownEvent = YES;
  _wantsKeyHandledForInsert = NO;

  // interpretKeyEvents treats Mac Eisu / Kana keydown as insertion of space
  // character in omnibox when the current input source is not Japanese.
  // processInputKeyBindings should be called to switch input sources.
  if (theEvent.keyCode == kVK_JIS_Eisu || theEvent.keyCode == kVK_JIS_Kana) {
    if ([NSTextInputContext
            respondsToSelector:@selector(processInputKeyBindings:)]) {
      [NSTextInputContext performSelector:@selector(processInputKeyBindings:)
                               withObject:theEvent];
    }
  } else {
    [self interpretKeyEvents:@[ theEvent ]];
  }

  // When there is marked text, -[NSView interpretKeyEvents:] may handle the
  // event by updating the IME state without updating the composition text.
  // That is, no signal is given either to the NSTextInputClient or the
  // NSTextInputContext, leaving |hasUnhandledKeyDownEvent_| to be true.
  // In such a case, the key down event should not processed as an accelerator.
  // TODO(kerenzhu): Note it may be valid to always mark the key down event as
  // handled by IME when there is marked text. For now, only certain keys are
  // skipped.
  if (hadMarkedTextAtKeyDown && ShouldIgnoreAcceleratorWithMarkedText(theEvent))
    _hasUnhandledKeyDownEvent = NO;

  // Even with marked text, some IMEs may follow with -insertNewLine:;
  // simultaneously confirming the composition. In this case, always generate
  // the corresponding ui::KeyEvent. Note this is done even if there was no
  // marked text, so it is orthogonal to the case above.
  if (_wantsKeyHandledForInsert)
    _hasUnhandledKeyDownEvent = YES;

  // If |hasUnhandledKeyDownEvent_| wasn't set to NO during
  // -interpretKeyEvents:, it wasn't handled. Give Widget accelerators a chance
  // to handle it.
  [self handleUnhandledKeyDownAsKeyEvent];
  DCHECK(!_hasUnhandledKeyDownEvent);
  _keyDownEvent = nil;
}

- (void)keyUp:(NSEvent*)theEvent {
  ui::KeyEvent event((base::apple::OwnedNSEvent(theEvent)));
  [self handleKeyEvent:&event];
}

- (void)flagsChanged:(NSEvent*)theEvent {
  if (theEvent.keyCode == 0) {
    // An event like this gets sent when sending some key commands via
    // AppleScript. Since 0 is VKEY_A, we end up interpreting this as Cmd+A
    // which is incorrect. The correct event for command up/down (keyCode = 55)
    // is also sent, so we should drop this one. See https://crbug.com/889618
    return;
  }
  ui::KeyEvent event((base::apple::OwnedNSEvent(theEvent)));
  [self handleKeyEvent:&event];
}

- (void)scrollWheel:(NSEvent*)theEvent {
  if (!_bridge)
    return;

  auto event =
      std::make_unique<ui::ScrollEvent>(base::apple::OwnedNSEvent(theEvent));
  [self adjustUiEventLocation:event.get() fromNativeEvent:theEvent];

  // Aura updates tooltips with the help of aura::Window::AddPreTargetHandler().
  // Mac hooks in here.
  [self updateTooltipIfRequiredAt:event->location()];
  _bridge->host()->OnScrollEvent(std::move(event));
}

// Called when we get a three-finger swipe, and they're enabled in System
// Preferences.
- (void)swipeWithEvent:(NSEvent*)event {
  if (!_bridge)
    return;

  // themblsha: In my testing all three-finger swipes send only a single event
  // with a value of +/-1 on either axis. Diagonal swipes are not handled by
  // AppKit.

  // We need to invert deltas in order to match GestureEventDetails's
  // directions.
  ui::GestureEventDetails swipeDetails(ui::EventType::kGestureSwipe,
                                       -[event deltaX], -[event deltaY]);
  swipeDetails.set_device_type(ui::GestureDeviceType::DEVICE_TOUCHPAD);
  swipeDetails.set_touch_points(3);

  base::apple::OwnedNSEvent owned_event(event);
  gfx::PointF location = ui::EventLocationFromNative(owned_event);
  // Note this uses the default unique_touch_event_id of 0 (Swipe events do not
  // support -[NSEvent eventNumber]). This doesn't seem like a real omission
  // because the three-finger swipes are instant and can't be tracked anyway.
  auto gestureEvent = std::make_unique<ui::GestureEvent>(
      location.x(), location.y(), ui::EventFlagsFromNative(owned_event),
      ui::EventTimeFromNative(owned_event), swipeDetails);
  _bridge->host()->OnGestureEvent(std::move(gestureEvent));
}

- (void)quickLookWithEvent:(NSEvent*)theEvent {
  if (!_bridge)
    return;

  const gfx::Point locationInContent = gfx::ToFlooredPoint(
      ui::EventLocationFromNative(base::apple::OwnedNSEvent(theEvent)));

  bool foundWord = false;
  gfx::DecoratedText decoratedWord;
  gfx::Point baselinePoint;
  _bridge->host_helper()->GetWordAt(locationInContent, &foundWord,
                                    &decoratedWord, &baselinePoint);
  if (!foundWord)
    return;

  NSPoint baselinePointAppKit = NSMakePoint(
      baselinePoint.x(), NSHeight([self frame]) - baselinePoint.y());
  [self showDefinitionForAttributedString:
            gfx::GetAttributedStringFromDecoratedText(decoratedWord)
                                  atPoint:baselinePointAppKit];
}

////////////////////////////////////////////////////////////////////////////////
// NSResponder Action Messages. Keep sorted according NSResponder.h (from the
// 10.9 SDK). The list should eventually be complete. Anything not defined will
// beep when interpretKeyEvents: would otherwise call it.
// TODO(tapted): Make this list complete, except for insert* methods which are
// dispatched as regular key events in doCommandBySelector:.

// views::Textfields are single-line only, map Paragraph and Document commands
// to Line. Also, Up/Down commands correspond to beginning/end of line.

// The insertText action message forwards to the TextInputClient unless a menu
// is active. Note that NSResponder's interpretKeyEvents: implementation doesn't
// direct insertText: through doCommandBySelector:, so this is still needed to
// handle the case when inputContext: is nil. When inputContext: returns non-nil
// text goes directly to insertText:replacementRange:.
- (void)insertText:(id)text {
  DCHECK_EQ(nil, [self inputContext]);
  [self insertTextInternal:text];
}

// Selection movement and scrolling.

- (void)moveForward:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_FORWARD
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveRight:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_RIGHT
             keyCode:ui::VKEY_RIGHT
             domCode:ui::DomCode::ARROW_RIGHT
          eventFlags:0];
}

- (void)moveBackward:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_BACKWARD
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveLeft:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_LEFT
             keyCode:ui::VKEY_LEFT
             domCode:ui::DomCode::ARROW_LEFT
          eventFlags:0];
}

- (void)moveUp:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_UP
             keyCode:ui::VKEY_UP
             domCode:ui::DomCode::ARROW_UP
          eventFlags:0];
}

- (void)moveDown:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_DOWN
             keyCode:ui::VKEY_DOWN
             domCode:ui::DomCode::ARROW_DOWN
          eventFlags:0];
}

- (void)moveWordForward:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_FORWARD
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveWordBackward:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_BACKWARD
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveToBeginningOfLine:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_BEGINNING_OF_LINE
             keyCode:ui::VKEY_LEFT
             domCode:ui::DomCode::ARROW_LEFT
          eventFlags:ui::EF_COMMAND_DOWN];
}

- (void)moveToEndOfLine:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_END_OF_LINE
             keyCode:ui::VKEY_RIGHT
             domCode:ui::DomCode::ARROW_RIGHT
          eventFlags:ui::EF_COMMAND_DOWN];
}

- (void)moveToBeginningOfParagraph:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_BEGINNING_OF_PARAGRAPH
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveToEndOfParagraph:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_END_OF_PARAGRAPH
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveToEndOfDocument:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_END_OF_DOCUMENT
             keyCode:ui::VKEY_DOWN
             domCode:ui::DomCode::ARROW_DOWN
          eventFlags:ui::EF_COMMAND_DOWN];
}

- (void)moveToBeginningOfDocument:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_TO_BEGINNING_OF_DOCUMENT
             keyCode:ui::VKEY_UP
             domCode:ui::DomCode::ARROW_UP
          eventFlags:ui::EF_COMMAND_DOWN];
}

- (void)pageDown:(id)sender {
  // The pageDown: action message is bound to the key combination
  // [Option+PageDown].
  [self handleAction:ui::TextEditCommand::MOVE_PAGE_DOWN
             keyCode:ui::VKEY_NEXT
             domCode:ui::DomCode::PAGE_DOWN
          eventFlags:ui::EF_ALT_DOWN];
}

- (void)pageUp:(id)sender {
  // The pageUp: action message is bound to the key combination [Option+PageUp].
  [self handleAction:ui::TextEditCommand::MOVE_PAGE_UP
             keyCode:ui::VKEY_PRIOR
             domCode:ui::DomCode::PAGE_UP
          eventFlags:ui::EF_ALT_DOWN];
}

- (void)moveBackwardAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_BACKWARD_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveForwardAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_FORWARD_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveWordForwardAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_FORWARD_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveWordBackwardAndModifySelection:(id)sender {
  [self
      handleAction:ui::TextEditCommand::MOVE_WORD_BACKWARD_AND_MODIFY_SELECTION
           keyCode:ui::VKEY_UNKNOWN
           domCode:ui::DomCode::NONE
        eventFlags:0];
}

- (void)moveUpAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_UP_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UP
             domCode:ui::DomCode::ARROW_UP
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveDownAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_DOWN_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_DOWN
             domCode:ui::DomCode::ARROW_DOWN
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveToBeginningOfLineAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_TO_BEGINNING_OF_LINE_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_HOME
             domCode:ui::DomCode::HOME
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveToEndOfLineAndModifySelection:(id)sender {
  [self
      handleAction:ui::TextEditCommand::MOVE_TO_END_OF_LINE_AND_MODIFY_SELECTION
           keyCode:ui::VKEY_END
           domCode:ui::DomCode::END
        eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveToBeginningOfParagraphAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_TO_BEGINNING_OF_PARAGRAPH_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveToEndOfParagraphAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_TO_END_OF_PARAGRAPH_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)moveToEndOfDocumentAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_TO_END_OF_DOCUMENT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_END
             domCode:ui::DomCode::END
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveToBeginningOfDocumentAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_TO_BEGINNING_OF_DOCUMENT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_HOME
             domCode:ui::DomCode::HOME
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)pageDownAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_PAGE_DOWN_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_NEXT
             domCode:ui::DomCode::PAGE_DOWN
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)pageUpAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_PAGE_UP_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_PRIOR
             domCode:ui::DomCode::PAGE_UP
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveParagraphForwardAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_PARAGRAPH_FORWARD_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_DOWN
             domCode:ui::DomCode::ARROW_DOWN
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)moveParagraphBackwardAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::
                         MOVE_PARAGRAPH_BACKWARD_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_UP
             domCode:ui::DomCode::ARROW_UP
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)moveWordRight:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_RIGHT
             keyCode:ui::VKEY_RIGHT
             domCode:ui::DomCode::ARROW_RIGHT
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)moveWordLeft:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_LEFT
             keyCode:ui::VKEY_LEFT
             domCode:ui::DomCode::ARROW_LEFT
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)moveRightAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_RIGHT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_RIGHT
             domCode:ui::DomCode::ARROW_RIGHT
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveLeftAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_LEFT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_LEFT
             domCode:ui::DomCode::ARROW_LEFT
          eventFlags:ui::EF_SHIFT_DOWN];
}

- (void)moveWordRightAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_RIGHT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_RIGHT
             domCode:ui::DomCode::ARROW_RIGHT
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)moveWordLeftAndModifySelection:(id)sender {
  [self handleAction:ui::TextEditCommand::MOVE_WORD_LEFT_AND_MODIFY_SELECTION
             keyCode:ui::VKEY_LEFT
             domCode:ui::DomCode::ARROW_LEFT
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)moveToLeftEndOfLine:(id)sender {
  [self isTextRTL] ? [self moveToEndOfLine:sender]
                   : [self moveToBeginningOfLine:sender];
}

- (void)moveToRightEndOfLine:(id)sender {
  [self isTextRTL] ? [self moveToBeginningOfLine:sender]
                   : [self moveToEndOfLine:sender];
}

- (void)moveToLeftEndOfLineAndModifySelection:(id)sender {
  [self isTextRTL] ? [self moveToEndOfLineAndModifySelection:sender]
                   : [self moveToBeginningOfLineAndModifySelection:sender];
}

- (void)moveToRightEndOfLineAndModifySelection:(id)sender {
  [self isTextRTL] ? [self moveToBeginningOfLineAndModifySelection:sender]
                   : [self moveToEndOfLineAndModifySelection:sender];
}

- (void)scrollPageDown:(id)sender {
  [self handleAction:ui::TextEditCommand::SCROLL_PAGE_DOWN
             keyCode:ui::VKEY_NEXT
             domCode:ui::DomCode::PAGE_DOWN
          eventFlags:ui::EF_NONE];
}

- (void)scrollPageUp:(id)sender {
  [self handleAction:ui::TextEditCommand::SCROLL_PAGE_UP
             keyCode:ui::VKEY_PRIOR
             domCode:ui::DomCode::PAGE_UP
          eventFlags:ui::EF_NONE];
}

- (void)scrollToBeginningOfDocument:(id)sender {
  [self handleAction:ui::TextEditCommand::SCROLL_TO_BEGINNING_OF_DOCUMENT
             keyCode:ui::VKEY_HOME
             domCode:ui::DomCode::HOME
          eventFlags:ui::EF_NONE];
}

- (void)scrollToEndOfDocument:(id)sender {
  [self handleAction:ui::TextEditCommand::SCROLL_TO_END_OF_DOCUMENT
             keyCode:ui::VKEY_END
             domCode:ui::DomCode::END
          eventFlags:ui::EF_NONE];
}

// Graphical Element transposition

- (void)transpose:(id)sender {
  [self handleAction:ui::TextEditCommand::TRANSPOSE
             keyCode:ui::VKEY_T
             domCode:ui::DomCode::US_T
          eventFlags:ui::EF_CONTROL_DOWN];
}

// Deletions.

- (void)deleteForward:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_FORWARD
             keyCode:ui::VKEY_DELETE
             domCode:ui::DomCode::DEL
          eventFlags:0];
}

- (void)deleteBackward:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_BACKWARD
             keyCode:ui::VKEY_BACK
             domCode:ui::DomCode::BACKSPACE
          eventFlags:0];
}

- (void)deleteWordForward:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_WORD_FORWARD
             keyCode:ui::VKEY_DELETE
             domCode:ui::DomCode::DEL
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)deleteWordBackward:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_WORD_BACKWARD
             keyCode:ui::VKEY_BACK
             domCode:ui::DomCode::BACKSPACE
          eventFlags:ui::EF_CONTROL_DOWN];
}

- (void)deleteToBeginningOfLine:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_TO_BEGINNING_OF_LINE
             keyCode:ui::VKEY_BACK
             domCode:ui::DomCode::BACKSPACE
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)deleteToEndOfLine:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_TO_END_OF_LINE
             keyCode:ui::VKEY_DELETE
             domCode:ui::DomCode::DEL
          eventFlags:ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN];
}

- (void)deleteToBeginningOfParagraph:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_TO_BEGINNING_OF_PARAGRAPH
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)deleteToEndOfParagraph:(id)sender {
  [self handleAction:ui::TextEditCommand::DELETE_TO_END_OF_PARAGRAPH
             keyCode:ui::VKEY_UNKNOWN
             domCode:ui::DomCode::NONE
          eventFlags:0];
}

- (void)yank:(id)sender {
  [self handleAction:ui::TextEditCommand::YANK
             keyCode:ui::VKEY_Y
             domCode:ui::DomCode::US_Y
          eventFlags:ui::EF_CONTROL_DOWN];
}

// Cancellation.

- (void)cancelOperation:(id)sender {
  [self handleAction:ui::TextEditCommand::INVALID_COMMAND
             keyCode:ui::VKEY_ESCAPE
             domCode:ui::DomCode::ESCAPE
          eventFlags:0];
}

// Support for Services in context menus.
// Currently we only support reading and writing plain strings.
- (id)validRequestorForSendType:(NSString*)sendType
                     returnType:(NSString*)returnType {
  BOOL canWrite = [sendType isEqualToString:NSPasteboardTypeString] &&
                  [self selectedRange].length > 0;
  BOOL canRead = [returnType isEqualToString:NSPasteboardTypeString];
  // Valid if (sendType, returnType) is either (string, nil), (nil, string),
  // or (string, string).
  BOOL valid =
      [self hasTextInputClient] && ((canWrite && (canRead || !returnType)) ||
                                    (canRead && (canWrite || !sendType)));
  return valid
             ? self
             : [super validRequestorForSendType:sendType returnType:returnType];
}

// NSServicesMenuRequestor protocol

- (BOOL)writeSelectionToPasteboard:(NSPasteboard*)pboard types:(NSArray*)types {
  // /!\ Compatibility hack!
  //
  // The NSServicesMenuRequestor protocol does not pass in the correct
  // NSPasteboardType constants in the `types` array, verified through macOS 13
  // (FB11838671). To keep the code below clean, if an obsolete type is passed
  // in, rewrite the array.
  //
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
  if ([types containsObject:NSStringPboardType] &&
      ![types containsObject:NSPasteboardTypeString]) {
    types = [types arrayByAddingObject:NSPasteboardTypeString];
  }
#pragma clang diagnostic pop
  // /!\ End compatibility hack.

  bool wasAbleToWriteAtLeastOneType = false;

  if ([types containsObject:NSPasteboardTypeString]) {
    bool result = false;
    std::u16string selection_text;
    if (_bridge)
      _bridge->text_input_host()->GetSelectionText(&result, &selection_text);

    if (result) {
      NSString* text = base::SysUTF16ToNSString(selection_text);
      wasAbleToWriteAtLeastOneType |= [pboard writeObjects:@[ text ]];
    }
  }

  return wasAbleToWriteAtLeastOneType;
}

- (BOOL)readSelectionFromPasteboard:(NSPasteboard*)pboard {
  NSArray* objects = [pboard readObjectsForClasses:@[ [NSString class] ]
                                           options:nil];
  if (!objects.count) {
    return NO;
  }

  // It's expected that there only will be one string object on the pasteboard,
  // but if there is more than one, catenate them. This is the same compat
  // technique used by the compatibility call, -[NSPasteboard stringForType:].
  NSString* allTheText = [objects componentsJoinedByString:@"\n"];
  [self insertText:allTheText];
  return YES;
}

// NSTextInputClient protocol implementation.

// IMPORTANT: Always null-check |[self textInputClient]|. It can change (or be
// cleared) in -setTextInputClient:, which requires informing AppKit that the
// -inputContext has changed and to update its raw pointer. However, the AppKit
// method which does that may also spin a nested run loop communicating with an
// IME window and cause it to *use* the exact same NSTextInputClient (i.e.,
// |self|) that we're trying to invalidate in -setTextInputClient:.
// See https://crbug.com/817097#c12 for further details on this atrocity.

- (NSAttributedString*)attributedSubstringForProposedRange:(NSRange)range
                                               actualRange:
                                                   (NSRangePointer)actualRange {
  std::u16string substring;
  gfx::Range actual_range = gfx::Range::InvalidRange();
  if (_bridge) {
    _bridge->text_input_host()->GetAttributedSubstringForRange(
        gfx::Range::FromPossiblyInvalidNSRange(range), &substring,
        &actual_range);
  }
  if (actualRange) {
    // To maintain consistency with NSTextView, return range {0,0} for an out of
    // bounds requested range.
    *actualRange =
        actual_range.IsValid() ? actual_range.ToNSRange() : NSMakeRange(0, 0);
  }

  return substring.empty()
             ? nil
             : [[NSAttributedString alloc]
                   initWithString:base::SysUTF16ToNSString(substring)];
}

- (NSUInteger)characterIndexForPoint:(NSPoint)aPoint {
  NOTIMPLEMENTED();
  return 0;
}

- (void)doCommandBySelector:(SEL)selector {
  // Like the renderer, handle insert action messages as a regular key dispatch.
  // This ensures, e.g., insertTab correctly changes focus between fields. This
  // handles:
  //  -insertTab:(id)sender
  //  -insertBacktab:
  //  -insertNewline:
  //  -insertParagraphSeparator:
  //  -insertNewlineIgnoringFieldEditor:
  //  -insertTabIgnoringFieldEditor:
  //  -insertLineBreak:
  //  -insertContainerBreak:
  //  -insertSingleQuoteIgnoringSubstitution:
  //  -insertDoubleQuoteIgnoringSubstitution:
  // It does not handle |-insertText:(id)insertString|, which is not a command.
  // I.e. AppKit invokes _either_ -insertText: or -doCommandBySelector:. Also
  // note -insertText: is only invoked if -inputContext: has returned nil.
  DCHECK_NE(@selector(insertText:), selector);
  if (_keyDownEvent && [NSStringFromSelector(selector) hasPrefix:@"insert"]) {
    // When return is pressed during IME composition, engines typically first
    // confirm the composition with a series of -insertText:replacementRange:
    // calls. Then, some also invoke -insertNewLine: (some do not). If an engine
    // DOES invokes -insertNewLine:, we always want a corresponding VKEY_RETURN
    // ui::KeyEvent generated. If it does NOT follow with -insertNewLine:, the
    // VKEY_RETURN must be suppressed in keyDown:, since it typically will have
    // merely dismissed the IME window: the composition is still ongoing.
    // Setting this ensures keyDown: always generates a ui::KeyEvent.
    _wantsKeyHandledForInsert = YES;
    return;  // Handle in -keyDown:.
  }

  if ([self respondsToSelector:selector]) {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Warc-performSelector-leaks"
    // ARC complains about -performSelector re lifetimes. The selectors used
    // here are from a very small set used for text editing, so there's no
    // danger of lifetime issues.
    [self performSelector:selector withObject:nil];
#pragma clang diagnostic pop
    _hasUnhandledKeyDownEvent = NO;
    return;
  }

  // For events that AppKit sends via doCommandBySelector:, first attempt to
  // handle as a Widget accelerator. Forward along the responder chain only if
  // the Widget doesn't handle it.
  if (![self handleUnhandledKeyDownAsKeyEvent])
    [[self nextResponder] doCommandBySelector:selector];
}

- (NSRect)firstRectForCharacterRange:(NSRange)range
                         actualRange:(NSRangePointer)actualNSRange {
  gfx::Rect rect;
  gfx::Range actualRange = gfx::Range::InvalidRange();
  if (_bridge) {
    _bridge->text_input_host()->GetFirstRectForRange(gfx::Range(range), &rect,
                                                     &actualRange);
  }
  if (actualNSRange)
    *actualNSRange = actualRange.ToNSRange();
  return gfx::ScreenRectToNSRect(rect);
}

- (BOOL)hasMarkedText {
  bool hasCompositionText = NO;
  if (_bridge)
    _bridge->text_input_host()->HasCompositionText(&hasCompositionText);
  return hasCompositionText;
}

- (void)insertText:(id)text replacementRange:(NSRange)replacementRange {
  if (!_bridge)
    return;
  _bridge->text_input_host()->DeleteRange(gfx::Range(replacementRange));
  [self insertTextInternal:text];
}

- (NSRange)markedRange {
  gfx::Range range = gfx::Range::InvalidRange();
  if (_bridge)
    _bridge->text_input_host()->GetCompositionTextRange(&range);
  return range.ToNSRange();
}

- (NSRange)selectedRange {
  gfx::Range range = gfx::Range::InvalidRange();
  if (_bridge)
    _bridge->text_input_host()->GetSelectionRange(&range);
  return range.ToNSRange();
}

- (void)setMarkedText:(id)text
        selectedRange:(NSRange)selectedRange
     replacementRange:(NSRange)replacementRange {
  if (![self hasTextInputClient])
    return;

  if ([text isKindOfClass:[NSAttributedString class]])
    text = [text string];
  _bridge->text_input_host()->SetCompositionText(base::SysNSStringToUTF16(text),
                                                 gfx::Range(selectedRange),
                                                 gfx::Range(replacementRange));
  _hasUnhandledKeyDownEvent = NO;
}

- (void)unmarkText {
  if (![self hasTextInputClient])
    return;

  _bridge->text_input_host()->ConfirmCompositionText();
  _hasUnhandledKeyDownEvent = NO;
}

- (NSArray*)validAttributesForMarkedText {
  return @[];
}

- (void)copyToFindPboard:(id)sender {
  NSString* selection =
      [[self attributedSubstringForProposedRange:[self selectedRange]
                                     actualRange:nullptr] string];
  if (selection.length > 0)
    [[FindPasteboard sharedInstance] setFindText:selection];
}

// NSUserInterfaceValidations protocol implementation.

- (BOOL)validateUserInterfaceItem:(id<NSValidatedUserInterfaceItem>)item {
  // Special case this, since there's no cross-platform text command for it.
  if (item.action == @selector(copyToFindPboard:))
    return [self selectedRange].length > 0;

  ui::TextEditCommand command = GetTextEditCommandForMenuAction([item action]);

  if (command == ui::TextEditCommand::INVALID_COMMAND)
    return NO;

  bool out_enabled = false;
  _bridge->text_input_host()->IsTextEditCommandEnabled(command, &out_enabled);
  if (out_enabled) {
    return YES;
  }

  // views::Label does not implement the TextInputClient interface but still
  // needs to intercept the Copy and Select All menu actions.
  // We can't tell if TextInputClient returned NO or was absent entirely.
  // So if we got NO from IsTextEditCommandEnabled, try this special case.
  if (command != ui::TextEditCommand::COPY &&
      command != ui::TextEditCommand::SELECT_ALL)
    return NO;

  bool is_textual = false;
  _bridge->host()->GetIsFocusedViewTextual(&is_textual);
  return is_textual;
}

// NSDraggingSource protocol implementation.

- (NSDragOperation)draggingSession:(NSDraggingSession*)session
    sourceOperationMaskForDraggingContext:(NSDraggingContext)context {
  return NSDragOperationEvery;
}

- (void)draggingSession:(NSDraggingSession*)session
           endedAtPoint:(NSPoint)screenPoint
              operation:(NSDragOperation)operation {
  remote_cocoa::DragDropClient* client = [self dragDropClient];
  if (client)
    client->EndDrag();
}

// NSAccessibility formal protocol implementation:

- (NSArray*)accessibilityChildren {
  if (id accessible = _bridge->host_helper()->GetNativeViewAccessible())
    return @[ accessible ];
  return [super accessibilityChildren];
}

// NSAccessibility informal protocol implementation:

- (id)accessibilityHitTest:(NSPoint)point {
  return [_bridge->host_helper()->GetNativeViewAccessible()
      accessibilityHitTest:point];
}

- (id)accessibilityFocusedUIElement {
  // This function should almost-never be called because when |self| is the
  // first responder for the key NSWindow, NativeWidgetMacNSWindowHost's
  // AccessibilityFocusOverrider will override the accessibility focus query.
  if (!_bridge)
    return nil;
  return [_bridge->host_helper()->GetNativeViewAccessible()
      accessibilityFocusedUIElement];
}

@end
