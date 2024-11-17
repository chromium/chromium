// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#include "base/check_op.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/cocoa/confirm_quit.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_registry_simple.h"
#include "ui/base/accelerators/accelerator.h"
#import "ui/base/accelerators/platform_accelerator_cocoa.h"
#include "ui/base/l10n/l10n_util_mac.h"
#include "ui/events/cocoa/cocoa_event_utils.h"
#include "ui/events/keycodes/keyboard_code_conversion_mac.h"

// Constants ///////////////////////////////////////////////////////////////////

// Leeway between the |targetDate| and the current time that will confirm a
// quit.
const NSTimeInterval kTimeDeltaFuzzFactor = 1.0;

// Custom Content View /////////////////////////////////////////////////////////

// The content view of the window that draws a custom frame.
@interface ConfirmQuitFrameView : NSView {
 @private
  NSTextField* __weak _message;
}
- (void)setMessageText:(NSString*)text;
@end

@implementation ConfirmQuitFrameView

- (instancetype)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    // The frame will be fixed up when |-setMessageText:| is called.
    NSTextField* message = [[NSTextField alloc] initWithFrame:NSZeroRect];
    message.editable = NO;
    message.selectable = NO;
    message.bezeled = NO;
    message.drawsBackground = NO;
    message.font = [NSFont boldSystemFontOfSize:24];
    message.textColor = NSColor.whiteColor;
    [self addSubview:message];
    _message = message;
  }
  return self;
}

- (void)drawRect:(NSRect)dirtyRect {
  const CGFloat kCornerRadius = 5.0;
  NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:self.bounds
                                                       xRadius:kCornerRadius
                                                       yRadius:kCornerRadius];

  NSColor* fillColor = [NSColor colorWithCalibratedWhite:0.2 alpha:0.75];
  [fillColor set];
  [path fill];
}

- (void)setMessageText:(NSString*)text {
  const CGFloat kHorizontalPadding = 30;  // In view coordinates.

  // Style the string.
  NSMutableAttributedString* attrString =
      [[NSMutableAttributedString alloc] initWithString:text];
  NSShadow* textShadow = [[NSShadow alloc] init];
  textShadow.shadowColor = [NSColor colorWithCalibratedWhite:0 alpha:0.6];
  textShadow.shadowOffset = NSMakeSize(0, -1);
  textShadow.shadowBlurRadius = 1.0;
  [attrString addAttribute:NSShadowAttributeName
                     value:textShadow
                     range:NSMakeRange(0, text.length)];
  _message.attributedStringValue = attrString;

  // Fixup the frame of the string.
  [_message sizeToFit];
  NSRect messageFrame = _message.frame;
  NSRect frameInViewSpace = [_message convertRect:self.window.frame
                                         fromView:nil];

  if (NSWidth(messageFrame) > NSWidth(frameInViewSpace)) {
    frameInViewSpace.size.width = NSWidth(messageFrame) + kHorizontalPadding;
  }

  messageFrame.origin.x = NSWidth(frameInViewSpace) / 2 - NSMidX(messageFrame);
  messageFrame.origin.y = NSHeight(frameInViewSpace) / 2 - NSMidY(messageFrame);

  [self.window setFrame:[_message convertRect:frameInViewSpace toView:nil]
                display:YES];
  _message.frame = messageFrame;
}

@end

// Animation ///////////////////////////////////////////////////////////////////

// This animation will run through all the windows of the passed-in
// NSApplication and will fade their alpha value to 0.0.
@interface FadeAllWindowsAnimation : NSAnimation <NSAnimationDelegate>
- (instancetype)initWithApplication:(NSApplication*)app
                  animationDuration:(NSTimeInterval)duration;
@end

@implementation FadeAllWindowsAnimation {
  NSApplication* __strong _application;
}

- (instancetype)initWithApplication:(NSApplication*)app
                  animationDuration:(NSTimeInterval)duration {
  if ((self = [super initWithDuration:duration
                       animationCurve:NSAnimationLinear])) {
    _application = app;
    self.delegate = self;
  }
  return self;
}

- (void)setCurrentProgress:(NSAnimationProgress)progress {
  for (NSWindow* window in _application.windows) {
    if (chrome::FindBrowserWithWindow(window))
      window.alphaValue = 1.0 - progress;
  }
}

@end

// Private Interface ///////////////////////////////////////////////////////////

@interface ConfirmQuitPanelController (Private) <CAAnimationDelegate>
// The menu item for the Quit menu item, or a thrown-together default one if no
// Quit menu item exists.
@property(class, readonly) NSMenuItem* quitMenuItem;

- (void)animateFadeOut;
- (NSEvent*)pumpEventQueueForKeyUp:(NSApplication*)app untilDate:(NSDate*)date;
- (void)hideAllWindowsForApplication:(NSApplication*)app
                        withDuration:(NSTimeInterval)duration;
- (void)sendAccessibilityAnnouncement;
@end

ConfirmQuitPanelController* __strong g_confirmQuitPanelController = nil;

////////////////////////////////////////////////////////////////////////////////

@implementation ConfirmQuitPanelController {
 @private
  // The content view of the window that this controller manages.
  ConfirmQuitFrameView* __weak _contentView;
}

+ (ConfirmQuitPanelController*)sharedController {
  if (!g_confirmQuitPanelController) {
    g_confirmQuitPanelController =
        [[ConfirmQuitPanelController alloc] init];
  }
  return g_confirmQuitPanelController;
}

- (instancetype)init {
  const NSRect kWindowFrame = NSMakeRect(0, 0, 350, 70);
  NSWindow* window =
      [[NSWindow alloc] initWithContentRect:kWindowFrame
                                  styleMask:NSWindowStyleMaskBorderless
                                    backing:NSBackingStoreBuffered
                                      defer:NO];
  if ((self = [super initWithWindow:window])) {
    window.delegate = self;
    window.backgroundColor = NSColor.clearColor;
    window.opaque = NO;
    window.hasShadow = NO;
    window.releasedWhenClosed = NO;

    // Create the content view. Take the frame from the existing content view.
    NSRect frame = window.contentView.frame;
    ConfirmQuitFrameView* frameView =
        [[ConfirmQuitFrameView alloc] initWithFrame:frame];
    window.contentView = frameView;

    // Set the proper string.
    NSString* message = l10n_util::GetNSStringF(
        IDS_CONFIRM_TO_QUIT_DESCRIPTION,
        base::SysNSStringToUTF16(ConfirmQuitPanelController.keyCommandString));
    frameView.messageText = message;
  }
  return self;
}

- (BOOL)runModalLoopForApplication:(NSApplication*)app {
  [[maybe_unused]] NS_VALID_UNTIL_END_OF_SCOPE ConfirmQuitPanelController*
      keepAlive = self;

  // If this is the second of two such attempts to quit within a certain time
  // interval, then just quit.
  // Time of last quit attempt, if any.
  static NSDate* lastQuitAttempt;  // Initially nil, as it's static.
  NSDate* timeNow = NSDate.date;
  if (lastQuitAttempt &&
      [timeNow timeIntervalSinceDate:lastQuitAttempt] < kTimeDeltaFuzzFactor) {
    // The panel tells users to Hold Cmd+Q. However, we also want to have a
    // double-tap shortcut that allows for a quick quit path. For the users who
    // tap Cmd+Q and then hold it with the window still open, this double-tap
    // logic will run and cause the quit to get committed. If the key
    // combination held down, the system will start sending the Cmd+Q event to
    // the next key application, and so on. This is bad, so instead we hide all
    // the windows (without animation) to look like we've "quit" and then wait
    // for the KeyUp event to commit the quit.
    [self hideAllWindowsForApplication:app withDuration:0];
    NSEvent* nextEvent = [self pumpEventQueueForKeyUp:app
                                            untilDate:NSDate.distantFuture];
    [app discardEventsMatchingMask:NSEventMaskAny beforeEvent:nextEvent];

    // Based on how long the user held the keys, record the metric.
    if ([NSDate.date timeIntervalSinceDate:timeNow] <
        confirm_quit::kDoubleTapTimeDelta.InSecondsF()) {
      confirm_quit::RecordHistogram(confirm_quit::kDoubleTap);
    } else {
      confirm_quit::RecordHistogram(confirm_quit::kTapHold);
    }
    return YES;
  } else {
    lastQuitAttempt = timeNow;  // Record this attempt for next time.
  }

  // Show the info panel that explains what the user must to do confirm quit.
  [self showWindow:self];

  // Explicitly announce the hold-to-quit message. For an ordinary modal dialog
  // VoiceOver would announce it and read its message, but VoiceOver does not do
  // this for windows whose styleMask is NSWindowStyleMaskBorderless, so do it
  // manually here. Without this screen reader users have no way to know why
  // their quit hotkey seems not to work.
  [self sendAccessibilityAnnouncement];

  // Spin a nested run loop until the |targetDate| is reached or a KeyUp event
  // is sent.
  NSDate* targetDate = [NSDate
      dateWithTimeIntervalSinceNow:confirm_quit::kShowDuration.InSecondsF()];
  BOOL willQuit = NO;
  NSEvent* nextEvent = nil;
  do {
    // Dequeue events until a key up is received. To avoid busy waiting, figure
    // out the amount of time that the thread can sleep before taking further
    // action.
    NSDate* waitDate = [NSDate
        dateWithTimeIntervalSinceNow:confirm_quit::kShowDuration.InSecondsF() -
                                     kTimeDeltaFuzzFactor];
    nextEvent = [self pumpEventQueueForKeyUp:app untilDate:waitDate];

    // Wait for the time expiry to happen. Once past the hold threshold,
    // commit to quitting and hide all the open windows.
    if (!willQuit) {
      NSDate* now = NSDate.date;
      NSTimeInterval difference = [targetDate timeIntervalSinceDate:now];
      if (difference < kTimeDeltaFuzzFactor) {
        willQuit = YES;

        // At this point, the quit has been confirmed and windows should all
        // fade out to convince the user to release the key combo to finalize
        // the quit.
        [self hideAllWindowsForApplication:app
                              withDuration:confirm_quit::kWindowFadeOutDuration
                                               .InSecondsF()];
      }
    }
  } while (!nextEvent);

  // The user has released the key combo. Discard any events (i.e. the
  // repeated KeyDown Cmd+Q).
  [app discardEventsMatchingMask:NSEventMaskAny beforeEvent:nextEvent];

  if (willQuit) {
    // The user held down the combination long enough that quitting should
    // happen.
    confirm_quit::RecordHistogram(confirm_quit::kHoldDuration);
    return YES;
  } else {
    // Slowly fade the confirm window out in case the user doesn't
    // understand what they have to do to quit.
    [self dismissPanel];
    return NO;
  }

  // Default case: terminate.
  return YES;
}

- (void)windowWillClose:(NSNotification*)notif {
  // Release all animations because CAAnimation retains its delegate (self),
  // which will cause a retain cycle. Break it!
  self.window.animations = @{};
  g_confirmQuitPanelController = nil;  // releases self
}

- (void)showWindow:(id)sender {
  // If a panel that is fading out is going to be reused here, make sure it
  // does not get released when the animation finishes.
  [[maybe_unused]] NS_VALID_UNTIL_END_OF_SCOPE ConfirmQuitPanelController*
      keepAlive = self;

  self.window.animations = @{};
  [self.window center];
  self.window.alphaValue = 1.0;
  [super showWindow:sender];
}

- (void)dismissPanel {
  [self performSelector:@selector(animateFadeOut)
             withObject:nil
             afterDelay:1.0];
}

- (void)animateFadeOut {
  NSWindow* window = self.window;
  CAAnimation* animation = [[window animationForKey:@"alphaValue"] copy];
  animation.delegate = self;
  animation.duration = 0.2;
  NSMutableDictionary* dictionary = [[window animations] mutableCopy];
  dictionary[@"alphaValue"] = animation;
  window.animations = dictionary;
  window.animator.alphaValue = 0.0;
}

- (void)animationDidStop:(CAAnimation*)theAnimation finished:(BOOL)finished {
  [self close];
}

+ (NSString*)keyCommandString {
  NSMenuItem* quitItem = self.quitMenuItem;
  ui::Accelerator accelerator(
      ui::KeyboardCodeFromCharCode([quitItem.keyEquivalent characterAtIndex:0]),
      ui::EventFlagsFromModifiers(quitItem.keyEquivalentModifierMask));
  return base::SysUTF16ToNSString(accelerator.GetShortcutText());
}

// Runs a nested loop that pumps the event queue until the next KeyUp event.
- (NSEvent*)pumpEventQueueForKeyUp:(NSApplication*)app untilDate:(NSDate*)date {
  return [app nextEventMatchingMask:NSEventMaskKeyUp
                          untilDate:date
                             inMode:NSEventTrackingRunLoopMode
                            dequeue:YES];
}

// Iterates through the list of open windows and hides them all.
- (void)hideAllWindowsForApplication:(NSApplication*)app
                        withDuration:(NSTimeInterval)duration {
  FadeAllWindowsAnimation* animation =
      [[FadeAllWindowsAnimation alloc] initWithApplication:app
                                         animationDuration:duration];

  // -startAnimation holds a strong reference to the animation until it is
  // complete.
  [animation startAnimation];
}

// This returns the NSMenuItem that quits the application.
+ (NSMenuItem*)quitMenuItem {
  // Get the application menu (i.e. Chromium).
  NSMenu* appMenu = [[NSApp.mainMenu itemAtIndex:0] submenu];
  for (NSMenuItem* item in appMenu.itemArray) {
    // Find the Quit item.
    if (item.action == @selector(terminate:)) {
      return item;
    }
  }

  // Default to Cmd+Q.
  NSMenuItem* item = [[NSMenuItem alloc] initWithTitle:@""
                                                action:@selector(terminate:)
                                         keyEquivalent:@"q"];
  item.keyEquivalentModifierMask = NSEventModifierFlagCommand;
  return item;
}

- (void)sendAccessibilityAnnouncement {
  NSString* message = l10n_util::GetNSStringF(
      IDS_CONFIRM_TO_QUIT_DESCRIPTION,
      base::SysNSStringToUTF16(ConfirmQuitPanelController.keyCommandString));

  NSAccessibilityPostNotificationWithUserInfo(
      NSApp.mainWindow, NSAccessibilityAnnouncementRequestedNotification, @{
        NSAccessibilityAnnouncementKey : message,
        NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
      });
}

@end
