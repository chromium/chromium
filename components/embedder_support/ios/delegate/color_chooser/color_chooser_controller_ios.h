// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_CONTROLLER_IOS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_CONTROLLER_IOS_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "components/embedder_support/ios/delegate/color_chooser/color_chooser_consumer_ios.h"

@protocol ColorChooserDelegateIOS;

// The color chooser controller that is opened as a popover.
@interface ColorChooserControllerIOS
    : UIColorPickerViewController <ColorChooserConsumerIOS,
                                   UIColorPickerViewControllerDelegate,
                                   UIPopoverPresentationControllerDelegate>

// The view controller this coordinator was initialized with.
@property(weak, nonatomic, readonly) UIViewController* baseViewController;

// Delegate used to handle the actions in the color chooser.
@property(nonatomic, weak) id<ColorChooserDelegateIOS> chooserDelegate;

// Initializer.
- (instancetype)initWithChooserDelegate:(id<ColorChooserDelegateIOS>)delegate
                                  color:(UIColor*)color;

@end

#endif  // COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_CONTROLLER_IOS_H_
