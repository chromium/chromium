// Copyright 2010 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_APPLESCRIPT_ELEMENT_APPLESCRIPT_H_
#define CHROME_BROWSER_UI_COCOA_APPLESCRIPT_ELEMENT_APPLESCRIPT_H_

#import <Cocoa/Cocoa.h>

// This class is the root class for all the other applescript classes.
// It takes care of all the infrastructure type operations.
@interface ElementAppleScript : NSObject {
 @protected
  // Used by the applescript runtime to identify each unique scriptable object.
  NSNumber* _uniqueID;
  // Used by object specifier to find a scriptable object's place in a
  // collection.
  id _container;
  NSString* _containerProperty;
}

@property(nonatomic, copy) NSNumber* uniqueID;
@property(nonatomic, retain) id container;
@property(nonatomic, copy) NSString* containerProperty;

// Calculates the objectspecifier by using the uniqueID, container and
// container property.
// An object specifier is used to identify objects within a
// collection.
- (NSScriptObjectSpecifier*)objectSpecifier;

// Sets both container and property, retains container and copies property.
- (void)setContainer:(id)value property:(NSString*)property;

@end

#endif// CHROME_BROWSER_UI_COCOA_APPLESCRIPT_ELEMENT_APPLESCRIPT_H_
