// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>
#import <QuartzCore/QuartzCore.h>

#import "chrome/browser/ui/cocoa/confirm_quit_panel_controller.h"

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/cocoa/confirm_quit.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_registry_simple.h"
#import "ui/base/accelerators/platform_accelerator_cocoa.h"
#include "ui/base/l10n/l10n_util_mac.h"

// Constants ///////////////////////////////////////////////////////////////////

// Leeway between the |targetDate| and the current time that will confirm a
// quit.
const NSTimeInterval kTimeDeltaFuzzFactor = 1.0;

// Custom Content View /////////////////////////////////////////////////////////

// The content view of the window that draws a custom frame.
@interface ConfirmQuitFrameView : NSView {
 @private
  NSTextField* message_;  // Weak, owned by the view hierarchy.
}
- (void)setMessageText:(NSString*)text;
@end

@implementation ConfirmQuitFrameView

- (id)initWithFrame:(NSRect)frameRect {
  if ((self = [super initWithFrame:frameRect])) {
    base::scoped_nsobject<NSTextField> message(
        // The frame will be fixed up when |-setMessageText:| is called.
        [[NSTextField alloc] initWithFrame:NSZeroRect]);
    message_ = message.get();
    [message_ setEditable:NO];
    [message_ setSelectable:NO];
    [message_ setBezeled:NO];
    [message_ setDrawsBackground:NO];
    [message_ setFont:[NSFont boldSystemFontOfSize:24]];
    [message_ setTextColor:[NSColor whiteColor]];
    [self addSubview:message_];
  }
  return self;
}

- (void)drawRect:(NSRect)dirtyRect {
  const CGFloat kCornerRadius = 5.0;
  NSBezierPath* path = [NSBezierPath bezierPathWithRoundedRect:[self bounds]
                                                       xRadius:kCornerRadius
                                                       yRadius:kCornerRadius];

  NSColor* fillColor = [NSColor colorWithCalibratedWhite:0.2 alpha:0.75];
  [fillColor set];
  [path fill];
}

- (void)setMessageText:(NSString*)text {
  const CGFloat kHorizontalPadding = 30;  // In view coordinates.

  // Style the string.
  base::scoped_nsobject<NSMutableAttributedString> attrString(
      [[NSMutableAttributedString alloc] initWithString:text]);
  base::scoped_nsobject<NSShadow> textShadow([[NSShadow alloc] init]);
  [textShadow.get() setShadowColor:[NSColor colorWithCalibratedWhite:0
                                                               alpha:0.6]];
  [textShadow.get() setShadowOffset:NSMakeSize(0, -1)];
  [textShadow setShadowBlurRadius:1.0];
  [attrString addAttribute:NSShadowAttributeName
                     value:textShadow
                     range:NSMakeRange(0, [text length])];
  [message_ setAttributedStringValue:attrString];

  // Fixup the frame of the string.
  [message_ sizeToFit];
  NSRect messageFrame = [message_ frame];
  NSRect frameInViewSpace =
      [message_ convertRect:[[self window] frame] fromView:nil];

  if (NSWidth(messageFrame) > NSWidth(frameInViewSpace))
    frameInViewSpace.size.width = NSWidth(messageFrame) + kHorizontalPadding;

  messageFrame.origin.x = NSWidth(frameInViewSpace) / 2 - NSMidX(messageFrame);
  messageFrame.origin.y = NSHeight(frameInViewSpace) / 2 - NSMidY(messageFrame);

  [[self window] setFrame:[message_ convertRect:frameInViewSpace toView:nil]
                  display:YES];
  [message_ setFrame:messageFrame];
}

@end

// Animation ///////////////////////////////////////////////////////////////////

// This animation will run through all the windows of the passed-in
// NSApplication and will fade their alpha value to 0.0. When the animation is
// complete, this will release itself.
@interface FadeAllWindowsAnimation : NSAnimation<NSAnimationDelegate> {
 @private
  NSApplication* application_;
}
- (id)initWithApplication:(NSApplication*)app
        animationDuration:(NSTimeInterval)duration;
@end


@implementation FadeAllWindowsAnimation

- (id)initWithApplication:(NSApplication*)app
        animationDuration:(NSTimeInterval)duration {
  if ((self = [super initWithDuration:duration
                       animationCurve:NSAnimationLinear])) {
    application_ = app;
    [self setDelegate:self];
  }
  return self;
}

- (void)setCurrentProgress:(NSAnimationProgress)progress {
  for (NSWindow* window in [application_ windows]) {
    if (chrome::FindBrowserWithWindow(window))
      [window setAlphaValue:1.0 - progress];
  }
}

- (void)animationDidStop:(NSAnimation*)anim {
  DCHECK_EQ(self, anim);
  [self autorelease];
}

@end

// Private Interface ///////////////////////////////////////////////////////////

@interface ConfirmQuitPanelController (Private) <CAAnimationDelegate>
- (void)animateFadeOut;
- (NSEvent*)pumpEventQueueForKeyUp:(NSApplication*)app untilDate:(NSDate*)date;
- (void)hideAllWindowsForApplication:(NSApplication*)app
                        withDuration:(NSTimeInterval)duration;
// Returns the menu item for the Quit menu item, or a thrown-together default
// one if no Quit menu item exists.
+ (NSMenuItem*)quitMenuItem;
- (void)sendAccessibilityAnnouncement;
@end

ConfirmQuitPanelController* g_confirmQuitPanelController = nil;

////////////////////////////////////////////////////////////////////////////////

@implementation ConfirmQuitPanelController

+ (ConfirmQuitPanelController*)sharedController {
  if (!g_confirmQuitPanelController) {
    g_confirmQuitPanelController =
        [[ConfirmQuitPanelController alloc] init];
  }
  return [[g_confirmQuitPanelController retain] autorelease];
}

- (id)init {
  const NSRect kWindowFrame = NSMakeRect(0, 0, 350, 70);
  base::scoped_nsobject<NSWindow> window(
      [[NSWindow alloc] initWithContentRect:kWindowFrame
                                  styleMask:NSBorderlessWindowMask
                                    backing:NSBackingStoreBuffered
                                      defer:NO]);
  if ((self = [super initWithWindow:window])) {
    [window setDelegate:self];
    [window setBackgroundColor:[NSColor clearColor]];
    [window setOpaque:NO];
    [window setHasShadow:NO];

    // Create the content view. Take the frame from the existing content view.
    NSRect frame = [[window contentView] frame];
    base::scoped_nsobject<ConfirmQuitFrameView> frameView(
        [[ConfirmQuitFrameView alloc] initWithFrame:frame]);
    contentView_ = frameView.get();
    [window setContentView:contentView_];

    // Set the proper string.
    NSString* message = l10n_util::GetNSStringF(IDS_CONFIRM_TO_QUIT_DESCRIPTION,
        base::SysNSStringToUTF16([[self class] keyCommandString]));
    [contentView_ setMessageText:message];
  }
  return self;
}

- (BOOL)runModalLoopForApplication:(NSApplication*)app {
  base::scoped_nsobject<ConfirmQuitPanelController> keepAlive([self retain]);

  // If this is the second of two such attempts to quit within a certain time
  // interval, then just quit.
  // Time of last quit attempt, if any.
  static NSDate* lastQuitAttempt;  // Initially nil, as it's static.
  NSDate* timeNow = [NSDate date];
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
                                            untilDate:[NSDate distantFuture]];
    [app discardEventsMatchingMask:NSAnyEventMask beforeEvent:nextEvent];

    // Based on how long the user held the keys, record the metric.
    if ([[NSDate date] timeIntervalSinceDate:timeNow] <
        confirm_quit::kDoubleTapTimeDelta.InSecondsF())
      confirm_quit::RecordHistogram(confirm_quit::kDoubleTap);
    else
      confirm_quit::RecordHistogram(confirm_quit::kTapHold);
    return YES;
  } else {
    [lastQuitAttempt release];  // Harmless if already nil.
    lastQuitAttempt = [timeNow retain];  // Record this attempt for next time.
  }

  // Show the info panel that explains what the user must to do confirm quit.
  [self showWindow:self];

  // Explicitly announce the hold-to-quit message. For an ordinary modal dialog
  // VoiceOver would announce it and read its message, but VoiceOver does not do
  // this for windows whose styleMask is NSBorderlessWindowMask, so do it
  // manually here. Without this screenreader users have no way to know why
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
      NSDate* now = [NSDate date];
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
  [app discardEventsMatchingMask:NSAnyEventMask beforeEvent:nextEvent];

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
  [[self window] setAnimations:[NSDictionary dictionary]];
  g_confirmQuitPanelController = nil;
  [self autorelease];
}

- (void)showWindow:(id)sender {
  // If a panel that is fading out is going to be reused here, make sure it
  // does not get released when the animation finishes.
  base::scoped_nsobject<ConfirmQuitPanelController> keepAlive([self retain]);
  [[self window] setAnimations:[NSDictionary dictionary]];
  [[self window] center];
  [[self window] setAlphaValue:1.0];
  [super showWindow:sender];
}

- (void)dismissPanel {
  [self performSelector:@selector(animateFadeOut)
             withObject:nil
             afterDelay:1.0];
}

- (void)animateFadeOut {
  NSWindow* window = [self window];
  base::scoped_nsobject<CAAnimation> animation(
      [[window animationForKey:@"alphaValue"] copy]);
  [animation setDelegate:self];
  [animation setDuration:0.2];
  NSMutableDictionary* dictionary =
      [NSMutableDictionary dictionaryWithDictionary:[window animations]];
  [dictionary setObject:animation forKey:@"alphaValue"];
  [window setAnimations:dictionary];
  [[window animator] setAlphaValue:0.0];
}

- (void)animationDidStart:(CAAnimation*)theAnimation {
  // CAAnimationDelegate method added on OSX 10.12.
}

- (void)animationDidStop:(CAAnimation*)theAnimation finished:(BOOL)finished {
  [self close];
}

// This looks at the Main Menu and determines what the user has set as the
// key combination for quit. It then gets the modifiers and builds a string
// to display them.
+ (NSString*)keyCommandString {
  return [[self class] keyCombinationForMenuItem:[self quitMenuItem]];
}

// Runs a nested loop that pumps the event queue until the next KeyUp event.
- (NSEvent*)pumpEventQueueForKeyUp:(NSApplication*)app untilDate:(NSDate*)date {
  return [app nextEventMatchingMask:NSKeyUpMask
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
  // Releases itself when the animation stops.
  [animation startAnimation];
}

// This returns the NSMenuItem that quits the application.
+ (NSMenuItem*)quitMenuItem {
  NSMenu* mainMenu = [NSApp mainMenu];
  // Get the application menu (i.e. Chromium).
  NSMenu* appMenu = [[mainMenu itemAtIndex:0] submenu];
  for (NSMenuItem* item in [appMenu itemArray]) {
    // Find the Quit item.
    if ([item action] == @selector(terminate:)) {
      return item;
    }
  }

  // Default to Cmd+Q.
  NSMenuItem* item = [[[NSMenuItem alloc] initWithTitle:@""
                                                 action:@selector(terminate:)
                                          keyEquivalent:@"q"] autorelease];
  item.keyEquivalentModifierMask = NSCommandKeyMask;
  return item;
}

+ (NSString*)keyCombinationForMenuItem:(NSMenuItem*)item {
  NSMutableString* string = [NSMutableString string];
  NSUInteger modifiers = item.keyEquivalentModifierMask;

  if (modifiers & NSCommandKeyMask)
    [string appendString:@"\u2318"];
  if (modifiers & NSControlKeyMask)
    [string appendString:@"\u2303"];
  if (modifiers & NSAlternateKeyMask)
    [string appendString:@"\u2325"];
  if (modifiers & NSShiftKeyMask)
    [string appendString:@"\u21E7"];

  [string appendString:[item.keyEquivalent uppercaseString]];
  return string;
}

- (void)sendAccessibilityAnnouncement {
  NSString* message = l10n_util::GetNSStringF(
      IDS_CONFIRM_TO_QUIT_DESCRIPTION,
      base::SysNSStringToUTF16([[self class] keyCommandString]));

  NSAccessibilityPostNotificationWithUserInfo(
      [NSApp mainWindow], NSAccessibilityAnnouncementRequestedNotification, @{
        NSAccessibilityAnnouncementKey : message,
        NSAccessibilityPriorityKey : @(NSAccessibilityPriorityHigh),
      });
}

@end
