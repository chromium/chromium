// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_FAKE_JS_AUTOFILL_MANAGER_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_FAKE_JS_AUTOFILL_MANAGER_H_

#import <Foundation/Foundation.h>

#import "components/autofill/ios/browser/js_autofill_manager.h"

// Used in unit tests to verify methods calls.
@interface FakeJSAutofillManager : JsAutofillManager

// The name of the form that was most recently passed to
// |clearAutofilledFieldsForFormName:fieldIdentifier:inFrame:completionHandler:|
@property(nonatomic, copy, readonly) NSString* lastClearedFormName;

// The field identifier that was most recently passed to
// |clearAutofilledFieldsForFormName:fieldIdentifier:inFrame:completionHandler:|
@property(nonatomic, copy, readonly) NSString* lastClearedFieldIdentifier;

// The field identifier that was most recently passed to
// |clearAutofilledFieldsForFormName:fieldIdentifier:inFrame:completionHandler:|
@property(nonatomic, copy, readonly) NSString* lastClearedFrameIdentifier;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_FAKE_JS_AUTOFILL_MANAGER_H_
