// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_H_

#import <Foundation/Foundation.h>

// Represents a user-selectable suggestion for a single field within a form
// on a web page.
@interface FormSuggestion : NSObject

// The string in the form to show to the user to represent the suggestion.
@property(copy, readonly, nonatomic) NSString* value;

// An optional user-visible description for this suggestion.
@property(copy, readonly, nonatomic) NSString* displayDescription;

// The string in the form to identify credit card icon.
@property(copy, readonly, nonatomic) NSString* icon;

// The integer identifier associated with the suggestion. Identifiers greater
// than zero are profile or credit card identifiers.
@property(assign, readonly, nonatomic) NSInteger identifier;

// Indicates if the user should re-authenticate with the device before applying
// the suggestion.
@property(assign, readonly, nonatomic) BOOL requiresReauth;

// If specified, this text will be announced when this suggestion is accepted.
@property(copy, readonly, nonatomic) NSString* acceptanceA11yAnnouncement;

// Returns FormSuggestion (immutable) with given values.
+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                    displayDescription:(NSString*)displayDescription
                                  icon:(NSString*)icon
                            identifier:(NSInteger)identifier
                        requiresReauth:(BOOL)requiresReauth
            acceptanceA11yAnnouncement:(NSString*)acceptanceA11yAnnouncement;

// Returns FormSuggestion (immutable) with given values.
+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                    displayDescription:(NSString*)displayDescription
                                  icon:(NSString*)icon
                            identifier:(NSInteger)identifier
                        requiresReauth:(BOOL)requiresReauth;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_H_
