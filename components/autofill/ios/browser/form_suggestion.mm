// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "components/autofill/ios/browser/form_suggestion.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

@implementation FormSuggestion

- (instancetype)initWithValue:(NSString*)value
            displayDescription:(NSString*)displayDescription
                          icon:(NSString*)icon
                    identifier:(NSInteger)identifier
                requiresReauth:(BOOL)requiresReauth
    acceptanceA11yAnnouncement:(NSString*)acceptanceA11yAnnouncement {
  self = [super init];
  if (self) {
    _value = [value copy];
    _displayDescription = [displayDescription copy];
    _icon = [icon copy];
    _identifier = identifier;
    _requiresReauth = requiresReauth;
    _acceptanceA11yAnnouncement = [acceptanceA11yAnnouncement copy];
  }
  return self;
}

+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                    displayDescription:(NSString*)displayDescription
                                  icon:(NSString*)icon
                            identifier:(NSInteger)identifier
                        requiresReauth:(BOOL)requiresReauth
            acceptanceA11yAnnouncement:(NSString*)acceptanceA11yAnnouncement {
  return [[FormSuggestion alloc] initWithValue:value
                            displayDescription:displayDescription
                                          icon:icon
                                    identifier:identifier
                                requiresReauth:requiresReauth
                    acceptanceA11yAnnouncement:acceptanceA11yAnnouncement];
}

+ (FormSuggestion*)suggestionWithValue:(NSString*)value
                    displayDescription:(NSString*)displayDescription
                                  icon:(NSString*)icon
                            identifier:(NSInteger)identifier
                        requiresReauth:(BOOL)requiresReauth {
  return [[FormSuggestion alloc] initWithValue:value
                            displayDescription:displayDescription
                                          icon:icon
                                    identifier:identifier
                                requiresReauth:requiresReauth
                    acceptanceA11yAnnouncement:nil];
}

@end
