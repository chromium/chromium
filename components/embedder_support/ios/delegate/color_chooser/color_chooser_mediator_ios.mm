// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/embedder_support/ios/delegate/color_chooser/color_chooser_mediator_ios.h"

#import <UIKit/UIKit.h>

#import "components/embedder_support/ios/delegate/color_chooser/color_chooser_consumer_ios.h"
#import "components/embedder_support/ios/delegate/color_chooser/color_chooser_ios.h"
#import "third_party/skia/include/core/SkColor.h"

@implementation ColorChooserMediatorIOS

- (instancetype)initWithColorChooser:
    (web_contents_delegate_ios::ColorChooserIOS*)colorChooser {
  if (!(self = [super init])) {
    return nil;
  }
  _colorChooser = colorChooser->AsWeakPtr();
  return self;
}

- (void)closeColorChooser {
  [self.consumer closeColorChooser];
}

- (void)setColor:(UIColor*)color {
  [self.consumer setColor:color];
}

#pragma mark - ColorChooserDelegateIOS

- (void)onColorChosen:(ColorChooserControllerIOS*)controller
        selectedColor:(UIColor*)color {
  CGFloat red = 0.0, green = 0.0, blue = 0.0, alpha = 0.0;
  [color getRed:&red green:&green blue:&blue alpha:&alpha];
  self.colorChooser->OnColorChosen(SkColorSetARGB(
      alpha * 255.0f, red * 255.0f, green * 255.0f, blue * 255.0f));
}

@end
