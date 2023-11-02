// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_PROVIDER_QUERY_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_PROVIDER_QUERY_H_

#import <Foundation/Foundation.h>

#include "components/autofill/core/common/unique_ids.h"

namespace {
NSString* const kPasswordFieldType = @"password";
}  // namespace

// A class containing the data necessary for FormSuggestionProvider to
// find and retrieve user-selectable suggestions for an input field of
// a web form.
@interface FormSuggestionProviderQuery : NSObject

// Form HTML 'name' attribute. If missing, its 'id' attribute. If also
// missing, a name assigned by Chrome in __gCrWeb.form.getFormIdentifier.
@property(readonly, nonatomic, copy) NSString* formName;

// Number ID, unique for a tab and stable within navigations.
@property(readonly, nonatomic) autofill::FormRendererId uniqueFormID;

// Field HTML 'id' attribute. If missing, its 'name' attribute. If also
// missing, a unique string path assigned in __gCrWeb.form.getFieldIdentifier.
@property(readonly, nonatomic, copy) NSString* fieldIdentifier;

// Number ID, unique for a tab and stable within navigations.
@property(readonly, nonatomic) autofill::FieldRendererId uniqueFieldID;

// HTML input field type (i.e. 'text', 'password').
@property(readonly, nonatomic, copy) NSString* fieldType;

// Type of form activity that initiates the query (i.e. 'focus', 'blur',
// 'form_changed').
@property(readonly, nonatomic, copy) NSString* type;

// The value contained in a field.
@property(readonly, nonatomic, copy) NSString* typedValue;

// ID of a frame containing the form.
@property(readonly, nonatomic, copy) NSString* frameID;

- (instancetype)initWithFormName:(NSString*)formName
                    uniqueFormID:(autofill::FormRendererId)uniqueFormID
                 fieldIdentifier:(NSString*)fieldIdentifier
                   uniqueFieldID:(autofill::FieldRendererId)uniqueFieldID
                       fieldType:(NSString*)fieldType
                            type:(NSString*)type
                      typedValue:(NSString*)typedValue
                         frameID:(NSString*)frameID NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Returns true if a query comes from a password field.
- (BOOL)isOnPasswordField;

// Returns true if a query comes from a focus on a field.
- (BOOL)hasFocusType;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_PROVIDER_QUERY_H_
