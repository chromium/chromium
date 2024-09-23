// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_COORDINATOR_IOS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_COORDINATOR_IOS_H_

#import <Foundation/Foundation.h>

namespace web_contents_delegate_ios {
class ColorChooserIOS;
}

@class ColorChooserControllerIOS;
@class ColorChooserMediatorIOS;
@class UIViewController;
@class UIColor;

// Holds a controller and a mediator so that communicates the controller and
// content implementations.
@interface ColorChooserCoordinatorIOS : NSObject

// The controller that has UI components.
@property(nonatomic, strong) ColorChooserControllerIOS* colorChooserController;

// The mediator that implements ColorChooserDelegateIOS.
@property(nonatomic, strong) ColorChooserMediatorIOS* colorChooserMediator;

// Initializer.
- (instancetype)initWithBaseViewController:(UIViewController*)baseViewController
                              colorChooser:
                                  (web_contents_delegate_ios::ColorChooserIOS*)
                                      colorChooser
                                     color:(UIColor*)color;

// Closes the color chooser.
- (void)closeColorChooser;

// Sets the selected color in the color chooser.
- (void)setColor:(UIColor*)color;

@end

#endif  // COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_COORDINATOR_IOS_H_
