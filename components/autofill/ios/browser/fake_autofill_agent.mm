// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/fake_autofill_agent.h"

#include "base/bind.h"
#include "base/task/post_task.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FakeAutofillAgent {
  NSMutableDictionary<NSString*, NSMutableArray<FormSuggestion*>*>*
      _suggestionsByFormAndFieldName;
  NSMutableDictionary<NSString*, FormSuggestion*>*
      _selectedSuggestionByFormAndFieldName;
}

- (instancetype)initWithPrefService:(PrefService*)prefService
                           webState:(web::WebState*)webState {
  self = [super initWithPrefService:prefService webState:webState];
  if (self) {
    _suggestionsByFormAndFieldName = [NSMutableDictionary dictionary];
    _selectedSuggestionByFormAndFieldName = [NSMutableDictionary dictionary];
  }
  return self;
}

#pragma mark - Public Methods

- (void)addSuggestion:(FormSuggestion*)suggestion
          forFormName:(NSString*)formName
      fieldIdentifier:(NSString*)fieldIdentifier
              frameID:(NSString*)frameID {
  NSString* key = [self keyForFormName:formName
                       fieldIdentifier:fieldIdentifier
                               frameID:frameID];
  NSMutableArray* suggestions = _suggestionsByFormAndFieldName[key];
  if (!suggestions) {
    suggestions = [NSMutableArray array];
    _suggestionsByFormAndFieldName[key] = suggestions;
  }
  [suggestions addObject:suggestion];
}

- (FormSuggestion*)selectedSuggestionForFormName:(NSString*)formName
                                 fieldIdentifier:(NSString*)fieldIdentifier
                                         frameID:(NSString*)frameID {
  NSString* key = [self keyForFormName:formName
                       fieldIdentifier:fieldIdentifier
                               frameID:frameID];
  return _selectedSuggestionByFormAndFieldName[key];
}

#pragma mark - FormSuggestionProvider

- (void)checkIfSuggestionsAvailableForForm:(NSString*)formName
                           fieldIdentifier:(NSString*)fieldIdentifier
                                 fieldType:(NSString*)fieldType
                                      type:(NSString*)type
                                typedValue:(NSString*)typedValue
                                   frameID:(NSString*)frameID
                               isMainFrame:(BOOL)isMainFrame
                            hasUserGesture:(BOOL)hasUserGesture
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  base::PostTask(
      FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
        NSString* key = [self keyForFormName:formName
                             fieldIdentifier:fieldIdentifier
                                     frameID:frameID];
        completion([_suggestionsByFormAndFieldName[key] count] ? YES : NO);
      }));
}

- (void)retrieveSuggestionsForForm:(NSString*)formName
                   fieldIdentifier:(NSString*)fieldIdentifier
                         fieldType:(NSString*)fieldType
                              type:(NSString*)type
                        typedValue:(NSString*)typedValue
                           frameID:(NSString*)frameID
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  base::PostTask(FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
                   NSString* key = [self keyForFormName:formName
                                        fieldIdentifier:fieldIdentifier
                                                frameID:frameID];
                   completion(_suggestionsByFormAndFieldName[key], self);
                 }));
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                       form:(NSString*)formName
            fieldIdentifier:(NSString*)fieldIdentifier
                    frameID:(NSString*)frameID
          completionHandler:(SuggestionHandledCompletion)completion {
  base::PostTask(FROM_HERE, {web::WebThread::UI}, base::BindOnce(^{
                   NSString* key = [self keyForFormName:formName
                                        fieldIdentifier:fieldIdentifier
                                                frameID:frameID];
                   _selectedSuggestionByFormAndFieldName[key] = suggestion;
                   completion();
                 }));
}

#pragma mark - Private Methods

- (NSString*)keyForFormName:(NSString*)formName
            fieldIdentifier:(NSString*)fieldIdentifier
                    frameID:(NSString*)frameID {
  // Uniqueness ensured because spaces are not allowed in html name attributes.
  return [NSString
      stringWithFormat:@"%@ %@ %@", formName, fieldIdentifier, frameID];
}

@end
