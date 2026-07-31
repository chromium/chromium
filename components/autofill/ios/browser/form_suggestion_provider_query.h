// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_PROVIDER_QUERY_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_PROVIDER_QUERY_H_

#import <Foundation/Foundation.h>

#import "components/autofill/core/common/unique_ids.h"
#import "components/autofill/ios/form_util/form_activity_params.h"

using ActivityType = autofill::FormActivityParams::ActivityType;
using FieldType = autofill::FormActivityParams::FieldType;

// A class containing the data necessary for FormSuggestionProvider to
// find and retrieve user-selectable suggestions for an input field of
// a web form.
@interface FormSuggestionProviderQuery : NSObject

// Form HTML 'name' attribute. If missing, its 'id' attribute. If also
// missing, a name assigned by Chrome in getFormIdentifier form utility function.
@property(readonly, nonatomic, copy) NSString* formName;

// Number ID, unique for a frame.
@property(readonly, nonatomic) autofill::FormRendererId formRendererID;

// Field HTML 'id' attribute. If missing, its 'name' attribute. If also
// missing, a unique string path assigned in getFieldIdentifier utility
// function defined in form.ts.
@property(readonly, nonatomic, copy) NSString* fieldIdentifier;

// Number ID, unique for a frame.
@property(readonly, nonatomic) autofill::FieldRendererId fieldRendererID;

// HTML input field type (i.e. 'text', 'password').
@property(readonly, nonatomic, assign) FieldType fieldType;

// Type of form activity that initiates the query.
@property(readonly, nonatomic, assign) ActivityType type;

// The value contained in a field.
@property(readonly, nonatomic, copy) NSString* typedValue;

// ID of a frame containing the form.
@property(readonly, nonatomic, copy) NSString* frameID;

// YES if only passwords should be returned as suggestions (e.g. no password
// generation suggestion).
@property(readonly, nonatomic, assign) BOOL onlyPassword;

- (instancetype)initWithFormName:(NSString*)formName
                  formRendererID:(autofill::FormRendererId)formRendererID
                 fieldIdentifier:(NSString*)fieldIdentifier
                 fieldRendererID:(autofill::FieldRendererId)fieldRendererID
                       fieldType:(FieldType)fieldType
                            type:(ActivityType)type
                      typedValue:(NSString*)typedValue
                         frameID:(NSString*)frameID
                    onlyPassword:(BOOL)onlyPassword NS_DESIGNATED_INITIALIZER;

- (instancetype)init NS_UNAVAILABLE;

// Returns true if a query comes from a focus on a field.
- (BOOL)hasFocusType;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_PROVIDER_QUERY_H_
