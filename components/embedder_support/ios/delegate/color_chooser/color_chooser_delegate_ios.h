// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_DELEGATE_IOS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_DELEGATE_IOS_H_

#import <Foundation/Foundation.h>

@class ColorChooserControllerIOS;
@class UIColor;

// Delegate to handle actions.
@protocol ColorChooserDelegateIOS <NSObject>

// Method invoked when the user closes the UIColorPickerViewController.
- (void)onColorChosen:(ColorChooserControllerIOS*)controller
        selectedColor:(UIColor*)color;

@end

#endif  // COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_DELEGATE_IOS_H_
