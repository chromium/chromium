// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/js_suggestion_manager.h"

#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/format_macros.h"
#include "base/json/string_escape.h"
#include "base/logging.h"
#include "base/strings/sys_string_conversions.h"
#include "base/values.h"
#import "components/autofill/ios/browser/autofill_util.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frames_manager.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation JsSuggestionManager {
  // The injection receiver used to evaluate JavaScript.
  __weak CRWJSInjectionReceiver* _receiver;
  web::WebFramesManager* _webFramesManager;
}

- (instancetype)initWithReceiver:(CRWJSInjectionReceiver*)receiver {
  DCHECK(receiver);
  self = [super init];
  if (self) {
    _receiver = receiver;
  }
  return self;
}

- (void)setWebFramesManager:(web::WebFramesManager*)framesManager {
  _webFramesManager = framesManager;
}

#pragma mark -
#pragma mark ProtectedMethods

- (void)selectNextElementInFrameWithID:(NSString*)frameID {
  [self selectNextElementInFrameWithID:frameID afterForm:@"" field:@""];
}

- (void)selectNextElementInFrameWithID:(NSString*)frameID
                             afterForm:(NSString*)formName
                                 field:(NSString*)fieldName {
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(base::SysNSStringToUTF8(formName)));
  parameters.push_back(base::Value(base::SysNSStringToUTF8(fieldName)));
  autofill::ExecuteJavaScriptFunction(
      "suggestion.selectNextElement", parameters,
      [self frameWithFrameID:frameID], _receiver,
      base::OnceCallback<void(NSString*)>());
}

- (void)selectPreviousElementInFrameWithID:(NSString*)frameID {
  [self selectPreviousElementInFrameWithID:frameID beforeForm:@"" field:@""];
}

- (void)selectPreviousElementInFrameWithID:(NSString*)frameID
                                beforeForm:(NSString*)formName
                                     field:(NSString*)fieldName {
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(base::SysNSStringToUTF8(formName)));
  parameters.push_back(base::Value(base::SysNSStringToUTF8(fieldName)));
  autofill::ExecuteJavaScriptFunction(
      "suggestion.selectPreviousElement", parameters,
      [self frameWithFrameID:frameID], _receiver,
      base::OnceCallback<void(NSString*)>());
}

- (void)fetchPreviousAndNextElementsPresenceInFrameWithID:(NSString*)frameID
                                        completionHandler:
                                            (void (^)(BOOL,
                                                      BOOL))completionHandler {
  [self fetchPreviousAndNextElementsPresenceInFrameWithID:frameID
                                                  forForm:@""
                                                    field:@""
                                        completionHandler:completionHandler];
}

- (void)fetchPreviousAndNextElementsPresenceInFrameWithID:(NSString*)frameID
                                                  forForm:(NSString*)formName
                                                    field:(NSString*)fieldName
                                        completionHandler:
                                            (void (^)(BOOL,
                                                      BOOL))completionHandler {
  DCHECK(completionHandler);
  std::vector<base::Value> parameters;
  parameters.push_back(base::Value(base::SysNSStringToUTF8(formName)));
  parameters.push_back(base::Value(base::SysNSStringToUTF8(fieldName)));
  autofill::ExecuteJavaScriptFunction(
      "suggestion.hasPreviousNextElements", parameters,
      [self frameWithFrameID:frameID], _receiver,
      base::BindOnce(^(NSString* result) {
        // The result maybe an empty string here due to 2 reasons:
        // 1) When there is an exception running the JS
        // 2) There is a race when the page is changing due to which
        // JSSuggestionManager has not yet injected __gCrWeb.suggestion
        // object Handle this case gracefully. If a page has overridden
        // Array.toString, the string returned may not contain a ",",
        // hence this is a defensive measure to early return.
        NSArray* components = [result componentsSeparatedByString:@","];
        if (components.count != 2) {
          completionHandler(NO, NO);
          return;
        }

        DCHECK([components[0] isEqualToString:@"true"] ||
               [components[0] isEqualToString:@"false"]);
        BOOL hasPreviousElement = [components[0] isEqualToString:@"true"];
        DCHECK([components[1] isEqualToString:@"true"] ||
               [components[1] isEqualToString:@"false"]);
        BOOL hasNextElement = [components[1] isEqualToString:@"true"];
        completionHandler(hasPreviousElement, hasNextElement);
      }));
}

- (void)closeKeyboardForFrameWithID:(NSString*)frameID {
  std::vector<base::Value> parameters;
  autofill::ExecuteJavaScriptFunction(
      "suggestion.blurActiveElement", parameters,
      [self frameWithFrameID:frameID], _receiver,
      base::OnceCallback<void(NSString*)>());
}

- (web::WebFrame*)frameWithFrameID:(NSString*)frameID {
  DCHECK(_webFramesManager);
  return _webFramesManager->GetFrameWithId(base::SysNSStringToUTF8(frameID));
}

@end
