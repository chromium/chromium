// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/form_suggestion_provider_query.h"

@implementation FormSuggestionProviderQuery

- (BOOL)hasFocusType {
  return [_type isEqual:@"focus"];
}

- (instancetype)initWithFormName:(NSString*)formName
                  formRendererID:(autofill::FormRendererId)formRendererID
                 fieldIdentifier:(NSString*)fieldIdentifier
                 fieldRendererID:(autofill::FieldRendererId)fieldRendererID
                       fieldType:(NSString*)fieldType
                            type:(NSString*)type
                      typedValue:(NSString*)typedValue
                         frameID:(NSString*)frameID {
  self = [super init];
  if (self) {
    _formName = [formName copy];
    _formRendererID = formRendererID;
    _fieldIdentifier = [fieldIdentifier copy];
    _fieldRendererID = fieldRendererID;
    _fieldType = [fieldType copy];
    _type = [type copy];
    _typedValue = [typedValue copy];
    _frameID = [frameID copy];
  }
  return self;
}

@end
