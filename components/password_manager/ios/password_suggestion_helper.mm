// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_suggestion_helper.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FormData;
using autofill::PasswordFormFillData;
using autofill::FormRendererId;
using autofill::FieldRendererId;
using base::SysNSStringToUTF16;
using base::SysNSStringToUTF8;
using base::SysUTF16ToNSString;
using base::SysUTF8ToNSString;
using password_manager::AccountSelectFillData;
using password_manager::FillData;

typedef void (^PasswordSuggestionsAvailableCompletion)(
    const password_manager::AccountSelectFillData* __nullable);

@implementation PasswordSuggestionHelper {
  // The C++ interface to cache and retrieve password suggestions.
  AccountSelectFillData _fillData;

  // YES indicates that extracted password form has been sent to the password
  // manager.
  BOOL _sentPasswordFormToPasswordManager;

  // The completion to inform the caller of -checkIfSuggestionsAvailableForForm:
  // that suggestions are available for a given form and field.
  PasswordSuggestionsAvailableCompletion _suggestionsAvailableCompletion;
}

- (instancetype)init {
  self = [super init];
  if (self) {
    _sentPasswordFormToPasswordManager = NO;
  }
  return self;
}

#pragma mark - Public methods

- (NSArray<FormSuggestion*>*)
    retrieveSuggestionsWithFormID:(FormRendererId)formIdentifier
                  fieldIdentifier:(FieldRendererId)fieldIdentifier
                        fieldType:(NSString*)fieldType {
  BOOL isPasswordField = [fieldType isEqual:kPasswordFieldType];

  NSMutableArray<FormSuggestion*>* results = [NSMutableArray array];

  if (_fillData.IsSuggestionsAvailable(formIdentifier, fieldIdentifier,
                                       isPasswordField)) {
    std::vector<password_manager::UsernameAndRealm> usernameAndRealms =
        _fillData.RetrieveSuggestions(formIdentifier, fieldIdentifier,
                                      isPasswordField);

    for (const auto& usernameAndRealm : usernameAndRealms) {
      NSString* username = SysUTF16ToNSString(usernameAndRealm.username);
      NSString* realm = nil;
      if (!usernameAndRealm.realm.empty()) {
        url::Origin origin = url::Origin::Create(GURL(usernameAndRealm.realm));
        realm = SysUTF8ToNSString(password_manager::GetShownOrigin(origin));
      }
      [results addObject:[FormSuggestion suggestionWithValue:username
                                          displayDescription:realm
                                                        icon:nil
                                                  identifier:0
                                              requiresReauth:YES]];
    }
  }

  return [results copy];
}

- (void)checkIfSuggestionsAvailableForForm:
            (FormSuggestionProviderQuery*)formQuery
                               isMainFrame:(BOOL)isMainFrame
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  // When password controller's -processWithPasswordFormFillData: is already
  // called, |completion| will be called immediately and |triggerFormExtraction|
  // will be skipped.
  // Otherwise, -suggestionHelperShouldTriggerFormExtraction: will be called
  // and |completion| will not be called until
  // -processWithPasswordFormFillData: is called.
  // For unsupported form, |completion| will be called immediately and
  // -suggestionHelperShouldTriggerFormExtraction: will be skipped.
  if (!isMainFrame) {
    web::WebFrame* frame =
        web::GetWebFrameWithId(webState, SysNSStringToUTF8(formQuery.frameID));
    if (!frame || webState->GetLastCommittedURL().GetOrigin() !=
                      frame->GetSecurityOrigin()) {
      // Passwords is only supported on main frame and iframes with the same
      // origin.
      completion(NO);
      return;
    }
  }

  BOOL isPasswordField = [formQuery isOnPasswordField];
  if (!_sentPasswordFormToPasswordManager && [formQuery hasFocusType]) {
    // Save the callback until fill data is ready.
    _suggestionsAvailableCompletion = ^(const AccountSelectFillData* fillData) {
      completion(!fillData ? NO
                           : fillData->IsSuggestionsAvailable(
                                 formQuery.uniqueFormID,
                                 formQuery.uniqueFieldID, isPasswordField));
    };
    // Form extraction is required for this check.
    [self.delegate suggestionHelperShouldTriggerFormExtraction:self];
    return;
  }

  completion(_fillData.IsSuggestionsAvailable(
      formQuery.uniqueFormID, formQuery.uniqueFieldID, isPasswordField));
}

- (std::unique_ptr<password_manager::FillData>)getFillDataForUsername:
    (NSString*)username {
  return _fillData.GetFillData(SysNSStringToUTF16(username));
}

- (void)resetForNewPage {
  _fillData.Reset();
  _sentPasswordFormToPasswordManager = NO;
  _suggestionsAvailableCompletion = nil;
}

- (void)processWithPasswordFormFillData:(const PasswordFormFillData&)formData {
  _fillData.Add(formData);

  if (_suggestionsAvailableCompletion) {
    _suggestionsAvailableCompletion(&_fillData);
    _suggestionsAvailableCompletion = nil;
  }
}
- (void)processWithNoSavedCredentials {
  if (_suggestionsAvailableCompletion) {
    _suggestionsAvailableCompletion(nullptr);
  }
  _suggestionsAvailableCompletion = nil;
}

- (void)updateStateOnPasswordFormExtracted {
  _sentPasswordFormToPasswordManager = YES;
}

@end
