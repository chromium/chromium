// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/first_run_dialog_controller.h"

#include "base/i18n/rtl.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#import "third_party/google_toolbox_for_mac/src/AppKit/GTMUILocalizerAndLayoutTweaker.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_mac.h"

namespace {

// Return the internationalized message |message_id|, with the product name
// substituted in for $1.
std::u16string String16WithProductName(int message_id) {
  return l10n_util::GetStringFUTF16(
      message_id, l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
}

// Return the internationalized message |message_id|, with the product name
// substituted in for $1.
NSString* NSStringWithProductName(int message_id) {
  return l10n_util::GetNSStringF(message_id,
                                 l10n_util::GetStringUTF16(IDS_PRODUCT_NAME));
}

// Reflows buttons. Requires them to be passed in vertical order, top down.
CGFloat VerticallyReflowButtons(NSArray<NSButton*>* buttons) {
  CGFloat localVerticalShift = 0;
  for (NSButton* button in buttons) {
    [GTMUILocalizerAndLayoutTweaker wrapButtonTitleForWidth:button];

    NSRect oldFrame = button.frame;
    [button sizeToFit];
    NSRect newFrame = button.frame;
    // -[NSControl sizeToFit], with no layout constraints, like here, will end
    // up horizontally resizing and keeping the upper left corner in place.
    // That wrecks RTL, and because all that's really needed is vertical
    // resizing, reset the horizontal size.
    newFrame.size.width = NSWidth(oldFrame);
    button.frame = newFrame;

    localVerticalShift += NSHeight(newFrame) - NSHeight(oldFrame);
    if (localVerticalShift) {
      NSPoint origin = button.frame.origin;
      origin.y -= localVerticalShift;
      [button setFrameOrigin:origin];
    }
  }
  return localVerticalShift;
}

void MoveViewsVertically(NSArray* views, CGFloat distance) {
  for (NSView* view : views) {
    NSRect frame = view.frame;
    frame.origin.y += distance;
    [view setFrame:frame];
  }
}

// Center |view| vertically within its own superview, so its horizontal
// centerline is the same as its superview's horizontal centerline.
void CenterVertically(NSView* view) {
  NSView* superview = view.superview;
  NSRect frame = view.frame;
  NSRect superframe = superview.frame;
  frame.origin.y = (NSHeight(superframe) - NSHeight(frame)) / 2.0;
  [view setFrame:frame];
}

}  // namespace

@implementation FirstRunDialogViewController {
  NSButton* __strong _defaultBrowserCheckbox;
  NSButton* __strong _statsCheckbox;
}

- (void)loadView {
  const int kDialogWidth = 480;

  NSBox* topBox =
      [[NSBox alloc] initWithFrame:NSMakeRect(0, 137, kDialogWidth, 52)];
  topBox.boxType = NSBoxCustom;
  topBox.contentViewMargins = NSZeroSize;
  topBox.fillColor = NSColor.controlColor;
  topBox.borderWidth = 0;

  // This string starts with the app name, which is strongly LTR, so force the
  // correct layout.
  std::u16string completeInstallationString =
      String16WithProductName(IDS_FIRSTRUN_DLG_MAC_COMPLETE_INSTALLATION_LABEL);
  base::i18n::AdjustStringForLocaleDirection(&completeInstallationString);
  NSTextField* completionLabel = [NSTextField
      labelWithString:base::SysUTF16ToNSString(completeInstallationString)];
  [completionLabel setFrame:NSMakeRect(13, 25, kDialogWidth - 2 * 13, 17)];

  _defaultBrowserCheckbox = [NSButton
      checkboxWithTitle:l10n_util::GetNSString(
                            IDS_FIRSTRUN_DLG_MAC_SET_DEFAULT_BROWSER_LABEL)
                 target:nil
                 action:nil];
  [_defaultBrowserCheckbox
      setFrame:NSMakeRect(45, 107, kDialogWidth - 2 * 45, 18)];
  [_defaultBrowserCheckbox setState:NSControlStateValueOn];

  _statsCheckbox = [NSButton
      checkboxWithTitle:NSStringWithProductName(
                            IDS_FIRSTRUN_DLG_MAC_OPTIONS_SEND_USAGE_STATS_LABEL)
                 target:nil
                 action:nil];
  [_statsCheckbox setFrame:NSMakeRect(45, 82, kDialogWidth - 2 * 45, 19)];
  [_statsCheckbox setState:NSControlStateValueOn];

  NSButton* startChromeButton =
      [NSButton buttonWithTitle:NSStringWithProductName(
                                    IDS_FIRSTRUN_DLG_MAC_START_CHROME_BUTTON)
                         target:self
                         action:@selector(ok:)];
  [startChromeButton setFrame:NSMakeRect(161, 12, 306, 32)];
  [startChromeButton setKeyEquivalent:@"\r"];

  NSBox* topSeparator =
      [[NSBox alloc] initWithFrame:NSMakeRect(0, 136, kDialogWidth, 1)];
  [topSeparator setBoxType:NSBoxSeparator];

  NSBox* bottomSeparator =
      [[NSBox alloc] initWithFrame:NSMakeRect(0, 55, kDialogWidth, 5)];
  [bottomSeparator setBoxType:NSBoxSeparator];

  [topBox addSubview:completionLabel];
  CenterVertically(completionLabel);

  self.view =
      [[NSView alloc] initWithFrame:NSMakeRect(0, 0, kDialogWidth, 190)];
  [self.view addSubview:topBox];
  [self.view addSubview:topSeparator];
  [self.view addSubview:_defaultBrowserCheckbox];
  [self.view addSubview:_statsCheckbox];
  [self.view addSubview:bottomSeparator];
  [self.view addSubview:startChromeButton];

  // Now that the content view is constructed, fix the layout. The first step is
  // to reflow the browser and stats checkbox texts, which can be quite lengthy
  // in some locales. They may wrap onto additional lines, and in doing so cause
  // the rest of the dialog to need to be rearranged.
  {
    CGFloat delta =
        VerticallyReflowButtons(@[ _defaultBrowserCheckbox, _statsCheckbox ]);
    if (delta) {
      // If reflowing the checkboxes produced a height delta, move the
      // checkboxes and the items above them in the content view upward, then
      // grow the content view to match. This has the effect of moving
      // everything visually-below the checkboxes downwards and expanding the
      // window, leaving the vertical space the checkboxes need for their text.
      MoveViewsVertically(
          @[ _defaultBrowserCheckbox, _statsCheckbox, topSeparator, topBox ],
          delta);
      NSRect frame = [self.view frame];
      frame.size.height += delta;
      self.view.autoresizesSubviews = NO;
      self.view.frame = frame;
      self.view.autoresizesSubviews = YES;
    }
  }

  // The "Start Chrome" button needs to be sized to fit the localized string
  // inside it, but it should still be at the right-most edge of the dialog, so
  // any width added or subtracted by |sizeToFit| is added to its x coord, which
  // keeps its right edge where it was.
  CGFloat oldWidth = NSWidth([startChromeButton frame]);
  [startChromeButton sizeToFit];
  NSRect frame = [startChromeButton frame];
  frame.origin.x += oldWidth - NSWidth([startChromeButton frame]);
  if (base::i18n::IsRTL())
    frame.origin.x = kDialogWidth - NSMaxX(frame);
  [startChromeButton setFrame:frame];
}

- (NSString*)windowTitle {
  return l10n_util::GetNSString(IDS_FIRST_RUN_DIALOG_WINDOW_TITLE);
}

- (BOOL)isStatsReportingEnabled {
  return [_statsCheckbox state] == NSControlStateValueOn;
}

- (BOOL)isMakeDefaultBrowserEnabled {
  return [_defaultBrowserCheckbox state] == NSControlStateValueOn;
}

- (void)ok:(id)sender {
  [self.view.window close];
  [NSApp stopModal];
}

@end
