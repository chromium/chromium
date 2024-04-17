// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_CONSUMER_IOS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_CONSUMER_IOS_H_

#import <Foundation/Foundation.h>

@class UIColor;

// Consumer for model to request actions to the color chooser UI.
@protocol ColorChooserConsumerIOS <NSObject>

// Closes the color chooser.
- (void)closeColorChooser;

// Sets the selected color in the color chooser with `color`.
- (void)setColor:(UIColor*)color;

@end

#endif  // COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_CONSUMER_IOS_H_
