// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_JS_SUGGESTION_MANAGER_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_JS_SUGGESTION_MANAGER_H_

#import "ios/web/public/deprecated/crw_js_injection_receiver.h"

namespace web {
class WebFramesManager;
}  // namespace

// Loads the JavaScript file, suggestion_manager.js, which contains form parsing
// and autofill functions.
@interface JsSuggestionManager : NSObject

// Designated initializer. |receiver| should not be nil.
- (instancetype)initWithReceiver:(CRWJSInjectionReceiver*)receiver
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Sets the WebFrames manager associated with the receiver.
- (void)setWebFramesManager:(web::WebFramesManager*)framesManager;

// Focuses the next focusable element in tab order inside the web frame with
// frame id |frameID|. No action if there is no such element.
- (void)selectNextElementInFrameWithID:(NSString*)frameID;

// Focuses the next focusable element in tab order after the element specified
// by |formName| and |fieldName| in tab order inside the web frame with frame id
// |frameID|. No action if there is no such element.
- (void)selectNextElementInFrameWithID:(NSString*)frameID
                             afterForm:(NSString*)formName
                                 field:(NSString*)fieldName;

// Focuses the previous focusable element in tab order inside the web frame with
// frame id |frameID|. No action if there is no such element.
- (void)selectPreviousElementInFrameWithID:(NSString*)frameID;

// Focuses the previous focusable element in tab order from the element
// specified by |formName| and |fieldName| in tab order inside the web frame
// with frame id |frameID|. No action if there is no such element.
- (void)selectPreviousElementInFrameWithID:(NSString*)frameID
                                beforeForm:(NSString*)formName
                                     field:(NSString*)fieldName;

// Checks if the frame with frame id |frameID| contains a next and previous
// element. |completionHandler| is called with 2 BOOLs, the first indicating if
// a previous element was found, and the second indicating if a next element was
// found. |completionHandler| cannot be nil.
- (void)fetchPreviousAndNextElementsPresenceInFrameWithID:(NSString*)frameID
                                        completionHandler:(void (^)(BOOL, BOOL))
                                                              completionHandler;

// Checks if the frame with frame id |frameID| contains a next and previous
// element starting from the field specified by |formName| and |fieldName|.
// |completionHandler| is called with 2 BOOLs, the first indicating if a
// previous element was found, and the second indicating if a next element was
// found. |completionHandler| cannot be nil.
- (void)fetchPreviousAndNextElementsPresenceInFrameWithID:(NSString*)frameID
                                                  forForm:(NSString*)formName
                                                    field:(NSString*)fieldName
                                        completionHandler:(void (^)(BOOL, BOOL))
                                                              completionHandler;

// Closes the keyboard and defocuses the active input element in the frame with
// frame id |frameID|.
- (void)closeKeyboardForFrameWithID:(NSString*)frameID;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_JS_SUGGESTION_MANAGER_H_
