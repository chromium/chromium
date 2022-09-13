// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_CRONET_IOS_CRONET_CONSUMER_CRONET_CONSUMER_APP_DELEGATE_H_
#define COMPONENTS_CRONET_IOS_CRONET_CONSUMER_CRONET_CONSUMER_APP_DELEGATE_H_

#import <UIKit/UIKit.h>

@class CronetConsumerViewController;

// The main app controller and UIApplicationDelegate.
@interface CronetConsumerAppDelegate : UIResponder<UIApplicationDelegate>

@property(strong, nonatomic) UIWindow* window;
@property(strong, nonatomic) CronetConsumerViewController* viewController;

@end

#endif  // COMPONENTS_CRONET_IOS_CRONET_CONSUMER_CRONET_CONSUMER_APP_DELEGATE_H_
