// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_ELEMENT_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_ELEMENT_APPLESCRIPT_H_

#import <Foundation/Foundation.h>

// This class is the root class for all the other AppleScript classes.
// It takes care of all the infrastructure type operations.
@interface ElementAppleScript : NSObject

@property(nonatomic, copy) NSString* uniqueID;
@property(nonatomic, strong) NSObject* container;
@property(nonatomic, copy) NSString* containerProperty;

// Calculates the object specifier by using the uniqueID, container and
// container property.
// An object specifier is used to identify objects within a
// collection.
- (NSScriptObjectSpecifier*)objectSpecifier;

// Sets both container and property, retains container and copies property.
- (void)setContainer:(NSObject*)value property:(NSString*)property;

@end

#endif// CHROME_BROWSER_UI_COCOA_APPLESCRIPT_ELEMENT_APPLESCRIPT_H_
