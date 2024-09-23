// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_REMOTE_COCOA_APP_SHIM_BRIDGED_CONTENT_VIEW_H_
#define COMPONENTS_REMOTE_COCOA_APP_SHIM_BRIDGED_CONTENT_VIEW_H_

#import <Cocoa/Cocoa.h>

#include <string>

#include "base/memory/raw_ptr_exclusion.h"
#include "components/remote_cocoa/app_shim/remote_cocoa_app_shim_export.h"
#import "ui/base/cocoa/tool_tip_base_view.h"
#import "ui/base/cocoa/tracking_area.h"

namespace remote_cocoa {
class NativeWidgetNSWindowBridge;
}  // namespace remote_cocoa

namespace ui {
class TextInputClient;
}  // namespace ui

// The NSView that sits as the root contentView of the NSWindow, whilst it has
// a views::RootView present. Bridges requests from Cocoa to the hosted
// views::View.
REMOTE_COCOA_APP_SHIM_EXPORT
@interface BridgedContentView : ToolTipBaseView <NSTextInputClient,
                                                 NSUserInterfaceValidations,
                                                 NSDraggingSource,
                                                 NSServicesMenuRequestor> {
 @private
  // Weak, reset by clearView.
  // This field is not a raw_ptr<> because it requires @property rewrite.
  RAW_PTR_EXCLUSION remote_cocoa::NativeWidgetNSWindowBridge* _bridge;

  // A tracking area installed to enable mouseMoved events.
  ui::ScopedCrTrackingArea _cursorTrackingArea;

  // The keyDown event currently being handled, nil otherwise.
  NSEvent* __strong _keyDownEvent;

  // Whether there's an active key down event which is not handled yet.
  BOOL _hasUnhandledKeyDownEvent;

  // Whether any -insertFoo: selector (e.g. -insertNewLine:) was passed to
  // -doCommandBySelector: during the processing of this keyDown. These must
  // always be dispatched as a ui::KeyEvent in -keyDown:.
  BOOL _wantsKeyHandledForInsert;

  // The last tooltip text, used to limit updates.
  std::u16string _lastTooltipText;
}

@property(readonly, nonatomic) remote_cocoa::NativeWidgetNSWindowBridge* bridge;
@property(assign, nonatomic) BOOL drawMenuBackgroundForBlur;
@property(strong, nonatomic) NSEvent* keyDownEventForTesting;

// Initialize the NSView -> views::View bridge. |viewToHost| must be non-NULL.
- (instancetype)initWithBridge:(remote_cocoa::NativeWidgetNSWindowBridge*)bridge
                        bounds:(gfx::Rect)rect;

// Clear the hosted view. For example, if it is about to be destroyed.
- (void)clearView;

// Process a mouse event captured while the widget had global mouse capture.
- (void)processCapturedMouseEvent:(NSEvent*)theEvent;

// Mac's version of views::corewm::TooltipController::UpdateIfRequired().
// Updates the tooltip on the ToolTipBaseView if the text needs to change.
// |locationInContent| is the position from the top left of the window's
// contentRect (also this NSView's frame), as given by a ui::LocatedEvent.
- (void)updateTooltipIfRequiredAt:(const gfx::Point&)locationInContent;

// Notifies the associated FocusManager whether full keyboard access is enabled
// or not.
- (void)updateFullKeyboardAccess;

// Update the cursor tracking area in response to the parent window's level
// changing.
// https://crbug.com/1214013
- (void)updateCursorTrackingArea;

// The TextInputClient of the currently focused views::View.
// TODO(ccameron): This cannot be relied on across processes.
- (ui::TextInputClient*)textInputClientForTesting;

// Returns true if it is needed to call -[NSApp updateWindows] while updating
// the text input client.
- (bool)needsUpdateWindows;

// Action for Cmd-E
- (void)copyToFindPboard:(id)sender;

@end

#endif  // COMPONENTS_REMOTE_COCOA_APP_SHIM_BRIDGED_CONTENT_VIEW_H_
