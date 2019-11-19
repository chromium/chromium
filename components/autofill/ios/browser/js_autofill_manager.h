// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_JS_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_JS_AUTOFILL_MANAGER_H_

#include "base/ios/block_types.h"
#include "base/values.h"
#include "components/autofill/core/common/autofill_constants.h"
#import "ios/web/public/deprecated/crw_js_injection_receiver.h"

namespace web {
class WebFrame;
}

// Loads the JavaScript file, autofill_controller.js, which contains form
// parsing and autofill functions.
@interface JsAutofillManager : NSObject

// Extracts forms from a web |frame|. Only forms with at least |requiredFields|
// fields are extracted.
// |completionHandler| is called with the JSON string of forms of a web page.
// |completionHandler| cannot be nil.
- (void)fetchFormsWithMinimumRequiredFieldsCount:(NSUInteger)requiredFieldsCount
                                         inFrame:(web::WebFrame*)frame
                               completionHandler:
                                   (void (^)(NSString*))completionHandler;

// Fills the data in JSON string |dataString| into the active form field in
// |frame|, then executes the |completionHandler|.
- (void)fillActiveFormField:(std::unique_ptr<base::Value>)data
                    inFrame:(web::WebFrame*)frame
          completionHandler:(ProceduralBlock)completionHandler;

// Fills a number of fields in the same named form for full-form Autofill.
// Applies Autofill CSS (i.e. yellow background) to filled elements.
// Only empty fields will be filled, except that field named
// |forceFillFieldIdentifier| will always be filled even if non-empty.
// |forceFillFieldIdentifier| may be null.
// Fields must be contained in |frame|.
// |completionHandler| is called after the forms are filled. |completionHandler|
// cannot be nil.
- (void)fillForm:(std::unique_ptr<base::Value>)data
    forceFillFieldIdentifier:(NSString*)forceFillFieldIdentifier
                     inFrame:(web::WebFrame*)frame
           completionHandler:(ProceduralBlock)completionHandler;

// Clear autofilled fields of the specified form and frame. Fields that are not
// currently autofilled are not modified. Field contents are cleared, and
// Autofill flag and styling are removed. 'change' events are sent for fields
// whose contents changed.
// |fieldIdentifier| identifies the field that initiated the clear action.
// |completionHandler| is called after the forms are filled. |completionHandler|
// cannot be nil.
- (void)clearAutofilledFieldsForFormName:(NSString*)formName
                         fieldIdentifier:(NSString*)fieldIdentifier
                                 inFrame:(web::WebFrame*)frame
                       completionHandler:(ProceduralBlock)completionHandler;

// Marks up the form with autofill field prediction data (diagnostic tool).
- (void)fillPredictionData:(std::unique_ptr<base::Value>)data
                   inFrame:(web::WebFrame*)frame;

// Adds a delay between filling the form fields in frame.
- (void)addJSDelayInFrame:(web::WebFrame*)frame;

// Toggles tracking form related changes in the frame.
- (void)toggleTrackingFormMutations:(BOOL)state inFrame:(web::WebFrame*)frame;

// Toggles tracking the source of the input events in the frame.
- (void)toggleTrackingUserEditedFields:(BOOL)state
                               inFrame:(web::WebFrame*)frame;

// Designated initializer. |receiver| should not be nil.
- (instancetype)initWithReceiver:(CRWJSInjectionReceiver*)receiver
    NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_JS_AUTOFILL_MANAGER_H_
