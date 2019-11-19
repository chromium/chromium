// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/js_translate_manager.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/logging.h"
#include "base/mac/bundle_locations.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation JsTranslateManager {
  NSString* _translationScript;
}

- (NSString*)script {
  return _translationScript;
}

- (void)setScript:(NSString*)script {
  NSString* path =
      [base::mac::FrameworkBundle() pathForResource:@"translate_ios"
                                             ofType:@"js"];
  DCHECK(path);
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  DCHECK(!error && [content length]);
  // Prepend so callbacks defined in translate_ios.js can be installed.
  script = [content stringByAppendingString:script];

  _translationScript = [script copy];
}

- (void)startTranslationFrom:(const std::string&)source
                          to:(const std::string&)target {
  NSString* script =
      [NSString stringWithFormat:@"cr.googleTranslate.translate('%s','%s')",
                                 source.c_str(), target.c_str()];
  [self.receiver executeJavaScript:script completionHandler:nil];
}

- (void)revertTranslation {
  if (![self hasBeenInjected])
    return;

  [self.receiver executeJavaScript:@"cr.googleTranslate.revert()"
                 completionHandler:nil];
}

#pragma mark -
#pragma mark CRWJSInjectionManager methods

- (void)inject {
  NSString* script = [self injectionContent];

  // Reset any state if previously injected.
  if ([self hasBeenInjected]) {
    NSString* resetScript =
        @"try {"
         "  cr.googleTranslate.revert();"
         "} catch (e) {"
         "}";
    script = [resetScript stringByAppendingString:script];
  }

  // The scripts need to be re-injected to ensure that the logic that
  // initializes translate can be restarted properly.
  [[self receiver] injectScript:script forClass:[self class]];
}

- (NSString*)injectionContent {
  DCHECK(_translationScript);
  NSString* translationScript = _translationScript;
  _translationScript = nil;
  return translationScript;
}

@end
