// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/fake_autofill_agent.h"

#include "base/functional/bind.h"
#include "ios/web/public/thread/web_task_traits.h"
#include "ios/web/public/thread/web_thread.h"

using autofill::FormRendererId;
using autofill::FieldRendererId;

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

- (void)checkIfSuggestionsAvailableForForm:
            (FormSuggestionProviderQuery*)formQuery
                            hasUserGesture:(BOOL)hasUserGesture
                                  webState:(web::WebState*)webState
                         completionHandler:
                             (SuggestionsAvailableCompletion)completion {
  web::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(^{
        NSArray<FormSuggestion*>* formSuggestions =
            [self suggestionsForFormName:formQuery.formName
                         fieldIdentifier:formQuery.fieldIdentifier
                                 frameID:formQuery.frameID];
        completion([formSuggestions count] ? YES : NO);
      }));
}

- (void)retrieveSuggestionsForForm:(FormSuggestionProviderQuery*)formQuery
                          webState:(web::WebState*)webState
                 completionHandler:(SuggestionsReadyCompletion)completion {
  web::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(^{
        NSArray<FormSuggestion*>* formSuggestions =
            [self suggestionsForFormName:formQuery.formName
                         fieldIdentifier:formQuery.fieldIdentifier
                                 frameID:formQuery.frameID];
        completion(formSuggestions, self);
      }));
}

- (void)didSelectSuggestion:(FormSuggestion*)suggestion
                    atIndex:(NSInteger)index
                       form:(NSString*)formName
             formRendererID:(FormRendererId)formRendererID
            fieldIdentifier:(NSString*)fieldIdentifier
            fieldRendererID:(FieldRendererId)fieldRendererID
                    frameID:(NSString*)frameID
          completionHandler:(SuggestionHandledCompletion)completion {
  web::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(^{
        [self selectSuggestion:suggestion
                   forFormName:formName
               fieldIdentifier:fieldIdentifier
                       frameID:frameID];
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

- (NSArray<FormSuggestion*>*)suggestionsForFormName:(NSString*)formName
                                    fieldIdentifier:(NSString*)fieldIdentifier
                                            frameID:(NSString*)frameID {
  NSString* key = [self keyForFormName:formName
                       fieldIdentifier:fieldIdentifier
                               frameID:frameID];
  return _suggestionsByFormAndFieldName[key];
}

- (void)selectSuggestion:(FormSuggestion*)formSuggestion
             forFormName:(NSString*)formName
         fieldIdentifier:(NSString*)fieldIdentifier
                 frameID:(NSString*)frameID {
  NSString* key = [self keyForFormName:formName
                       fieldIdentifier:fieldIdentifier
                               frameID:frameID];
  _selectedSuggestionByFormAndFieldName[key] = formSuggestion;
}

@end
