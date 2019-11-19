// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/mac/availability.h"
#import "base/mac/scoped_nsobject.h"
#import "base/mac/sdk_forward_declarations.h"
#include "base/strings/sys_string_conversions.h"
#import "components/remote_cocoa/app_shim/bridged_content_view.h"
#import "components/remote_cocoa/app_shim/native_widget_ns_window_bridge.h"
#include "components/remote_cocoa/common/native_widget_ns_window_host.mojom.h"
#import "ui/base/cocoa/touch_bar_forward_declarations.h"

namespace {

NSString* const kTouchBarDialogButtonsGroupId =
    @"com.google.chrome-DIALOG-BUTTONS-GROUP";
NSString* const kTouchBarOKId = @"com.google.chrome-OK";
NSString* const kTouchBarCancelId = @"com.google.chrome-CANCEL";

}  // namespace

@interface BridgedContentView (TouchBarAdditions) <NSTouchBarDelegate>
- (void)touchBarButtonAction:(id)sender;
@end

@implementation BridgedContentView (TouchBarAdditions)

- (void)touchBarButtonAction:(id)sender {
  ui::DialogButton type = static_cast<ui::DialogButton>([sender tag]);
  if (bridge_)
    bridge_->host()->DoDialogButtonAction(type);
}

// NSTouchBarDelegate protocol implementation.

- (NSTouchBarItem*)touchBar:(NSTouchBar*)touchBar
      makeItemForIdentifier:(NSTouchBarItemIdentifier)identifier
    API_AVAILABLE(macos(10.12.2)) {
  if (!bridge_)
    return nil;

  if ([identifier isEqualToString:kTouchBarDialogButtonsGroupId]) {
    NSMutableArray* items = [NSMutableArray arrayWithCapacity:2];
    for (NSTouchBarItemIdentifier i in @[ kTouchBarCancelId, kTouchBarOKId ]) {
      NSTouchBarItem* item = [self touchBar:touchBar makeItemForIdentifier:i];
      if (item)
        [items addObject:item];
    }
    if ([items count] == 0)
      return nil;
    return [NSClassFromString(@"NSGroupTouchBarItem")
        groupItemWithIdentifier:identifier
                          items:items];
  }

  ui::DialogButton type = ui::DIALOG_BUTTON_NONE;
  if ([identifier isEqualToString:kTouchBarOKId])
    type = ui::DIALOG_BUTTON_OK;
  else if ([identifier isEqualToString:kTouchBarCancelId])
    type = ui::DIALOG_BUTTON_CANCEL;
  else
    return nil;

  bool buttonExists = false;
  base::string16 buttonLabel;
  bool isButtonEnabled = false;
  bool isButtonDefault = false;
  bridge_->host()->GetDialogButtonInfo(type, &buttonExists, &buttonLabel,
                                       &isButtonEnabled, &isButtonDefault);
  if (!buttonExists)
    return nil;

  base::scoped_nsobject<NSCustomTouchBarItem> item([[NSClassFromString(
      @"NSCustomTouchBarItem") alloc] initWithIdentifier:identifier]);
  NSButton* button =
      [NSButton buttonWithTitle:base::SysUTF16ToNSString(buttonLabel)
                         target:self
                         action:@selector(touchBarButtonAction:)];
  if (isButtonDefault) {
    // NSAlert uses a private NSButton subclass (_NSTouchBarGroupButton) with
    // more bells and whistles. It doesn't use -setBezelColor: directly, but
    // this gives an appearance matching the default _NSTouchBarGroupButton.
    [button setBezelColor:[NSColor colorWithSRGBRed:0.168
                                              green:0.51
                                               blue:0.843
                                              alpha:1.0]];
  }
  [button setEnabled:isButtonEnabled];
  [button setTag:type];
  [item setView:button];
  return item.autorelease();
}

// NSTouchBarProvider protocol implementation (via NSResponder category).

- (NSTouchBar*)makeTouchBar {
  if (!bridge_)
    return nil;

  bool buttonsExist = false;
  bridge_->host()->GetDoDialogButtonsExist(&buttonsExist);
  if (!buttonsExist)
    return nil;

  base::scoped_nsobject<NSTouchBar> bar(
      [[NSClassFromString(@"NSTouchBar") alloc] init]);
  [bar setDelegate:self];

  // Use a group rather than individual items so they can be centered together.
  [bar setDefaultItemIdentifiers:@[ kTouchBarDialogButtonsGroupId ]];

  // Setting the group as principal will center it in the TouchBar.
  [bar setPrincipalItemIdentifier:kTouchBarDialogButtonsGroupId];
  return bar.autorelease();
}

@end
