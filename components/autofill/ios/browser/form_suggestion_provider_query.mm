// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/form_suggestion_provider_query.h"

@implementation FormSuggestionProviderQuery

- (BOOL)hasFocusType {
  return _type == ActivityType::kFocus;
}

- (instancetype)initWithFormName:(NSString*)formName
                  formRendererID:(autofill::FormRendererId)formRendererID
                 fieldIdentifier:(NSString*)fieldIdentifier
                 fieldRendererID:(autofill::FieldRendererId)fieldRendererID
                       fieldType:(FieldType)fieldType
                            type:(ActivityType)type
                      typedValue:(NSString*)typedValue
                         frameID:(NSString*)frameID
                    onlyPassword:(BOOL)onlyPassword {
  self = [super init];
  if (self) {
    _formName = [formName copy];
    _formRendererID = formRendererID;
    _fieldIdentifier = [fieldIdentifier copy];
    _fieldRendererID = fieldRendererID;
    _fieldType = fieldType;
    _type = type;
    _typedValue = [typedValue copy];
    _frameID = [frameID copy];
    _onlyPassword = onlyPassword;
  }
  return self;
}

@end
