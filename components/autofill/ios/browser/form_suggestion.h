// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_H_
#define COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_H_

#import <Foundation/Foundation.h>
#import <UIKit/UIKit.h>

// Represents a user-selectable suggestion for a single field within a form
// on a web page.
@interface FormSuggestion : NSObject

// The string in the form to show to the user to represent the suggestion.
@property(copy, readonly, nonatomic) NSString* value;

// An optional user-visible description for this suggestion.
@property(copy, readonly, nonatomic) NSString* displayDescription;

// The credit card icon; either a custom icon if available, or the network icon
// otherwise.
@property(copy, readonly, nonatomic) UIImage* icon;

// The integer identifier associated with the suggestion. Identifiers greater
// than zero are profile or credit card identifiers.
// The frontend ids will be deprecated. See crbug.com/1394920. Along with it,
// `identifier` would be changed to PopupItemId.
@property(assign, readonly, nonatomic) NSInteger identifier;

// Indicates if the user should re-authenticate with the device before applying
// the suggestion.
@property(assign, readonly, nonatomic) BOOL requiresReauth;

// If specified, this text will be announced when this suggestion is accepted.
@property(copy, readonly, nonatomic) NSString* acceptanceA11yAnnouncement;

// If specified, shows in-product help for the suggestion.
@property(copy, nonatomic) NSString* featureForIPH;

// The `Suggestion::BackendId` associated with this suggestion. Would be GUID
// for the addresses and credit cards where `identifier` > 0.
@property(copy, readonly, nonatomic) NSString* backendIdentifier;

// Returns FormSuggestion (immutable) with given values.
+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                    displayDescription:(NSString*)displayDescription
                                  icon:(UIImage*)icon
                            identifier:(NSInteger)identifier
                     backendIdentifier:(NSString*)backendIdentifier
                        requiresReauth:(BOOL)requiresReauth
            acceptanceA11yAnnouncement:(NSString*)acceptanceA11yAnnouncement;

// Returns FormSuggestion (immutable) with given values.
+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                    displayDescription:(NSString*)displayDescription
                                  icon:(UIImage*)icon
                            identifier:(NSInteger)identifier
                     backendIdentifier:(NSString*)backendIdentifier
                        requiresReauth:(BOOL)requiresReauth;

@end

#endif  // COMPONENTS_AUTOFILL_IOS_BROWSER_FORM_SUGGESTION_H_
