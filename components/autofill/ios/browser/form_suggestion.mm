// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/form_suggestion.h"

#import <optional>

#import "components/autofill/ios/form_util/form_activity_params.h"

@implementation FormSuggestion

- (instancetype)initWithValue:(NSString*)value
                     minorValue:(NSString*)minorValue
             displayDescription:(NSString*)displayDescription
                           icon:(UIImage*)icon
          hasCustomCardArtImage:(BOOL)hasCustomCardArtImage
                           type:(autofill::SuggestionType)type
                        payload:(autofill::Suggestion::Payload)payload
    fieldByFieldFillingTypeUsed:(autofill::FieldType)fieldByFieldFillingTypeUsed
                 requiresReauth:(BOOL)requiresReauth
     acceptanceA11yAnnouncement:(NSString*)acceptanceA11yAnnouncement
                       metadata:(FormSuggestionMetadata)metadata
                         params:
                             (std::optional<autofill::FormActivityParams>)params
                       provider:(id<FormSuggestionProvider>)provider
                  featureForIPH:(SuggestionFeatureForIPH)featureForIPH
             suggestionIconType:(SuggestionIconType)suggestionIconType {
  self = [super init];
  if (self) {
    _value = [value copy];
    _minorValue = [minorValue copy];
    _displayDescription = [displayDescription copy];
    _icon = [icon copy];
    _hasCustomCardArtImage = hasCustomCardArtImage;
    _type = type;
    _payload = payload;
    _fieldByFieldFillingTypeUsed = fieldByFieldFillingTypeUsed;
    _requiresReauth = requiresReauth;
    _acceptanceA11yAnnouncement = [acceptanceA11yAnnouncement copy];
    _metadata = metadata;
    _params = params;
    _provider = provider;
    _featureForIPH = featureForIPH;
    _suggestionIconType = suggestionIconType;
  }
  return self;
}

+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                            minorValue:(NSString*)minorValue
                    displayDescription:(NSString*)displayDescription
                                  icon:(UIImage*)icon
                 hasCustomCardArtImage:(BOOL)hasCustomCardArtImage
                                  type:(autofill::SuggestionType)type
                               payload:(autofill::Suggestion::Payload)payload
           fieldByFieldFillingTypeUsed:
               (autofill::FieldType)fieldByFieldFillingTypeUsed
                        requiresReauth:(BOOL)requiresReauth
            acceptanceA11yAnnouncement:(NSString*)acceptanceA11yAnnouncement {
  return [[FormSuggestion alloc] initWithValue:value
                                    minorValue:minorValue
                            displayDescription:displayDescription
                                          icon:icon
                         hasCustomCardArtImage:hasCustomCardArtImage
                                          type:type
                                       payload:payload
                   fieldByFieldFillingTypeUsed:fieldByFieldFillingTypeUsed
                                requiresReauth:requiresReauth
                    acceptanceA11yAnnouncement:acceptanceA11yAnnouncement
                                      metadata:FormSuggestionMetadata()
                                        params:std::nullopt
                                      provider:nil
                                 featureForIPH:SuggestionFeatureForIPH::kUnknown
                            suggestionIconType:SuggestionIconType::kNone];
}

+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                    displayDescription:(NSString*)displayDescription
                                  icon:(UIImage*)icon
                                  type:(autofill::SuggestionType)type
                               payload:(autofill::Suggestion::Payload)payload
                        requiresReauth:(BOOL)requiresReauth
            acceptanceA11yAnnouncement:(NSString*)acceptanceA11yAnnouncement
                              metadata:(FormSuggestionMetadata)metadata {
  return [[FormSuggestion alloc] initWithValue:value
                                    minorValue:nil
                            displayDescription:displayDescription
                                          icon:icon
                         hasCustomCardArtImage:NO
                                          type:type
                                       payload:payload
                   fieldByFieldFillingTypeUsed:autofill::FieldType::EMPTY_TYPE
                                requiresReauth:requiresReauth
                    acceptanceA11yAnnouncement:acceptanceA11yAnnouncement
                                      metadata:metadata
                                        params:std::nullopt
                                      provider:nil
                                 featureForIPH:SuggestionFeatureForIPH::kUnknown
                            suggestionIconType:SuggestionIconType::kNone];
}

+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                            minorValue:(NSString*)minorValue
                    displayDescription:(NSString*)displayDescription
                                  icon:(UIImage*)icon
                                  type:(autofill::SuggestionType)type
                               payload:(autofill::Suggestion::Payload)payload
           fieldByFieldFillingTypeUsed:
               (autofill::FieldType)fieldByFieldFillingTypeUsed
                        requiresReauth:(BOOL)requiresReauth
            acceptanceA11yAnnouncement:(NSString*)acceptanceA11yAnnouncement {
  return [[FormSuggestion alloc] initWithValue:value
                                    minorValue:minorValue
                            displayDescription:displayDescription
                                          icon:icon
                         hasCustomCardArtImage:NO
                                          type:type
                                       payload:payload
                   fieldByFieldFillingTypeUsed:fieldByFieldFillingTypeUsed
                                requiresReauth:requiresReauth
                    acceptanceA11yAnnouncement:acceptanceA11yAnnouncement
                                      metadata:FormSuggestionMetadata()
                                        params:std::nullopt
                                      provider:nil
                                 featureForIPH:SuggestionFeatureForIPH::kUnknown
                            suggestionIconType:SuggestionIconType::kNone];
}

+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                    displayDescription:(NSString*)displayDescription
                                  icon:(UIImage*)icon
                                  type:(autofill::SuggestionType)type
                               payload:(autofill::Suggestion::Payload)payload
                        requiresReauth:(BOOL)requiresReauth {
  return [[FormSuggestion alloc] initWithValue:value
                                    minorValue:nil
                            displayDescription:displayDescription
                                          icon:icon
                         hasCustomCardArtImage:NO
                                          type:type
                                       payload:payload
                   fieldByFieldFillingTypeUsed:autofill::FieldType::EMPTY_TYPE
                                requiresReauth:requiresReauth
                    acceptanceA11yAnnouncement:nil
                                      metadata:FormSuggestionMetadata()
                                        params:std::nullopt
                                      provider:nil
                                 featureForIPH:SuggestionFeatureForIPH::kUnknown
                            suggestionIconType:SuggestionIconType::kNone];
}

+ (FormSuggestion*)copy:(FormSuggestion*)formSuggestionToCopy
           andSetParams:(std::optional<autofill::FormActivityParams>)params
               provider:(id<FormSuggestionProvider>)provider {
  return [[FormSuggestion alloc]
                    initWithValue:formSuggestionToCopy.value
                       minorValue:formSuggestionToCopy.minorValue
               displayDescription:formSuggestionToCopy.displayDescription
                             icon:formSuggestionToCopy.icon
            hasCustomCardArtImage:formSuggestionToCopy.hasCustomCardArtImage
                             type:formSuggestionToCopy.type
                          payload:formSuggestionToCopy.payload
      fieldByFieldFillingTypeUsed:formSuggestionToCopy
                                      .fieldByFieldFillingTypeUsed
                   requiresReauth:formSuggestionToCopy.requiresReauth
       acceptanceA11yAnnouncement:formSuggestionToCopy
                                      .acceptanceA11yAnnouncement
                         metadata:formSuggestionToCopy.metadata
                           params:params
                         provider:provider
                    featureForIPH:formSuggestionToCopy.featureForIPH
               suggestionIconType:formSuggestionToCopy.suggestionIconType];
}

@end
