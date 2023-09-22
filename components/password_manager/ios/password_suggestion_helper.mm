// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/password_manager/ios/password_suggestion_helper.h"

#include "base/strings/sys_string_conversions.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/password_form_fill_data.h"
#import "components/autofill/ios/browser/form_suggestion.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/ios/account_select_fill_data.h"
#import "components/password_manager/ios/password_manager_ios_util.h"
#import "components/password_manager/ios/password_manager_java_script_feature.h"
#include "ios/web/public/js_messaging/web_frame.h"
#import "ios/web/public/js_messaging/web_frames_manager.h"
#import "ios/web/public/web_state.h"

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

@protocol FillDataProvider <NSObject>

// True if suggestions are available for the field in form.
- (bool)
    areSuggestionsAvailableForFrameId:(NSString*)frameId
                       formRendererId:(autofill::FormRendererId)formRendererId
                      fieldRendererId:(autofill::FieldRendererId)fieldRendererId
                      isPasswordField:(bool)isPasswordField;

@end

// Represents a pending form query to be completed later with
// -runCompletion.
@interface PendingFormQuery : NSObject

// ID of the frame targeted by the query.
@property(nonatomic, strong, readonly) NSString* frameId;

// Initializes the object with a `query` to complete with `completion` for
// frame with id `frameId`.
- (instancetype)initWithQuery:(FormSuggestionProviderQuery*)query
                   completion:(SuggestionsAvailableCompletion)completion
             fillDataProvider:(id<FillDataProvider>)fillDataProvider
                      frameId:(NSString*)frameId;

// Runs the completion callback with the available fill data. This can only be
// done once in the lifetime of the query object.
- (void)runCompletion;

@end

@implementation PendingFormQuery {
  FormSuggestionProviderQuery* _query;
  SuggestionsAvailableCompletion _completion;
  id<FillDataProvider> _fillDataProvider;
}

- (instancetype)initWithQuery:(FormSuggestionProviderQuery*)query
                   completion:(SuggestionsAvailableCompletion)completion
             fillDataProvider:(id<FillDataProvider>)fillDataProvider
                      frameId:(NSString*)frameId {
  self = [super init];
  if (self) {
    _query = query;
    _completion = completion;
    _fillDataProvider = fillDataProvider;
    _frameId = frameId;
  }
  return self;
}

- (void)runCompletion {
  // Check that the completion was never run as -runCompletion
  // can only be called once.
  CHECK(_completion);

  _completion([_fillDataProvider
      areSuggestionsAvailableForFrameId:self.frameId
                         formRendererId:_query.uniqueFormID
                        fieldRendererId:_query.uniqueFieldID
                        isPasswordField:[_query isOnPasswordField]]);
  _completion = nil;
}

@end

@interface PasswordSuggestionHelper () <FillDataProvider>

@end

@implementation PasswordSuggestionHelper {
  base::WeakPtr<web::WebState> _webState;

  // Fill data keyed by frame id for the frames' forms in the webstate.
  base::flat_map<std::string, std::unique_ptr<AccountSelectFillData>>
      _fillDataMap;

  // Pending form queries that are waiting for forms extraction results.
  NSMutableArray<PendingFormQuery*>* _pendingFormQueries;
}

#pragma mark - Initialization

- (instancetype)initWithWebState:(web::WebState*)webState {
  self = [super init];
  if (self) {
    _webState = webState->GetWeakPtr();
    _pendingFormQueries = [NSMutableArray array];
  }
  return self;
}

#pragma mark - Public methods

- (NSArray<FormSuggestion*>*)
    retrieveSuggestionsWithFormID:(FormRendererId)formIdentifier
                  fieldIdentifier:(FieldRendererId)fieldIdentifier
                       forFrameId:(const std::string&)frameId
                        fieldType:(NSString*)fieldType {
  AccountSelectFillData* fillData = [self fillDataForFrameId:frameId];

  BOOL isPasswordField = [fieldType isEqual:kPasswordFieldType];

  NSMutableArray<FormSuggestion*>* results = [NSMutableArray array];

  if (fillData->IsSuggestionsAvailable(formIdentifier, fieldIdentifier,
                                       isPasswordField)) {
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
      [results
          addObject:[FormSuggestion suggestionWithValue:username
                                     displayDescription:realm
                                                   icon:nil
                                            popupItemId:autofill::PopupItemId::
                                                            kAutocompleteEntry
                                      backendIdentifier:nil
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
  // called, `completion` will be called immediately and `triggerFormExtraction`
  // will be skipped.
  // Otherwise, -suggestionHelperShouldTriggerFormExtraction: will be called
  // and `completion` will not be called until
  // -processWithPasswordFormFillData: is called.
  DCHECK(_webState.get());

  const std::string frame_id = SysNSStringToUTF8(formQuery.frameID);
  web::WebFrame* frame = [self frameWithId:frame_id];
  DCHECK(frame);
  PendingFormQuery* query =
      [[PendingFormQuery alloc] initWithQuery:formQuery
                                   completion:completion
                             fillDataProvider:self
                                      frameId:formQuery.frameID];

  AccountSelectFillData* fillData = [self fillDataForFrameId:frame_id];

  if (![formQuery hasFocusType] || !fillData->Empty()) {
    // If the query isn't triggered by focusing on the form or there is fill
    // data available, complete the check immediately with the available fill
    // data. If there is fill data, it doesn't mean that there are suggestions
    // for the form targeted by the query, but at least there are some chances
    // that suggestions will be available.
    [query runCompletion];
    return;
  }

  // Queue the form query until the fill data is processed. The queue can handle
  // concurent calls to -checkIfSuggestionsAvailableForForm, which may happen
  // when there is more than one consumer of suggestions.
  [_pendingFormQueries addObject:query];

  // Try to extract password forms from the frame's renderer content
  // because there is no knowledge of any extraction done yet. If
  // -checkIfSuggestionsAvailableForForm is called before the first forms
  // are extracted, this may result in extracting the forms twice, which
  // is fine.
  //
  // It is important to always call -suggestionHelperShouldTriggerFormExtraction
  // when there is a new query queued to make sure that the pending query is
  // completed when processing the form extraction results. Leaving a query
  // uncompleted may result in the caller waiting forever for query results
  // (e.g. having the keyboard input accessory not showing any suggestion
  // because the pipeline is blocked by an hanging request).
  [self.delegate suggestionHelperShouldTriggerFormExtraction:self
                                                     inFrame:frame];
}

- (std::unique_ptr<password_manager::FillData>)
    passwordFillDataForUsername:(NSString*)username
                     forFrameId:(const std::string&)frameId {
  return [self fillDataForFrameId:frameId]->GetFillData(
      SysNSStringToUTF16(username));
}

- (void)resetForNewPage {
  _fillDataMap.clear();
  [_pendingFormQueries removeAllObjects];
}

- (void)processWithPasswordFormFillData:(const PasswordFormFillData&)formData
                             forFrameId:(const std::string&)frameId
                            isMainFrame:(BOOL)isMainFrame
                      forSecurityOrigin:(const GURL&)origin {
  DCHECK(_webState.get());
  [self fillDataForFrameId:frameId]->Add(
      formData, IsCrossOriginIframe(_webState.get(), isMainFrame, origin));

  // "attachListenersForBottomSheet" is used to add event listeners
  // to fields which must trigger a specific behavior. In this case,
  // the username and password fields' renderer ids are sent through
  // "attachListenersForBottomSheet" so that they may trigger the
  // password bottom sheet on focus events for these specific fields.
  std::vector<autofill::FieldRendererId> rendererIds(2);
  rendererIds[0] = formData.username_element_renderer_id;
  rendererIds[1] = formData.password_element_renderer_id;
  [self.delegate attachListenersForBottomSheet:rendererIds forFrameId:frameId];

  [self completePendingFormQueriesForFrameId:frameId];
}

- (void)processWithNoSavedCredentialsWithFrameId:(const std::string&)frameId {
  [self completePendingFormQueriesForFrameId:frameId];
}

#pragma mark - FillDataProvider

- (bool)
    areSuggestionsAvailableForFrameId:(NSString*)frameId
                       formRendererId:(autofill::FormRendererId)formRendererId
                      fieldRendererId:(autofill::FieldRendererId)fieldRendererId
                      isPasswordField:(bool)isPasswordField {
  return [self fillDataForFrameId:SysNSStringToUTF8(frameId)]
      ->IsSuggestionsAvailable(formRendererId, fieldRendererId,
                               isPasswordField);
}

#pragma mark - Private

- (web::WebFrame*)frameWithId:(const std::string&)frameId {
  password_manager::PasswordManagerJavaScriptFeature* feature =
      password_manager::PasswordManagerJavaScriptFeature::GetInstance();
  return feature->GetWebFramesManager(_webState.get())->GetFrameWithId(frameId);
}

// Completes all the pending form queries that were queued for the frame that
// corresponds to `frameId`. The fill data may not be the freshest if there are
// still other outgoing forms extractions queries pending for the frame, but at
// least something will be provided and the queries completed (avoiding the
// query caller waiting indefinitely for a callback).
- (void)completePendingFormQueriesForFrameId:(const std::string&)frameId {
  NSMutableArray<PendingFormQuery*>* remainingQueries = [NSMutableArray array];
  for (PendingFormQuery* query in _pendingFormQueries) {
    if ([query.frameId isEqualToString:SysUTF8ToNSString(frameId)]) {
      [query runCompletion];
    } else {
      [remainingQueries addObject:query];
    }
  }
  _pendingFormQueries = remainingQueries;
}

- (AccountSelectFillData*)fillDataForFrameId:(const std::string&)frameId {
  // Create empty AccountSelectFillData for the frame if it doesn't exist.
  return _fillDataMap
      .insert(
          std::make_pair(frameId, std::make_unique<AccountSelectFillData>()))
      .first->second.get();
}

@end
