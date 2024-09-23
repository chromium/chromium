// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/form_suggestion_provider_query.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using autofill::FormRendererId;
using autofill::FieldRendererId;

namespace {
NSString* const kTestFormName = @"login_form";
FormRendererId const kTestFormRendererID = FormRendererId(0);
NSString* const kTestUsernameFieldIdentifier = @"username";
NSString* const kTestPasswordFieldIdentifier = @"pw";
FieldRendererId const kTestFieldRendererID = FieldRendererId(1);
NSString* const kTestTextFieldType = @"text";
NSString* const kTestFocusType = @"focus";
NSString* const kTestInputType = @"input";
NSString* const kTestTypedValue = @"smth";
NSString* const kTestFrameID = @"someframe";
}  // namespace

using FormSuggestionProviderQueryTest = PlatformTest;

// Tests that a query caused by focusing an obfuscated field is processed
// correctly.
TEST_F(FormSuggestionProviderQueryTest, PasswordFieldFocused) {
  FormSuggestionProviderQuery* formQuery = [[FormSuggestionProviderQuery alloc]
      initWithFormName:kTestFormName
        formRendererID:kTestFormRendererID
       fieldIdentifier:kTestPasswordFieldIdentifier
       fieldRendererID:kTestFieldRendererID
             fieldType:kObfuscatedFieldType
                  type:kTestFocusType
            typedValue:kTestTypedValue
               frameID:kTestFrameID];

  EXPECT_TRUE([formQuery hasFocusType]);
}

// Tests that a query caused by input in a non-obfuscated field id processed
// correctly.
TEST_F(FormSuggestionProviderQueryTest, InputInTextField) {
  FormSuggestionProviderQuery* formQuery = [[FormSuggestionProviderQuery alloc]
      initWithFormName:kTestFormName
        formRendererID:kTestFormRendererID
       fieldIdentifier:kTestUsernameFieldIdentifier
       fieldRendererID:kTestFieldRendererID
             fieldType:kTestTextFieldType
                  type:kTestInputType
            typedValue:kTestTypedValue
               frameID:kTestFrameID];

  EXPECT_FALSE([formQuery hasFocusType]);
}
