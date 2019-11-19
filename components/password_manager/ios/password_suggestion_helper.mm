// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_suggestion_helper.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FormData;
using autofill::PasswordFormFillData;
using base::SysNSStringToUTF16;
using base::SysNSStringToUTF8;
using base::SysUTF16ToNSString;
using base::SysUTF8ToNSString;
using password_manager::AccountSelectFillData;
using password_manager::FillData;

typedef void (^PasswordSuggestionsAvailableCompletion)(
    const password_manager::AccountSelectFillData* __nullable);

namespace {
NSString* const kPasswordFieldType = @"password";
}  // namespace

@interface PasswordSuggestionHelper ()
// Delegate to receive callbacks.
@property(nonatomic, weak, readonly) id<PasswordSuggestionHelperDelegate>
    delegate;

@end

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

@synthesize delegate = _delegate;

- (instancetype)initWithDelegate:
    (id<PasswordSuggestionHelperDelegate>)delegate {
  self = [super init];
  if (self) {
    _sentPasswordFormToPasswordManager = NO;
    _delegate = delegate;
  }
  return self;
}

#pragma mark - Public methods

- (NSArray<FormSuggestion*>*)
retrieveSuggestionsWithFormName:(NSString*)formName
                fieldIdentifier:(NSString*)fieldIdentifier
                      fieldType:(NSString*)fieldType {
  base::string16 utfFormName = SysNSStringToUTF16(formName);
  base::string16 utfFieldIdentifier = SysNSStringToUTF16(fieldIdentifier);
  BOOL isPasswordField = [fieldType isEqual:kPasswordFieldType];

  NSMutableArray<FormSuggestion*>* results = [NSMutableArray array];

  if (_fillData.IsSuggestionsAvailable(utfFormName, utfFieldIdentifier,
                                       isPasswordField)) {
    std::vector<password_manager::UsernameAndRealm> usernameAndRealms =
        _fillData.RetrieveSuggestions(utfFormName, utfFieldIdentifier,
                                      isPasswordField);

    for (const auto& usernameAndRealm : usernameAndRealms) {
      NSString* username = SysUTF16ToNSString(usernameAndRealm.username);
      NSString* realm = usernameAndRealm.realm.empty()
                            ? nil
                            : SysUTF8ToNSString(usernameAndRealm.realm);
      [results addObject:[FormSuggestion suggestionWithValue:username
                                          displayDescription:realm
                                                        icon:nil
                                                  identifier:0]];
    }
  }

  return [results copy];
}

- (void)checkIfSuggestionsAvailableForForm:(NSString*)formName
                           fieldIdentifier:(NSString*)fieldIdentifier
                                 fieldType:(NSString*)fieldType
                                      type:(NSString*)type
                                   frameID:(NSString*)frameID
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
        web::GetWebFrameWithId(webState, SysNSStringToUTF8(frameID));
    if (!frame || webState->GetLastCommittedURL().GetOrigin() !=
                      frame->GetSecurityOrigin()) {
      // Passwords is only supported on main frame and iframes with the same
      // origin.
      completion(NO);
      return;
    }
  }

  BOOL isPasswordField = [fieldType isEqual:kPasswordFieldType];
  if (!_sentPasswordFormToPasswordManager && [type isEqual:@"focus"]) {
    // Save the callback until fill data is ready.
    _suggestionsAvailableCompletion = ^(const AccountSelectFillData* fillData) {
      completion(!fillData ? NO
                           : fillData->IsSuggestionsAvailable(
                                 SysNSStringToUTF16(formName),
                                 SysNSStringToUTF16(fieldIdentifier),
                                 isPasswordField));
    };
    // Form extraction is required for this check.
    [self.delegate suggestionHelperShouldTriggerFormExtraction:self];
    return;
  }

  completion(_fillData.IsSuggestionsAvailable(
      SysNSStringToUTF16(formName), SysNSStringToUTF16(fieldIdentifier),
      isPasswordField));
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
