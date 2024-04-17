// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/embedder_support/ios/delegate/color_chooser/color_chooser_controller_ios.h"

#import "components/embedder_support/ios/delegate/color_chooser/color_chooser_delegate_ios.h"

@implementation ColorChooserControllerIOS
- (instancetype)initWithChooserDelegate:
                    (id<ColorChooserDelegateIOS>)chooserDelegate
                                  color:(UIColor*)color {
  if (!(self = [super init])) {
    return nil;
  }

  _chooserDelegate = chooserDelegate;
  [self setDelegate:self];
  [self setColor:color];
  [self setSupportsAlpha:FALSE];
  return self;
}

- (void)viewWillDisappear:(BOOL)animated {
  // Update the selected color here when UIColorPickerViewController is closed
  // by tapping the close button or swiping down the popover.
  // `UIColorPickerViewControllerDelegate::colorPickerViewControllerDidFinish`
  // is called only when a user taps the close button.
  [self.chooserDelegate onColorChosen:self selectedColor:self.selectedColor];
}

#pragma mark - ColorChooserConsumerIOS

- (void)closeColorChooser {
  [self dismissViewControllerAnimated:YES completion:nil];
}

- (void)setColor:(UIColor*)color {
  [self setSelectedColor:color];
}

@end
