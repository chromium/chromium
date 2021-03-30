// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/translate/ios/browser/js_translate_manager.h"

#import <Foundation/Foundation.h>

#include <memory>

#include "base/check.h"
#include "base/mac/bundle_locations.h"
#include "base/strings/stringprintf.h"
#import "base/strings/sys_string_conversions.h"
#import "base/strings/utf_string_conversions.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

// Returns an autoreleased string containing the JavaScript loaded from a
// bundled resource file with the given name (excluding extension).
NSString* GetPageScript(NSString* script_file_name) {
  DCHECK(script_file_name);
  NSString* path =
      [base::mac::FrameworkBundle() pathForResource:script_file_name
                                             ofType:@"js"];
  DCHECK(path) << "Script file not found: "
               << base::SysNSStringToUTF8(script_file_name) << ".js";
  NSError* error = nil;
  NSString* content = [NSString stringWithContentsOfFile:path
                                                encoding:NSUTF8StringEncoding
                                                   error:&error];
  DCHECK(!error) << "Error fetching script: "
                 << base::SysNSStringToUTF8(error.description);
  DCHECK(content);
  return content;
}

}  // namespace

@interface JsTranslateManager ()
@property(nonatomic) web::WebState* web_state;
@property(nonatomic) bool injected;
@end

@implementation JsTranslateManager

- (instancetype)initWithWebState:(web::WebState*)web_state {
  self = [super init];
  if (self) {
    _web_state = web_state;
    _injected = false;
  }
  return self;
}

- (void)injectWithTranslateScript:(const std::string&)translate_script {
  // Prepend translate_ios.js
  NSString* translate_ios = GetPageScript(@"translate_ios");
  NSString* script = [translate_ios
      stringByAppendingString:base::SysUTF8ToNSString(translate_script)];

  // Reset translate state if previously injected.
  if (_injected) {
    NSString* resetScript = @"try {"
                             "  cr.googleTranslate.revert();"
                             "} catch (e) {"
                             "}";
    script = [resetScript stringByAppendingString:script];
  }

  _injected = true;

  _web_state->ExecuteJavaScript(base::SysNSStringToUTF16(script));
}

- (void)startTranslationFrom:(const std::string&)source
                          to:(const std::string&)target {
  std::string script =
      base::StringPrintf("cr.googleTranslate.translate('%s','%s')",
                         source.c_str(), target.c_str());
  _web_state->ExecuteJavaScript(base::UTF8ToUTF16(script));
}

- (void)revertTranslation {
  if (!_injected)
    return;

  _web_state->ExecuteJavaScript(u"cr.googleTranslate.revert()");
}

- (void)handleTranslateResponseWithURL:(const std::string&)URL
                             requestID:(int)requestID
                          responseCode:(int)responseCode
                            statusText:(const std::string&)statusText
                           responseURL:(const std::string&)responseURL
                          responseText:(const std::string&)responseText {
  DCHECK(_injected);

  // Return the response details to function defined in translate_ios.js.
  std::string script = base::StringPrintf(
      "__gCrWeb.translate.handleResponse('%s', %d, %d, '%s', '%s', '%s')",
      URL.c_str(), requestID, responseCode, statusText.c_str(),
      responseURL.c_str(), responseText.c_str());
  _web_state->ExecuteJavaScript(base::UTF8ToUTF16(script));
}

@end
