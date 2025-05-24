// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/browser/suggestions/suggestion.h"
#import "components/autofill/core/browser/suggestions/suggestion_type.h"
#import "components/autofill/ios/form_util/form_activity_params.h"

@protocol FormSuggestionProvider;

// Metadata tied to the form suggestion that gives more context around the
// suggestion.
struct FormSuggestionMetadata {
  // True if the suggestion is for a single username form.
  bool is_single_username_form = false;
  // True if the field that triggered the suggestion was (1) obfuscated and (2)
  // determined to be likely a real password field based on a best guess.
  bool likely_from_real_password_field = false;
};

// Enum class used to determine the feature for in-product help for the
// suggestion.
enum class SuggestionFeatureForIPH {
  // Default value
  kUnknown = 0,
  // Denoting IPH for the external account profile suggestion.
  kAutofillExternalAccountProfile = 1,
  // Denoting IPH for the plus address create suggestion.
  kPlusAddressCreation = 2,
  // Denoting IPH for the home and work address suggestion.
  kHomeWorkAddressSuggestion = 3
};

// Enum class used to determine the icon for the suggestion.
enum class SuggestionIconType {
  // Default value.
  kNone = 0,
  // Home address profile icon.
  kAccountHome = 1,
  // Work address profile icon.
  kAccountWork = 2
};

// Represents a user-selectable suggestion for a single field within a form
// on a web page.
@interface FormSuggestion : NSObject

// The string in the form to show to the user to represent the suggestion.
@property(copy, readonly, nonatomic) NSString* value;

// An optional user-visible string to hold a piece of text following the value.
@property(copy, readonly, nonatomic) NSString* minorValue;

// An optional user-visible description for this suggestion.
@property(copy, readonly, nonatomic) NSString* displayDescription;

// The suggestion icon; either a custom icon if available, or the network icon
// otherwise.
@property(copy, readonly, nonatomic) UIImage* icon;

// Denotes the suggestion type.
@property(assign, readonly, nonatomic) autofill::SuggestionType type;

// Denotes the field's filling type.
@property(assign, readonly, nonatomic)
    autofill::FieldType fieldByFieldFillingTypeUsed;

// Indicates if the user should re-authenticate with the device before applying
// the suggestion.
@property(assign, readonly, nonatomic) BOOL requiresReauth;

// If specified, this text will be announced when this suggestion is accepted.
@property(copy, readonly, nonatomic) NSString* acceptanceA11yAnnouncement;

// If specified, shows in-product help for the suggestion.
@property(assign, nonatomic) SuggestionFeatureForIPH featureForIPH;

// If specified, describes the icon type for the suggestion.
@property(assign, nonatomic) SuggestionIconType suggestionIconType;

// The payload associated with this suggestion.
@property(assign, readonly, nonatomic) autofill::Suggestion::Payload payload;

// Metadata tied to the suggestion that gives more context.
@property(assign, readonly, nonatomic) FormSuggestionMetadata metadata;

// Parameters giving the context surrounding the form activity for which that
// suggestion was generated. Must be set before the suggestion is filled when
// using the stateless FormSuggestionController.
@property(assign, nonatomic) std::optional<autofill::FormActivityParams> params;

// The FormSuggestionProvider that provided this suggestion. This allows
// knowing which provider to use for filling the suggestion. Must be set before
// the suggestion is filled when kStatelessFormSuggestionController is enabled.
@property(nonatomic, weak) id<FormSuggestionProvider> provider;

// Returns FormSuggestion (immutable) with given values.
+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                    displayDescription:(NSString*)displayDescription
                                  icon:(UIImage*)icon
                                  type:(autofill::SuggestionType)type
                               payload:(autofill::Suggestion::Payload)payload
                        requiresReauth:(BOOL)requiresReauth
            acceptanceA11yAnnouncement:(NSString*)acceptanceA11yAnnouncement
                              metadata:(FormSuggestionMetadata)metadata;

// Returns FormSuggestion (immutable) with given values.
+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                            minorValue:(NSString*)minorValue
                    displayDescription:(NSString*)displayDescription
                                  icon:(UIImage*)icon
                                  type:(autofill::SuggestionType)type
                               payload:(autofill::Suggestion::Payload)payload
           fieldByFieldFillingTypeUsed:
               (autofill::FieldType)fieldByFieldFillingTypeUsed
                        requiresReauth:(BOOL)requiresReauth
            acceptanceA11yAnnouncement:(NSString*)acceptanceA11yAnnouncement;

// Returns FormSuggestion (immutable) with given values.
+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                    displayDescription:(NSString*)displayDescription
                                  icon:(UIImage*)icon
                                  type:(autofill::SuggestionType)type
                               payload:(autofill::Suggestion::Payload)payload
                        requiresReauth:(BOOL)requiresReauth;

// Copies the contents of `formSuggestionToCopy` and sets/overrides the
// params and provider of the copy.
+ (FormSuggestion*)copy:(FormSuggestion*)formSuggestionToCopy
           andSetParams:(std::optional<autofill::FormActivityParams>)params
               provider:(id<FormSuggestionProvider>)provider;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_H_
