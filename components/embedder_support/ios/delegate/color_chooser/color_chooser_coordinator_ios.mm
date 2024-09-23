// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/embedder_support/ios/delegate/color_chooser/color_chooser_coordinator_ios.h"

#import "components/embedder_support/ios/delegate/color_chooser/color_chooser_controller_ios.h"
#import "components/embedder_support/ios/delegate/color_chooser/color_chooser_mediator_ios.h"
#import "ui/gfx/native_widget_types.h"

@implementation ColorChooserCoordinatorIOS

- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                              colorChooser:
                                  (web_contents_delegate_ios::ColorChooserIOS*)
                                      colorChooser
                                     color:(UIColor*)color {
  if (!(self = [super init])) {
    return nil;
  }

  _colorChooserMediator =
      [[ColorChooserMediatorIOS alloc] initWithColorChooser:colorChooser];

  _colorChooserController = [[ColorChooserControllerIOS alloc]
      initWithChooserDelegate:_colorChooserMediator
                        color:color];
  _colorChooserController.modalInPresentation = YES;
  _colorChooserController.modalPresentationStyle = UIModalPresentationPopover;
  _colorChooserController.popoverPresentationController.delegate =
      _colorChooserController;
  _colorChooserController.popoverPresentationController.sourceView =
      baseViewController.view;
  _colorChooserController.popoverPresentationController.sourceRect =
      baseViewController.view.bounds;

  _colorChooserMediator.consumer = _colorChooserController;
  [baseViewController presentViewController:_colorChooserController
                                   animated:YES
                                 completion:nil];
  return self;
}

- (void)closeColorChooser {
  [self.colorChooserMediator closeColorChooser];
}

- (void)setColor:(UIColor*)color {
  [self.colorChooserMediator setColor:color];
}

@end
