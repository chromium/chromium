// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_suggestion_helper.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#import "components/password_manager/ios/password_manager_ios_util.h"
#include "ios/web/public/js_messaging/web_frame.h"
#include "ios/web/public/js_messaging/web_frame_util.h"
#import "ios/web/public/web_state.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

using autofill::FieldRendererId;
using autofill::FormData;
using autofill::FormRendererId;
using autofill::PasswordFormFillData;
using base::SysNSStringToUTF16;
using base::SysNSStringToUTF8;
using base::SysUTF16ToNSString;
using base::SysUTF8ToNSString;
using password_manager::AccountSelectFillData;
using password_manager::FillData;
using password_manager::IsCrossOriginIframe;

typedef void (^PasswordSuggestionsAvailableCompletion)(
    const password_manager::AccountSelectFillData* __nullable);

@implementation PasswordSuggestionHelper {
  base::WeakPtr<web::WebState> _webState;

  // The value of the map is a C++ interface to cache and retrieve password
  // suggestions. The interfaces are grouped by the frame of the form.
  base::flat_map<web::WebFrame*, std::unique_ptr<AccountSelectFillData>>
      _fillDataMap;

  // YES indicates that extracted password form has been sent to the password
  // manager.
  BOOL _sentPasswordFormToPasswordManager;

  // YES indicates that suggestions from the password manager have been
  // processed.
  BOOL _processedPasswordSuggestions;

  // The completion to inform the caller of -checkIfSuggestionsAvailableForForm:
  // that suggestions are available for a given form and field.
  PasswordSuggestionsAvailableCompletion _suggestionsAvailableCompletion;
}

#pragma mark - Initialization

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _webState = webState->GetWeakPtr();
    _sentPasswordFormToPasswordManager = NO;
  }
  return self;
}

#pragma mark - Public methods

- (NSArray<FormSuggestion*>*)
    retrieveSuggestionsWithFormID:(FormRendererId)formIdentifier
                  fieldIdentifier:(FieldRendererId)fieldIdentifier
                          inFrame:(web::WebFrame*)frame
                        fieldType:(NSString*)fieldType {
  AccountSelectFillData* fillData = [self getFillDataFromFrame:frame];

  BOOL isPasswordField = [fieldType isEqual:kPasswordFieldType];

  NSMutableArray<FormSuggestion*>* results = [NSMutableArray array];

  if (fillData && fillData->IsSuggestionsAvailable(
                      formIdentifier, fieldIdentifier, isPasswordField)) {
    std::vector<password_manager::UsernameAndRealm> usernameAndRealms =
        fillData->RetrieveSuggestions(formIdentifier, fieldIdentifier,
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
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  // When password controller's -processWithPasswordFormFillData: is already
  // called, |completion| will be called immediately and |triggerFormExtraction|
  // will be skipped.
  // Otherwise, -suggestionHelperShouldTriggerFormExtraction: will be called
  // and |completion| will not be called until
  // -processWithPasswordFormFillData: is called.
  DCHECK(_webState.get());
  web::WebFrame* frame = web::GetWebFrameWithId(
      _webState.get(), SysNSStringToUTF8(formQuery.frameID));
  DCHECK(frame);

  BOOL isPasswordField = [formQuery isOnPasswordField];
  if ([formQuery hasFocusType] &&
      (!_sentPasswordFormToPasswordManager || !_processedPasswordSuggestions)) {
    // Save the callback until fill data is ready.
    _suggestionsAvailableCompletion = ^(const AccountSelectFillData* fillData) {
      completion(!fillData ? NO
                           : fillData->IsSuggestionsAvailable(
                                 formQuery.uniqueFormID,
                                 formQuery.uniqueFieldID, isPasswordField));
    };
    if (!_sentPasswordFormToPasswordManager) {
      // Form extraction is required for this check.
      [self.delegate suggestionHelperShouldTriggerFormExtraction:self
                                                         inFrame:frame];
    }
    return;
  }

  AccountSelectFillData* fillData = [self getFillDataFromFrame:frame];

  completion(fillData && fillData->IsSuggestionsAvailable(
                             formQuery.uniqueFormID, formQuery.uniqueFieldID,
                             isPasswordField));
}

- (std::unique_ptr<password_manager::FillData>)
    passwordFillDataForUsername:(NSString*)username
                        inFrame:(web::WebFrame*)frame {
  AccountSelectFillData* fillData = [self getFillDataFromFrame:frame];
  return fillData ? fillData->GetFillData(SysNSStringToUTF16(username))
                  : nullptr;
}

- (void)resetForNewPage {
  _fillDataMap.clear();
  _sentPasswordFormToPasswordManager = NO;
  _processedPasswordSuggestions = NO;
  _suggestionsAvailableCompletion = nil;
}

- (void)processWithPasswordFormFillData:(const PasswordFormFillData&)formData
                                inFrame:(web::WebFrame*)frame
                            isMainFrame:(BOOL)isMainFrame
                      forSecurityOrigin:(const GURL&)origin {
  // TODO(crbug.com/1383214): Rewrite the code to eliminate the chance of using
  // |frame| after its destruction.
  AccountSelectFillData* fillData = [self getFillDataFromFrame:frame];
  if (!fillData) {
    auto it = _fillDataMap.insert(
        std::make_pair(frame, std::make_unique<AccountSelectFillData>()));
    fillData = it.first->second.get();
  }

  DCHECK(_webState.get());
  fillData->Add(formData,
                IsCrossOriginIframe(_webState.get(), isMainFrame, origin));

  _processedPasswordSuggestions = YES;

  if (_suggestionsAvailableCompletion) {
    _suggestionsAvailableCompletion(fillData);
    _suggestionsAvailableCompletion = nil;
  }
}
- (void)processWithNoSavedCredentials {
  // Only update |_processedPasswordSuggestions| if PasswordManager was
  // queried for some forms. This is needed to protect against a case when
  // there are no forms on the pageload and they are added dynamically.
  if (_sentPasswordFormToPasswordManager) {
    _processedPasswordSuggestions = YES;
  }

  if (_suggestionsAvailableCompletion) {
    _suggestionsAvailableCompletion(nullptr);
  }
  _suggestionsAvailableCompletion = nil;
}

- (void)updateStateOnPasswordFormExtracted {
  _sentPasswordFormToPasswordManager = YES;
}

#pragma mark - Private methods

- (AccountSelectFillData*)getFillDataFromFrame:(web::WebFrame*)frame {
  auto it = _fillDataMap.find(frame);
  if (it == _fillDataMap.end()) {
    return nullptr;
  }
  return it->second.get();
}

@end
