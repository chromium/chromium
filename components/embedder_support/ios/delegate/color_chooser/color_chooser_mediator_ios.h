// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_MEDIATOR_IOS_H_
#define COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_MEDIATOR_IOS_H_

#import <Foundation/Foundation.h>

#import "base/memory/weak_ptr.h"
#import "components/embedder_support/ios/delegate/color_chooser/color_chooser_delegate_ios.h"

namespace web_contents_delegate_ios {
class ColorChooserIOS;
}  // namespace web_contents_delegate_ios

@protocol ColorChooserConsumerIOS;
@class UIColor;

// The mediator that implements ColorChooserDelegateIOS. It also communicates
// with the controller, referring to `consumer`.
@interface ColorChooserMediatorIOS : NSObject <ColorChooserDelegateIOS>

@property(nonatomic, assign)
    base::WeakPtr<web_contents_delegate_ios::ColorChooserIOS>
        colorChooser;

// Consumer that is configured by this mediator.
@property(nonatomic, assign) id<ColorChooserConsumerIOS> consumer;

// Initializer.
- (instancetype)initWithColorChooser:
    (web_contents_delegate_ios::ColorChooserIOS*)colorChooser;

// Closes the color chooser.
- (void)closeColorChooser;

// Sets the selected color in the color chooser.
- (void)setColor:(UIColor*)color;

@end

#endif  // COMPONENTS_EMBEDDER_SUPPORT_IOS_DELEGATE_COLOR_CHOOSER_COLOR_CHOOSER_MEDIATOR_IOS_H_
