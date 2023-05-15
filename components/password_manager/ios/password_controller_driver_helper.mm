// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_controller_driver_helper.h"

#import "components/password_manager/ios/ios_password_manager_driver_factory.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation PasswordControllerDriverHelper {
  web::WebState* _webState;
}
#pragma mark - Initialization

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];

  if (self) {
    _webState = webState;
  }
  return self;
}

#pragma mark - Public methods

- (IOSPasswordManagerDriver*)PasswordManagerDriver:(web::WebFrame*)frame {
  return IOSPasswordManagerDriverFactory::FromWebStateAndWebFrame(_webState,
                                                                  frame);
}

- (password_manager::PasswordGenerationFrameHelper*)PasswordGenerationHelper:
    (web::WebFrame*)frame {
  return [self PasswordManagerDriver:frame]->GetPasswordGenerationHelper();
}

@end
