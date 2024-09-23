// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

#import "components/autofill/core/browser/field_types.h"
#import "components/autofill/core/browser/ui/suggestion_type.h"

// Metadata tied to the form suggestion that gives more context around the
// suggestion.
struct FormSuggestionMetadata {
  // True if the suggestion is for a single username form.
  bool is_single_username_form = false;
};

// Enum class used to determine the feature for in-product help for the
// suggestion.
enum class SuggestionFeatureForIPH {
  // Default value
  kUnknown = 0,
  // Denoting IPH for the external account profile suggestion.
  kAutofillExternalAccountProfile = 1,
  // Denoting IPH for the plus address create suggestion.
  kPlusAddressCreation = 2
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

// The credit card icon; either a custom icon if available, or the network icon
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

// The `Suggestion::BackendId` associated with this suggestion. Would be GUID
// for the addresses and credit cards where `identifier` > 0.
@property(copy, readonly, nonatomic) NSString* backendIdentifier;

// Metadata tied to the suggestion that gives more context.
@property(assign, readonly, nonatomic) FormSuggestionMetadata metadata;

// Returns FormSuggestion (immutable) with given values.
+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                    displayDescription:(NSString*)displayDescription
                                  icon:(UIImage*)icon
                                  type:(autofill::SuggestionType)type
                     backendIdentifier:(NSString*)backendIdentifier
                        requiresReauth:(BOOL)requiresReauth
            acceptanceA11yAnnouncement:(NSString*)acceptanceA11yAnnouncement
                              metadata:(FormSuggestionMetadata)metadata;

// Returns FormSuggestion (immutable) with given values.
+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                            minorValue:(NSString*)minorValue
                    displayDescription:(NSString*)displayDescription
                                  icon:(UIImage*)icon
                                  type:(autofill::SuggestionType)type
                     backendIdentifier:(NSString*)backendIdentifier
           fieldByFieldFillingTypeUsed:
               (autofill::FieldType)fieldByFieldFillingTypeUsed
                        requiresReauth:(BOOL)requiresReauth
            acceptanceA11yAnnouncement:(NSString*)acceptanceA11yAnnouncement;

// Returns FormSuggestion (immutable) with given values.
+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                    displayDescription:(NSString*)displayDescription
                                  icon:(UIImage*)icon
                                  type:(autofill::SuggestionType)type
                     backendIdentifier:(NSString*)backendIdentifier
                        requiresReauth:(BOOL)requiresReauth;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_H_
